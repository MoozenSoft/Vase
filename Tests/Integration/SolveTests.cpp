// spec §6 的逐格判据（本段 = 前置/验证/参与/配置/路径；依赖图用例随 T6 增补）。
// Notes 一律按 Kind/Cause 字段断言（D62），不匹配消息子串。

#include "Vase/Catalog/PluginCatalog.h"

#include "Vase/Catalog/LoadRequest.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/Preset.h"
#include "Vase/Config/Value.h"
#include "Vase/Host/ConfigBlob.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/PluginDescriptor.h"

#include "CatalogSandbox.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace
{

using testing_support::CatalogSandbox;
using testing_support::RefreshOrFail;
using testing_support::StageManifests;
using vase::LoadPlan;
using vase::LoadPlanEntry;
using vase::LoadRequest;
using vase::PluginCatalog;
using vase::SolveNoteKind;
using vase::SolveOutcome;

// bugprone-unchecked-optional-access 不认 ASSERT/EXPECT 级 has_value 断言（同 ConfigBlobTests 的裁定）。
template <typename T>
T Unwrap(std::optional<T> found)
{
    if (!found.has_value())
    {
        ADD_FAILURE() << "Find miss";
        return T{};
    }
    return *found;
}

std::string ManifestWith(std::string_view id, std::string_view extraFields)
{
    std::string out = R"({"schemaVersion":1,"id":")" + std::string(id) + "\"";
    if (!extraFields.empty())
    {
        out += ',';
        out += extraFields;
    }
    out += '}';
    return out;
}

// 计划的可逐字节比较形态（D53 确定性判据）。沙箱路径皆 ASCII，编码选择不影响等值比对
// （generic_u8string 在 C++20 返 std::u8string，接不进 std::string）。
std::string PlanText(const LoadPlan& plan)
{
    std::string text;
    for (const LoadPlanEntry& entry : plan.Ordered)
    {
        text += std::string(entry.Id) + "|" + (entry.Decision == vase::LoadDecision::kLoad ? "load" : "skip") + "|" +
                std::to_string(static_cast<int>(entry.Reason)) + "|" + entry.BinaryPath.generic_string() + "|";
        for (const auto& kv : entry.ResolvedConfig.Entries())
        {
            text += kv.Key + "=";
            std::visit(
                [&text](const auto& v)
                {
                    using T = std::remove_cvref_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, std::string>)
                    {
                        text += v;
                    }
                    else
                    {
                        text += std::to_string(v);
                    }
                },
                kv.Stored);
            text += ';';
        }
        text += '\n';
    }
    return text;
}

std::filesystem::path ExpectedBinaryPath(const CatalogSandbox& sandbox, const std::string& dir, const std::string& stem)
{
    std::filesystem::path expected = sandbox.Root / dir;
#ifdef _WIN32
    expected /= stem + ".dll";
#else
    expected /= "lib" + stem + ".so";
#endif
    return expected;
}

SolveOutcome SolveOrDie(PluginCatalog& catalog, const LoadRequest& request)
{
    auto solved = catalog.Solve(request);
    EXPECT_TRUE(solved.IsOk()) << solved.GetError().Message();
    return solved.IsOk() ? std::move(solved.Value()) : SolveOutcome{};
}

vase::Preset LoadPresetOrDie(const CatalogSandbox& sandbox, const std::string& name, std::string_view text)
{
    sandbox.WriteFile(name, text);
    auto loaded = vase::LoadPreset(sandbox.Root / name);
    EXPECT_TRUE(loaded.IsOk()) << loaded.GetError().Message();
    return loaded.IsOk() ? std::move(loaded.Value()) : vase::Preset{};
}

class SolveA : public ::testing::Test
{
public:
    CatalogSandbox Sandbox{"solve-a"};
    PluginCatalog Catalog;

    void Stage(const std::vector<std::pair<std::string, std::string>>& dirToJson)
    {
        StageManifests(Sandbox, dirToJson);
        RefreshOrFail(Catalog, Sandbox);
    }
};

TEST_F(SolveA, BeforeRefreshRejects) // D63
{
    LoadRequest request;
    const auto solved = Catalog.Solve(request);
    ASSERT_FALSE(solved.IsOk());
    EXPECT_NE(solved.GetError().Message().find("Refresh"), std::string::npos); // 最小区分子串
}

