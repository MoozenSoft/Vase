#include "Vase/Catalog/PluginCatalog.h"

#include "CatalogSandbox.h"
#include "PackageDescriptorProbe.h"
#include "Vase/Catalog/LoadRequest.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PackagePlan.h"

#include <array>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

namespace
{

std::string Pack(std::string_view source, std::string_view id = "Probe.A")
{
    return R"({"schemaVersion":1,"sourceId":")" + std::string(source) +
           R"(","source":{"kind":"inProcess"},"plugins":[{"id":")" + std::string(id) +
           R"(","config":[{"key":"Text","type":"string","default":"资源值"}]}]})";
}

TEST(CatalogResources, OwnsInputAfterBuffersAreDestroyed)
{
    vase::PluginCatalog catalog;
    {
        const std::string location = "assets/资源包/pack.json";
        const std::string contents = Pack("PackA");
        const std::array resources{vase::PackageResource{.Location = location, .Contents = contents}};
        ASSERT_TRUE(catalog.RefreshResources(resources).IsOk());
    }
    const auto* entry = catalog.Find("Probe.A");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->Subdirectory, "PackA");
    ASSERT_EQ(entry->Config.size(), 1U);
    EXPECT_EQ(entry->Config.front().DefaultStr, "资源值");
    const auto* source = catalog.SourceFor("Probe.A");
    ASSERT_NE(source, nullptr);
    EXPECT_EQ(source->ResourceLocation, "assets/资源包/pack.json");
    EXPECT_TRUE(source->ManifestPath.empty());
    EXPECT_TRUE(source->BinaryPath.empty());
    EXPECT_TRUE(catalog.Directory().empty());
    EXPECT_TRUE(catalog.Warnings().empty());
    const std::array bindings{vase::InProcessBinding{.SourceId = "PackA", .Entries = testing_support::ProbeEntries()}};
    const vase::LoadRequest request;
    const auto solved = catalog.Solve(request, bindings);
    ASSERT_TRUE(solved.IsOk()) << solved.GetError().Message();
    ASSERT_EQ(solved.Value().Plan.Ordered.size(), 1U);
    EXPECT_EQ(solved.Value().Plan.Ordered.front().Id, "Probe.A");
    EXPECT_EQ(solved.Value().Plan.Ordered.front().Id.data(), entry->Id.data());
    EXPECT_TRUE(solved.Value().Plan.Ordered.front().BinaryPath.empty());
}

TEST(CatalogResources, LocationsAreOpaqueBytesAndDetermineSourceOrder)
{
    vase::PluginCatalog catalog;
    const std::string upper = Pack("Upper", "Probe.Upper");
    const std::string lower = Pack("Lower", "Probe.Lower");
    const std::string dot = Pack("Dot", "Probe.Dot");
    const std::string plain = Pack("Plain", "Probe.Plain");
    const std::array resources{
        vase::PackageResource{.Location = "asset:/a/pack.json", .Contents = lower},
        vase::PackageResource{.Location = "asset:/A/pack.json", .Contents = upper},
        vase::PackageResource{.Location = "asset:/a/./pack.json", .Contents = dot},
        vase::PackageResource{.Location = "not a disk path:<>|?*", .Contents = plain},
    };
    ASSERT_TRUE(catalog.RefreshResources(resources).IsOk());
    ASSERT_EQ(catalog.Sources().size(), 4U);
    auto source = catalog.Sources().begin();
    EXPECT_EQ((source++)->Id, "Upper");
    EXPECT_EQ((source++)->Id, "Dot");
    EXPECT_EQ((source++)->Id, "Lower");
    EXPECT_EQ(source->Id, "Plain");
    EXPECT_EQ(catalog.Ids(), (std::vector<std::string>{"Probe.Dot", "Probe.Lower", "Probe.Plain", "Probe.Upper"}));
}

TEST(CatalogResources, InvalidBatchesPreserveSnapshotAndBorrowedPlan)
{
    const testing_support::CatalogSandbox sandbox{"resource-failure"};
    sandbox.WriteFile("base/pack.json", Pack("Base"));
    sandbox.CreateDir("missing");
    vase::PluginCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    const std::array bindings{vase::InProcessBinding{.SourceId = "Base", .Entries = testing_support::ProbeEntries()}};
    const vase::LoadRequest request;
    const auto solved = catalog.Solve(request, bindings);
    ASSERT_TRUE(solved.IsOk());
    const auto* original = catalog.Find("Probe.A");
    const auto* originalSource = catalog.SourceFor("Probe.A");
    const auto directory = catalog.Directory();
    ASSERT_EQ(catalog.Warnings().size(), 1U);
    const auto warning = catalog.Warnings().front();
    const std::string first = Pack("One");
    const std::string second = Pack("Two", "Probe.B");
    const std::string duplicateSource = Pack("One", "Probe.B");
    const std::string duplicateId = Pack("Two");
    const std::string dynamic =
        R"({"schemaVersion":1,"sourceId":"Dynamic","source":{"kind":"dynamic","binary":"Library"},"plugins":[{"id":"Probe.D"}]})";
    const std::string nulLocation{"asset\0name", 10};
    const std::vector<std::vector<vase::PackageResource>> batches{
        {{.Location = "same", .Contents = first}, {.Location = "same", .Contents = second}},
        {{.Location = "", .Contents = first}},
        {{.Location = nulLocation, .Contents = first}},
        {{.Location = "one", .Contents = first}, {.Location = "two", .Contents = duplicateSource}},
        {{.Location = "one", .Contents = first}, {.Location = "two", .Contents = duplicateId}},
        {{.Location = "dynamic", .Contents = dynamic}},
        {{.Location = "broken", .Contents = "{"}},
    };
    for (const auto& resources : batches)
    {
        const auto refreshed = catalog.RefreshResources(resources);
        ASSERT_FALSE(refreshed.IsOk());
        EXPECT_EQ(catalog.Ids(), (std::vector<std::string>{"Probe.A"}));
        EXPECT_EQ(catalog.Find("Probe.A"), original);
        EXPECT_EQ(catalog.SourceFor("Probe.A"), originalSource);
        EXPECT_EQ(catalog.Directory(), directory);
        ASSERT_EQ(catalog.Sources().size(), 1U);
        EXPECT_EQ(catalog.Sources().front().Id, "Base");
        EXPECT_TRUE(catalog.Sources().front().ResourceLocation.empty());
        ASSERT_EQ(catalog.Warnings().size(), 1U);
        EXPECT_EQ(catalog.Warnings().front().Subdirectory, warning.Subdirectory);
        EXPECT_EQ(catalog.Warnings().front().Message, warning.Message);
        EXPECT_EQ(solved.Value().Plan.Ordered.front().Id, "Probe.A");
        EXPECT_EQ(solved.Value().Plan.Ordered.front().Id.data(), original->Id.data());
    }
}

