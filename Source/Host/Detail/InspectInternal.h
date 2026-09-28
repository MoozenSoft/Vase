#pragma once

// 两条描述符读法共用的版本闸（M5/D123）：按 Id 读（InspectBinary，CreatePod/Adopt 走）
// 与枚举读（InspectDescriptors，工具走）必须过同一段代码——文案因此也只有一份。

#include "Vase/Detail/Result.h"
#include "Vase/PluginDescriptor.h"

namespace vase::detail
{

// 失败文案是契约（回放证人钉它）：加载线既有的原文一字不改地搬到这里。
[[nodiscard]] Result<void> CheckHeaderVersion(const PluginDescriptor& desc);

} // namespace vase::detail
