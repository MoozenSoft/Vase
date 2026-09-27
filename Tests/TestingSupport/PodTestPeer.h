#pragma once

// 测试缝（M1 形态的 #10/D14）。**边界必须说清**：这里注入的「未回收 Scope」只验
// 诊断报告的点名机制——Scope 的析构兜底（T3）意味着泄漏不会真的跨局存活。
// 插件侧真·绕道注册是 §9.3 契约束，无机制可拦（M3/D97 已核查：无可达形态——拿不到 Pod 的 ScopePool）。
// 把这个文件读成「#10 已全量闭环」是错的，读成「报告机器可测」才对。

#include "Vase/Pod/Pod.h"

#include <cstddef>
#include <string>
#include <vector>

namespace vase
{

class PodTestPeer
{
public:
    static EffectScope& InjectLeakedScope(Pod& pod, const char* ownerLabel, std::size_t effectCount);

    // 证人用例（M2b 波1 T7）读取装载序：数组序 = 计划序（Pod.h:174 注释即依据）。
    static std::vector<std::string> InstanceOrder(const Pod& pod);
};

} // namespace vase
