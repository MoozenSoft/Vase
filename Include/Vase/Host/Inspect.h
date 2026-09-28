#pragma once

// 枚举读法（M5/D118/D123）：读一个二进制里的**全部**描述符。只给工具用——装载跳仍唯一
// （VasePlugin_GetPlugin，§8.1），宿主侧不走这里。
//
// 寿命纪律（借用面）：返回的指针指向镜像只读段，其内部的 string_view / MetaArray 同样如此。
// 寿命 = 传入 BinaryRecord 的驻留期——调用方必须在 Loader::Unload **之前**把需要的东西
// 物化成拥有形（scan → 清单值；validate → 期望与结论）。Unload 之后解引用是悬垂。

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

} // namespace vase
