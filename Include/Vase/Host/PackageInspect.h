#pragma once

#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Inspect.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/PluginDescriptor.h"

#include <span>
#include <vector>

namespace vase
{

// 版本/Meta/Id 有效、Id 唯一、双入口同指针；按 Id 排序。指针借用来源镜像驻留期。
VASE_HOST_API Result<std::vector<const PluginDescriptor*>> InspectPackageMembers(PackageEntryPoints entries);
VASE_HOST_API Result<std::vector<const PluginDescriptor*>> InspectPackageMembers(const detail::BinaryRecord& record);

// 先检查所有输入描述符，再比较完整 Id 集；逐字段规则唯一委托 CompareDescriptor。
VASE_HOST_API Result<void> ComparePackage(std::span<const ManifestExpectation> expected,
                                          std::span<const PluginDescriptor* const> actual);

} // namespace vase
