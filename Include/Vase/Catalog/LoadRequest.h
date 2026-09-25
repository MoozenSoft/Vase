#pragma once

// 求解输入面（spec §3）：目录的属主是快照，不在这里出现（D46）；
// HostProvided 借调用方的串，窗口 = 本次 Solve（D60）。

#include "Vase/Catalog/Preset.h"
#include "Vase/PluginDescriptor.h" // ServiceRef

#include <optional>
#include <span>

namespace vase
{

struct LoadRequest
{
    std::optional<Preset> Preset;
    std::span<const PluginOverride> Overrides; // 会话临时覆盖；重复 Id → Err（D64）
    std::span<const ServiceRef> HostProvided;  // 宿主声明本局 Stage0 会注册什么（D60）
};

} // namespace vase
