// T12 §12.1：热插拔主循环的端到端——M1 核心承诺的唯一证伪者。
// 流程 = CreatePod{A,B} → 行为==A → Eject(A) → A′ 覆盖 A 的位置（覆盖本身即
// 「锁真解了」的断言）→ Adopt(A) → 行为==A′ → B 的实例从第一步起没被碰过。
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

    std::filesystem::path Dir;
    std::filesystem::path APath;
};

// A 与 B 同一局；A 的路径是工作副本（会被覆盖），B 是构建产物本体（只读地加载）。
vase::LoadPlan LoopPlan(const std::filesystem::path& aPath)
{
    vase::LoadPlan plan;
    plan.Ordered.push_back({.Id = "Vase.VersionedA", .BinaryPath = aPath});
    plan.Ordered.push_back({.Id = "Vase.NeighborB", .BinaryPath = VASE_FIXTURE_NEIGHBORB});
    return plan;
}

// 局内 A 的 Adopt 请求：路径 = 工作副本位（换件写字的地方），兄弟集 = 这局另一只二进制名。
// 期望用 MakeVersionedAExpectation 一份到底——A/A′ 描述符逐字节相同（T5 评审），换件不换期望。
vase::AdoptRequest LoopAdoptRequest(const vase::ManifestExpectation& expected, const std::filesystem::path& aPath)
{
    vase::AdoptRequest request;
    request.Id = expected.Id;
    request.BinaryPath = aPath;
    request.Expected = &expected;
    request.SiblingBinaries = {std::filesystem::path{VASE_FIXTURE_NEIGHBORB}.filename().string()};
    return request;
}

// 宿主侧解析（§5.6：宿主的解析不落边）——Eject(A) 能过本身就是这条的活体证明：
// 宿主「缓存」着指针，账本看不见也不需要看见（9.3 的宿主纪律在测试里的演练形态）。
int CounterValue(vase::Pod* pod)
{
    return pod->Root().Get<samples_fixture::ICounter>().Value(); // Get<T>() 返回 T&（R112）
}

TEST(HotSwap, FullLoopFlipsBehaviorAndNeverTouchesNeighbor)
{
    const SwapWorkspace ws; // 先于 host 声明：见 SwapWorkspace 的析构序说明
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(LoopPlan(ws.APath)).Value();
    vase::Pod* const pod = host.Resolve(h);

    EXPECT_EQ(CounterValue(pod), 1); // 行为 == A
    const auto* const heart1 = &pod->Root().Get<samples_fixture::IHeart>();
    for (int i = 1; i <= 5; ++i)
    {
        pod->Root().Emit(samples_fixture::TickEvent{i}); // B 心跳正常
    }
    EXPECT_EQ(heart1->Beats(), 5); // 切换前恰好 5 次 tick：一次都不能丢

    const vase::ManifestExpectation versionedA = testing_support::MakeVersionedAExpectation();
    ASSERT_TRUE(host.EjectPlugin(h, "Vase.VersionedA").IsOk()); // 三档之档一、档二在报告里结算
    ws.InstallPrime(); // A′ 覆盖 A 的位置；A/A′ 描述符逐字节相同，期望不需要跟着换
    const vase::Result<vase::AdoptReport> adopt = host.AdoptPlugin(h, LoopAdoptRequest(versionedA, ws.APath));
    ASSERT_TRUE(adopt.IsOk()) << adopt.GetError().Message();
    // §12.1 要的是「真 Eject 之后走**全新装载**分支」。Linux 上 `ReusedResidentImage` 是唯一
    // 判据；Windows 上 `InstallPrime()` 的覆盖断言先红（镜像还映射着就写不动），它是语义锚点。
    // IdentityVerified 在成功路径上被无条件置 true（PluginHost.cpp），是报告字段不是判据。
    EXPECT_FALSE(adopt.Value().ReusedResidentImage);

    EXPECT_EQ(CounterValue(pod), 2); // 行为 == A′（不是 A！）
    const auto* const heart2 = &pod->Root().Get<samples_fixture::IHeart>();
    // B 实例指针在这里不能判别：新对象会落回被释放的堆块（实测同一二进制时红时绿）——
    // 「邻居未被触碰」由下面那条精确计数承担。
    pod->Root().Emit(samples_fixture::TickEvent{99});
    EXPECT_EQ(heart2->Beats(), 6); // 5 次 tick + 这一次恰好 6：计数没被重置过（真正承重的那条）

    const vase::PodReport report = host.DestroyPod(h);
    EXPECT_TRUE(report.Clean());
    ASSERT_EQ(report.HotSwapLog.size(), 2U); // §9.2 v3「中途进出过谁」
    // 迭代器而非 operator[]：非常量下标过不了 cppcoreguidelines-pro-bounds-*（计划「tidy 形态约束」）。
    const auto ejectEntry = report.HotSwapLog.begin();
    EXPECT_NE(ejectEntry->find("eject:Vase.VersionedA"), std::string::npos);
    EXPECT_NE(std::next(ejectEntry)->find("adopt:Vase.VersionedA"), std::string::npos);
}

TEST(HotSwap, FiveRoundsBehaveLikeFirstTime)
{
    // 「可重复无数次」的 M1 剂量（5 轮；50 轮的全量形随 M3 的 3a 一起加）。
    const SwapWorkspace ws;
    vase::PluginHost host;
    const vase::ManifestExpectation versionedA = testing_support::MakeVersionedAExpectation();
    const vase::PodHandle h = host.CreatePod(LoopPlan(ws.APath)).Value();
    bool prime = false;
    for (int round = 0; round < 5; ++round)
    {
        vase::Pod* const pod = host.Resolve(h);
        ASSERT_EQ(CounterValue(pod), prime ? 2 : 1) << "round " << round;
        ASSERT_TRUE(host.EjectPlugin(h, "Vase.VersionedA").IsOk());
        std::error_code ec;
        const std::filesystem::path src = prime ? VASE_FIXTURE_VERSIONEDA : VASE_FIXTURE_VERSIONEDAPRIME; // 装回另一版
        std::filesystem::copy_file(src, ws.APath, std::filesystem::copy_options::overwrite_existing, ec);
        ASSERT_FALSE(ec) << ec.message(); // Eject 之后写不动 = 锁没解，卸载路径坏了
        // 两版描述符逐字节相同（T5 评审）——一份期望跑完五轮换件。
        ASSERT_TRUE(host.AdoptPlugin(h, LoopAdoptRequest(versionedA, ws.APath)).IsOk());
        prime = !prime;
        ASSERT_EQ(CounterValue(host.Resolve(h)), prime ? 2 : 1); // 换装后立刻对得上「如同首次」
    }
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

} // namespace
