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

// id 不在快照或来源为 inProcess → Err，先于 Host 调用；动态来源就地重读其 plugin.json 或 pack.json。
// 装载目标取快照来源，重读只供成员期望；读失败或包已不含该 Id 时响亮拒绝。
VASE_CATALOG_API Result<AdoptReport> AdoptInto(const PluginCatalog& catalog, PluginHost& host, PodHandle handle,
                                               std::string_view id);

} // namespace vase
