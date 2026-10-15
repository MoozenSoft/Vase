#include "CatalogSandbox.h"
#include "Cli.h"
#include "PackageServices.h"
#include "Validate.h"
#include "Vase/Catalog/LoadRequest.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Catalog/Preset.h"
#include "Vase/Config/Value.h"
#include "Vase/Detail/Counters.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PackagePlan.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Context.h"
#include "Vase/Pod/Pod.h"
#include "VasePackageConsumer.entries.h"
#include "VasePackageProvider.entries.h"
#include "VasePluginsStatic.entries.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace
{

class StaticPackageAssemblyFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const auto refreshed = Catalog.Refresh(VASE_PACKAGE_STATIC_STAGE);
        ASSERT_TRUE(refreshed.IsOk()) << refreshed.GetError().Message();
    }

public:
    vase::PluginCatalog Catalog;
    const std::array<vase::InProcessBinding, 2> Bindings{
        vase::InProcessBinding{.SourceId = "Provider", .Entries = vase::pack::vasepackageprovider::kEntryPoints},
        vase::InProcessBinding{.SourceId = "Consumer", .Entries = vase::pack::vasepackageconsumer::kEntryPoints},
    };
};

// 套件名与 ctest focused 选择子一致，fixture 保留任务约定名。
using StaticPackageAssembly = StaticPackageAssemblyFixture;

} // namespace

TEST_F(StaticPackageAssembly, TwoPackagesConsumeConfiguredServiceAndStopCleanly)
{
    vase::PluginOverride provider;
    provider.Id = "Vase.PackageProvider";
    provider.Enabled = true;
    provider.Config.Set("Value", vase::Value::From(std::int32_t{7}));
    const std::array overrides{provider};
    vase::LoadRequest request;
    request.Overrides = overrides;
    const auto solved = Catalog.Solve(request, Bindings);
    ASSERT_TRUE(solved.IsOk());
    ASSERT_EQ(solved.Value().Plan.PackageSources.size(), 2U);
    vase::PluginHost host;
    const auto created = host.CreatePod(solved.Value().Plan);
    ASSERT_TRUE(created.IsOk());
    EXPECT_EQ(host.Resolve(created.Value())->Root().Get<package_fixture::IResult>().Observed(), 7);
    EXPECT_TRUE(host.DestroyPod(created.Value()).Clean());
}

TEST_F(StaticPackageAssembly, RepeatedPodsConsumeFreshConfigurationAndReleaseAllEdges)
{
    vase::PluginHost host;
    for (const std::int32_t value : {7, 3, 7})
    {
        vase::PluginOverride provider;
        provider.Id = "Vase.PackageProvider";
        provider.Enabled = true;
        provider.Config.Set("Value", vase::Value::From(value));
        const std::array overrides{provider};
        vase::LoadRequest request;
        request.Overrides = overrides;
        const auto solved = Catalog.Solve(request, Bindings);
        ASSERT_TRUE(solved.IsOk());
        const auto created = host.CreatePod(solved.Value().Plan);
        ASSERT_TRUE(created.IsOk());
        EXPECT_EQ(host.Resolve(created.Value())->Root().Get<package_fixture::IResult>().Observed(), value);
        EXPECT_EQ(host.ForTestLedgerEdgeCount(), 1U);
        EXPECT_TRUE(host.DestroyPod(created.Value()).Clean());
        EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U);
        EXPECT_EQ(host.ForTestCounters().PluginInstances, 0U);
        EXPECT_EQ(host.ForTestCounters().Scopes, 0U);
    }
}

TEST_F(StaticPackageAssembly, InvalidConfigurationCannotReachTheService)
{
    vase::PluginOverride provider;
    provider.Id = "Vase.PackageProvider";
    provider.Enabled = true;
    provider.Config.Set("Value", vase::Value::From(true));
    const std::array overrides{provider};
    vase::LoadRequest request;
    request.Overrides = overrides;
    const auto solved = Catalog.Solve(request, Bindings);
    ASSERT_FALSE(solved.IsOk());
    EXPECT_NE(solved.GetError().Message().find("Value"), std::string::npos);
}

