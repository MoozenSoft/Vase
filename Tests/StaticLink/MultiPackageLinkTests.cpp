#include "PackageServices.h"
#include "Vase/Config/Value.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/ConfigBlob.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/PluginDescriptor.h"
#include "Vase/Pod/Context.h"
#include "Vase/Pod/Pod.h"
#include "VasePackageConsumer.entries.h"
#include "VasePackageProvider.entries.h"

#include <cstdint>

#include <gtest/gtest.h>

TEST(MultiPackageLink, EntriesRouteToTheirOwnMembers)
{
    constexpr auto kProvider = vase::pack::vasepackageprovider::kEntryPoints;
    constexpr auto kConsumer = vase::pack::vasepackageconsumer::kEntryPoints;
    EXPECT_NE(kProvider.Lookup, kConsumer.Lookup);
    ASSERT_NE(kProvider.Lookup("Vase.PackageProvider"), nullptr);
    ASSERT_NE(kConsumer.Lookup("Vase.PackageConsumer"), nullptr);
    EXPECT_EQ(kProvider.Lookup("Vase.PackageConsumer"), nullptr);
    EXPECT_EQ(kConsumer.Lookup("Vase.PackageProvider"), nullptr);
}

TEST(MultiPackageLink, EnumerationsContainOnlyTheirOwnMembers)
{
    constexpr auto kProvider = vase::pack::vasepackageprovider::kEntryPoints;
    constexpr auto kConsumer = vase::pack::vasepackageconsumer::kEntryPoints;
    EXPECT_NE(kProvider.Enumerate, kConsumer.Enumerate);
    std::uint32_t providerCount = 0;
    std::uint32_t consumerCount = 0;
    const auto* providerTable = kProvider.Enumerate(&providerCount);
    const auto* consumerTable = kConsumer.Enumerate(&consumerCount);
    ASSERT_EQ(providerCount, 1U);
    ASSERT_EQ(consumerCount, 1U);
    ASSERT_NE(providerTable, nullptr);
    ASSERT_NE(consumerTable, nullptr);
    ASSERT_NE(*providerTable, nullptr);
    ASSERT_NE(*consumerTable, nullptr);
    ASSERT_NE((*providerTable)->Meta, nullptr);
    ASSERT_NE((*consumerTable)->Meta, nullptr);
    EXPECT_EQ((*providerTable)->Meta->Id, "Vase.PackageProvider");
    EXPECT_EQ((*consumerTable)->Meta->Id, "Vase.PackageConsumer");
    EXPECT_EQ(*providerTable, kProvider.Lookup("Vase.PackageProvider"));
    EXPECT_EQ(*consumerTable, kConsumer.Lookup("Vase.PackageConsumer"));
}

TEST(MultiPackageLink, CrossPackageServiceUsesEachPodsConfiguration)
{
    vase::LoadPlan plan;
    vase::LoadPlanEntry provider{};
    provider.Id = "Vase.PackageProvider";
    provider.InProcessEntry = vase::pack::vasepackageprovider::kEntryPoints.Lookup;
    plan.Ordered.push_back(provider);
    vase::LoadPlanEntry consumer{};
    consumer.Id = "Vase.PackageConsumer";
    consumer.InProcessEntry = vase::pack::vasepackageconsumer::kEntryPoints.Lookup;
    plan.Ordered.push_back(consumer);

    vase::PluginHost host;
    const auto defaultPod = host.CreatePod(plan);
    ASSERT_TRUE(defaultPod.IsOk()) << defaultPod.GetError().Message();
    ASSERT_NE(host.Resolve(defaultPod.Value()), nullptr);
    ASSERT_EQ(host.Resolve(defaultPod.Value())->PluginCount(), 2U);
    EXPECT_EQ(host.Resolve(defaultPod.Value())->Root().Get<package_fixture::IResult>().Observed(), 3);

    plan.Ordered.begin()->ResolvedConfig.Set("Value", vase::Value::From<std::int32_t>(17));
    const auto configuredPod = host.CreatePod(plan);
    ASSERT_TRUE(configuredPod.IsOk()) << configuredPod.GetError().Message();
    ASSERT_NE(host.Resolve(configuredPod.Value()), nullptr);
    ASSERT_EQ(host.Resolve(configuredPod.Value())->PluginCount(), 2U);
    EXPECT_EQ(host.Resolve(configuredPod.Value())->Root().Get<package_fixture::IResult>().Observed(), 17);
    EXPECT_EQ(host.Resolve(defaultPod.Value())->Root().Get<package_fixture::IResult>().Observed(), 3);
    EXPECT_EQ(host.ForTestLedgerEdgeCount(), 2U);
    EXPECT_TRUE(host.DestroyPod(configuredPod.Value()).Clean());
    EXPECT_TRUE(host.DestroyPod(defaultPod.Value()).Clean());
    EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U);
}
