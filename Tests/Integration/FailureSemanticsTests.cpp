#include "Vase/Detail/Result.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Pod.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace
{

vase::LoadPlan Plan(std::initializer_list<std::pair<std::string_view, std::filesystem::path>> items)
{
    vase::LoadPlan plan;
    for (const auto& [id, path] : items)
    {
        plan.Ordered.push_back({.Id = id, .BinaryPath = path});
    }
    return plan;
}

TEST(FailureSemantics, StaleHeaderPluginRejectedAndRecorded)
{
    // 12 节 #12（承重）：HeaderVersion 不匹配 → 拒绝加载并报告，不是崩溃、不是静默。
    vase::PluginHost host;
    const vase::Result<vase::PodHandle> r = host.CreatePod(Plan({{"Vase.StaleHeader", VASE_FIXTURE_STALEHEADER}}));
    ASSERT_TRUE(r.IsOk()); // 宽容模式：失败记在案，局照开
    const vase::Pod* pod = host.Resolve(r.Value());
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(pod->PluginCount(), 0U);
    ASSERT_EQ(pod->Failures().size(), 1U);
    // 取首条用迭代器而非 operator[]：后者过不了 cppcoreguidelines-pro-bounds-*，
    // 而本任务三处都就地抑制会越出全仓同类 NOLINT 的额度（2 + 3 > 4）——写法同 T3 的 MetaArray 用例。
    const vase::FailedPluginRecord& failure = *pod->Failures().begin();
    EXPECT_EQ(failure.Id, "Vase.StaleHeader"); // 记录点名到计划里的那个 Id，而不是空/占位
    EXPECT_EQ(failure.Stage, vase::Phase::kLoad);
    EXPECT_NE(failure.Message.find("HeaderVersion"), std::string::npos);
    EXPECT_TRUE(host.DestroyPod(r.Value()).Clean()); // 被拒的插件没留任何计数
}

TEST(FailureSemantics, FailingPluginRecordedNeighborsUnharmed)
{
    // §0.3-4 失败不致命：坏插件 Failed 记录在案，好插件照常 Started。
    vase::PluginHost host;
    const vase::LoadPlan plan =
        Plan({{"Vase.FailingLoad", VASE_FIXTURE_FAILINGLOAD}, {"Vase.Hello", VASE_FIXTURE_HELLO}});
    const vase::Result<vase::PodHandle> r = host.CreatePod(plan);
    ASSERT_TRUE(r.IsOk());
    const vase::Pod* pod = host.Resolve(r.Value());
    EXPECT_EQ(pod->PluginCount(), 1U); // Hello 活着
    ASSERT_EQ(pod->Failures().size(), 1U);
    EXPECT_EQ(pod->Failures().begin()->Id, "Vase.FailingLoad");
    const vase::PodReport report = host.DestroyPod(r.Value());
    EXPECT_TRUE(report.Clean()); // OnLoad 当场回收（§5.2），不留半个 Scope
}

TEST(FailureSemantics, StrictModeDestroysHalfBuiltPod)
{
    // §5.5：Strict 下任一失败 → CreatePod 返回 Err，半个 Pod 拆干净。
    vase::PluginHost host;
    vase::PodOptions options;
    options.Strict = true;
    const vase::LoadPlan plan =
        Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}, {"Vase.FailingStart", VASE_FIXTURE_FAILINGSTART}});
    const vase::Result<vase::PodHandle> r = host.CreatePod(plan, options);
    ASSERT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("start failure"), std::string::npos);
    const auto& counters = host.ForTestCounters();
    EXPECT_EQ(counters.PluginInstances, 0U); // 半个局也没留下
    EXPECT_EQ(counters.Effects, 0U);
}

TEST(FailureSemantics, TolerantModeReclaimsFailedStartScopeImmediately)
{
    // §5.2：OnStart 失败要**当场**回收自己的 Scope。判据必须落在宽容模式：Strict 会
    // 整拆半成品，~Pod 的兜底 Dispose 同样抹平计数，分不出「当场回收」与「拆局时顺手
    // 回收」。宽容模式不整拆，而 CountersDiff 在 Pod 析构**之前**算——失败 Scope 若还
    // 活着，Scopes/Effects 必然非零。这条因此只有真的做了即时回收才会绿。
    vase::PluginHost host;
    const vase::Result<vase::PodHandle> r = host.CreatePod(Plan({{"Vase.FailingStart", VASE_FIXTURE_FAILINGSTART}}));
    ASSERT_TRUE(r.IsOk()); // 宽容模式：失败记在案，局照开
    const vase::Pod* pod = host.Resolve(r.Value());
    ASSERT_NE(pod, nullptr);
    ASSERT_EQ(pod->Failures().size(), 1U);
    EXPECT_EQ(pod->Failures().begin()->Stage, vase::Phase::kStart);
    EXPECT_EQ(pod->PluginCount(), 0U);               // 失败实例不在活集合里
    EXPECT_TRUE(host.DestroyPod(r.Value()).Clean()); // 只有即时回收才会绿
}

TEST(FailureSemantics, MissingDeclaredServiceTerminatesWithFullIdentity)
{
    // §6.2：required 解析失败 = 编程错误 → 终止，消息含插件 Id + 服务名 + 版本。
    // （M1 无求解器兜底，这条证明运行期最后一道墙是响的。）
    EXPECT_DEATH(
        {
            vase::PluginHost host;
            static_cast<void>(host.CreatePod(Plan({{"Vase.RequiresMissingConsumer", VASE_FIXTURE_MISSINGCONSUMER}})));
        },
        "Vase\\.RequiresMissingConsumer.*Vase\\.Ghost' v1 ");
}

} // namespace
