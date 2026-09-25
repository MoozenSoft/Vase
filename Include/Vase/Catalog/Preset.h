#pragma once

// Preset 值形态（spec §5）。Preset 条目与会话临时覆盖共用 PluginOverride——
// §4.1「临时调整与保存为 Preset 之间没有转换代码」的类型兑现。

#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/ConfigBlob.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace vase
{

struct PluginOverride
{
    std::string Id;              // 引用未知 Id → Solve 记 note（warn 族）
    std::optional<bool> Enabled; // nullopt = 未提及
    ConfigBlob Config;           // 只存增量；数值按 D59 以 JSON 原样入（int64/double）
};

class Preset;

// 仅结构/语法（D59）：JSON 合法、形状对、schemaVersion major 闸、config 值域 = 标量（嵌套即拒，§4.2）；
// Id/类型核对归 Solve。导出属性挂在前声明与 friend 两处——只挂一处 clang 报重声明属性不一致。
VASE_CATALOG_API Result<Preset> LoadPreset(const std::filesystem::path& file);

class VASE_CATALOG_API Preset
{
public:
    [[nodiscard]] std::uint32_t SchemaVersion() const { return Version; }
    [[nodiscard]] const std::string& DisplayName() const { return Name; }
    [[nodiscard]] const std::vector<PluginOverride>& Entries() const
    {
        return Items;
    } // Id 字典序（JSON 解析器的对象 = std::map，非文件序；D55 不许本目录提解析器名）

private:
    friend VASE_CATALOG_API Result<Preset> LoadPreset(const std::filesystem::path& file);

    std::uint32_t Version = 1;
    std::string Name;
    std::vector<PluginOverride> Items;
};

} // namespace vase
