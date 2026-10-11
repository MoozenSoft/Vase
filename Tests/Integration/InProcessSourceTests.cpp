// 进程内来源的机制与宿主级证人（本波 D216/D225；不需要静态链接，故住 VaseTests）。
// 静态链入的端到端（真 .a 链进可执行）住 Tests/StaticLink，见 spec §5.1 的分工。
#include "Vase/Detail/Result.h"
#include "Vase/Host/Evidence.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/Loader.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/PluginDescriptor.h"
#include "Vase/Pod/Pod.h"

#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using GetPluginFn = const vase::PluginDescriptor* (*)(const char*);

std::filesystem::path FixturePath(const char* defineValue) { return std::filesystem::path{defineValue}; }

// 取一枚真入口当「进程内入口」用：先经平台装载拿到它。本文件所有用例都只需「宿主持有入口」
// 这一件事，而入口怎么来的与它无关——这正是 spec §2.2 说的「入口从哪来」是唯一的分岔。
//
// Loader 是局部量：它析构**不卸货**（§8.1——卸货只发生在 Eject / Shutdown），故取出的
// 函数指针在用例期间一直有效。与 Tests/Integration/VasePackTests.cpp 的 PackedEntry 同法。
GetPluginFn HelloEntry()
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> record = loader.EnsureResident(FixturePath(VASE_FIXTURE_HELLO));
    if (!record.IsOk())
    {
        return nullptr;
    }
    const vase::Result<void*> raw = vase::detail::Loader::Symbol(*record.Value(), "VasePlugin_GetPlugin");
    if (!raw.IsOk())
    {
        return nullptr;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) 同 PluginHost.cpp:70
    return reinterpret_cast<GetPluginFn>(raw.Value());
}

constexpr std::string_view kHelloId = "Vase.Hello";

vase::LoadPlanEntry MakeHelloEntry(bool throughInProcessSource)
{
    vase::LoadPlanEntry entry{};
    entry.Id = kHelloId;
    if (throughInProcessSource)
    {
        entry.InProcessEntry = HelloEntry();
    }
    else
    {
        entry.BinaryPath = FixturePath(VASE_FIXTURE_HELLO);
    }
    return entry;
}

} // namespace

TEST(InProcessSource, BothSourcesSetIsRejected)
{
    vase::PluginHost host;
    vase::LoadPlan plan;
    vase::LoadPlanEntry entry = MakeHelloEntry(true);
    entry.BinaryPath = FixturePath(VASE_FIXTURE_HELLO); // 两个来源同填 = 误用
    plan.Ordered.push_back(entry);

    const vase::Result<vase::PodHandle> created = host.CreatePod(plan);
    ASSERT_FALSE(created.IsOk());
    EXPECT_NE(created.GetError().Message().find("both binary path and in-process entry set"), std::string::npos)
        << created.GetError().Message();
}

TEST(InProcessSource, EjectIsRefusedBeforeAnyTeardown)
{
    vase::PluginHost host;
    vase::LoadPlan plan;
    plan.Ordered.push_back(MakeHelloEntry(true));
    const vase::Result<vase::PodHandle> created = host.CreatePod(plan);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();

    const vase::Result<vase::EjectReport> ejected = host.EjectPlugin(created.Value(), kHelloId);
    ASSERT_FALSE(ejected.IsOk());
    EXPECT_NE(ejected.GetError().Message().find("linked-in binary cannot be ejected"), std::string::npos)
        << ejected.GetError().Message();

    // 早闸在任何拆除之前：实例仍在（下一句 DestroyPod 能干净拆掉就是证据）
    ASSERT_NE(host.Resolve(created.Value()), nullptr);
    static_cast<void>(host.DestroyPod(created.Value()));
}

TEST(InProcessSource, DestroyPodStillWorks)
{
    vase::PluginHost host;
    vase::LoadPlan plan;
    plan.Ordered.push_back(MakeHelloEntry(true));
    const vase::Result<vase::PodHandle> created = host.CreatePod(plan);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
    const vase::PodReport report = host.DestroyPod(created.Value()); // 拆局不卸货（§8.1）
    static_cast<void>(report);
    EXPECT_EQ(host.Resolve(created.Value()), nullptr);
}

