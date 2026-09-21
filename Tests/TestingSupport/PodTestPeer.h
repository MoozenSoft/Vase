#pragma once

// 测试缝（M1 形态的 #10/D14）。**边界必须说清**：这里注入的「未回收 Scope」只验
// 诊断报告的点名机制——Scope 的析构兜底（T3）意味着泄漏不会真的跨局存活。
// 插件侧真·绕道注册（9.3 契约违背）要 M3 的完整属主追踪器才抓得住。
// 把这个文件读成「#10 已全量闭环」是错的，读成「报告机器可测」才对。

#include "Vase/Pod/Pod.h"

#include <cstddef>

namespace vase
{

class PodTestPeer
{
public:
    static EffectScope& InjectLeakedScope(Pod& pod, const char* ownerLabel, std::size_t effectCount);
};

} // namespace vase
