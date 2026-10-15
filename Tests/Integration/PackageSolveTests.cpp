// spec §4.1/§4.2（T5）的逐格判据：一套纯数据分析算法（Analyze）与可执行来源绑定（Solve 重载）。
// 分析不装载、不执行入口、不依赖绑定；绑定规则逐条拒绝；物化只复制函数指针、只查来源表。
// 探针（testing_support::PackageDescriptorProbe）记录 Lookup/Enumerate 调用次数——0 是硬判据。
// Notes 按 Kind/Cause 字段断言（D62），不匹配消息子串。

#include "Vase/Catalog/Analysis.h"
#include "Vase/Catalog/LoadRequest.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Catalog/Preset.h"
#include "Vase/Config/Value.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PackagePlan.h"
#include "Vase/PluginDescriptor.h"

#include "CatalogSandbox.h"
#include "PackageDescriptorProbe.h"

#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using testing_support::CatalogSandbox;
using testing_support::ProbeEntries;
using testing_support::ReadPackageProbeCounts;
using testing_support::RefreshOrFail;
using testing_support::ResetPackageProbe;
using vase::AnalysisOutcome;
using vase::AnalyzedPlugin;
using vase::InProcessBinding;
using vase::LoadDecision;
using vase::LoadPlan;
using vase::LoadPlanEntry;
using vase::LoadRequest;
using vase::PackagePlanSource;
using vase::PluginCatalog;
using vase::SkipReason;
using vase::SolveNoteKind;

std::string StaticPack(const std::string& sourceId, const std::string& members)
{
    return R"({"schemaVersion":1,"sourceId":")" + sourceId + R"(","source":{"kind":"inProcess"},"plugins":[)" +
           members + "]}";
}

std::string DynamicPack(const std::string& sourceId, const std::string& binary, const std::string& members)
{
    return R"({"schemaVersion":1,"sourceId":")" + sourceId + R"(","source":{"kind":"dynamic","binary":")" + binary +
           R"("},"plugins":[)" + members + "]}";
}

std::string Member(const std::string& id, bool enabled = true)
{
    return R"({"id":")" + id + R"(","enabledByDefault":)" + (enabled ? "true" : "false") + "}";
}

std::string ProviderMember(const std::string& id, const std::string& service, bool enabled = true)
{
    return R"({"id":")" + id + R"(","provides":[{"service":")" + service + R"(","version":1}],"enabledByDefault":)" +
           (enabled ? "true" : "false") + "}";
}

std::string ConsumerMember(const std::string& id, const std::string& service)
{
    return R"({"id":")" + id + R"(","requires":[{"service":")" + service + R"(","version":1}]})";
}

std::string ConfigMember(const std::string& id)
{
    return R"({"id":")" + id + R"(","config":[{"key":"Amount","type":"int32","default":1}]})";
}

const LoadPlanEntry* FindEntry(const LoadPlan& plan, std::string_view id)
{
    for (const LoadPlanEntry& entry : plan.Ordered)
    {
        if (entry.Id == id)
        {
            return &entry;
        }
    }
    return nullptr;
}

// bugprone-unchecked-optional-access 不认 ASSERT/EXPECT 级 has_value 断言（同 SolveTests 的裁定）。
template <typename T>
T Unwrap(std::optional<T> found)
{
    if (!found.has_value())
    {
        ADD_FAILURE() << "optional empty";
        return T{};
    }
    return *found;
}

} // namespace

// ① 分析不需要任何可执行绑定；同一快照上 Solve 无绑定时响亮拒绝（brief 的给定用例）。
TEST(PackageSolve, AnalyzeDoesNotRequireExecutableBinding)
{
    CatalogSandbox sandbox{"package-solve-analyze"};
    sandbox.WriteFile("P/pack.json", StaticPack("P", Member("Probe.A")));
    PluginCatalog catalog;
    RefreshOrFail(catalog, sandbox);

    LoadRequest request;
    const auto analyzed = catalog.Analyze(request);
    ASSERT_TRUE(analyzed.IsOk()) << analyzed.GetError().Message();
    EXPECT_FALSE(catalog.Solve(request).IsOk()); // 没给静态入口 → 整局 Err
}

