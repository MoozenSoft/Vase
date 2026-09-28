// validate 的四项（M5/D120/D130/D127/D132）：快照不建成 = 第二项；逐插件比对 = 第一项；
// 依赖图 = 第三项；服务命名前缀与宿主越界 = 第四项。
#include "Validate.h"

#include "Cli.h"
#include "Scan.h"
#include "Vase/Catalog/LibraryFileName.h"

#include "CatalogSandbox.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

namespace
{

using testing_support::CatalogSandbox;

void StagePlugin(const CatalogSandbox& sandbox, const std::string& subdirectory, const std::string& stem,
                 const char* fixturePath)
{
    sandbox.CopyFile(subdirectory + "/" + vase::catalog_detail::LibraryFileName(stem), fixturePath);
}

// 与 LoadProbe.cpp 的描述符逐字对齐的清单（scan 会生成同样的东西，这里手写以隔离变量）。
// **`binary` 必须写**：缺它时解析器按子目录名（D54 缺省）拼路径，StagePlugin 放的是
// LibraryFileName("LoadProbe")，两者不一致 → 会红在「装载失败」而不是想验的那一项。
constexpr const char* kLoadProbeManifest = R"({
    "schemaVersion": 1,
    "id": "Vase.LoadProbe",
    "displayName": "装载探针",
    "version": "0.0.1",
    "binary": "LoadProbe"
})";

TEST(VaseCliValidate, AgreeingManifestAndBinaryPass)
{
    const CatalogSandbox sandbox("validate-ok");
    StagePlugin(sandbox, "Probe", "LoadProbe", VASE_FIXTURE_LOADPROBE);
    sandbox.WriteFile("Probe/plugin.json", kLoadProbeManifest);

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunValidate({"validate", sandbox.Root.string()}, out, err), tools::cli::kExitOk) << err.str();
}

TEST(VaseCliValidate, DriftedDisplayNameIsCaughtByComparison)
{
    const CatalogSandbox sandbox("validate-drift");
    StagePlugin(sandbox, "Probe", "LoadProbe", VASE_FIXTURE_LOADPROBE);
    // 只改 displayName 一个字段：单变量，命中的必须是第①项而不是别的。
    sandbox.WriteFile("Probe/plugin.json", R"({
        "schemaVersion": 1, "id": "Vase.LoadProbe", "displayName": "改过的名字", "version": "0.0.1",
        "binary": "LoadProbe"})");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunValidate({"validate", sandbox.Root.string()}, out, err), tools::cli::kExitCheckFailed);
    EXPECT_NE(out.str().find("displayName"), std::string::npos); // 比对器已给出字段路径
}

TEST(VaseCliValidate, DuplicateIdIsTheSecondCheckAndSaysTheSnapshotWasNotBuilt)
{
    const CatalogSandbox sandbox("validate-dup");
    StagePlugin(sandbox, "One", "LoadProbe", VASE_FIXTURE_LOADPROBE);
    StagePlugin(sandbox, "Two", "UnloadProbe", VASE_FIXTURE_UNLOADPROBE);
    // 两份清单声明同一个 Id → Refresh 硬报错（D50）。
    sandbox.WriteFile("One/plugin.json", kLoadProbeManifest);
    sandbox.WriteFile("Two/plugin.json", kLoadProbeManifest);

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunValidate({"validate", sandbox.Root.string()}, out, err), tools::cli::kExitCheckFailed);
    // D130：快照没建成时不许假装其余三项通过——输出里必须说清这件事。
    EXPECT_NE(out.str().find("snapshot not built"), std::string::npos);
}

TEST(VaseCliValidate, MissingDirectoryIsAUsageError)
{
    const CatalogSandbox sandbox("validate-nodir");
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunValidate({"validate", (sandbox.Root / "nope").string()}, out, err),
              tools::cli::kExitUsage);
}

TEST(VaseCliValidate, EmptyTreeIsNotASilentSuccess)
{
    const CatalogSandbox sandbox("validate-empty");
    sandbox.CreateDir("docs"); // 零个插件的树：Refresh 成功、Ids() 为空（D135 第二档）

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunValidate({"validate", sandbox.Root.string()}, out, err), tools::cli::kExitUsage);
    EXPECT_NE(out.str().find("no plugins found"), std::string::npos);
}

// 第三项的语料用 DependentPlugin：它 Requires `Vase.Hello.Greeter`（Hello 不在树里即缺失），
// 且它的 provides 是 `Vase.Dependent.Farewell`——**第四项合规**（T7 改名之后），
// 所以「第三项的红」是单变量的。清单由 scan 现场生成，不手写——手写就得逐字猜描述符。
void StageDependentWithGeneratedManifest(const CatalogSandbox& sandbox)
{
    StagePlugin(sandbox, "Dependent", "DependentPlugin", VASE_FIXTURE_DEPENDENT);
    std::ostringstream scanOut;
    std::ostringstream scanErr;
    ASSERT_EQ(tools::cli::RunScan(sandbox.Root.string(), scanOut, scanErr), tools::cli::kExitOk) << scanErr.str();
}

