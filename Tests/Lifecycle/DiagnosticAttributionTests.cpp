#include "PodTestPeer.h" // Tests/TestingSupport 进 include 路径
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Pod.h"

#include <gtest/gtest.h>

namespace
{

vase::LoadPlan HelloPlan()
{
    vase::LoadPlan plan;
    plan.Ordered.push_back({.Id = "Vase.Hello", .BinaryPath = VASE_FIXTURE_HELLO});
    return plan;
}

TEST(DiagnosticAttribution, ResidualScopeIsNamedByOwnerLabel)
{
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(HelloPlan()).Value();
    vase::Pod* pod = host.Resolve(h);
    vase::PodTestPeer::InjectLeakedScope(*pod, "Vase.Test.LeakProbe", 2); // 2 个未回收 Effect

    const vase::PodReport report = host.DestroyPod(h);
    EXPECT_FALSE(report.Clean()); // 报告如实说不干净
    ASSERT_EQ(report.Residuals.size(), 1U);
    // 取首条用迭代器而非 operator[]（同 T3 的 MetaArray 用例写法）。
    const vase::ResidualEntry& residual = *report.Residuals.begin();
    EXPECT_EQ(residual.OwnerLabel, "Vase.Test.LeakProbe"); // #10 的点名机制
    EXPECT_EQ(residual.Count, 2U);
    EXPECT_EQ(report.CountersDiff.Effects, 2U);

    // 注入 Scope 随 Pod 析构被兜底 Dispose（T3 的析构设计）——计数回基线：
    // 本探针验「报告机器」，真·跨局泄漏属 M3（PodTestPeer.h 头注已划界）。
    EXPECT_EQ(host.ForTestCounters().Effects, 0U);
}

TEST(DiagnosticAttribution, DestroyPodOnLeakedPodStillReturnsReport)
{
    // 12 节 #18（承重）：Destroy 永不失败——泄漏的局也拿得到报告，而不是 abort/半个局。
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(HelloPlan()).Value();
    vase::PodTestPeer::InjectLeakedScope(*host.Resolve(h), "Vase.Test.LeakProbe2", 3);
    vase::PodReport report;
    ASSERT_NO_FATAL_FAILURE(report = host.DestroyPod(h)); // 「永不失败」的形态验证
    EXPECT_FALSE(report.HandleWasStale);
    EXPECT_FALSE(report.Clean());
    EXPECT_EQ(host.Resolve(h), nullptr); // 不管泄漏与否，局都结束了
}

} // namespace
