#pragma once

// 纯数据分析的拥有形结果（T5，spec §4.1）：插件级顺序、load/skip 决策与原因、来源标识、
// 已合并配置、逐插件期望与 Notes。**不含描述符指针或入口地址**——不装载库、不调用入口、
// 不依赖静态绑定；故同一结果既能喂 CLI 的计划展示，也能喂 Solve 物化可执行计划。
// 字段全自持（D84 同格）：整棵树可安全 move，Catalog 销毁后仍是有效值。

#include "Vase/Catalog/ManifestView.h" // SolveNote
#include "Vase/Host/ConfigBlob.h"
#include "Vase/Host/LoadPlan.h" // LoadDecision / SkipReason
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/Host/PackagePlan.h" // PackageSourceKind

#include <cstdint>
#include <string>
#include <vector>

namespace vase
{

// NOLINTBEGIN(readability-redundant-member-init) `= {}` NSDMI 是指定初始化省略豁免的前提
// （clang 的 -Wmissing-designated-field-initializers 在本工具链是错误），「冗余」是豁免的代价
// 而非笔误；同 ManifestExpectation.h 的头注与同一处理。
struct AnalyzedPlugin
{
    std::string Id = {};
    std::string SourceId = {}; // 所属来源键：单插件 `@single/<子目录>`、包为包 sourceId
    PackageSourceKind Kind = PackageSourceKind::kDynamic;
    LoadDecision Decision = LoadDecision::kLoad;
    SkipReason Reason{};               // 仅 kSkip 有意义（kDisabled/kMissingDependency/kVersionMismatch）
    ConfigBlob ResolvedConfig = {};    // 三层合并后的配置；仅 kLoad 条目填（与旧计划条目同权）
    ManifestExpectation Expected = {}; // 装配期的逐字段比对依据（D84），kSkip 亦填
};
// NOLINTEND(readability-redundant-member-init)

struct AnalysisOutcome
{
    // kLoad 段 = 拓扑序；kSkip 段尾随、按快照 Id 序（与计划条目序同一契约）。
    std::vector<AnalyzedPlugin> Ordered;
    std::vector<SolveNote> Notes;
};

} // namespace vase
