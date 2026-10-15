#include "PackageBuild.h"

#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Host/Loader.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/Host/PackageInspect.h"
#include "Vase/Host/PackagePlan.h"
#include "Vase/PluginDescriptor.h"
#include "VasePackageProvider.entries.h"

#include "CatalogSandbox.h"
#include "PackageBuildProbe.h"
#include "PackageDescriptorProbe.h"

#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

// 真实 STATIC 包成员的描述符函数：定义在 Provider fixture 的实现里（分发表按类名拼该符号）。
// NOLINTNEXTLINE(readability-identifier-naming) 名字是 VASE_PLUGIN 生成物的定名 extern "C" 符号，无代码级出路
extern "C" const vase::PluginDescriptor* VasePluginDesc_PackageProviderPlugin();

namespace
{

const vase::ManifestEntry* Find(const vase::PackageManifest& manifest, const std::string& id)
{
    const auto found =
        std::ranges::find_if(manifest.Plugins, [&id](const vase::ManifestEntry& entry) { return entry.Id == id; });
    return found == manifest.Plugins.end() ? nullptr : &*found;
}

// 与探针成员同名（Probe.A）但不在包枚举集里的描述符：用来证伪「成员描述符必须属于包枚举」。
const vase::PluginDescriptor* OutsiderDescriptor()
{
    static const vase::PluginMeta kMeta{
        .Id = "Probe.A",
        .DisplayName = "Outsider",
        .Version = "9.9.9",
        .Requires = {},
        .Provides = {},
    };
    static const vase::PluginDescriptor kDesc{.HeaderVersion = vase::kHeaderVersion, .Meta = &kMeta};
    return &kDesc;
}

} // namespace

TEST(PackageBuild, DynamicReadsRealSharedPackage)
{
    tools::cli::PackageBuildOptions options;
    options.SourceId = "VasePlugins";
    options.BinaryPath = VASE_FIXTURE_VASEPLUGINS;
    options.Members = {
        {.DescriptorSymbol = "VasePluginDesc_HelloPlugin", .SourceManifest = VASE_FIXTURE_HELLO_MANIFEST},
        {.DescriptorSymbol = "VasePluginDesc_DependentPlugin", .SourceManifest = VASE_FIXTURE_DEPENDENT_MANIFEST},
        {.DescriptorSymbol = "VasePluginDesc_FailingPlugin", .SourceManifest = VASE_FIXTURE_FAILING_MANIFEST},
    };
    auto result = tools::cli::GeneratePackageManifest(options);
    ASSERT_TRUE(result.IsOk()) << result.GetError().Message();
    EXPECT_EQ(result.Value().SourceId, "VasePlugins");
    EXPECT_EQ(result.Value().Kind, vase::PackageSourceKind::kDynamic);
    EXPECT_EQ(result.Value().Binary, "VasePlugins");
    ASSERT_EQ(result.Value().Plugins.size(), 3U);
    vase::detail::Loader loader;
    const auto resident = loader.EnsureResident(options.BinaryPath);
    ASSERT_TRUE(resident.IsOk()) << resident.GetError().Message();
    const auto descriptors = vase::InspectPackageMembers(*resident.Value());
    ASSERT_TRUE(descriptors.IsOk()) << descriptors.GetError().Message();
    for (const auto& member : result.Value().Plugins)
    {
        const auto found = std::ranges::find_if(descriptors.Value(), [&member](const vase::PluginDescriptor* descriptor)
                                                { return descriptor->Meta->Id == member.Id; });
        ASSERT_NE(found, descriptors.Value().end());
        EXPECT_TRUE(vase::CompareDescriptor(vase::BuildExpectation(member), **found).IsOk()) << member.Id;
    }
    loader.Unload(*resident.Value());

    const testing_support::CatalogSandbox sandbox{"package-build-move"};
    sandbox.CreateDir("output");
    const vase::PackageManifest moved = std::move(result.Value());
    const auto path = sandbox.Root / "output/pack.json";
    const auto written = vase::WritePackageManifestFile(path, moved);
    ASSERT_TRUE(written.IsOk()) << written.GetError().Message();
    const auto parsed = vase::ParsePackageManifestFile(path);
    ASSERT_TRUE(parsed.IsOk()) << parsed.GetError().Message();
    EXPECT_EQ(parsed.Value().Plugins.size(), 3U);
}

