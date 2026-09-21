#pragma once

// 插件作者唯一需要的头（v3 §3）：宏（VASE_PLUGIN / 后续 VASE_CONFIG）、基类、
// Context、Result、Error 全在这一扇门后面。Include/Vase/ 下按模块分的子目录
// 是给实现和宿主用的——作者记七个路径，等于每个新插件抄一遍示例再抄错一遍。
//
// 仓库内部一律引号包含（CLAUDE.md 规矩 3）；wiki 示例里的尖括号是给仓库外的
// 插件作者的，不要在本仓库里模仿。
//
// IWYU pragma：本头是**伞形头**（作者只包含它一个，符号实体却在下面这些头里）。
// misc-include-cleaner 不认识这层转出关系，会对着 `#include "Vase/Plugin.h"` 报
// 「包含但没用」+ 每个符号「没有直接包含的提供者」。begin/end_exports 正是 IWYU
// 给伞形头的语用意（「本头转出它包含的东西」），加上之后检查器就不再误报——比给
// 每个使用者加 NOLINT 干净。
// IWYU pragma: begin_exports
#include "Vase/Detail/Export.h"
#include "Vase/Detail/MetaArray.h"
#include "Vase/Detail/Result.h"
#include "Vase/Effect/EffectHandle.h"
#include "Vase/Effect/EffectScope.h"
#include "Vase/Effect/IEffect.h"
#include "Vase/Event/Event.h"
#include "Vase/PluginDescriptor.h"
#include "Vase/Pod/Context.h"
#include "Vase/Service/Service.h"
// IWYU pragma: end_exports