TEST(CatalogResources, EmptyBatchSuccessfullyCreatesEmptySnapshot)
{
    vase::PluginCatalog catalog;
    EXPECT_FALSE(catalog.Solve(vase::LoadRequest{}).IsOk());
    ASSERT_TRUE(catalog.RefreshResources({}).IsOk());
    EXPECT_TRUE(catalog.Solve(vase::LoadRequest{}).IsOk());
    const std::string contents = Pack("PackA");
    const std::array resources{vase::PackageResource{.Location = "assets/pack.json", .Contents = contents}};
    ASSERT_TRUE(catalog.RefreshResources(resources).IsOk());
    ASSERT_TRUE(catalog.RefreshResources({}).IsOk());
    EXPECT_TRUE(catalog.Ids().empty());
    EXPECT_TRUE(catalog.Sources().empty());
    EXPECT_TRUE(catalog.Warnings().empty());
    EXPECT_TRUE(catalog.Directory().empty());
    const auto solved = catalog.Solve(vase::LoadRequest{});
    ASSERT_TRUE(solved.IsOk());
    EXPECT_TRUE(solved.Value().Plan.Ordered.empty());
}

TEST(CatalogResources, DirectoryAndResourcesReplaceEachOtherTransactionally)
{
    const testing_support::CatalogSandbox sandbox{"resource-switch"};
    sandbox.WriteFile("disk/pack.json", Pack("Disk"));
    sandbox.CreateDir("missing");
    vase::PluginCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    ASSERT_FALSE(catalog.Directory().empty());
    ASSERT_EQ(catalog.Warnings().size(), 1U);
    {
        const std::string contents = Pack("Resource");
        const std::array resources{vase::PackageResource{.Location = "assets/pack.json", .Contents = contents}};
        ASSERT_TRUE(catalog.RefreshResources(resources).IsOk());
    }
    EXPECT_TRUE(catalog.Directory().empty());
    EXPECT_TRUE(catalog.Warnings().empty());
    ASSERT_EQ(catalog.Sources().size(), 1U);
    EXPECT_EQ(catalog.Sources().front().ResourceLocation, "assets/pack.json");
    EXPECT_TRUE(catalog.Sources().front().ManifestPath.empty());
    const auto* resourceEntry = catalog.Find("Probe.A");
    const auto* resourceSource = catalog.SourceFor("Probe.A");
    const vase::InProcessBinding binding{.SourceId = "Resource", .Entries = testing_support::ProbeEntries()};
    const std::array bindings{binding};
    const vase::LoadRequest request;
    const auto solved = catalog.Solve(request, bindings);
    ASSERT_TRUE(solved.IsOk());
    const std::array invalid{vase::PackageResource{.Location = "assets/bad.json", .Contents = "{"}};
    EXPECT_FALSE(catalog.RefreshResources(invalid).IsOk());
    EXPECT_EQ(catalog.Find("Probe.A"), resourceEntry);
    EXPECT_EQ(catalog.SourceFor("Probe.A"), resourceSource);
    sandbox.WriteFile("disk/pack.json", "{");
    EXPECT_FALSE(catalog.Refresh(sandbox.Root).IsOk());
    EXPECT_EQ(catalog.Find("Probe.A"), resourceEntry);
    EXPECT_EQ(catalog.SourceFor("Probe.A"), resourceSource);
    EXPECT_EQ(solved.Value().Plan.Ordered.front().Id.data(), resourceEntry->Id.data());
    EXPECT_EQ(solved.Value().Plan.Ordered.front().Id, "Probe.A");
    EXPECT_TRUE(catalog.Directory().empty());
    EXPECT_EQ(catalog.Sources().front().ResourceLocation, "assets/pack.json");
    sandbox.WriteFile("disk/pack.json", Pack("Disk"));
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    EXPECT_EQ(catalog.Directory(), sandbox.Root);
    EXPECT_EQ(catalog.Sources().front().ManifestPath, sandbox.Root / "disk/pack.json");
    EXPECT_TRUE(catalog.Sources().front().ResourceLocation.empty());
    EXPECT_EQ(catalog.Find("Probe.A")->Subdirectory, "disk");
    ASSERT_EQ(catalog.Warnings().size(), 1U);
}

} // namespace
