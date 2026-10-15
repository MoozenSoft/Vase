#pragma once

// 返回指针及其元数据借用镜像只读段，只在来源镜像驻留期内有效。
// 调用方须在 Loader::Unload 前将需要保留的事实物化成拥有值。

#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"
#include "Vase/PluginDescriptor.h"

#include <vector>

namespace vase
{

namespace detail
{
struct BinaryRecord; // 声明在 Vase/Host/Loader.h；这里只取引用，不必拖进整个 Loader
} // namespace detail

// 装载/卸载的配对与归属留给调用方（工具自己 EnsureResident 与 Unload），与 CreatePod 同形。
// 缺枚举入口 / 入口返回空 / 条数为零 / 任一条 HeaderVersion 不符 —— 四种都是 Err，无静默降级。
VASE_HOST_API Result<std::vector<const PluginDescriptor*>> InspectDescriptors(const detail::BinaryRecord& record);
VASE_HOST_API Result<std::vector<const PluginDescriptor*>> InspectDescriptors(PluginEnumerationEntryPoint enumerate);

} // namespace vase
