#include "Vase/Pod/DependencyLedger.h"

#include <cassert>
#include <cstdint>
#include <vector>

// —— 零开销条约登记（.claude/skills/cpp20-zero-overhead 第 8 组）——
// 本文件的两个付费点都在「进出结算」上，调用点是 Eject 反查（T10）与 Pod 拆除，不在
// 每帧 / 每事件 / 稳态每入口的路径里（条约 4.2）。未做阶梯 1/2/3/4 的任何测量，
// 故不主张任何性能结论；下面写的是「付了什么」与「为什么值」。
//
//   位置              付了什么                        为什么值                   测量点        复核触发
//   Record            Edges.push_back：超容时一次      账本要能长大；M1 边数以十   未做阶梯测量  边数进入三位数，
//                     重分配，否则 0 次               计，摊还成本可忽略                       或 Record 进稳态路径
//   EdgesTo/          每次调用构造 vector<const        调用点只有 Eject 反查与     未做阶梯测量  EdgesTo/EdgesFrom
//   EdgesFrom         LedgerEdge*>：空结果 0 次        Pod 拆除的 Debug 断言，                  进了每帧或每事件
//                     分配，非空 1 次堆分配            不在热路径

namespace vase::detail
{

void DependencyLedger::Record(LedgerEdge edge)
{
    // 两个「永不落」由调用点保证（Context::ResolveRaw），这里把不变式钉在账本入口。
    assert(edge.Consumer != nullptr && edge.Provider != nullptr);
    Edges.push_back(edge);
}

std::vector<const LedgerEdge*> DependencyLedger::EdgesTo(const void* provider) const
{
    std::vector<const LedgerEdge*> edges;
    for (const LedgerEdge& edge : Edges)
    {
        if (edge.Provider == provider)
        {
            edges.push_back(&edge);
        }
    }
    return edges;
}

std::vector<const LedgerEdge*> DependencyLedger::EdgesFrom(const void* consumer) const
{
    std::vector<const LedgerEdge*> edges;
    for (const LedgerEdge& edge : Edges)
    {
        if (edge.Consumer == consumer)
        {
            edges.push_back(&edge);
        }
    }
    return edges;
}

void DependencyLedger::RemoveByInstance(const void* instance)
{
    // 进出两向一起摘：实例作为消费者与作为提供方的边都随它消失（§5.6 规则②）。
    std::erase_if(Edges, [instance](const LedgerEdge& edge)
                  { return edge.Consumer == instance || edge.Provider == instance; });
}

void DependencyLedger::ClearPod(std::uint32_t podIndex)
{
    std::erase_if(Edges, [podIndex](const LedgerEdge& edge) { return edge.PodIndex == podIndex; });
}

} // namespace vase::detail
