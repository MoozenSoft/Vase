#include "PodTestPeer.h"

#include "Vase/Effect/EffectScope.h"
#include "Vase/Effect/IEffect.h"
#include "Vase/Pod/Pod.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace
{

// 只在本 TU 内构造，故留匿名命名空间（misc-use-internal-linkage）。
// 带一个负载：零参形态会让 EffectScope::Create<T> 实例化出 std::tuple<>，那是
// EffectScope 的 Create 唯一会被 misc-const-correctness 报的形状（实测：EffectTests 的
// 每次 Create 都带参，所以那里从没报过）。
class Noop final : public vase::IEffect
{
public:
    explicit Noop(std::size_t ordinal)
        : Ordinal(ordinal)
    {
    }

    void Recycle() override { static_cast<void>(Ordinal); } // 读一下就够：负载没有语义

private:
    std::size_t Ordinal;
};

} // namespace

namespace vase
{

EffectScope& PodTestPeer::InjectLeakedScope(Pod& pod, const char* ownerLabel, std::size_t effectCount)
{
    pod.LeakedScopesForTest.push_back(std::make_unique<EffectScope>(pod.Pool, &pod.Counters, ownerLabel));
    EffectScope& scope = *pod.LeakedScopesForTest.back();
    for (std::size_t i = 0; i < effectCount; ++i)
    {
        scope.Create<Noop>(i);
    }
    return scope;
}

std::vector<std::string> PodTestPeer::InstanceOrder(const Pod& pod)
{
    std::vector<std::string> order;
    order.reserve(pod.Instances.size());
    for (const auto& live : pod.Instances)
    {
        order.emplace_back(live->OwnerLabel);
    }
    return order;
}

} // namespace vase
