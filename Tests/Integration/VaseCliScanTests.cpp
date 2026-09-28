// scan 的目录发现与跳过口径（M5/D133）：spec §2.3 那张表逐行一条用例。
// 沙箱复用 CatalogSandbox（temp 下建树、析构清理），二进制从 VASE_FIXTURE_* 取。
#include "Scan.h"

#include "Cli.h"
#include "Validate.h"
#include "Vase/Catalog/LibraryFileName.h"
#include "Vase/Catalog/LoadRequest.h"
#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/PluginHost.h"

#include "CatalogSandbox.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>

namespace
{

using testing_support::CatalogSandbox;

// 往沙箱里放一个插件子目录：库文件的**名字**按本平台规则拼（这正是 LibraryFileName 的用处）。
void StagePlugin(const CatalogSandbox& sandbox, const std::string& subdirectory, const std::string& stem,
                 const char* fixturePath)
{
    sandbox.CopyFile(subdirectory + "/" + vase::catalog_detail::LibraryFileName(stem), fixturePath);
}

TEST(VaseCliScan, FindsOneLibraryPerSubdirectory)
{
    const CatalogSandbox sandbox("scan-find");
    StagePlugin(sandbox, "Probe", "LoadProbe", VASE_FIXTURE_LOADPROBE);

    const vase::Result<tools::cli::Discovery> found = tools::cli::DiscoverPlugins(sandbox.Root);
    ASSERT_TRUE(found.IsOk()) << found.GetError().Message();
    ASSERT_EQ(found.Value().Found.size(), 1U);
    EXPECT_EQ(found.Value().Found.front().Stem, "LoadProbe");
    EXPECT_EQ(found.Value().Found.front().Directory.filename().string(), "Probe");
    EXPECT_TRUE(found.Value().Skipped.empty());
    EXPECT_TRUE(found.Value().Failed.empty());
}

TEST(VaseCliScan, SkipsSubdirectoriesThatAreNeitherLibraryNorManifest)
{
    const CatalogSandbox sandbox("scan-skip");
    StagePlugin(sandbox, "Probe", "LoadProbe", VASE_FIXTURE_LOADPROBE);
    sandbox.CreateDir("docs"); // 既无库也无清单的时间目录：跳过，不是错误
    sandbox.CreateDir("shaders");

    const vase::Result<tools::cli::Discovery> found = tools::cli::DiscoverPlugins(sandbox.Root);
    ASSERT_TRUE(found.IsOk()) << found.GetError().Message();
    EXPECT_EQ(found.Value().Found.size(), 1U);
    EXPECT_EQ(found.Value().Skipped.size(), 2U);
    EXPECT_TRUE(found.Value().Failed.empty());
}

TEST(VaseCliScan, TwoLibrariesInOneSubdirectoryIsLoud)
{
    const CatalogSandbox sandbox("scan-twolibs");
    StagePlugin(sandbox, "Probe", "LoadProbe", VASE_FIXTURE_LOADPROBE);
    StagePlugin(sandbox, "Probe", "UnloadProbe", VASE_FIXTURE_UNLOADPROBE);

    const vase::Result<tools::cli::Discovery> found = tools::cli::DiscoverPlugins(sandbox.Root);
    ASSERT_TRUE(found.IsOk()) << found.GetError().Message();
    EXPECT_TRUE(found.Value().Found.empty());
    ASSERT_EQ(found.Value().Failed.size(), 1U);
    EXPECT_NE(found.Value().Failed.front().find("Probe"), std::string::npos);
}

TEST(VaseCliScan, ManifestWithoutLibraryIsLoud)
{
    const CatalogSandbox sandbox("scan-manifestonly");
    sandbox.WriteFile("Ghost/plugin.json", R"({"schemaVersion":1,"id":"Vase.Ghost"})");

    const vase::Result<tools::cli::Discovery> found = tools::cli::DiscoverPlugins(sandbox.Root);
    ASSERT_TRUE(found.IsOk()) << found.GetError().Message();
    EXPECT_TRUE(found.Value().Found.empty());
    EXPECT_EQ(found.Value().Failed.size(), 1U);
}

TEST(VaseCliScan, MissingDirectoryIsAnErrorNotAnEmptySuccess)
{
    const CatalogSandbox sandbox("scan-nodir");
    const vase::Result<tools::cli::Discovery> found = tools::cli::DiscoverPlugins(sandbox.Root / "nope");
    EXPECT_FALSE(found.IsOk());
}

TEST(VaseCliScan, PreservesEnabledByDefaultAndReportsBinaryDrift)
{
    const CatalogSandbox sandbox("scan-preserve");
    StagePlugin(sandbox, "Probe", "LoadProbe", VASE_FIXTURE_LOADPROBE);
    // 旧清单：人手关了默认启用，且 binary 写的是个过时的名字。
    sandbox.WriteFile("Probe/plugin.json", R"({"schemaVersion":1,"id":"Vase.LoadProbe",
        "displayName":"装载探针","version":"0.0.1","binary":"StaleName","enabledByDefault":false})");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunScan(sandbox.Root.string(), out, err), tools::cli::kExitOk) << err.str();

