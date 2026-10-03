// plan（M6/D141–D145/D154）：顺序与静态跳过、preset/HostProvided、三档退出码、零装载事实。
#include "Plan.h"

#include "Cli.h"

#include "CatalogSandbox.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>

namespace
{

using testing_support::CatalogSandbox;

// plan 从不碰二进制（D145）——所以这些 manifest 连 binary 键都可以不写，
// 缺它时按子目录名（D54）拼一个 plan 侧永不会解析的路径，恰好是「零装载」的反证材料。
constexpr const char* kAlpha = R"({
    "schemaVersion": 1, "id": "Vase.Alpha", "displayName": "甲", "version": "0.0.1"
})";
constexpr const char* kBetaDisabled = R"({
    "schemaVersion": 1, "id": "Vase.Beta", "displayName": "乙", "version": "0.0.1",
    "enabledByDefault": false
})";
constexpr const char* kCycA = R"({
    "schemaVersion": 1, "id": "Vase.CycA", "displayName": "环A", "version": "0.0.1",
    "requires": [ { "service": "Vase.CycB.Svc", "version": 1 } ],
    "provides": [ { "service": "Vase.CycA.Svc", "version": 1 } ]
})";
constexpr const char* kCycB = R"({
    "schemaVersion": 1, "id": "Vase.CycB", "displayName": "环B", "version": "0.0.1",
    "requires": [ { "service": "Vase.CycA.Svc", "version": 1 } ],
    "provides": [ { "service": "Vase.CycB.Svc", "version": 1 } ]
})";

int RunPlanIn(const CatalogSandbox& sandbox, std::ostringstream& out, std::ostringstream& err,
              const std::vector<std::string>& extra = {})
{
    std::vector<std::string> args{"plan", sandbox.Root.string()};
    for (const std::string& arg : extra)
    {
        args.push_back(arg);
    }
    return tools::cli::RunPlan(args, out, err);
}

TEST(VaseCliPlan, ListsLoadAndStaticSkipsInPlanOrder)
{
    const CatalogSandbox sandbox("plan-order");
    sandbox.WriteFile("Alpha/plugin.json", kAlpha);
    sandbox.WriteFile("Beta/plugin.json", kBetaDisabled);

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(RunPlanIn(sandbox, out, err), tools::cli::kExitOk) << out.str() << err.str();
    EXPECT_NE(out.str().find("Vase.Alpha load"), std::string::npos);
    EXPECT_NE(out.str().find("Vase.Beta skip disabled"), std::string::npos);
    EXPECT_NE(out.str().find("plan: ok"), std::string::npos);
}

TEST(VaseCliPlan, PresetOverrideDisablesAPlugin)
{
    const CatalogSandbox sandbox("plan-preset");
    sandbox.WriteFile("Alpha/plugin.json", kAlpha);
    sandbox.WriteFile("Beta/plugin.json", R"({
        "schemaVersion": 1, "id": "Vase.Beta", "displayName": "乙", "version": "0.0.1"
    })");
    sandbox.WriteFile("preset.json", R"({
        "schemaVersion": 1, "displayName": "预览", "overrides": { "Vase.Beta": { "enabled": false } }
    })");

    std::ostringstream out;
    std::ostringstream err;
    const std::string preset = (sandbox.Root / "preset.json").string();
    EXPECT_EQ(RunPlanIn(sandbox, out, err, {preset}), tools::cli::kExitOk) << out.str() << err.str();
    EXPECT_NE(out.str().find("Vase.Beta skip disabled"), std::string::npos);
    EXPECT_NE(out.str().find("(preset:"), std::string::npos); // 头部回显 preset 来源
}

TEST(VaseCliPlan, MissingDependencyIsAHardSkipAndFail)
{
    const CatalogSandbox sandbox("plan-missing");
    sandbox.CopyFile("Dep/plugin.json", VASE_FIXTURE_DEPENDENT_MANIFEST);

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(RunPlanIn(sandbox, out, err), tools::cli::kExitCheckFailed) << out.str();
    EXPECT_NE(out.str().find("Vase.Dependent skip missing-dependency"), std::string::npos);
    EXPECT_NE(out.str().find("plan: FAIL"), std::string::npos);
}

TEST(VaseCliPlan, HostProvidedTurnsTheHardSkipIntoLoad)
{
    const CatalogSandbox sandbox("plan-hostprov");
    sandbox.CopyFile("Dep/plugin.json", VASE_FIXTURE_DEPENDENT_MANIFEST);

    std::ostringstream out;
    std::ostringstream err;
    // 与 validate 同名证人 HostProvidedServicesSatisfyTheThirdCheck 同参——「同判」是 D154
    // 结构论证（同一个 Solve + 共用 argv），本用例只钉 plan 侧行为，不做跨命令断言。
    EXPECT_EQ(RunPlanIn(sandbox, out, err, {"--host-provides", "Vase.Hello.Greeter@1"}), tools::cli::kExitOk)
        << out.str();
    EXPECT_NE(out.str().find("Vase.Dependent load"), std::string::npos);
}

TEST(VaseCliPlan, DependencyCycleIsFailWithSolveMessageVerbatim)
{
    const CatalogSandbox sandbox("plan-cycle");
    sandbox.WriteFile("A/plugin.json", kCycA);
    sandbox.WriteFile("B/plugin.json", kCycB);

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(RunPlanIn(sandbox, out, err), tools::cli::kExitCheckFailed) << out.str();
    EXPECT_NE(out.str().find("cycle or blocked"), std::string::npos); // D52/D127：原文透传，不另造分类
}