TEST_F(SolveA, SinglePluginPlanShape)
{
    Stage({
        {"solo", ManifestWith("Vase.Solo", R"("config":[{"key":"n","type":"int32","default":5}])")},
    });
    LoadRequest request;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    ASSERT_EQ(outcome.Plan.Ordered.size(), 1U);
    const LoadPlanEntry& entry = *outcome.Plan.Ordered.begin();
    EXPECT_EQ(entry.Id, "Vase.Solo");
    EXPECT_EQ(entry.Decision, vase::LoadDecision::kLoad);
    EXPECT_EQ(entry.BinaryPath, ExpectedBinaryPath(Sandbox, "solo", "solo")); // binary 缺省=子目录名（D54）
    const auto n = Unwrap(entry.ResolvedConfig.Find("n")); // ⑥ 清单默认值已并入计划（D23 清单侧兑现）
    EXPECT_EQ(n.GetAs<std::int32_t>(), 5);
    EXPECT_TRUE(outcome.Notes.empty());
}

TEST_F(SolveA, EnabledThreeLayers)
{
    Stage({
        {"a", ManifestWith("Vase.A", R"("enabledByDefault":false)")},
        {"b", ManifestWith("Vase.B", "")},
        {"c", ManifestWith("Vase.C", "")},
    });
    // 清单 false → preset true（A 参与）；preset 未提 B（默认 true）；override false 压过 B/C 之上的层。
    vase::Preset preset =
        LoadPresetOrDie(Sandbox, "Client.preset.json",
                        R"({"schemaVersion":1,"overrides":{"Vase.A":{"enabled":true},"Vase.B":{"enabled":true}}})");
    const std::vector<vase::PluginOverride> overrides{
        vase::PluginOverride{.Id = "Vase.B", .Enabled = false, .Config = {}},
    };
    LoadRequest request;
    request.Preset = std::move(preset);
    request.Overrides = overrides;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    ASSERT_EQ(outcome.Plan.Ordered.size(), 3U); // kLoad 段在前（Id 序），skip 段尾随
    const auto first = outcome.Plan.Ordered.begin();
    EXPECT_EQ(first->Id, "Vase.A"); // A: preset 层 true 压过清单 false → load
    EXPECT_EQ(first->Decision, vase::LoadDecision::kLoad);
    EXPECT_EQ(std::next(first, 1)->Id, "Vase.C"); // C: 三层均未提及 → enabledByDefault → load
    EXPECT_EQ(std::next(first, 1)->Decision, vase::LoadDecision::kLoad);
    EXPECT_EQ(std::next(first, 2)->Id, "Vase.B"); // B: override 层压过 preset 层 → skip
    EXPECT_EQ(std::next(first, 2)->Decision, vase::LoadDecision::kSkip);
    EXPECT_EQ(std::next(first, 2)->Reason, vase::SkipReason::kDisabled);
}

TEST_F(SolveA, NoteKindsAreFields)
{
    Stage({
        {"a", ManifestWith("Vase.A", R"("config":[{"key":"known","type":"int32","default":1}])")},
    });
    vase::Preset preset = LoadPresetOrDie(
        Sandbox, "Client.preset.json",
        R"({"schemaVersion":1,"overrides":{"Vase.Ghost":{"enabled":true},"Vase.A":{"config":{"nope":1}}}})");
    LoadRequest request;
    request.Preset = std::move(preset);
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    ASSERT_EQ(outcome.Notes.size(), 2U); // 层内按 Entries() 的 Id 字典序：A 在 Ghost 前
    const auto note = outcome.Notes.begin();
    EXPECT_EQ(note->Kind, SolveNoteKind::kUnknownConfigKey);
    EXPECT_EQ(note->Key, "nope");
    EXPECT_EQ(std::next(note, 1)->Kind, SolveNoteKind::kUnknownPluginId);
    EXPECT_EQ(std::next(note, 1)->PluginId, "Vase.Ghost");
}

