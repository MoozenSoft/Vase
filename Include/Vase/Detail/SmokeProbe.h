#pragma once

// M0 冒烟专用。M1 的公开面落地后删除本文件。
//
// 存在理由（spec 2.4 验收 #4）：把「导出宏 / 动态库链接 / 运行时库查找路径」
// 这三件最容易在 M1 才炸的事，提前到还没有任何架构代码时撞上。

#include "Vase/Detail/Export.h"

#include <cstdint>

namespace vase
{

// 定义在 VaseSession。
VASE_SESSION_API std::uint32_t SessionSmokeProbe();

// 定义在 VaseHost，内部调用 SessionSmokeProbe()——让这次调用真的跨过动态库边界。
VASE_HOST_API std::uint32_t HostSmokeProbe();

} // namespace vase
