#include "CatalogSandbox.h"
#include "Cli.h"
#include "MobileScenario.h"
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
#include <fstream>
#include <ios>
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

std::string ReadStageManifest(const std::filesystem::path& path)
{
    const std::ifstream input{path, std::ios::binary};
    EXPECT_TRUE(input.is_open()) << path;
    std::ostringstream contents;
    contents << input.rdbuf();
    EXPECT_FALSE(input.bad()) << path;
    return contents.str();
}

class StaticResourceAssembly : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const std::filesystem::path stage{VASE_PACKAGE_STATIC_STAGE};
        const std::string provider = ReadStageManifest(stage / "provider pkg" / "pack.json");
        const std::string consumer = ReadStageManifest(stage / "consumer pkg" / "pack.json");
        const std::array resources{
            vase::PackageResource{.Location = "assets/vase/Provider/pack.json", .Contents = provider},
            vase::PackageResource{.Location = "assets/vase/Consumer/pack.json", .Contents = consumer},
        };
        const auto imported = Catalog.RefreshResources(resources);
        ASSERT_TRUE(imported.IsOk()) << imported.GetError().Message();
        // 缓冲只活到 SetUp 返回；全部证人在随后消费 Catalog 自持的资源快照。
    }

public:
    vase::PluginCatalog Catalog;
    const std::array<vase::InProcessBinding, 2> Bindings{
        vase::InProcessBinding{.SourceId = "Provider", .Entries = vase::pack::vasepackageprovider::kEntryPoints},
        vase::InProcessBinding{.SourceId = "Consumer", .Entries = vase::pack::vasepackageconsumer::kEntryPoints},
    };
};

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

TEST_F(StaticResourceAssembly, TwoPackagesConsumeConfiguredServiceAfterInputBuffersAreReleased)
{
    EXPECT_TRUE(Catalog.Directory().empty());
    ASSERT_EQ(Catalog.Sources().size(), 2U);
    for (const auto& source : Catalog.Sources())
    {
        EXPECT_EQ(source.ResourceLocation, "assets/vase/" + source.Id + "/pack.json");
        EXPECT_TRUE(source.ManifestPath.empty());
        EXPECT_TRUE(source.BinaryPath.empty());
    }
    vase::PluginOverride provider;
    provider.Id = "Vase.PackageProvider";
    provider.Enabled = true;
    provider.Config.Set("Value", vase::Value::From(std::int32_t{7}));
    const std::array overrides{provider};
    vase::LoadRequest request;
    request.Overrides = overrides;
    const auto solved = Catalog.Solve(request, Bindings);
    ASSERT_TRUE(solved.IsOk()) << solved.GetError().Message();
    ASSERT_EQ(solved.Value().Plan.PackageSources.size(), 2U);
    vase::PluginHost host;
    const auto created = host.CreatePod(solved.Value().Plan);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
    EXPECT_EQ(host.Resolve(created.Value())->Root().Get<package_fixture::IResult>().Observed(), 7);
    EXPECT_TRUE(host.DestroyPod(created.Value()).Clean());
}

TEST_F(StaticResourceAssembly, FiftyPodsConsumeFreshConfigurationAndReleaseAllEdges)
{
    vase::PluginHost host;
    for (std::size_t round = 0; round < 50; ++round)
    {
        SCOPED_TRACE(round);
        const std::int32_t value = round % 3 == 1 ? 3 : 7;
        vase::PluginOverride provider;
        provider.Id = "Vase.PackageProvider";
        provider.Enabled = true;
        provider.Config.Set("Value", vase::Value::From(value));
        const std::array overrides{provider};
        vase::LoadRequest request;
        request.Overrides = overrides;
        const auto solved = Catalog.Solve(request, Bindings);
        ASSERT_TRUE(solved.IsOk()) << solved.GetError().Message();
        const auto created = host.CreatePod(solved.Value().Plan);
        ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
        EXPECT_EQ(host.Resolve(created.Value())->Root().Get<package_fixture::IResult>().Observed(), value);
        EXPECT_EQ(host.ForTestLedgerEdgeCount(), 1U);
        EXPECT_EQ(host.ForTestCounters().PluginInstances, 2U);
        EXPECT_TRUE(host.DestroyPod(created.Value()).Clean());
        EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U);
        EXPECT_EQ(host.ForTestCounters().PluginInstances, 0U);
        EXPECT_EQ(host.ForTestCounters().Scopes, 0U);
    }
}

TEST_F(StaticResourceAssembly, SharedMobileScenarioRunsFiftyRoundsAndCleansEveryPod)
{
    const std::filesystem::path stage{VASE_PACKAGE_STATIC_STAGE};
    const std::string provider = ReadStageManifest(stage / "provider pkg" / "pack.json");
    const std::string consumer = ReadStageManifest(stage / "consumer pkg" / "pack.json");
    const std::array resources{
        vase::PackageResource{.Location = "assets/vase/Provider/pack.json", .Contents = provider},
        vase::PackageResource{.Location = "assets/vase/Consumer/pack.json", .Contents = consumer},
    };

    const auto scenario = vase_mobile::RunPackageScenario(resources);
    ASSERT_TRUE(scenario.IsOk());
    EXPECT_EQ(scenario.Value().CompletedRounds, 50U);
    EXPECT_TRUE(scenario.Value().Clean);
}