TEST_F(StaticPackageAssembly, ParticipatingPackageRequiresItsOwnSourceBinding)
{
    vase::PluginOverride provider;
    provider.Id = "Vase.PackageProvider";
    provider.Enabled = true;
    const std::array overrides{provider};
    vase::LoadRequest request;
    request.Overrides = overrides;
    const std::array wrongBindings{
        vase::InProcessBinding{.SourceId = "wrong-provider", .Entries = vase::pack::vasepackageprovider::kEntryPoints},
        Bindings.back(),
    };
    const auto solved = Catalog.Solve(request, wrongBindings);
    ASSERT_FALSE(solved.IsOk());
    EXPECT_NE(solved.GetError().Message().find("wrong-provider"), std::string::npos);
}

TEST_F(StaticPackageAssembly, EntirelyDisabledPackagesNeedNoBindingsButValidateIsIncomplete)
{
    std::vector<vase::PluginOverride> overrides;
    for (const auto& id : Catalog.Ids())
    {
        vase::PluginOverride disabled;
        disabled.Id = id;
        disabled.Enabled = false;
        overrides.push_back(disabled);
    }
    vase::LoadRequest request;
    request.Overrides = overrides;
    const auto solved = Catalog.Solve(request);
    ASSERT_TRUE(solved.IsOk());
    EXPECT_TRUE(solved.Value().Plan.PackageSources.empty());
    vase::PluginHost host;
    const auto created = host.CreatePod(solved.Value().Plan);
    ASSERT_TRUE(created.IsOk());
    EXPECT_EQ(host.Resolve(created.Value())->PluginCount(), 0U);
    EXPECT_EQ(host.ForTestResidentBinaryCount(), 0U);
    EXPECT_TRUE(host.DestroyPod(created.Value()).Clean());
    tools::cli::ValidateOptions options;
    options.PluginDirectory = VASE_PACKAGE_STATIC_STAGE;
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::ValidateDirectory(options, out, err), tools::cli::kExitCheckFailed) << out.str() << err.str();
    EXPECT_NE(out.str().find("cannot verify"), std::string::npos) << out.str();
}

TEST_F(StaticPackageAssembly, DisabledMemberInParticipatingStaticPackageIsCheckedOnEveryPod)
{
    const testing_support::CatalogSandbox sandbox{"static-release-drift"};
    sandbox.CopyFile("samples/pack.json", std::filesystem::path{VASE_PACKAGE_STATIC_SAMPLE_STAGE} / "pack.json");
    vase::PluginCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    const vase::InProcessBinding sampleBinding{
        .SourceId = "StaticSamples",
        .Entries = vase::pack::vasepluginsstatic::kEntryPoints,
    };
    const std::array bindings{sampleBinding};
    vase::PluginOverride failing;
    failing.Id = "Vase.Failing";
    failing.Enabled = false;
    const std::array overrides{failing};
    vase::LoadRequest request;
    request.Overrides = overrides;
    const auto first = catalog.Solve(request, bindings);
    ASSERT_TRUE(first.IsOk());
    vase::PluginHost host;
    const auto created = host.CreatePod(first.Value().Plan);
    ASSERT_TRUE(created.IsOk());
    EXPECT_TRUE(host.DestroyPod(created.Value()).Clean());
    const auto manifestPath = sandbox.Root / "samples" / "pack.json";
    auto manifest = vase::ParsePackageManifestFile(manifestPath);
    ASSERT_TRUE(manifest.IsOk());
    for (auto& member : manifest.Value().Plugins)
    {
        if (member.Id == "Vase.Failing")
        {
            member.DisplayName += " changed";
        }
    }
    ASSERT_TRUE(vase::WritePackageManifestFile(manifestPath, manifest.Value()).IsOk());
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    const auto next = catalog.Solve(request, bindings);
    ASSERT_TRUE(next.IsOk());
    std::size_t stage0Calls = 0;
    vase::PodOptions options;
    options.Stage0 = [&stage0Calls](vase::Context&) { ++stage0Calls; };
    const auto refused = host.CreatePod(next.Value().Plan, options);
    ASSERT_FALSE(refused.IsOk());
    EXPECT_NE(refused.GetError().Message().find("Vase.Failing"), std::string::npos);
    EXPECT_NE(refused.GetError().Message().find("displayName"), std::string::npos);
    EXPECT_EQ(stage0Calls, 0U);
    EXPECT_EQ(host.ForTestCounters().PluginInstances, 0U);
    EXPECT_EQ(host.ForTestCounters().Scopes, 0U);
    EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U);
    EXPECT_EQ(host.ForTestResidentBinaryCount(), 1U);
}
