#pragma once

// Host 内部的建局前整包预检（T6，spec §4.3）。纯计划结构 + 本进程**实际持有的镜像**两段：
// 前者不碰 Loader（误用不该触发读盘），后者按来源去重 Ensure 一次、枚举全部成员并逐字段比对。

#include "Vase/Detail/Result.h"
#include "Vase/Host/LoadPlan.h"

#include <vector>

namespace vase::detail
{

class Loader;
struct BinaryRecord;

// 按 plan.PackageSources 顺序返回每个参与来源的镜像记录（全跳过包的位置为 nullptr），
// 供随后的装配复用同一份镜像——不写回计划，也不跨建局缓存。任一环节不成立即整局 Err，
// 归因带来源与出事的成员；调用方须保证失败局不建 Pod、不注册进程级状态（§4.3 留架规则）。
[[nodiscard]] Result<std::vector<BinaryRecord*>> PreflightPackages(Loader& loader, const LoadPlan& plan);

} // namespace vase::detail