TEST(VaseCliValidate, UnsatisfiedDependencyFailsTheThirdCheck)
{
    const CatalogSandbox sandbox("validate-missing");
    StageDependentWithGeneratedManifest(sandbox);

    std::ostringstream out;
    std::ostringstream err;
    // 不喂 --host-provides：Vase.Hello.Greeter 没人提供 → 第三项未过。
    EXPECT_EQ(tools::cli::RunValidate({"validate", sandbox.Root.string()}, out, err), tools::cli::kExitCheckFailed);
    EXPECT_NE(out.str().find("check 3"), std::string::npos);
    EXPECT_NE(out.str().find("Vase.Dependent"), std::string::npos);
}

TEST(VaseCliValidate, HostProvidedServicesSatisfyTheThirdCheck)
{
    const CatalogSandbox sandbox("validate-hostprov");
    StageDependentWithGeneratedManifest(sandbox);

    std::ostringstream out;
    std::ostringstream err;
    // 宿主声明它会注册这条服务（D60 的既有模型）：第三项不再误报，其余三项本就全过。
    const int rc = tools::cli::RunValidate(
        {"validate", sandbox.Root.string(), "--host-provides", "Vase.Hello.Greeter@1"}, out, err);
    EXPECT_EQ(rc, tools::cli::kExitOk) << out.str() << err.str();
}

TEST(VaseCliValidate, MalformedHostProvidesIsAUsageError)
{
    const CatalogSandbox sandbox("validate-badprov");
    StageDependentWithGeneratedManifest(sandbox);

    std::ostringstream out;
    std::ostringstream err;
    // 版本必须 ≥1（与 D64 的解析期闸同口径）：0 与「没有 @」都是用法错误。
    EXPECT_EQ(tools::cli::RunValidate({"validate", sandbox.Root.string(), "--host-provides", "Vase.Hello.Greeter@0"},
                                      out, err),
              tools::cli::kExitUsage);
    EXPECT_EQ(
        tools::cli::RunValidate({"validate", sandbox.Root.string(), "--host-provides", "Vase.Hello.Greeter"}, out, err),
        tools::cli::kExitUsage);
}

// SharedProviderPlugin：Id `Vase.SharedProvider`，provides 写的是 `Vase.Test.Shared`——第四项的
// 单变量反例（事实取证注⑧）。
TEST(VaseCliValidate, NamingViolationIsReportedForSharedProvider)
{
    const CatalogSandbox sandbox("validate-naming");
    StagePlugin(sandbox, "SharedProvider", "SharedProviderPlugin", VASE_FIXTURE_SHAREDPROVIDER);
    sandbox.CopyFile("SharedProvider/plugin.json",
                     std::filesystem::path{VASE_FIXTURE_MANIFESTS} / "shared_provider" / "plugin.json");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunValidate({"validate", sandbox.Root.string()}, out, err), tools::cli::kExitCheckFailed);
    EXPECT_NE(out.str().find("check 4"), std::string::npos);
    EXPECT_NE(out.str().find("Vase.Test.Shared"), std::string::npos); // 违规条点名了服务
    // 第①项也跑到了（该 fixture 的清单按 D57 逐字义务与描述符一致，故它应当 ok）——
    // 若这一行红，说明清单与描述符漂了，那是**发现**，不是本用例写错。
    EXPECT_NE(out.str().find("check 1 (manifest vs binary):\n  Vase.SharedProvider: ok"), std::string::npos);
}

TEST(VaseCliValidate, HostProvidedNameUnderAPluginIdIsCaught)
{
    const CatalogSandbox sandbox("validate-squat");
    StagePlugin(sandbox, "SharedProvider", "SharedProviderPlugin", VASE_FIXTURE_SHAREDPROVIDER);
    sandbox.CopyFile("SharedProvider/plugin.json",
                     std::filesystem::path{VASE_FIXTURE_MANIFESTS} / "shared_provider" / "plugin.json");

    std::ostringstream out;
    std::ostringstream err;
    // 宿主占用某插件 Id 之下的名字 = 越界（D132 的第二条判据）。
    const int rc = tools::cli::RunValidate(
        {"validate", sandbox.Root.string(), "--host-provides", "Vase.SharedProvider.Hijacked@1"}, out, err);
    EXPECT_EQ(rc, tools::cli::kExitCheckFailed);
    EXPECT_NE(out.str().find("Hijacked"), std::string::npos);
}

TEST(VaseCliValidate, SamplesScanThenValidateHasNoNamingViolation)
{
    // D121 的验收：改名后 Samples 的两只合规插件应当零违规。
    const CatalogSandbox sandbox("validate-samples");
    StagePlugin(sandbox, "Hello", "HelloPlugin", VASE_FIXTURE_HELLO);
    StagePlugin(sandbox, "Dependent", "DependentPlugin", VASE_FIXTURE_DEPENDENT);

    std::ostringstream scanOut;
    std::ostringstream scanErr;
    ASSERT_EQ(tools::cli::RunScan(sandbox.Root.string(), scanOut, scanErr), tools::cli::kExitOk) << scanErr.str();

    std::ostringstream out;
    std::ostringstream err;
    // 依赖链：Dependent 要求 Vase.Hello.Greeter（Hello 提供）→ 第三项应满足。
    EXPECT_EQ(tools::cli::RunValidate({"validate", sandbox.Root.string()}, out, err), tools::cli::kExitOk)
        << out.str() << err.str();
}

} // namespace