// 三个条目共享一个 BinaryPath：组合库「一库 N 插件」经真宿主的**首条**证人（spec §5.1 #1 / D225）。
// 桌面波那六条只到 Loader 为止（VasePackTests 的 EnsureResident），这里把它交给 PluginHost——
// 同一张 BinaryPath 靠「一文件一记录」复用同一记录，装配出一个真 Pod。
TEST(CombinedLibraryHost, ThreeEntriesShareOneBinaryPath)
{
    vase::PluginHost host;
    vase::LoadPlan plan;
    const std::vector<std::string> ids{"Vase.Hello", "Vase.Dependent", "Vase.Failing"};
    for (const std::string& id : ids)
    {
        vase::LoadPlanEntry entry{};
        entry.Id = id;
        entry.BinaryPath = FixturePath(VASE_FIXTURE_VASEPLUGINS);
        plan.Ordered.push_back(entry);
    }

    const vase::Result<vase::PodHandle> created = host.CreatePod(plan);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message(); // 宽容语义：Failed 不翻整局（D70 同形）
    const vase::Pod* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);

    // FailingPlugin 的 OnStart 返回 Err → 只落 Failed 记录、实例不进活集合；另两只照常活。
    ASSERT_EQ(pod->Failures().size(), 1U);
    EXPECT_EQ(pod->Failures().begin()->Id, "Vase.Failing");
    EXPECT_EQ(pod->Failures().begin()->Stage, vase::Phase::kStart);

    // 三个 Id 各自的实例都在（Failing 按设计只剩记录）。
    EXPECT_TRUE(pod->HasPlugin("Vase.Hello"));
    EXPECT_TRUE(pod->HasPlugin("Vase.Dependent"));
    EXPECT_FALSE(pod->HasPlugin("Vase.Failing"));
    EXPECT_EQ(pod->PluginCount(), 2U);

    // 服务解析走通：Dependent 的 OnStart 凭声明 Get<IGreeter>（Get 失败在本项目是 terminate，
    // 它活着即链通）；账本那一条边正对应该解析。
    EXPECT_EQ(host.ForTestLedgerEdgeCount(), 1U);

    EXPECT_TRUE(host.DestroyPod(created.Value()).Clean());
}

// 一库 N 插件时的进程级状态（spec §2.5 / D236/D237）：Eject 必须重置**全部**成员的
// 登记状态，不是只剩最后装配那一个。本用例先以「未修」状态红——那是 D236 的判决材料。
TEST(CombinedLibraryHost, EjectResetsEveryMemberProcessState)
{
    vase::PluginHost host;
    vase::LoadPlan plan;
    const std::vector<std::string> ids = {"Vase.Test.StatefulPackA", "Vase.Test.StatefulPackB"};
    for (const std::string& id : ids)
    {
        vase::LoadPlanEntry entry{};
        entry.Id = id;
        entry.BinaryPath = FixturePath(VASE_FIXTURE_VASEPLUGINSSTATEFUL);
        plan.Ordered.push_back(entry);
    }
    const vase::Result<vase::PodHandle> created = host.CreatePod(plan);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();

    // 宽容语义下单个成员失败不翻局（D70 同形），故「读数 1」必须先把「两个成员都真入局」
    // 钉住——否则它与「只有一个成员走到 MakeInstance」不可区分，判决材料就不自足。
    const vase::Pod* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);
    ASSERT_EQ(pod->PluginCount(), 2U);

    // 逐个 Eject：只有最后一个能把整份 binary 请出架、从而触发 Reset。
    // 前面的 Eject 会因「同局（进程内）还有持有者」被 kept-resident（那是设计的），
    // 故本用例在一个 Pod 里依次拆——最后那次的 EjectReport 必须点名**两个**成员各自的状态名。
    vase::Result<vase::EjectReport> last = vase::Result<vase::EjectReport>::Err(vase::Error{"unset"});
    for (const std::string& id : ids)
    {
        last = host.EjectPlugin(created.Value(), id);
        ASSERT_TRUE(last.IsOk()) << last.GetError().Message();
    }
    ASSERT_EQ(last.Value().Status, vase::EjectStatus::kEjected);
    EXPECT_EQ(last.Value().ProcessStatesReset.size(), 2U)
        << "只有 " << last.Value().ProcessStatesReset.size() << " 个成员的状态被重置——"
        << "MakeInstance 的覆盖缺陷（spec §2.5）";
    static_cast<void>(host.DestroyPod(created.Value()));
}