TEST_F(SolveA, TypeAndRangeErrors)
{
    Stage({
        {"a", ManifestWith("Vase.A", R"("config":[{"key":"n","type":"int32","default":5,"min":1,"max":9}])")},
    });
    {
        vase::Preset preset = LoadPresetOrDie(Sandbox, "bad-type.json",
                                              R"({"schemaVersion":1,"overrides":{"Vase.A":{"config":{"n":"five"}}}})");
        LoadRequest request;
        request.Preset = std::move(preset);
        EXPECT_FALSE(Catalog.Solve(request).IsOk()); // 类型不符 = error（D51）
    }
    {
        vase::Preset preset = LoadPresetOrDie(Sandbox, "bad-range.json",
                                              R"({"schemaVersion":1,"overrides":{"Vase.A":{"config":{"n":42}}}})");
        LoadRequest request;
        request.Preset = std::move(preset);
        const auto solved = Catalog.Solve(request);
        ASSERT_FALSE(solved.IsOk()); // 越界 = error（D35 执法点）
        EXPECT_NE(solved.GetError().Message().find("out of manifest range"), std::string::npos);
    }
}

TEST_F(SolveA, WidenInt64ToFloatField)
{
    Stage({
        {"a", ManifestWith("Vase.A", R"("config":[{"key":"f","type":"float","default":2.0,"min":1.0,"max":10.0}])")},
    });
    vase::Preset preset = LoadPresetOrDie(Sandbox, "Client.preset.json",
                                          R"({"schemaVersion":1,"overrides":{"Vase.A":{"config":{"f":7}}}})");
    LoadRequest request;
    request.Preset = std::move(preset);
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    ASSERT_EQ(outcome.Plan.Ordered.size(), 1U);
    const auto f = Unwrap(outcome.Plan.Ordered.begin()->ResolvedConfig.Find("f")); // int→float 加宽（D51）
    EXPECT_EQ(f.Kind, vase::ValueKind::kFloat);
    EXPECT_FLOAT_EQ(f.GetAs<float>(), 7.0F);
}

TEST_F(SolveA, DuplicateOverrideIdRejects) // D64
{
    Stage({
        {"a", ManifestWith("Vase.A", "")},
    });
    const std::vector<vase::PluginOverride> overrides{
        vase::PluginOverride{.Id = "Vase.A", .Enabled = true, .Config = {}},
        vase::PluginOverride{.Id = "Vase.A", .Enabled = false, .Config = {}},
    };
    LoadRequest request;
    request.Overrides = overrides;
    EXPECT_FALSE(Catalog.Solve(request).IsOk());
}

TEST_F(SolveA, AllDisabledIsPlanNotError)
{
    Stage({
        {"a", ManifestWith("Vase.A", R"("enabledByDefault":false)")},
    });
    LoadRequest request;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    ASSERT_EQ(outcome.Plan.Ordered.size(), 1U);
    EXPECT_EQ(outcome.Plan.Ordered.begin()->Decision, vase::LoadDecision::kSkip);
    EXPECT_TRUE(outcome.Plan.Ordered.begin()->ResolvedConfig.Entries().empty()); // skip 空 blob（D53）
}

TEST_F(SolveA, OverrideConfigLayerApplies) // §4.2 三层合并的 override 层真执行（此前只证到 preset 层）
{
    Stage({
        {"a", ManifestWith("Vase.A", R"("config":[{"key":"n","type":"int32","default":1}])")},
    });
    constexpr const char* kPresetJson = R"({"schemaVersion":1,"overrides":{"Vase.A":{"config":{"n":5}}}})";

    // 仅 preset 层：清单默认 1 被 5 压过。
    vase::Preset preset1 = LoadPresetOrDie(Sandbox, "layer.json", kPresetJson);
    LoadRequest presetOnly;
    presetOnly.Preset = std::move(preset1);
    const SolveOutcome o1 = SolveOrDie(Catalog, presetOnly);
    ASSERT_EQ(o1.Plan.Ordered.size(), 1U);
    EXPECT_EQ(Unwrap(o1.Plan.Ordered.begin()->ResolvedConfig.Find("n")).GetAs<std::int32_t>(), 5);

    // 会话 override 层再压 5 成 9——「后层整体替换该键」的可见执行。
    vase::Preset preset2 = LoadPresetOrDie(Sandbox, "layer.json", kPresetJson);
    vase::ConfigBlob cfg;
    cfg.Set("n", vase::Value::From<std::int64_t>(9)); // D59 原样形：整数入 int64，Solve 窄化到声明型
    const std::vector<vase::PluginOverride> overrides{
        vase::PluginOverride{.Id = "Vase.A", .Enabled = std::nullopt, .Config = std::move(cfg)},
    };
    LoadRequest both;
    both.Preset = std::move(preset2);
    both.Overrides = overrides;
    const SolveOutcome o2 = SolveOrDie(Catalog, both);
    ASSERT_EQ(o2.Plan.Ordered.size(), 1U);
    EXPECT_EQ(Unwrap(o2.Plan.Ordered.begin()->ResolvedConfig.Find("n")).GetAs<std::int32_t>(), 9);
}

