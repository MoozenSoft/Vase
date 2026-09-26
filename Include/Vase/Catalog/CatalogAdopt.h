#pragma once

// Catalog 编排的 Adopt 面（T9/D81/D84）：快照定位 + 清单就地重读 + 期望装配 + 兄弟集收集，
// 一次调用交给 Host 的清单轨。自由函数而非 PluginCatalog 成员——全部输入走公开访问器
// （Find/Directory/Ids），不必把重的 Vase/Host/PluginHost.h 拉进 PluginCatalog.h。

#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/PluginHost.h"

#include <string_view>

namespace vase
{

// id 不在快照 → Err（D81 归因，先于任何 Host 调用）。快照内 → 就地重读
// <Directory()>/<Subdirectory>/plugin.json（不吃快照条目，§5.6①），读失败响亮透传。
// 定位吃快照（Binary/Subdirectory），重读只供期望——清单 binary 键事后篡改不改装载目标（D73/D81 分工）。
VASE_CATALOG_API Result<AdoptReport> AdoptInto(const PluginCatalog& catalog, PluginHost& host, PodHandle handle,
                                               std::string_view id);

} // namespace vase
