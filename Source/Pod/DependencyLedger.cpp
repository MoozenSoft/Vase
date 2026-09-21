#include "Vase/Pod/DependencyLedger.h"

#include <cassert>
#include <cstdint>
#include <vector>

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
