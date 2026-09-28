#pragma once

// 快照与诊断的只读形状（spec §3/§6）。字符串全自持；字符串型配置值不存借用的
// const char*，由访问器按需物化——与 D61 拆掉的 SSO 雷同源：短串搬进容器再搬家即悬垂。

#include "Vase/Config/FieldInfo.h"
#include "Vase/Config/Value.h"
#include "Vase/Detail/Export.h"
#include "Vase/Host/LoadPlan.h"

#include <cstdint>
#include <filesystem>
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

// enum choices 条目（D80）：拥有形——快照自持，比对域含它（D72 末句）。
struct ManifestChoice
{
    std::int32_t Value;
    std::string Label;
};

// 导出靠类级宏：三个访问器是 DLL 内的出定义，Windows 下测试二进制（链 VaseCatalog）
// 不导入就 unresolved external——与 PluginCatalog 同机制。
struct VASE_CATALOG_API ManifestConfigField
{
    std::string Key;
    ValueKind Kind = ValueKind::kNone; // 七型（spec §4）；enum 自波 2 合法（D49 预留兑现）

    std::uint64_t DefaultBits = 0;
    std::string DefaultStr; // kString 值；enum 中间形存 default 的 label（换 value 归 Solve，D80）

    std::uint64_t MinBits = 0;
    std::uint64_t MaxBits = 0;
    bool HasMin = false; // min/max 仅数值型可出现（解析期保证），故无 Str
    bool HasMax = false;

    std::string DisplayName;

    std::vector<ManifestChoice> Choices; // 拥有形；Kind==kEnum 时非空，非 enum 恒空（D80）

    [[nodiscard]] Value DefaultValue() const; // kString 与 enum 的 label 中间形借本条目 DefaultStr（快照内稳定）
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
    std::vector<std::string> ProcessStates; // M3/D93：进程级状态的**名字**清单（缺键 = 空集）
};

// 清单事实 → 加载期期望（D84）：拥有值形、逐字段深拷（D61 借用窗不随迁）；enum default 的
// label→value 换算在此收口（「期望形只存 value 形」，D80）。定义在 Source/Catalog/PluginCatalog.cpp。
VASE_CATALOG_API ManifestExpectation BuildExpectation(const ManifestEntry& entry);

// 描述符字段 → 清单字段（M5/D122）：BuildExpectation 的反方向。enum 的 default 在清单侧存
// **label**（D80），故此处走 value → label；标签查不到 = 描述符自相矛盾（D80 的 CHECK 点挡在前面），
// 走 ProgrammerError 与 BuildExpectation 同形。
VASE_CATALOG_API ManifestConfigField MakeManifestConfigField(const FieldInfo& field);

// 清单序列化（M5/D122/D134）：scan 的落盘口。**原子写**（临时文件 + rename 覆盖）——
// 盘上任何时刻不出现半个 JSON，而那份 JSON 正是加载期要读的权威副本。
// 写出的键必须全部落在 ParseManifestFile 的白名单内；两份清单同 TU，round-trip 用例守同源。
VASE_CATALOG_API Result<void> WriteManifestFile(const std::filesystem::path& file, const ManifestEntry& entry);

} // namespace vase