TEST_F(StaticResourceAssembly, BooleanConfigurationCannotReplaceInt32)
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

TEST_F(StaticResourceAssembly, ParticipatingPackageRequiresItsBinding)
{
    vase::PluginOverride provider;
    provider.Id = "Vase.PackageProvider";
    provider.Enabled = true;
    const std::array overrides{provider};
    vase::LoadRequest request;
    request.Overrides = overrides;
    const std::array missingProvider{Bindings.back()};
    const auto solved = Catalog.Solve(request, missingProvider);
    ASSERT_FALSE(solved.IsOk());
    EXPECT_NE(solved.GetError().Message().find("Provider"), std::string::npos);
}

TEST_F(StaticResourceAssembly, WrongPackageEntryIsRejectedBeforePodCreation)
{
    vase::PluginOverride provider;
    provider.Id = "Vase.PackageProvider";
    provider.Enabled = true;
    const std::array overrides{provider};
    vase::LoadRequest request;
    request.Overrides = overrides;
    const std::array wrongEntries{
        vase::InProcessBinding{.SourceId = "Provider", .Entries = vase::pack::vasepackageconsumer::kEntryPoints},
        vase::InProcessBinding{.SourceId = "Consumer", .Entries = vase::pack::vasepackageprovider::kEntryPoints},
    };
    const auto solved = Catalog.Solve(request, wrongEntries);
    ASSERT_TRUE(solved.IsOk()) << solved.GetError().Message();
    vase::PluginHost host;
    std::size_t stage0Calls = 0;
    vase::PodOptions options;
    options.Stage0 = [&stage0Calls](vase::Context&) { ++stage0Calls; };
    const auto refused = host.CreatePod(solved.Value().Plan, options);
    ASSERT_FALSE(refused.IsOk());
    EXPECT_NE(refused.GetError().Message().find("Consumer"), std::string::npos);
    EXPECT_EQ(stage0Calls, 0U);
    EXPECT_EQ(host.ForTestCounters().PluginInstances, 0U);
    EXPECT_EQ(host.ForTestCounters().Scopes, 0U);
    EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U);
}

TEST_F(StaticResourceAssembly, EntirelyDisabledPackagesNeedNoBindingsAndDoNotEnterThePlan)
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
    ASSERT_TRUE(solved.IsOk()) << solved.GetError().Message();
    EXPECT_TRUE(solved.Value().Plan.PackageSources.empty());
    vase::PluginHost host;
    const auto created = host.CreatePod(solved.Value().Plan);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
    EXPECT_EQ(host.Resolve(created.Value())->PluginCount(), 0U);
    EXPECT_EQ(host.ForTestResidentBinaryCount(), 0U);
    EXPECT_TRUE(host.DestroyPod(created.Value()).Clean());
    EXPECT_EQ(host.ForTestCounters().PluginInstances, 0U);
    EXPECT_EQ(host.ForTestCounters().Scopes, 0U);
    EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U);
}

TEST_F(StaticResourceAssembly, DisabledMemberDriftIsRejectedOnEveryPodBeforeAnyInstance)
{
    vase::PluginCatalog catalog;
    const auto importSamples = [&catalog](bool drift)
    {
        std::string contents = ReadStageManifest(std::filesystem::path{VASE_PACKAGE_STATIC_SAMPLE_STAGE} / "pack.json");
        if (drift)
        {
            const std::string original = "失败演示";
            const auto position = contents.find(original);
            ASSERT_NE(position, std::string::npos);
            contents.replace(position, original.size(), "changed disabled member");
        }
        const vase::PackageResource resource{.Location = "assets/vase/StaticSamples/pack.json", .Contents = contents};
        const std::array resources{resource};
        const auto imported = catalog.RefreshResources(resources);
        ASSERT_TRUE(imported.IsOk()) << imported.GetError().Message();
    };
    ASSERT_NO_FATAL_FAILURE(importSamples(false));
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
    vase::PluginHost host;
    {
        const auto first = catalog.Solve(request, bindings);
        ASSERT_TRUE(first.IsOk()) << first.GetError().Message();
        const auto created = host.CreatePod(first.Value().Plan);
        ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
        EXPECT_TRUE(host.DestroyPod(created.Value()).Clean());
    }
    ASSERT_NO_FATAL_FAILURE(importSamples(true));
    const auto next = catalog.Solve(request, bindings);
    ASSERT_TRUE(next.IsOk()) << next.GetError().Message();
    ASSERT_EQ(next.Value().Plan.PackageSources.size(), 1U);
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
