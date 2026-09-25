#pragma once

// 快照与诊断的只读形状（spec §3/§6）。字符串全自持；字符串型配置值不存借用的
// const char*，由访问器按需物化——与 D61 拆掉的 SSO 雷同源：短串搬进容器再搬家即悬垂。

#include "Vase/Config/Value.h"
#include "Vase/Detail/Export.h"
#include "Vase/Host/LoadPlan.h"

#include <cstdint>
#include <string>
#include <vector>

namespace vase
{

struct CatalogWarning
{
    std::string Subdirectory;
    std::string Message;
};

enum class SolveNoteKind : std::uint8_t
{
    kUnknownPluginId,
    kUnknownConfigKey,
    kProviderSkipped,
    kVersionMismatchProvider,
};

struct SolveNote
{
    SolveNoteKind Kind = SolveNoteKind::kUnknownPluginId;
    std::string PluginId;
    std::string Key;   // config 相关 Kind 用
    std::string Cause; // 非平凡跳过的责任主体（D62）
    std::string Message;
};

struct SolveOutcome
{
    LoadPlan Plan; // 条目 Id 借用 Catalog 快照（D61）；本结构无被借字段，move 安全
    std::vector<SolveNote> Notes;
};

struct ManifestDependency
{
    std::string Service;
    std::uint32_t Version = 0; // 解析期保证 ≥1（D64）
};

// 导出靠类级宏：三个访问器是 DLL 内的出定义，Windows 下测试二进制（链 VaseCatalog）
// 不导入就 unresolved external——与 PluginCatalog 同机制。
struct VASE_CATALOG_API ManifestConfigField
{
    std::string Key;
    ValueKind Kind = ValueKind::kNone; // 六型（spec §4）；enum 波 2 开闸

    std::uint64_t DefaultBits = 0;
    std::string DefaultStr; // kString 专属

    std::uint64_t MinBits = 0;
    std::uint64_t MaxBits = 0;
    bool HasMin = false; // min/max 仅数值型可出现（解析期保证），故无 Str
    bool HasMax = false;

    std::string DisplayName;

    [[nodiscard]] Value DefaultValue() const; // kString 借本条目 DefaultStr（快照内稳定）
    [[nodiscard]] Value MinValue() const;     // !HasMin → kNone
    [[nodiscard]] Value MaxValue() const;
};

struct ManifestEntry
{
    std::string Id;
    std::string DisplayName;  // 缺省 = Id
    std::string Version;      // 纯展示（§3.3），缺省 ""
    std::string Subdirectory; // 与 DLL 同目录即配对（456078d）
    std::string Binary;       // stem；缺省 = 子目录名（D54）
    bool EnabledByDefault = true;

    std::vector<ManifestDependency> Requires;
    std::vector<ManifestDependency> OptionalRequires;
    std::vector<ManifestDependency> Provides;
    std::vector<ManifestConfigField> Config;
};

} // namespace vase
