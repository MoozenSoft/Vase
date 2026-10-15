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
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Context.h"
#include "Vase/Pod/Pod.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

namespace
{

class PackageAssembly : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const auto refreshed = Catalog.Refresh(VASE_PACKAGE_DYNAMIC_STAGE);
        ASSERT_TRUE(refreshed.IsOk()) << refreshed.GetError().Message();
    }

public:
    static std::array<vase::PluginOverride, 2> ConfiguredOverrides()
    {
        vase::PluginOverride provider;
        provider.Id = "Vase.PackageProvider";
        provider.Enabled = true;
        provider.Config.Set("Value", vase::Value::From(std::int32_t{7}));
        vase::PluginOverride failing;
        failing.Id = "Vase.Failing";
        failing.Enabled = false;
        return {provider, failing};
    }

    vase::PluginCatalog Catalog;
};

} // namespace

TEST_F(PackageAssembly, StagedDynamicPackageConsumesConfigurationAcrossRepeatedPods)
{
    const auto overrides = ConfiguredOverrides();
    vase::LoadRequest request;
    request.Overrides = overrides;
    const auto solved = Catalog.Solve(request);
    ASSERT_TRUE(solved.IsOk()) << solved.GetError().Message();
    ASSERT_EQ(solved.Value().Plan.PackageSources.size(), 1U);
    const auto& source = solved.Value().Plan.PackageSources.front();
    EXPECT_EQ(source.BinaryPath.parent_path(), std::filesystem::path{VASE_PACKAGE_DYNAMIC_STAGE} / "assembly pkg");
    EXPECT_EQ(source.Members.size(), 3U);
    vase::PluginHost host;
    for (int round = 0; round < 3; ++round)
    {
        const auto created = host.CreatePod(solved.Value().Plan);
        ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
        ASSERT_NE(host.Resolve(created.Value()), nullptr);
        EXPECT_EQ(host.Resolve(created.Value())->PluginCount(), 2U);
        EXPECT_EQ(host.Resolve(created.Value())->Root().Get<package_fixture::IResult>().Observed(), 7);
        EXPECT_EQ(host.ForTestLedgerEdgeCount(), 1U);
        EXPECT_TRUE(host.DestroyPod(created.Value()).Clean());
        EXPECT_EQ(host.ForTestCounters().PluginInstances, 0U);
        EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U);
    }
}

TEST_F(PackageAssembly, InvalidConfigurationIsRejectedBeforeLoading)
{
    auto overrides = ConfiguredOverrides();
    overrides.front().Config.Set("Value", vase::Value::From(true));
    vase::LoadRequest request;
    request.Overrides = overrides;
    const auto solved = Catalog.Solve(request);
    ASSERT_FALSE(solved.IsOk());
    EXPECT_NE(solved.GetError().Message().find("Value"), std::string::npos);
}

TEST_F(PackageAssembly, DisabledMemberDriftRejectsNextPodBeforeStage0AndKeepsImageResident)
{
    const testing_support::CatalogSandbox sandbox{"release-drift"};
    std::error_code ec;
    std::filesystem::copy(VASE_PACKAGE_DYNAMIC_STAGE, sandbox.Root, std::filesystem::copy_options::recursive, ec);
    ASSERT_FALSE(ec) << ec.message();
    vase::PluginCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    const auto overrides = ConfiguredOverrides();
    vase::LoadRequest request;
    request.Overrides = overrides;
    const auto first = catalog.Solve(request);
    ASSERT_TRUE(first.IsOk());
    vase::PluginHost host;
    const auto created = host.CreatePod(first.Value().Plan);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
    EXPECT_TRUE(host.DestroyPod(created.Value()).Clean());

    const auto manifestPath = sandbox.Root / "assembly pkg" / "pack.json";
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
    const auto next = catalog.Solve(request);
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
    EXPECT_EQ(host.ForTestCounters().Services, 0U);
    EXPECT_EQ(host.ForTestCounters().Scopes, 0U);
    EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U);
    EXPECT_EQ(host.ForTestResidentBinaryCount(), 1U);
}

TEST_F(PackageAssembly, EntirelyDisabledPackageNeedsNoLibraryButValidateReportsFailure)
{
    const testing_support::CatalogSandbox sandbox{"release-missing-library"};
    sandbox.CopyFile("assembly pkg/pack.json",
                     std::filesystem::path{VASE_PACKAGE_DYNAMIC_STAGE} / "assembly pkg" / "pack.json");
    vase::PluginCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    std::vector<vase::PluginOverride> overrides;
    for (const auto& id : catalog.Ids())
    {
        vase::PluginOverride disabled;
        disabled.Id = id;
        disabled.Enabled = false;
        overrides.push_back(disabled);
    }
    vase::LoadRequest request;
    request.Overrides = overrides;
    const auto solved = catalog.Solve(request);
    ASSERT_TRUE(solved.IsOk());
    EXPECT_TRUE(solved.Value().Plan.PackageSources.empty());
    vase::PluginHost host;
    const auto created = host.CreatePod(solved.Value().Plan);
    ASSERT_TRUE(created.IsOk());
    EXPECT_EQ(host.Resolve(created.Value())->PluginCount(), 0U);
    EXPECT_EQ(host.ForTestResidentBinaryCount(), 0U);
    EXPECT_TRUE(host.DestroyPod(created.Value()).Clean());
    tools::cli::ValidateOptions options;
    options.PluginDirectory = sandbox.Root;
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::ValidateDirectory(options, out, err), tools::cli::kExitCheckFailed) << out.str() << err.str();
}

TEST_F(PackageAssembly, RealCallbackFailureRetainsToleranceAndStrictSemantics)
{
    auto overrides = ConfiguredOverrides();
    overrides.back().Enabled = true;
    vase::LoadRequest request;
    request.Overrides = overrides;
    const auto solved = Catalog.Solve(request);
    ASSERT_TRUE(solved.IsOk());
    vase::PluginHost host;
    const auto tolerant = host.CreatePod(solved.Value().Plan);
    ASSERT_TRUE(tolerant.IsOk());
    EXPECT_EQ(host.Resolve(tolerant.Value())->PluginCount(), 2U);
    ASSERT_EQ(host.Resolve(tolerant.Value())->Failures().size(), 1U);
    EXPECT_EQ(host.Resolve(tolerant.Value())->Failures().front().Id, "Vase.Failing");
    EXPECT_EQ(host.Resolve(tolerant.Value())->Root().Get<package_fixture::IResult>().Observed(), 7);
    EXPECT_TRUE(host.DestroyPod(tolerant.Value()).Clean());
    vase::PodOptions options;
    options.Strict = true;
    EXPECT_FALSE(host.CreatePod(solved.Value().Plan, options).IsOk());
    EXPECT_EQ(host.ForTestCounters().PluginInstances, 0U);
    EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U);
    EXPECT_EQ(host.ForTestResidentBinaryCount(), 1U);
}