TEST_F(SolveA, SkippedPluginTypeErrorStillFires) // D66：验证跑快照全量，与本局参与与否无关
{
    Stage({
        {"a", ManifestWith("Vase.A", R"("enabledByDefault":false,"config":[{"key":"n","type":"int32","default":1}])")},
    });
    // A 整体走 kSkip，但它的 preset 型不符仍是整个 Solve 的 Err，不是 note——D66 的反面格。
    vase::Preset preset = LoadPresetOrDie(Sandbox, "bad-skipped.json",
                                          R"({"schemaVersion":1,"overrides":{"Vase.A":{"config":{"n":"five"}}}})");
    LoadRequest request;
    request.Preset = std::move(preset);
    EXPECT_FALSE(Catalog.Solve(request).IsOk());
}

TEST_F(SolveA, EmptySnapshotSolvesToEmptyPlan) // §6⑧ 空快照半格
{
    // 空目录扫描成功（HasScanned 置位）→ Solve Ok、计划与 Notes 皆空——与 BeforeRefreshRejects 的「未扫即 Err」对偶。
    Sandbox.CreateDir("empty-dir");
    const auto refreshed = Catalog.Refresh(Sandbox.Root / "empty-dir");
    ASSERT_TRUE(refreshed.IsOk()) << refreshed.GetError().Message();
    LoadRequest request;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    EXPECT_TRUE(outcome.Plan.Ordered.empty());
    EXPECT_TRUE(outcome.Notes.empty());
}

TEST_F(SolveA, DoubleRunIsByteIdentical) // D53
{
    Stage({
        {"b", ManifestWith("Vase.B", "")},
        {"a", ManifestWith("Vase.A", "")},
    });
    LoadRequest request;
    const std::string first = PlanText(SolveOrDie(Catalog, request).Plan);
    const std::string second = PlanText(SolveOrDie(Catalog, request).Plan);
    EXPECT_EQ(first, second);
    EXPECT_EQ(first.find("Vase.A|load"), 0U); // 本段无依赖图：kLoad 段 = Id 序（T6 后此判据移交拓扑用例）
}

class SolveB : public ::testing::Test
{
public:
    CatalogSandbox Sandbox{"solve-b"};
    PluginCatalog Catalog;

    void Stage(const std::vector<std::pair<std::string, std::string>>& dirToJson)
    {
        StageManifests(Sandbox, dirToJson);
        RefreshOrFail(Catalog, Sandbox);
    }

    static std::vector<std::string> LoadIds(const LoadPlan& plan)
    {
        std::vector<std::string> ids;
        for (const LoadPlanEntry& entry : plan.Ordered)
        {
            if (entry.Decision == vase::LoadDecision::kLoad)
            {
                ids.emplace_back(entry.Id);
            }
        }
        return ids;
    }

    static std::vector<std::string> SkipIds(const LoadPlan& plan)
    {
        std::vector<std::string> ids;
        for (const LoadPlanEntry& entry : plan.Ordered)
        {
            if (entry.Decision == vase::LoadDecision::kSkip)
            {
                ids.emplace_back(entry.Id);
            }
        }
        return ids;
    }
};

constexpr const char* kProvS = R"("provides":[{"service":"S","version":1}])";
constexpr const char* kReqS = R"("requires":[{"service":"S","version":1}])";
// 环用例的两对声明面：提成常量以免 Stage 元素折行（多行闭合的 braced 列表受 trailing-comma 管）。
constexpr const char* kHardCycleA =
    R"("requires":[{"service":"TB","version":1}],"provides":[{"service":"TA","version":1}])";
constexpr const char* kHardCycleB =
    R"("requires":[{"service":"TA","version":1}],"provides":[{"service":"TB","version":1}])";
constexpr const char* kOptCycleA =
    R"("optionalRequires":[{"service":"TB","version":1}],"provides":[{"service":"TA","version":1}])";
