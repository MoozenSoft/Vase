// 动态包领回只取请求成员的盘上期望；同一物理库的其他成员不是兄弟库。
#include "Cli.h"
#include "Scan.h"

#include "Vase/Catalog/CatalogAdopt.h"
#include "Vase/Catalog/LibraryFileName.h"
#include "Vase/Catalog/LoadRequest.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Host/Evidence.h"
#include "Vase/Host/PluginHost.h"

#include "CatalogSandbox.h"

#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

class PackageAdopt : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const std::filesystem::path library = Sandbox.Root / "P" / vase::catalog_detail::LibraryFileName("VasePlugins");
        Sandbox.CopyFile("P/" + library.filename().string(), VASE_FIXTURE_VASEPLUGINS);
        tools::cli::PackageScanOptions options;
        options.BinaryPath = library;
        options.SourceId = "P";
        std::ostringstream out;
        std::ostringstream err;
        ASSERT_EQ(tools::cli::ScanPackage(options, out, err), tools::cli::kExitOk) << err.str();

        auto parsed = vase::ParsePackageManifestFile(PackPath());
        ASSERT_TRUE(parsed.IsOk()) << parsed.GetError().Message();
        for (vase::ManifestEntry& member : parsed.Value().Plugins)
        {
            member.EnabledByDefault = false;
        }
        ASSERT_TRUE(vase::WritePackageManifestFile(PackPath(), parsed.Value()).IsOk());
        ASSERT_TRUE(Catalog.Refresh(Sandbox.Root).IsOk());
        const auto solved = Catalog.Solve(vase::LoadRequest{});
        ASSERT_TRUE(solved.IsOk()) << solved.GetError().Message();
        const auto created = Host.CreatePod(solved.Value().Plan);
        ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
        Handle = created.Value();
        Ready = true;
    }

    void TearDown() override
    {
        if (Ready)
        {
            EXPECT_TRUE(Host.DestroyPod(Handle).Clean());
        }
    }

    [[nodiscard]] std::filesystem::path PackPath() const { return Sandbox.Root / "P" / "pack.json"; }

    [[nodiscard]] const testing_support::CatalogSandbox& SandboxRef() const { return Sandbox; }
    [[nodiscard]] vase::PluginHost& HostRef() { return Host; }
    [[nodiscard]] vase::PluginCatalog& CatalogRef() { return Catalog; }
    [[nodiscard]] vase::PodHandle Pod() const { return Handle; }
    [[nodiscard]] bool IsReady() const { return Ready; }

private:
    testing_support::CatalogSandbox Sandbox{"package-adopt"};
    vase::PluginHost Host;
    vase::PluginCatalog Catalog;
    vase::PodHandle Handle{};
    bool Ready = false;
};

TEST_F(PackageAdopt, ReadsChangedMemberFromDisk)
{
    ASSERT_TRUE(IsReady());
    auto parsed = vase::ParsePackageManifestFile(PackPath());
    ASSERT_TRUE(parsed.IsOk());
    const auto found =
        std::ranges::find(parsed.Value().Plugins, std::string_view{"Vase.Hello"}, &vase::ManifestEntry::Id);
    ASSERT_NE(found, parsed.Value().Plugins.end());
    found->Version = "9.9.9";
    ASSERT_TRUE(vase::WritePackageManifestFile(PackPath(), parsed.Value()).IsOk());

    const auto refused = vase::AdoptInto(CatalogRef(), HostRef(), Pod(), "Vase.Hello");
    ASSERT_FALSE(refused.IsOk());
    EXPECT_NE(refused.GetError().Message().find("manifest/binary mismatch"), std::string::npos)
        << refused.GetError().Message();
}

TEST_F(PackageAdopt, RejectsMemberRemovedAfterSnapshot)
{
    ASSERT_TRUE(IsReady());
    auto parsed = vase::ParsePackageManifestFile(PackPath());
    ASSERT_TRUE(parsed.IsOk());
    std::erase_if(parsed.Value().Plugins, [](const vase::ManifestEntry& member) { return member.Id == "Vase.Hello"; });
    ASSERT_TRUE(vase::WritePackageManifestFile(PackPath(), parsed.Value()).IsOk());

    const auto refused = vase::AdoptInto(CatalogRef(), HostRef(), Pod(), "Vase.Hello");
    ASSERT_FALSE(refused.IsOk());
    EXPECT_NE(refused.GetError().Message().find("Vase.Hello"), std::string::npos) << refused.GetError().Message();
}

TEST_F(PackageAdopt, RejectsMalformedPackageAfterSnapshot)
{
    ASSERT_TRUE(IsReady());
    SandboxRef().WriteFile(
        "P/pack.json",
        R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"dynamic","binary":"VasePlugins"},"plugins":[{"id":"Vase.Hello"},{"id":"Vase.Hello"}]})");
    const auto refused = vase::AdoptInto(CatalogRef(), HostRef(), Pod(), "Vase.Hello");
    ASSERT_FALSE(refused.IsOk());
    EXPECT_NE(refused.GetError().Message().find("duplicate"), std::string::npos) << refused.GetError().Message();
}

TEST_F(PackageAdopt, OtherMemberOfSameBinaryIsNotAnImportedSibling)
{
    ASSERT_TRUE(IsReady());
    const auto hello = vase::AdoptInto(CatalogRef(), HostRef(), Pod(), "Vase.Hello");
    ASSERT_TRUE(hello.IsOk()) << hello.GetError().Message();
    EXPECT_EQ(hello.Value().Status, vase::AdoptStatus::kAdopted);
    const auto dependent = vase::AdoptInto(CatalogRef(), HostRef(), Pod(), "Vase.Dependent");
    ASSERT_TRUE(dependent.IsOk()) << dependent.GetError().Message();
    EXPECT_EQ(dependent.Value().Status, vase::AdoptStatus::kAdopted);
}

TEST_F(PackageAdopt, RefusesInProcessSource)
{
    ASSERT_TRUE(IsReady());
    const testing_support::CatalogSandbox sandbox{"package-adopt-static"};
    sandbox.WriteFile(
        "P/pack.json",
        R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"inProcess"},"plugins":[{"id":"Probe.A","enabledByDefault":false}]})");
    vase::PluginCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    const auto adopted = vase::AdoptInto(catalog, HostRef(), Pod(), "Probe.A");
    ASSERT_FALSE(adopted.IsOk());
    EXPECT_NE(adopted.GetError().Message().find("in-process source"), std::string::npos)
        << adopted.GetError().Message();
}

} // namespace