// ② 分析与物化都不触碰入口：探针计数为零，计划里的指针只是被复制进来的那一枚。
TEST(PackageSolve, AnalysisAndSolveNeverInvokeEntryPoints)
{
    CatalogSandbox sandbox{"package-solve-no-touch"};
    sandbox.WriteFile("P/pack.json", StaticPack("P", Member("Probe.A") + "," + Member("Probe.B")));
    PluginCatalog catalog;
    RefreshOrFail(catalog, sandbox);
    ResetPackageProbe();

    LoadRequest request;
    const auto analyzed = catalog.Analyze(request);
    ASSERT_TRUE(analyzed.IsOk()) << analyzed.GetError().Message();
    ASSERT_EQ(analyzed.Value().Ordered.size(), 2U);

    const std::array bindings{InProcessBinding{.SourceId = "P", .Entries = ProbeEntries()}};
    const auto solved = catalog.Solve(request, bindings);
    ASSERT_TRUE(solved.IsOk()) << solved.GetError().Message();

    const testing_support::PackageProbeCounts counts = ReadPackageProbeCounts();
    EXPECT_EQ(counts.Lookup, 0U); // 分析/物化都不执行查询入口
    EXPECT_EQ(counts.Enumerate, 0U);
    EXPECT_EQ(counts.Create, 0U);
    EXPECT_EQ(counts.CreateConfig, 0U);
    EXPECT_EQ(counts.OnLoad, 0U);

    ASSERT_EQ(solved.Value().Plan.Ordered.size(), 2U);
    for (const LoadPlanEntry& entry : solved.Value().Plan.Ordered)
    {
        EXPECT_EQ(entry.Decision, LoadDecision::kLoad);
        EXPECT_EQ(entry.InProcessEntry, bindings.front().Entries.Lookup); // 转交，不是调用
        EXPECT_TRUE(entry.BinaryPath.empty());                            // 静态条目不给磁盘路径
        EXPECT_EQ(entry.PackageSourceId, "P");
    }
}

// ③ 参与包的整包信息含**被跳过的**成员（spec §4.2 末段）：只留 kLoad 集合就没有比对面。
TEST(PackageSolve, PlanCarriesWholePackageIncludingSkippedMembers)
{
    CatalogSandbox sandbox{"package-solve-members"};
    sandbox.WriteFile("P/pack.json",
                      StaticPack("P", ProviderMember("Probe.A", "Probe.Svc") + "," +
                                          ConsumerMember("Probe.B", "Probe.Svc") + "," + Member("Probe.C", false)));
    PluginCatalog catalog;
    RefreshOrFail(catalog, sandbox);

    LoadRequest request;
    const std::array bindings{InProcessBinding{.SourceId = "P", .Entries = ProbeEntries()}};
    const auto solved = catalog.Solve(request, bindings);
    ASSERT_TRUE(solved.IsOk()) << solved.GetError().Message();

    const LoadPlanEntry* skipped = FindEntry(solved.Value().Plan, "Probe.C");
    ASSERT_NE(skipped, nullptr);
    EXPECT_EQ(skipped->Decision, LoadDecision::kSkip);
    EXPECT_EQ(skipped->Reason, SkipReason::kDisabled);

    ASSERT_EQ(solved.Value().Plan.PackageSources.size(), 1U);
    const PackagePlanSource& source = solved.Value().Plan.PackageSources.front();
    EXPECT_EQ(source.SourceId, "P");
    EXPECT_EQ(source.Kind, vase::PackageSourceKind::kInProcess);
    EXPECT_EQ(source.Entries.Lookup, bindings.front().Entries.Lookup);
    EXPECT_NE(source.Entries.Enumerate, nullptr);
    EXPECT_TRUE(source.BinaryPath.empty());
    ASSERT_EQ(source.Members.size(), 3U);            // 含 Probe.C（kSkip）
    EXPECT_EQ(source.Members.front().Id, "Probe.A"); // 成员按 Id 序
    EXPECT_EQ(source.Members.back().Id, "Probe.C");
}

