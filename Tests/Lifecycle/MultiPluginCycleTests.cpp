// §9.2 反复 Play/Stop 的归零判据升到多插件：健康局与级联局都要每轮全零。
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Pod.h"

#include <gtest/gtest.h>

#include "../Integration/fixtures/SharedCommon.h"

namespace
{

class HostMarker final : public samples_fixture::IHostOnlyService
{
public:
    [[nodiscard]] int Marker() const override { return 1; }
};

vase::LoadPlan ConsumerPlan()
{
    vase::LoadPlan plan;
    plan.Ordered.push_back({.Id = "Vase.SharedProvider", .BinaryPath = VASE_FIXTURE_SHAREDPROVIDER});
    plan.Ordered.push_back({.Id = "Vase.EdgeConsumer", .BinaryPath = VASE_FIXTURE_EDGECONSUMER});
    return plan;
}

TEST(MultiPluginCycle, RepeatedMultiPluginRoundZeroes)
{
    // §9.2 的反复 Play/Stop 判据从单插件升到多插件：每轮差分全零、账本全空。
    vase::PluginHost host;
    HostMarker marker;
    vase::PodOptions options;
    options.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::IHostOnlyService>(marker); };
    for (int round = 0; round < 3; ++round)
    {
        const vase::PodHandle h = host.CreatePod(ConsumerPlan(), options).Value();
        EXPECT_EQ(host.Resolve(h)->PluginCount(), 2U);
        EXPECT_TRUE(host.DestroyPod(h).Clean()) << "round " << round;
        EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U);
    }
}

TEST(MultiPluginCycle, CascadeRoundStillZeroesEverything)
{
    // 判据 5 的局同样要归零：拆除不留半个 Scope、不留一条边、不留一个实例计数。
    vase::PluginHost host;
    for (int round = 0; round < 3; ++round)
    {
        vase::LoadPlan plan;
        plan.Ordered.push_back({.Id = "Vase.StartFailProvider", .BinaryPath = VASE_FIXTURE_STARTFAILPROVIDER});
        plan.Ordered.push_back({.Id = "Vase.BehindStrictUsed", .BinaryPath = VASE_FIXTURE_BEHINDSTRICTUSED});
        plan.Ordered.push_back({.Id = "Vase.BehindFar", .BinaryPath = VASE_FIXTURE_BEHINDFAR});
        const vase::PodHandle h = host.CreatePod(plan).Value();
        EXPECT_EQ(host.Resolve(h)->PluginCount(), 0U); // 全被拆/跳；opt-unused 缺席版
        EXPECT_TRUE(host.DestroyPod(h).Clean()) << "round " << round;
        EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U);
    }
}

} // namespace