TEST(PackageBuild, StaticReadsRealLinkedPackage)
{
    tools::cli::PackageBuildOptions options;
    options.SourceId = "Provider";
    options.Kind = vase::PackageSourceKind::kInProcess;
    options.Entries = vase::pack::vasepackageprovider::kEntryPoints;
    options.Members = {
        {.Descriptor = &VasePluginDesc_PackageProviderPlugin, .SourceManifest = VASE_FIXTURE_PACKAGEPROVIDER_MANIFEST},
    };
    const auto result = tools::cli::GeneratePackageManifest(options);
    ASSERT_TRUE(result.IsOk()) << result.GetError().Message();
    EXPECT_EQ(result.Value().Kind, vase::PackageSourceKind::kInProcess);
    EXPECT_TRUE(result.Value().Binary.empty());
    ASSERT_EQ(result.Value().Plugins.size(), 1U);
    EXPECT_EQ(result.Value().Plugins.front().Id, "Vase.PackageProvider");
    EXPECT_FALSE(result.Value().Plugins.front().EnabledByDefault);
    const auto descriptors = vase::InspectPackageMembers(options.Entries);
    ASSERT_TRUE(descriptors.IsOk()) << descriptors.GetError().Message();
    ASSERT_EQ(descriptors.Value().size(), 1U);
    EXPECT_TRUE(
        vase::CompareDescriptor(vase::BuildExpectation(result.Value().Plugins.front()), *descriptors.Value().front())
            .IsOk());
}

TEST(PackageBuild, SourcePolicyFalseAndUndeclaredDefaultsTrue)
{
    testing_support::ResetPackageProbe();
    const testing_support::CatalogSandbox sandbox{"package-build-policy"};
    sandbox.WriteFile("a/plugin.json", R"({"schemaVersion":1,"id":"Probe.A","enabledByDefault":false})");
    auto options = testing_support::MakeProbePackageBuildOptions();
    options.Members.front().SourceManifest = sandbox.Root / "a/plugin.json";
    const auto result = tools::cli::GeneratePackageManifest(options);
    ASSERT_TRUE(result.IsOk()) << result.GetError().Message();
    ASSERT_NE(Find(result.Value(), "Probe.A"), nullptr);
    ASSERT_NE(Find(result.Value(), "Probe.B"), nullptr);
    EXPECT_FALSE(Find(result.Value(), "Probe.A")->EnabledByDefault);
    EXPECT_TRUE(Find(result.Value(), "Probe.B")->EnabledByDefault);
}

TEST(PackageBuild, RejectsWrongSourceManifestId)
{
    testing_support::ResetPackageProbe();
    const testing_support::CatalogSandbox sandbox{"package-build"};
    sandbox.WriteFile("wrong/plugin.json", R"({"schemaVersion":1,"id":"Wrong.Id"})");
    auto options = testing_support::MakeProbePackageBuildOptions();
    options.Members.begin()->SourceManifest = sandbox.Root / "wrong/plugin.json";
    EXPECT_FALSE(tools::cli::GeneratePackageManifest(options).IsOk());
}

TEST(PackageBuild, RejectsMemberDescriptorOutsidePackageEnumeration)
{
    testing_support::ResetPackageProbe();
    auto options = testing_support::MakeProbePackageBuildOptions();
    options.Members.begin()->Descriptor = &OutsiderDescriptor;
    EXPECT_FALSE(tools::cli::GeneratePackageManifest(options).IsOk());
}

TEST(PackageBuild, RejectsMissingAndMalformedSourceManifest)
{
    const testing_support::CatalogSandbox sandbox{"package-build-bad-json"};
    auto options = testing_support::MakeProbePackageBuildOptions();
    options.Members.front().SourceManifest = sandbox.Root / "absent/plugin.json";
    EXPECT_FALSE(tools::cli::GeneratePackageManifest(options).IsOk());
    sandbox.WriteFile("bad/plugin.json", "{");
    options.Members.front().SourceManifest = sandbox.Root / "bad/plugin.json";
    EXPECT_FALSE(tools::cli::GeneratePackageManifest(options).IsOk());
}

TEST(PackageBuild, RejectsMissingOrDuplicateInputMember)
{
    auto options = testing_support::MakeProbePackageBuildOptions();
    options.Members.pop_back();
    EXPECT_FALSE(tools::cli::GeneratePackageManifest(options).IsOk());
    options.Members.push_back(options.Members.front());
    EXPECT_FALSE(tools::cli::GeneratePackageManifest(options).IsOk());
}

TEST(PackageBuild, DescriptorFactsOverrideSourceDisplayFields)
{
    const testing_support::CatalogSandbox sandbox{"package-build-facts"};
    sandbox.WriteFile(
        "a/plugin.json",
        R"({"schemaVersion":1,"id":"Probe.A","displayName":"Other","version":"9.9.9","enabledByDefault":false})");
    auto options = testing_support::MakeProbePackageBuildOptions();
    options.Members.front().SourceManifest = sandbox.Root / "a/plugin.json";
    auto result = tools::cli::GeneratePackageManifest(options);
    ASSERT_TRUE(result.IsOk()) << result.GetError().Message();
    EXPECT_EQ(Find(result.Value(), "Probe.A")->DisplayName, "Probe A");
    EXPECT_EQ(Find(result.Value(), "Probe.A")->Version, "1.0.0");
    EXPECT_FALSE(Find(result.Value(), "Probe.A")->EnabledByDefault);
}