// ④ 禁用提供方 → 依赖方静态跳过并归因；全跳过包免绑定、免加载。
TEST(PackageSolve, DisabledProviderSkipsConsumerWithAttribution)
{
    CatalogSandbox sandbox{"package-solve-attribution"};
    sandbox.WriteFile("P/pack.json", StaticPack("P", ProviderMember("Probe.Provider", "Probe.Svc", false) + "," +
                                                         ConsumerMember("Probe.Consumer", "Probe.Svc")));
    PluginCatalog catalog;
    RefreshOrFail(catalog, sandbox);

    LoadRequest request;
    const auto solved = catalog.Solve(request); // 全跳过 → 不需要绑定
    ASSERT_TRUE(solved.IsOk()) << solved.GetError().Message();

    const LoadPlanEntry* provider = FindEntry(solved.Value().Plan, "Probe.Provider");
    ASSERT_NE(provider, nullptr);
    EXPECT_EQ(provider->Decision, LoadDecision::kSkip);
    EXPECT_EQ(provider->Reason, SkipReason::kDisabled);

    const LoadPlanEntry* consumer = FindEntry(solved.Value().Plan, "Probe.Consumer");
    ASSERT_NE(consumer, nullptr);
    EXPECT_EQ(consumer->Decision, LoadDecision::kSkip);
    EXPECT_EQ(consumer->Reason, SkipReason::kMissingDependency);

    EXPECT_TRUE(solved.Value().Plan.PackageSources.empty()); // 全跳过包不进计划
    ASSERT_EQ(solved.Value().Notes.size(), 1U);
    EXPECT_EQ(solved.Value().Notes.front().Kind, SolveNoteKind::kProviderSkipped);
    EXPECT_EQ(solved.Value().Notes.front().PluginId, "Probe.Consumer");
    EXPECT_EQ(solved.Value().Notes.front().Cause, "Probe.Provider");
}

// ⑤ 三层合并后的配置进的是分析结果的拥有字段（不是 Solve 的临时对象）。
TEST(PackageSolve, OverrideMergesIntoAnalysisResolvedConfig)
{
    CatalogSandbox sandbox{"package-solve-override"};
    sandbox.WriteFile("P/pack.json", StaticPack("P", ConfigMember("Probe.A")));
    PluginCatalog catalog;
    RefreshOrFail(catalog, sandbox);

    vase::PluginOverride overrideEntry;
    overrideEntry.Id = "Probe.A";
    overrideEntry.Config.Set("Amount", vase::Value::From(std::int32_t{7}));
    const std::array holder{overrideEntry};
    LoadRequest request;
    request.Overrides = holder;

    const auto analyzed = catalog.Analyze(request);
    ASSERT_TRUE(analyzed.IsOk()) << analyzed.GetError().Message();
    ASSERT_EQ(analyzed.Value().Ordered.size(), 1U);
    const AnalyzedPlugin& plugin = analyzed.Value().Ordered.front();
    EXPECT_EQ(plugin.Id, "Probe.A");
    EXPECT_EQ(plugin.SourceId, "P");
    EXPECT_EQ(plugin.Kind, vase::PackageSourceKind::kInProcess);
    EXPECT_EQ(Unwrap(plugin.ResolvedConfig.Find("Amount")).GetAs<std::int32_t>(), 7);
}

// ⑥ 静态入口缺任一（查询或枚举为空）→ 拒绝。
TEST(PackageSolve, RejectsIncompleteBindingEntries)
{
    CatalogSandbox sandbox{"package-solve-incomplete"};
    sandbox.WriteFile("P/pack.json", StaticPack("PackStat", Member("Probe.A")));
    PluginCatalog catalog;
    RefreshOrFail(catalog, sandbox);
    LoadRequest request;

    InProcessBinding lookupOnly;
    lookupOnly.SourceId = "PackStat";
    lookupOnly.Entries.Lookup = ProbeEntries().Lookup;
    const std::array<InProcessBinding, 1> lookupOnlyBindings{lookupOnly};
    const auto missingEnumeration = catalog.Solve(request, lookupOnlyBindings);
    ASSERT_FALSE(missingEnumeration.IsOk());
    EXPECT_NE(missingEnumeration.GetError().Message().find("PackStat"), std::string::npos);

    InProcessBinding enumerationOnly;
    enumerationOnly.SourceId = "PackStat";
    enumerationOnly.Entries.Enumerate = ProbeEntries().Enumerate;
    const std::array<InProcessBinding, 1> enumerationOnlyBindings{enumerationOnly};
    const auto missingLookup = catalog.Solve(request, enumerationOnlyBindings);
    ASSERT_FALSE(missingLookup.IsOk());
    EXPECT_NE(missingLookup.GetError().Message().find("PackStat"), std::string::npos);
}