constexpr const char* kOptCycleB =
    R"("optionalRequires":[{"service":"TA","version":1}],"provides":[{"service":"TB","version":1}])";

TEST_F(SolveB, TopoRespectsHardEdges)
{
    Stage({
        {"c", ManifestWith("Vase.C", kReqS)},
        {"p", ManifestWith("Vase.P", kProvS)},
    });
    LoadRequest request;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    // 层0 = {P}（C 有入边），弹出 P 后 C 就绪 → 拓扑序 [P, C]，**与 Id 序相反**——这条才是拓扑判据。
    EXPECT_EQ(LoadIds(outcome.Plan), (std::vector<std::string>{"Vase.P", "Vase.C"}));
    EXPECT_TRUE(SkipIds(outcome.Plan).empty());
}

TEST_F(SolveB, SameLayerIdLex)
{
    Stage({
        {"z", ManifestWith("Vase.Z", kReqS)},
        {"m", ManifestWith("Vase.M", "")},
        {"a", ManifestWith("Vase.A", "")},
        {"p", ManifestWith("Vase.P", kProvS)},
    });
    LoadRequest request;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    EXPECT_EQ(LoadIds(outcome.Plan),
              (std::vector<std::string>{"Vase.A", "Vase.M", "Vase.P", "Vase.Z"})); // 同层 Id 字典序（D53）
}

TEST_F(SolveB, CascadeSkipNamesProvider) // D62：B 因 A 被禁而跳，C 因 B 被跳而跳
{
    Stage({
        {"a", ManifestWith("Vase.A", std::string(kProvS) + R"(,"enabledByDefault":false)")},
        {"b", ManifestWith("Vase.B", std::string(kReqS) + R"(,"provides":[{"service":"T","version":1}])")},
        {"c", ManifestWith("Vase.C", R"("requires":[{"service":"T","version":1}])")},
    });
    LoadRequest request;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    EXPECT_TRUE(LoadIds(outcome.Plan).empty());
    const auto skipIds = SkipIds(outcome.Plan);
    ASSERT_EQ(skipIds.size(), 3U); // A(kDisabled)+B+C 全部示众，尾部 Id 序
    EXPECT_EQ(skipIds, (std::vector<std::string>{"Vase.A", "Vase.B", "Vase.C"}));
    ASSERT_EQ(outcome.Notes.size(), 2U);
    const auto note = outcome.Notes.begin();
    EXPECT_EQ(note->Kind, SolveNoteKind::kProviderSkipped);
    EXPECT_EQ(note->PluginId, "Vase.B");
    EXPECT_EQ(note->Cause, "Vase.A");
    EXPECT_EQ(std::next(note, 1)->Kind, SolveNoteKind::kProviderSkipped);
    EXPECT_EQ(std::next(note, 1)->PluginId, "Vase.C");
    EXPECT_EQ(std::next(note, 1)->Cause, "Vase.B");
}

TEST_F(SolveB, VersionMismatchSkipsWithNamedCause) // §6②
{
    Stage({
        {"p", ManifestWith("Vase.P", kProvS)},
        {"c", ManifestWith("Vase.C", R"("requires":[{"service":"S","version":2}])")},
    });
    LoadRequest request;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    EXPECT_EQ(LoadIds(outcome.Plan), (std::vector<std::string>{"Vase.P"}));
    ASSERT_EQ(outcome.Plan.Ordered.size(), 2U); // P(kLoad) + C(kSkip)，先钉长度再取尾部
    ASSERT_EQ(outcome.Plan.Ordered.back().Reason, vase::SkipReason::kVersionMismatch);
    ASSERT_EQ(outcome.Notes.size(), 1U);
    EXPECT_EQ(outcome.Notes.begin()->Kind, SolveNoteKind::kVersionMismatchProvider);
    EXPECT_EQ(outcome.Notes.begin()->Cause, "Vase.P");
}

TEST_F(SolveB, AbsentProviderSkipsWithoutNote) // §6② 沉默线
{
    Stage({
        {"c", ManifestWith("Vase.C", R"("requires":[{"service":"Nowhere","version":1}])")},
    });
    LoadRequest request;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    ASSERT_EQ(outcome.Plan.Ordered.size(), 1U);
    EXPECT_EQ(outcome.Plan.Ordered.begin()->Reason, vase::SkipReason::kMissingDependency);
    EXPECT_TRUE(outcome.Notes.empty());
}

