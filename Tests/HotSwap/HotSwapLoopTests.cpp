// T12 §12.1：热插拔主循环的端到端——M1 核心承诺的唯一证伪者。
// 流程 = CreatePod{A,B,C} → 行为==A → Eject(A) → A′ 覆盖 A 的位置（覆盖本身即
// 「锁真解了」的断言）→ Adopt(A) → 行为==A′ → B/C 的实例从第一步起没被碰过。
// 12.2：本目录自 M1 起按主干对待——此后任何动 Loader / 账本 / Eject 路径的改动都必须跑它。
#include "Vase/Host/PluginHost.h"
#include "fixtures/BCommon.h"

#include "AdoptExpectations.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Evidence.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/Pod/Context.h"
#include "Vase/Pod/Pod.h"

#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <iterator>
#include <string>
#include <vector>
// 两平台对 std::error_code 的归属判定不同（MSVC STL 的映射认 <filesystem> 也提供它，
// libc++ 只认 <system_error>）：本行留着，Windows 报「未被直接使用」、Linux 报「没有头
// 提供它」；摘掉则反过来。只有「留着 + 在 Windows 抑制」能同时过两条 debug 线。
#include <system_error> // NOLINT(misc-include-cleaner)

namespace
{

// 工作副本目录：测试私有，避免碰构建产物本体（覆盖动作改的是「A 的位置」上的文件）。
//
// **声明顺序承重**（R114）：`ws` 必须声明在 `PluginHost` 之前——~PluginHost 卸载全部
// 驻留二进制，而目录的删除会失败于仍被映射的文件；先声明的先析构，恰好给出
// 「host 先死、目录后删」的序。
class SwapWorkspace
{
public:
    SwapWorkspace()
    {
        std::error_code ec;
        Dir = std::filesystem::temp_directory_path(ec) /
              ("vase-hotswap-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(Dir, ec);
        APath = Dir / std::filesystem::path(VASE_FIXTURE_VERSIONEDA).filename(); // 同名！
        std::filesystem::copy_file(VASE_FIXTURE_VERSIONEDA, APath, std::filesystem::copy_options::overwrite_existing,
                                   ec);
        EXPECT_FALSE(ec) << ec.message();
    }

    ~SwapWorkspace()
    {
        std::error_code ec;
        // 不留尾巴：每次跑都在 temp 下留一个 vase-hotswap-* 目录是纯污染。
        // 不 assert——清理失败不该把用例判红（真失败会在下一轮的路径冲突里显形）。
        std::filesystem::remove_all(Dir, ec);
    }

    SwapWorkspace(const SwapWorkspace&) = delete;
    SwapWorkspace& operator=(const SwapWorkspace&) = delete;
    SwapWorkspace(SwapWorkspace&&) = delete;
    SwapWorkspace& operator=(SwapWorkspace&&) = delete;

    // §12.1 的核心动作：把 A′ 的字节写到「A 的位置」上。**这一步本身就是断言**——
    // Eject 之后文件必须能被覆盖写；写不动（Windows sharing violation）就是卸载路径坏了，
    // 该修的是卸载路径，不是放宽这里。
    void InstallPrime() const
    {
        std::error_code ec;
        std::filesystem::copy_file(VASE_FIXTURE_VERSIONEDAPRIME, APath,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        EXPECT_FALSE(ec) << ec.message();
    }

    // 与 InstallPrime 同一动作、只是参数化：换件谱的每一级要落不同的字节。
    // 与 InstallPrime 同一条断言——写不动（Windows sharing violation）就是卸载路径坏了。
    void Install(const std::filesystem::path& source) const
    {
        std::error_code ec;
        std::filesystem::copy_file(source, APath, std::filesystem::copy_options::overwrite_existing, ec);
        EXPECT_FALSE(ec) << ec.message();
    }

    std::filesystem::path Dir;
    std::filesystem::path APath;
};

// A 与 B、C 同一局；A 的路径是工作副本（会被覆盖），B/C 是构建产物本体（只读地加载）。
vase::LoadPlan LoopPlan(const std::filesystem::path& aPath)
{
    vase::LoadPlan plan;
    plan.Ordered.push_back({.Id = "Vase.VersionedA", .BinaryPath = aPath});
    plan.Ordered.push_back({.Id = "Vase.NeighborB", .BinaryPath = VASE_FIXTURE_NEIGHBORB});
    plan.Ordered.push_back({.Id = "Vase.NeighborC", .BinaryPath = VASE_FIXTURE_NEIGHBORC});
    return plan;
}

// 局内 A 的 Adopt 请求：路径 = 工作副本位（换件写字的地方），兄弟集 = 这局另两只二进制名。
// 期望用 MakeVersionedAExpectation 一份到底——A/A′ 描述符逐字节相同（T5 评审），换件不换期望。
vase::AdoptRequest LoopAdoptRequest(const vase::ManifestExpectation& expected, const std::filesystem::path& aPath)
{
    vase::AdoptRequest request;
    request.Id = expected.Id;
    request.BinaryPath = aPath;
    request.Expected = &expected;
    request.SiblingBinaries = {
        std::filesystem::path{VASE_FIXTURE_NEIGHBORB}.filename().string(),
        std::filesystem::path{VASE_FIXTURE_NEIGHBORC}.filename().string(),
    };
    return request;
}

// 宿主侧解析（§5.6：宿主的解析不落边）。`readsV2` = 被读的那一版提供的是 v2 接口
// （rung3 声明并注册 v2；其余级都是 v1）——`Provide<T>` 的键来自接口常量，不是描述符。
int CounterValue(vase::Pod* pod, bool readsV2)
{
    if (readsV2)
    {
        return pod->Root().Get<samples_fixture::ICounterV2>().Value();
    }
    return pod->Root().Get<samples_fixture::ICounter>().Value(); // Get<T>() 返回 T&（R112）
}

TEST(HotSwap, FullLoopFlipsBehaviorAndNeverTouchesNeighbor)
{
    const SwapWorkspace ws; // 先于 host 声明：见 SwapWorkspace 的析构序说明
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(LoopPlan(ws.APath)).Value();
    vase::Pod* const pod = host.Resolve(h);

    EXPECT_EQ(CounterValue(pod, false), 1); // 行为 == A
    const auto* const heart1 = &pod->Root().Get<samples_fixture::IHeart>();
    const auto* const pulse1 = &pod->Root().Get<samples_fixture::IPulse>();
    for (int i = 1; i <= 5; ++i)
    {
        pod->Root().Emit(samples_fixture::TickEvent{i}); // B、C 心跳/脉冲正常
    }
    EXPECT_EQ(heart1->Beats(), 5);  // 切换前恰好 5 次 tick：一次都不能丢
    EXPECT_EQ(pulse1->Pulses(), 5); // C 同刻度（M3/D90：两只邻居都要有证人）

    const vase::ManifestExpectation versionedA = testing_support::MakeVersionedAExpectation();
    ASSERT_TRUE(host.EjectPlugin(h, "Vase.VersionedA").IsOk()); // 三档之档一、档二在报告里结算
    ws.InstallPrime(); // A′ 覆盖 A 的位置；A/A′ 描述符逐字节相同，期望不需要跟着换
    const vase::Result<vase::AdoptReport> adopt = host.AdoptPlugin(h, LoopAdoptRequest(versionedA, ws.APath));
    ASSERT_TRUE(adopt.IsOk()) << adopt.GetError().Message();
    // §12.1 要的是「真 Eject 之后走**全新装载**分支」。POSIX（Linux/macOS）上 `ReusedResidentImage`
    // 是唯一判据；Windows 上 `InstallPrime()` 的覆盖断言先红（镜像还映射着就写不动），它是语义锚点。
    // IdentityVerified 在成功路径上被无条件置 true（PluginHost.cpp），是报告字段不是判据。
    EXPECT_FALSE(adopt.Value().ReusedResidentImage);

    EXPECT_EQ(CounterValue(pod, false), 2); // 行为 == A′（不是 A！）
    const auto* const heart2 = &pod->Root().Get<samples_fixture::IHeart>();
    const auto* const pulse2 = &pod->Root().Get<samples_fixture::IPulse>();
    // B/C 实例指针在这里不能判别：新对象会落回被释放的堆块（实测同一二进制时红时绿）——
    // 「邻居未被触碰」由下面两条精确计数承担。
    pod->Root().Emit(samples_fixture::TickEvent{99});
    EXPECT_EQ(heart2->Beats(), 6); // 5 次 tick + 这一次恰好 6：计数没被重置过（真正承重的那条）
    EXPECT_EQ(pulse2->Pulses(), 6);

    const vase::PodReport report = host.DestroyPod(h);
    EXPECT_TRUE(report.Clean());
    ASSERT_EQ(report.HotSwapLog.size(), 2U); // §9.2 v3「中途进出过谁」
    // 迭代器而非 operator[]：非常量下标过不了 cppcoreguidelines-pro-bounds-*（计划「tidy 形态约束」）。
    const auto ejectEntry = report.HotSwapLog.begin();
    EXPECT_NE(ejectEntry->find("eject:Vase.VersionedA"), std::string::npos);
    EXPECT_NE(std::next(ejectEntry)->find("adopt:Vase.VersionedA"), std::string::npos);
}

TEST(HotSwap, FiftyRoundsBehaveLikeFirstTime)
{
    // §12.1 判据 3a 的全量形（M3/D90/D98）：三插件局、对叶 A 做 50 轮 Eject+Adopt，
    // 断言**其余实例从第一步起全程未受扰**。每轮派 k 拍是承重的——不派拍的话
    // 「Beats 恰为累计拍数」两端都是 0、断言恒真（TimerNoReentryTests 注释点名的假绿世界）。
    constexpr int kTicksPerRound = 2;
    const SwapWorkspace ws;
    vase::PluginHost host;
    const vase::ManifestExpectation versionedA = testing_support::MakeVersionedAExpectation();
    const vase::PodHandle h = host.CreatePod(LoopPlan(ws.APath)).Value();
    bool prime = false;
    int ticks = 0;
    for (int round = 0; round < 50; ++round)
    {
        vase::Pod* const pod = host.Resolve(h);
        ASSERT_EQ(CounterValue(pod, false), prime ? 2 : 1) << "round " << round;
        for (int step = 0; step < kTicksPerRound; ++step)
        {
            pod->Root().Emit(samples_fixture::TickEvent{ticks});
            ++ticks;
        }
        // 两只邻居的**精确计数**：循环内每轮都断（失败能定位到轮次）。
        ASSERT_EQ(pod->Root().Get<samples_fixture::IHeart>().Beats(), ticks) << "round " << round;
        ASSERT_EQ(pod->Root().Get<samples_fixture::IPulse>().Pulses(), ticks) << "round " << round;

        ASSERT_TRUE(host.EjectPlugin(h, "Vase.VersionedA").IsOk());
        std::error_code ec;
        const std::filesystem::path src = prime ? VASE_FIXTURE_VERSIONEDA : VASE_FIXTURE_VERSIONEDAPRIME; // 装回另一版
        std::filesystem::copy_file(src, ws.APath, std::filesystem::copy_options::overwrite_existing, ec);
        ASSERT_FALSE(ec) << ec.message(); // Eject 之后写不动 = 锁没解，卸载路径坏了
        // 两版描述符逐字节相同（T5 评审）——一份期望跑完 50 轮换件。
        ASSERT_TRUE(host.AdoptPlugin(h, LoopAdoptRequest(versionedA, ws.APath)).IsOk());
        prime = !prime;
        ASSERT_EQ(CounterValue(host.Resolve(h), false), prime ? 2 : 1)
            << "round " << round; // 换装后立刻对得上「如同首次」
    }
    EXPECT_EQ(ticks, 100); // 50 轮 × 2 拍——本行同时拦住「循环轮数被悄悄改小」
    // 尾巴一轮也得有账：循环内断言在 Eject 之前，最后一轮 Adopt 之后的邻居态由这两行收口（R-T6-1）。
    EXPECT_EQ(host.Resolve(h)->Root().Get<samples_fixture::IHeart>().Beats(), 100);
    EXPECT_EQ(host.Resolve(h)->Root().Get<samples_fixture::IPulse>().Pulses(), 100);
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

// §12.3 的 M4 行（换件谱，M4/D113）：五步阶梯，每步只差**一维**描述符；含**回退方向**（回滚拆两步，
// 否则一次退两维、归因说不清）。正例是主体——描述符变了的版本也能走完 Eject→落字节→换期望→Adopt；
// 负例是附属——把本步期望沿本步那一维 tamper 回上一级的值，比对拒。
//
// 每步三条证人的由来：负例那次 Adopt 在 ③.5 比对处被拒，而 AdoptImpl 的 EnsureResident **先于**
// CompareDescriptor——所以它已经把新字节装载驻留了。于是紧接着换对期望必然走**复用分支**，
// `ReusedResidentImage` 的真值在这一趟里就有证人；再 Eject 一次换回**全新装载**分支。两个分支都断。
//
// 例外：S1（代码维）**没有负例**——rung0 与 rung1 描述符逐字节相同，没有字段可漂；硬造一条只能去
// 篡改一个与本步无关的字段，那测的是比对器不是换件谱。故 S1 只做一条全新装载正例。
TEST(HotSwap, DescriptorDriftLadderSwapsBothWays)
{
    const SwapWorkspace ws; // 先于 host 声明：见 SwapWorkspace 的析构序说明
    vase::PluginHost host;

    const vase::ManifestExpectation rung01 = testing_support::MakeVersionedAExpectation();            // rung0 / rung1
    const vase::ManifestExpectation rung2 = testing_support::MakeVersionedAStampDriftExpectation();   // Version 1.1.0
    const vase::ManifestExpectation rung3 = testing_support::MakeVersionedAServiceDriftExpectation(); // + Provides v2

    struct Step
    {
        const char* Label;
        const char* Source; // 本步落在 A 位置上的字节（宏，运行期解成路径）
        const vase::ManifestExpectation* Current;
        const vase::ManifestExpectation* Stale; // 一维 tamper 后的期望；nullptr = 本步无负例
        bool ReadsV2;                           // rung3 提供的是 v2，行为读数随之换接口
        int ExpectedCounter;
    };

    const std::vector<Step> steps = {
        {
            .Label = "S1 代码",
            .Source = VASE_FIXTURE_VERSIONEDAPRIME,
            .Current = &rung01,
            .Stale = nullptr,
            .ReadsV2 = false,
            .ExpectedCounter = 2,
        },
        {
            .Label = "S2 Version 去",
            .Source = VASE_FIXTURE_VERSIONEDASTAMPDRIFT,
            .Current = &rung2,
            .Stale = &rung01,
            .ReadsV2 = false,
            .ExpectedCounter = 2,
        },
        {
            .Label = "S3 Provides 去",
            .Source = VASE_FIXTURE_VERSIONEDASERVICEDRIFT,
            .Current = &rung3,
            .Stale = &rung2,
            .ReadsV2 = true,
            .ExpectedCounter = 2,
        },
        {
            .Label = "S4 Provides 回",
            .Source = VASE_FIXTURE_VERSIONEDASTAMPDRIFT,
            .Current = &rung2,
            .Stale = &rung3,
            .ReadsV2 = false,
            .ExpectedCounter = 2,
        },
        {
            .Label = "S5 Version 回",
            .Source = VASE_FIXTURE_VERSIONEDA,
            .Current = &rung01,
            .Stale = &rung2,
            .ReadsV2 = false,
            .ExpectedCounter = 1,
        },
    };

    const vase::PodHandle h = host.CreatePod(LoopPlan(ws.APath)).Value();
    EXPECT_EQ(CounterValue(host.Resolve(h), false), 1); // 起点 = rung0

    int ticks = 0;
    for (const Step& step : steps)
    {
        SCOPED_TRACE(step.Label); // 失败定位到步（50 轮循环用 << "round " << round，同一用意）
        ASSERT_TRUE(host.EjectPlugin(h, "Vase.VersionedA").IsOk());
        ws.Install(step.Source);

        if (step.Stale == nullptr)
        {
            // S1：本维无描述符可漂 ⇒ 无负例，也就没有「负例留下驻留」这回事。
            const vase::Result<vase::AdoptReport> only = host.AdoptPlugin(h, LoopAdoptRequest(*step.Current, ws.APath));
            ASSERT_TRUE(only.IsOk()) << only.GetError().Message();
            EXPECT_FALSE(only.Value().ReusedResidentImage); // 全新装载分支
        }
        else
        {
            // 负例：期望停在本步之前那一级的取值 → 比对拒。
            // 只钉总 token——D88 口径：一个子串即够，字段细节留给人（不为此扩格式器）。
            const vase::Result<vase::AdoptReport> stale = host.AdoptPlugin(h, LoopAdoptRequest(*step.Stale, ws.APath));
            ASSERT_FALSE(stale.IsOk()); // ASSERT_：下一行取 GetError()，Ok 上取会终止进程
            EXPECT_NE(stale.GetError().Message().find("manifest/binary mismatch"), std::string::npos)
                << stale.GetError().Message();

            // 正例甲：上一次调用已把二进制装载驻留 ⇒ 这一次走**复用分支**。
            const vase::Result<vase::AdoptReport> reused =
                host.AdoptPlugin(h, LoopAdoptRequest(*step.Current, ws.APath));
            ASSERT_TRUE(reused.IsOk()) << reused.GetError().Message();
            EXPECT_TRUE(reused.Value().ReusedResidentImage);
            EXPECT_EQ(CounterValue(host.Resolve(h), step.ReadsV2), step.ExpectedCounter);

            // 正例乙：再卸一次 → **全新装载分支**。
            ASSERT_TRUE(host.EjectPlugin(h, "Vase.VersionedA").IsOk());
            const vase::Result<vase::AdoptReport> fresh =
                host.AdoptPlugin(h, LoopAdoptRequest(*step.Current, ws.APath));
            ASSERT_TRUE(fresh.IsOk()) << fresh.GetError().Message();
            EXPECT_FALSE(fresh.Value().ReusedResidentImage);
        }

        // 每步派两拍，并核两只邻居的**精确**累计数（不派拍则两端皆 0、断言恒真）。
        vase::Pod* const pod = host.Resolve(h);
        pod->Root().Emit(samples_fixture::TickEvent{ticks});
        ++ticks;
        pod->Root().Emit(samples_fixture::TickEvent{ticks});
        ++ticks;
        EXPECT_EQ(pod->Root().Get<samples_fixture::IHeart>().Beats(), ticks) << step.Label;
        EXPECT_EQ(pod->Root().Get<samples_fixture::IPulse>().Pulses(), ticks) << step.Label;
    }

    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

} // namespace
