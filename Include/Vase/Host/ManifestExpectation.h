#pragma once

// 加载期期望（D67）：清单事实的 Host 面投影片段，由调用方搬运、CreatePod/Adopt 消费，
// 与描述符的比对判据唯一住 detail::CompareDescriptor（域规则 D72）。不含 SchemaVersion
// （D74：读取处即闸，描述符侧无对位物）。
// 拥有与借用（契约）：全树无借用、零借用窗——拥有值形令 LoadPlan 拷贝/移动自包含（D68），
// Solve 逐条目深拷进 Entry.Expected（D84）。纯数据不过 ABI 界，无导出宏（与 LoadPlan.h 同格）。
// 全成员 NSDMI：指定初始化点省略字段在本工具链是 error（同 LoadPlan.h 的 OptionalRequires 教训）。

#include "Vase/Config/Value.h"
#include "Vase/Host/ConfigBlob.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace vase
{

// NOLINTBEGIN(readability-redundant-member-init) `= {}` NSDMI 是指定初始化省略豁免的前提（头注），
// 「冗余」是豁免的代价而非笔误；同 PluginDescriptor.h 的 OptionalRequires 先例。
struct ExpectedService
{
    std::string Name = {};
    std::uint32_t Version = 0;
};

struct ExpectedChoice
{
    std::int32_t Value = 0;
    std::string Label = {};
};

struct ExpectedConfigField
{
    std::string Key = {};
    ValueKind Kind = ValueKind::kNone;               // 七型含 kEnum；kind 漂移在此现形（D72）
    std::optional<ConfigBlob::Storage> Default = {}; // nullopt = 未设；描述符侧对位 Value.Kind==kNone
    std::optional<ConfigBlob::Storage> Min = {};
    std::optional<ConfigBlob::Storage> Max = {};
    std::string Label = {};
    std::vector<ExpectedChoice> Choices = {}; // 按序比对（D72 末段：编辑器展示序有语义）
};

struct ManifestExpectation
{
    std::string Id = {};
    std::string DisplayName = {};
    std::string Version = {};
    std::vector<ExpectedService> Requires = {};         // 三类各按 (name,version) 多重集比对，
    std::vector<ExpectedService> OptionalRequires = {}; // 序不敏感、含条数（D72）
    std::vector<ExpectedService> Provides = {};
    std::vector<ExpectedConfigField> Config = {}; // key 对齐，双向都数得着（D89 天然覆盖）
};
// NOLINTEND(readability-redundant-member-init)

} // namespace vase