TEST_F(SolveB, HostProvidedSatisfiesRequires) // D60
{
    Stage({
        {"c", ManifestWith("Vase.C", R"("requires":[{"service":"HostThing","version":1}])")},
    });
    const std::array<vase::ServiceRef, 1> host{{vase::ServiceRef{.Name = "HostThing", .Version = 1}}};
    LoadRequest request;
    request.HostProvided = host;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    EXPECT_EQ(LoadIds(outcome.Plan), (std::vector<std::string>{"Vase.C"}));
}

TEST_F(SolveB, HostProvidedCollisionRejects) // D60
{
    Stage({
        {"p", ManifestWith("Vase.P", R"("provides":[{"service":"HostThing","version":1}])")},
    });
    const std::array<vase::ServiceRef, 1> host{{vase::ServiceRef{.Name = "HostThing", .Version = 1}}};
    LoadRequest request;
    request.HostProvided = host;
    const auto solved = Catalog.Solve(request);
    ASSERT_FALSE(solved.IsOk());
    EXPECT_NE(solved.GetError().Message().find("collides with HostProvided"), std::string::npos);
}

TEST_F(SolveB, PluginCollisionRejects) // §6③
{
    Stage({
        {"a", ManifestWith("Vase.A", kProvS)},
        {"b", ManifestWith("Vase.B", kProvS)},
    });
    LoadRequest request;
    const auto solved = Catalog.Solve(request);
    ASSERT_FALSE(solved.IsOk());
    EXPECT_NE(solved.GetError().Message().find("provided by both"), std::string::npos);
}

TEST_F(SolveB, CycleRejectsNamingMembers) // §6④
{
    Stage({
        {"a", ManifestWith("Vase.A", kHardCycleA)},
        {"b", ManifestWith("Vase.B", kHardCycleB)},
    });
    LoadRequest request;
    const auto solved = Catalog.Solve(request);
    ASSERT_FALSE(solved.IsOk());
    EXPECT_NE(solved.GetError().Message().find("Vase.A"), std::string::npos);
    EXPECT_NE(solved.GetError().Message().find("Vase.B"), std::string::npos);
}

TEST_F(SolveB, OptionalCycleAlsoRejects) // D52
{
    Stage({
        {"a", ManifestWith("Vase.A", kOptCycleA)},
        {"b", ManifestWith("Vase.B", kOptCycleB)},
    });
    LoadRequest request;
    EXPECT_FALSE(Catalog.Solve(request).IsOk());
}

TEST_F(SolveB, OptionalEdgeOrdersWhenPresent) // D52：在场则先行
{
    Stage({
        {"a", ManifestWith("Vase.A", R"("optionalRequires":[{"service":"S","version":1}])")},
        {"z", ManifestWith("Vase.Z", kProvS)},
    });
    LoadRequest request;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    EXPECT_EQ(LoadIds(outcome.Plan),
              (std::vector<std::string>{"Vase.Z", "Vase.A"})); // 非 Id 序 → 是 optional 边在起作用
}

TEST_F(SolveB, OptionalAbsentDoesNotSkip) // §6②：optional 缺失不跳（D52）
{
    Stage({
        {"a", ManifestWith("Vase.A", R"("optionalRequires":[{"service":"Ghost","version":1}])")},
    });
    LoadRequest request;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    EXPECT_EQ(LoadIds(outcome.Plan), (std::vector<std::string>{"Vase.A"}));
    EXPECT_TRUE(outcome.Notes.empty());
}

TEST_F(SolveB, DoubleRunIsByteIdenticalWithGraph) // D53（带图版）
{
    Stage({
        {"c", ManifestWith("Vase.C", std::string(kReqS) + R"(,"provides":[{"service":"T2","version":1}])")},
        {"d", ManifestWith("Vase.D", R"("requires":[{"service":"T2","version":1}])")},
        {"p", ManifestWith("Vase.P", kProvS)},
    });
    LoadRequest request;
    const std::string first = PlanText(SolveOrDie(Catalog, request).Plan);
    const std::string second = PlanText(SolveOrDie(Catalog, request).Plan);
    EXPECT_EQ(first, second);
    EXPECT_EQ(first.find("Vase.P|"), 0U); // 拓扑序在文本判据里同样成立：P 第一
}

} // namespace