// ⑦ 同一 sourceId 的重复绑定 → 拒绝。
TEST(PackageSolve, RejectsDuplicateBindingForSameSource)
{
    CatalogSandbox sandbox{"package-solve-duplicate-binding"};
    sandbox.WriteFile("P/pack.json", StaticPack("P", Member("Probe.A")));
    PluginCatalog catalog;
    RefreshOrFail(catalog, sandbox);

    const std::array bindings{
        InProcessBinding{.SourceId = "P", .Entries = ProbeEntries()},
        InProcessBinding{.SourceId = "P", .Entries = ProbeEntries()},
    };
    LoadRequest request;
    const auto solved = catalog.Solve(request, bindings);
    ASSERT_FALSE(solved.IsOk());
    EXPECT_NE(solved.GetError().Message().find("duplicate in-process binding"), std::string::npos)
        << solved.GetError().Message();
}

// ⑧ 绑定指向未知来源、或指向动态来源 → 各自拒绝（不尝试查同名文件或进程符号）。
TEST(PackageSolve, RejectsUnknownAndDynamicSourceBindings)
{
    CatalogSandbox sandbox{"package-solve-bad-target"};
    sandbox.WriteFile("dyn-dir/pack.json", DynamicPack("DynPack", "DynLib", Member("Vase.Dyn")));
    PluginCatalog catalog;
    RefreshOrFail(catalog, sandbox);
    LoadRequest request;

    const std::array unknown{InProcessBinding{.SourceId = "Nope", .Entries = ProbeEntries()}};
    const auto unknownResult = catalog.Solve(request, unknown);
    ASSERT_FALSE(unknownResult.IsOk());
    EXPECT_NE(unknownResult.GetError().Message().find("unknown source"), std::string::npos)
        << unknownResult.GetError().Message();

    const std::array<InProcessBinding, 1> dynamic{InProcessBinding{.SourceId = "DynPack", .Entries = ProbeEntries()}};
    const auto dynamicResult = catalog.Solve(request, dynamic);
    ASSERT_FALSE(dynamicResult.IsOk());
    EXPECT_NE(dynamicResult.GetError().Message().find("not an in-process package source"), std::string::npos)
        << dynamicResult.GetError().Message();

    // 动态来源本身不需要绑定：空绑定集合照常出计划。
    const auto plain = catalog.Solve(request);
    ASSERT_TRUE(plain.IsOk()) << plain.GetError().Message();
    const LoadPlanEntry* dynamicEntry = FindEntry(plain.Value().Plan, "Vase.Dyn");
    ASSERT_NE(dynamicEntry, nullptr);
    EXPECT_EQ(dynamicEntry->PackageSourceId, "DynPack");
    EXPECT_FALSE(dynamicEntry->BinaryPath.empty());
    EXPECT_EQ(dynamicEntry->InProcessEntry, nullptr);
}

// ⑨ 两个来源标识绑同一个查询入口 → 拒绝（一个入口不能代表两个来源）。
TEST(PackageSolve, RejectsTwoSourcesSharingOneLookup)
{
    CatalogSandbox sandbox{"package-solve-shared-lookup"};
    sandbox.WriteFile("one/pack.json", StaticPack("PackOne", Member("Vase.One")));
    sandbox.WriteFile("two/pack.json", StaticPack("PackTwo", Member("Vase.Two")));
    PluginCatalog catalog;
    RefreshOrFail(catalog, sandbox);

    const std::array bindings{
        InProcessBinding{.SourceId = "PackOne", .Entries = ProbeEntries()},
        InProcessBinding{.SourceId = "PackTwo", .Entries = ProbeEntries()},
    };
    LoadRequest request;
    const auto solved = catalog.Solve(request, bindings);
    ASSERT_FALSE(solved.IsOk());
    EXPECT_NE(solved.GetError().Message().find("reuses the lookup entry"), std::string::npos)
        << solved.GetError().Message();
}

