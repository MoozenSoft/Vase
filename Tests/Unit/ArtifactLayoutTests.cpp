// 插件产物布局不变式（spec D183/D189/D194）：每个插件独占 <产物根>/<target 名>/。
// 本波之前没有任何测试枚举构建产物根（spec 取证注⑦），这份是布局规则唯一的自动化证据。

#include <filesystem>
#include <string>

#include <gtest/gtest.h>

namespace
{

// 两个断言一起才有意义：文件真在盘上，且它的父目录名就是 target 名。
void ExpectOwnsDirectory(const char* binaryPath, const std::string& targetName)
{
    const std::filesystem::path binary{binaryPath};
    EXPECT_TRUE(std::filesystem::exists(binary)) << binary.string();
    EXPECT_EQ(binary.parent_path().filename().string(), targetName) << binary.string();
}

} // namespace

TEST(ArtifactLayout, PluginBinariesLiveInTheirOwnTargetDirectory)
{
    ExpectOwnsDirectory(VASE_FIXTURE_HELLO, "HelloPlugin");
    ExpectOwnsDirectory(VASE_FIXTURE_DEPENDENT, "DependentPlugin");
    ExpectOwnsDirectory(VASE_FIXTURE_FAILING, "FailingPlugin");
    ExpectOwnsDirectory(VASE_FIXTURE_LOADPROBE, "LoadProbe");
    ExpectOwnsDirectory(VASE_FIXTURE_NOIDENTITY, "NoIdentityPlugin");
    ExpectOwnsDirectory(VASE_FIXTURE_NEIGHBORC, "NeighborC");
}

// D194：四个同 OUTPUT_NAME 的变体维持各自的 stage 目录，不进产物根——
// 「顺手统一」会当场撞回 Ninja 的 multiple rules generate .../VersionedA.lib。
TEST(ArtifactLayout, VersionedVariantsKeepTheirStageDirectories)
{
    ExpectOwnsDirectory(VASE_FIXTURE_VERSIONEDA, "stageA");
    ExpectOwnsDirectory(VASE_FIXTURE_VERSIONEDAPRIME, "stageAPrime");
    ExpectOwnsDirectory(VASE_FIXTURE_VERSIONEDASTAMPDRIFT, "stageStampDrift");
    ExpectOwnsDirectory(VASE_FIXTURE_VERSIONEDASERVICEDRIFT, "stageServiceDrift");
}

// D181/D182：清单随插件住进产物目录，只有 Samples 三个带。
TEST(ArtifactLayout, HandWrittenManifestsAreStagedBesideTheirBinary)
{
    // 带清单的那一侧。
    const std::filesystem::path hello{VASE_FIXTURE_HELLO};
    EXPECT_TRUE(std::filesystem::exists(hello.parent_path() / "plugin.json")) << hello.parent_path().string();

    // D184：prime 与 HelloPlugin 同 Id，同一棵树里放清单会让 Refresh 整棵树报错——它必须无清单。
    const std::filesystem::path prime{VASE_FIXTURE_HELLOPRIME};
    EXPECT_FALSE(std::filesystem::exists(prime.parent_path() / "plugin.json")) << prime.parent_path().string();

    // NeighborC 是普通 fixture：不传 MANIFEST 的目标不该被凭空补上清单。
    const std::filesystem::path neighbor{VASE_FIXTURE_NEIGHBORC};
    EXPECT_FALSE(std::filesystem::exists(neighbor.parent_path() / "plugin.json")) << neighbor.parent_path().string();
}

// VasePack spec D215：组合库是唯一一条**反向**布局规则——不是插件 fixture、平铺在产物根，
// 与「每个插件独占 <产物根>/<名>/」相反。取证注⑪：Refresh 只扫子目录、本文件又逐目标断言，
// 故平铺这条此前没有任何东西看得见它（本用例补上）。
TEST(ArtifactLayout, PackedLibraryStaysFlatInTheProductRoot)
{
    const std::filesystem::path packed{VASE_FIXTURE_VASEPLUGINS};
    EXPECT_TRUE(std::filesystem::exists(packed)) << packed.string();

    const std::filesystem::path root{VASE_FIXTURE_HELLO}; // 插件在 <根>/<名>/，故父目录的父目录就是根
    EXPECT_EQ(packed.parent_path(), root.parent_path().parent_path()) << packed.string();
    // 它**不是**自己的一个目录（对照：插件必须是）。
    EXPECT_NE(packed.parent_path().filename().string(), "VasePlugins") << packed.string();
}
