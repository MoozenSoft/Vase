#include "Vase/Host/PluginHost.h"

#include "Vase/Detail/Result.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Pod/Pod.h"

#include <filesystem>
#include <gtest/gtest.h>

namespace
{

// 路径不写成命名空间级对象：静态初始化那一类会被 cert-err58-cpp 盯上（与 LoaderTests 同写法）。
std::filesystem::path HelloBinary() { return std::filesystem::path{VASE_FIXTURE_HELLO}; }

vase::LoadPlan HelloPlan()
{
    vase::LoadPlan plan;
    plan.Ordered.push_back(vase::LoadPlanEntry{.Id = "Vase.Hello", .BinaryPath = HelloBinary()});
    return plan;
}

TEST(Lifecycle, PodCyclesReturnAllCountersToBaseline)
{
    // 12 节 #1（承重）：反复建销后五项计数全部回基线。
    vase::PluginHost host;
    const vase::LoadPlan plan = HelloPlan();
    for (int i = 0; i < 20; ++i)
    {
        vase::Result<vase::PodHandle> r = host.CreatePod(plan);
        ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
        EXPECT_EQ(host.Resolve(r.Value())->PluginCount(), 1U);
        const vase::PodReport report = host.DestroyPod(r.Value());
        EXPECT_TRUE(report.Clean()) << "cycle " << i;
        EXPECT_EQ(report.Failures.size(), 0U);
    }
    const auto& counters = host.ForTestCounters();
    EXPECT_EQ(counters.Effects, 0U);
    EXPECT_EQ(counters.Services, 0U);
    EXPECT_EQ(counters.Subscriptions, 0U);
    EXPECT_EQ(counters.PluginInstances, 0U);
    EXPECT_EQ(counters.Scopes, 0U);
}

TEST(Lifecycle, StaleHandleIsDetectedAfterDestroy)
{
    // 12 节 #15：销毁后 Resolve 返回 nullptr；索引复用后旧句柄仍无效（代际）。
    vase::PluginHost host;
    const vase::PodHandle first = host.CreatePod(HelloPlan()).Value();
    EXPECT_NE(host.Resolve(first), nullptr);
    host.DestroyPod(first);
    EXPECT_EQ(host.Resolve(first), nullptr);

    const vase::PodHandle second = host.CreatePod(HelloPlan()).Value();
    EXPECT_EQ(second.Index, first.Index);           // 同槽复用
    EXPECT_NE(second.Generation, first.Generation); // 代际已推进
    EXPECT_EQ(host.Resolve(first), nullptr);        // 旧句柄不被新 Pod 冒充
    host.DestroyPod(second);
}

} // namespace
