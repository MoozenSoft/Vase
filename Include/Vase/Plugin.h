#pragma once

// 插件作者的唯一入口。
//
// M0 只在这里建立目录位置与导出宏的接线（架构文档第 10 节的目录布局已确认）；
// VASE_PLUGIN 宏、Plugin 基类、Context、Result、Error 在 M1 落地。

#include "Vase/Detail/Export.h"