TEST(VaseCliPlan, UnknownPresetIdIsNoteOnlyNotFailure)
{
    const CatalogSandbox sandbox("plan-note");
    sandbox.WriteFile("Alpha/plugin.json", kAlpha);
    sandbox.WriteFile("preset.json", R"({
        "schemaVersion": 1, "overrides": { "Vase.Ghost": { "enabled": true } }
    })");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(RunPlanIn(sandbox, out, err, {(sandbox.Root / "preset.json").string()}), tools::cli::kExitOk)
        << out.str();
    EXPECT_NE(out.str().find("note: unknownPluginId Vase.Ghost"), std::string::npos);
}

TEST(VaseCliPlan, ProviderSkippedNoteCarriesCauseAttribution)
{
    // Solve ②b：提供方被禁（selfDisabled）不出局归因、消费者被翻出 → kProviderSkipped，
    // Key 空、Cause = 提供方 Id（Solve.cpp:477-483）。plan 的 Cause 支缺位时这行只剩裸消费者名。
    const CatalogSandbox sandbox("plan-cause");
    sandbox.WriteFile("Alpha/plugin.json", R"({
        "schemaVersion": 1, "id": "Vase.Alpha", "displayName": "甲", "version": "0.0.1",
        "provides": [ { "service": "Vase.Alpha.Svc", "version": 1 } ]
    })");
    sandbox.WriteFile("Beta/plugin.json", R"({
        "schemaVersion": 1, "id": "Vase.Beta", "displayName": "乙", "version": "0.0.1",
        "requires": [ { "service": "Vase.Alpha.Svc", "version": 1 } ]
    })");
    sandbox.WriteFile("preset.json", R"({
        "schemaVersion": 1, "overrides": { "Vase.Alpha": { "enabled": false } }
    })");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(RunPlanIn(sandbox, out, err, {(sandbox.Root / "preset.json").string()}), tools::cli::kExitCheckFailed)
        << out.str() << err.str();
    EXPECT_NE(out.str().find("Vase.Alpha skip disabled"), std::string::npos);
    EXPECT_NE(out.str().find("Vase.Beta skip missing-dependency"), std::string::npos);
    // note 行同时点名两侧：消费者 = PluginId 槽，提供方 = Cause 槽（空 Key 时经 Cause 支打出）。
    EXPECT_NE(out.str().find("note: providerSkipped Vase.Beta Vase.Alpha"), std::string::npos) << out.str();
}

TEST(VaseCliPlan, PlanNeverTouchesBinaries)
{
    const CatalogSandbox sandbox("plan-noload");
    sandbox.WriteFile("Ghost/plugin.json", R"({
        "schemaVersion": 1, "id": "Vase.Ghost", "displayName": "无中生有", "version": "0.0.1",
        "binary": "NoSuchBinary"
    })");

    std::ostringstream out;
    std::ostringstream err;
    // binary 指向磁盘上不存在的文件而 plan 照出计划——「零装载」(D145) 的直接证人。
    EXPECT_EQ(RunPlanIn(sandbox, out, err), tools::cli::kExitOk) << out.str() << err.str();
    EXPECT_NE(out.str().find("Vase.Ghost load"), std::string::npos);
}

TEST(VaseCliPlan, GarbagePresetFileIsAUsageError)
{
    const CatalogSandbox sandbox("plan-badpreset");
    sandbox.WriteFile("Alpha/plugin.json", kAlpha);
    sandbox.WriteFile("preset.json", "not json at all");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(RunPlanIn(sandbox, out, err, {(sandbox.Root / "preset.json").string()}), tools::cli::kExitUsage)
        << err.str();
}

TEST(VaseCliPlan, MissingDirectoryIsAUsageError)
{
    const CatalogSandbox sandbox("plan-nodir");
    std::ostringstream out;
    std::ostringstream err;
    const std::vector<std::string> args{"plan", (sandbox.Root / "absent").string()};
    EXPECT_EQ(tools::cli::RunPlan(args, out, err), tools::cli::kExitUsage) << err.str();
}

TEST(VaseCliPlan, EmptyTreeIsNotASilentSuccess)
{
    const CatalogSandbox sandbox("plan-empty");
    sandbox.CreateDir("docs"); // 零个插件的树：Refresh 成功、Ids() 为空——钉的必须是这一支而非 not-a-directory

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(RunPlanIn(sandbox, out, err), tools::cli::kExitUsage) << out.str(); // D135 同一律
    EXPECT_NE(out.str().find("no plugins found in the snapshot"), std::string::npos);
}

TEST(VaseCliPlan, DuplicateIdIsSnapshotFailNotUsage)
{
    const CatalogSandbox sandbox("plan-dup");
    sandbox.WriteFile("One/plugin.json", kAlpha);
    sandbox.WriteFile("Two/plugin.json", kAlpha); // 同 Id 两份 ⇒ Refresh Err（D50）

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(RunPlanIn(sandbox, out, err), tools::cli::kExitCheckFailed) << out.str(); // D142：1 不是 2
    EXPECT_NE(out.str().find("snapshot not built"), std::string::npos);
}

} // namespace