// ⑩ 任何 kLoad 静态条目缺完整绑定时，Err 必须点名来源**和**插件。
TEST(PackageSolve, MissingBindingNamesSourceAndPlugin)
{
    CatalogSandbox sandbox{"package-solve-missing-binding"};
    sandbox.WriteFile("stat-dir/pack.json", StaticPack("StatPack", Member("Probe.A")));
    PluginCatalog catalog;
    RefreshOrFail(catalog, sandbox);

    LoadRequest request;
    const auto solved = catalog.Solve(request);
    ASSERT_FALSE(solved.IsOk());
    const std::string& message = solved.GetError().Message();
    EXPECT_NE(message.find("StatPack"), std::string::npos) << message;
    EXPECT_NE(message.find("Probe.A"), std::string::npos) << message;
}

// ⑪ 全禁用包（无依赖形态）：无需绑定、计划仍展示 kSkip。
TEST(PackageSolve, FullySkippedPackageNeedsNoBinding)
{
    CatalogSandbox sandbox{"package-solve-all-skipped"};
    sandbox.WriteFile("P/pack.json", StaticPack("P", Member("Probe.A", false)));
    PluginCatalog catalog;
    RefreshOrFail(catalog, sandbox);

    LoadRequest request;
    const auto solved = catalog.Solve(request);
    ASSERT_TRUE(solved.IsOk()) << solved.GetError().Message();
    const LoadPlanEntry* entry = FindEntry(solved.Value().Plan, "Probe.A");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->Decision, LoadDecision::kSkip);
    EXPECT_EQ(entry->Reason, SkipReason::kDisabled);
    EXPECT_TRUE(entry->BinaryPath.empty());
    EXPECT_EQ(entry->InProcessEntry, nullptr);
    EXPECT_TRUE(solved.Value().Plan.PackageSources.empty());
}

// ⑫ 分析结果全自持（move 后、Catalog 销毁后仍可读）；Solve 的 Id 仍借快照（D61）。
TEST(PackageSolve, AnalysisOutcomeOwnsFieldsAndSolveBorrowsSnapshot)
{
    CatalogSandbox sandbox{"package-solve-owning"};
    sandbox.WriteFile("P/pack.json", StaticPack("P", ConfigMember("Probe.A")));

    std::optional<AnalysisOutcome> moved;
    {
        PluginCatalog catalog;
        RefreshOrFail(catalog, sandbox);

        vase::PluginOverride overrideEntry;
        overrideEntry.Id = "Probe.A";
        overrideEntry.Config.Set("Amount", vase::Value::From(std::int32_t{7}));
        const std::array holder{overrideEntry};
        LoadRequest request;
        request.Overrides = holder;

        auto analyzed = catalog.Analyze(request);
        ASSERT_TRUE(analyzed.IsOk()) << analyzed.GetError().Message();
        moved = std::move(analyzed.Value()); // 脱离快照搬运

        const auto solved = catalog.Solve(request);
        ASSERT_FALSE(solved.IsOk()); // 静态条目仍要绑定
        const std::array bindings{InProcessBinding{.SourceId = "P", .Entries = ProbeEntries()}};
        const auto bound = catalog.Solve(request, bindings);
        ASSERT_TRUE(bound.IsOk()) << bound.GetError().Message();

        const vase::ManifestEntry* held = catalog.Find("Probe.A");
        ASSERT_NE(held, nullptr);
        // 快照窗：计划的 Id 就是快照里那条串本身，不是分析结果里的副本。
        EXPECT_EQ(bound.Value().Plan.Ordered.front().Id.data(), held->Id.data());
    }

    ASSERT_TRUE(moved.has_value());
    ASSERT_EQ(moved->Ordered.size(), 1U);
    const AnalyzedPlugin& plugin = moved->Ordered.front();
    EXPECT_EQ(plugin.Id, "Probe.A");
    EXPECT_EQ(plugin.SourceId, "P");
    EXPECT_EQ(plugin.Expected.Id, "Probe.A");
    EXPECT_EQ(Unwrap(plugin.ResolvedConfig.Find("Amount")).GetAs<std::int32_t>(), 7);
}
