#pragma once

// 加载期期望比对的唯一判据（D72 域规则）与唯一消息格式器（spec §6）——CreatePod 与 Adopt
// （T8 接入）共用。住 Source/Host/Detail：DLL 内私有面，不进公开头、不挂导出宏。

#include "Vase/Detail/Result.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/PluginDescriptor.h"

namespace vase::detail
{

// 全字段严格等值、零规范化；失败消息点名字段路径与两侧值。调用方保证 desc 过了
// HeaderVersion 闸（本函数按完整布局读 desc.Meta）。
[[nodiscard]] Result<void> CompareDescriptor(const ManifestExpectation& expected, const PluginDescriptor& desc);

} // namespace vase::detail