    const auto written = vase::ParseManifestFile(sandbox.Root / "Probe" / "plugin.json", "Probe");
    ASSERT_TRUE(written.IsOk()) << written.GetError().Message();
    EXPECT_FALSE(written.Value().EnabledByDefault);                // 保真（D122）
    EXPECT_EQ(written.Value().Binary, "LoadProbe");                // 改成观察事实（D122）
    EXPECT_TRUE(out.str().find("StaleName") != std::string::npos); // 改动被打印，不静默
}

TEST(VaseCliScan, UnreadableOldManifestIsRegeneratedWithLoudWarning)
{
    // 坏清单（D49 unknown 键）读不出：以二进制为准重写，但归因必须出声（D134 失败逐条报）。
    const CatalogSandbox sandbox("scan-corrupt");
    StagePlugin(sandbox, "Probe", "LoadProbe", VASE_FIXTURE_LOADPROBE);
    sandbox.WriteFile("Probe/plugin.json", R"({"schemaVersion":1,"id":"Vase.LoadProbe","bogus":true})");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunScan(sandbox.Root.string(), out, err), tools::cli::kExitOk) << err.str();

    const auto written = vase::ParseManifestFile(sandbox.Root / "Probe" / "plugin.json", "Probe");
    ASSERT_TRUE(written.IsOk()) << written.GetError().Message();
    EXPECT_EQ(written.Value().Binary, "LoadProbe");
    EXPECT_TRUE(written.Value().EnabledByDefault); // 旧值随不可读丢失，但丢失本身可见
    EXPECT_NE(out.str().find("unreadable"), std::string::npos);
}

TEST(VaseCliScan, EmptyTreeIsNotASilentSuccess)
{
    const CatalogSandbox sandbox("scan-empty");
    sandbox.CreateDir("docs");
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunScan(sandbox.Root.string(), out, err), tools::cli::kExitUsage);
}

TEST(VaseCliScan, GeneratedManifestIsAcceptedByValidateAndByTheLoader)
{
    // 三处一致的证人（spec §4）：scan 生成的清单 → validate 四项全过 → 加载期也接受。
    // 三处走的是同一份 CompareDescriptor，所以这条同时证 D117 的连带收益。
    const CatalogSandbox sandbox("scan-e2e");
    StagePlugin(sandbox, "Hello", "HelloPlugin", VASE_FIXTURE_HELLO);

    std::ostringstream scanOut;
    std::ostringstream scanErr;
    ASSERT_EQ(tools::cli::RunScan(sandbox.Root.string(), scanOut, scanErr), tools::cli::kExitOk) << scanErr.str();

    std::ostringstream validateOut;
    std::ostringstream validateErr;
    ASSERT_EQ(tools::cli::RunValidate({"validate", sandbox.Root.string()}, validateOut, validateErr),
              tools::cli::kExitOk)
        << validateErr.str();

    // 加载期：按生成的清单建快照 → 求解 → 装局。
    vase::PluginCatalog catalog;
    const auto refreshed = catalog.Refresh(sandbox.Root);
    ASSERT_TRUE(refreshed.IsOk()) << refreshed.GetError().Message();
    const auto solved = catalog.Solve(vase::LoadRequest{});
    ASSERT_TRUE(solved.IsOk()) << solved.GetError().Message();
    vase::PluginHost host;
    const auto pod = host.CreatePod(solved.Value().Plan);
    ASSERT_TRUE(pod.IsOk()) << pod.GetError().Message();
    EXPECT_TRUE(host.DestroyPod(pod.Value()).Clean());
}

} // namespace
