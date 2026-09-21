#include "Vase/Pod/Pod.h"

#include "Vase/Detail/Counters.h"
#include "Vase/Detail/RegistryBus.h"
#include "Vase/Effect/EffectScope.h"
#include "Vase/Pod/Context.h"
#include "Vase/Pod/DependencyLedger.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace vase
{

Pod::Pod(detail::ScopePool& pool, detail::DiagnosticCounters& counters, detail::DependencyLedger* ledger,
         std::uint32_t podIndex)
    : Pool(pool)
    , Counters(counters)
    , Ledger(ledger)
    , PodIndex(podIndex)
{
    // 顺序承重：Bus 不接计数（§9.2 的账只由注册通道记一处），根 Scope 先于根 Context
    // 构造（Context 借用它的引用）。根 Scope 的标签点名宿主自己的注册（stage-0 落这里）。
    // Context 的构造走裸 new：构造函数私有，std::make_unique 的访问检查在 std 的上下文里。
    Registry = std::make_unique<detail::ServiceRegistry>();
    Bus = std::make_unique<detail::EventBus>();
    RootScope = std::make_unique<EffectScope>(Pool, &Counters, "Vase.Host");
    RootContext = std::unique_ptr<Context>{new Context(*RootScope, *Registry, *Bus, &Counters, nullptr)};
}

Pod::~Pod() = default;

bool PodReport::Clean() const
{
    return CountersDiff.Effects == 0 && CountersDiff.Services == 0 && CountersDiff.Subscriptions == 0 &&
           CountersDiff.PluginInstances == 0 && CountersDiff.Scopes == 0 && Residuals.empty();
}

Context& Pod::Root() { return *RootContext; }

std::size_t Pod::PluginCount() const
{
    // 「活集合」= 实例仍在世的那些（§5.6）。失败的条目留在 Instances 里但 Instance 已置空，
    // 故这里数 Instance 而不是 Instances.size()。
    std::size_t count = 0;
    for (const std::unique_ptr<LiveInstance>& instance : Instances)
    {
        if (instance->Instance != nullptr)
        {
            ++count;
        }
    }
    return count;
}

bool Pod::HasPlugin(std::string_view id) const
{
    return std::ranges::any_of(Instances, [id](const std::unique_ptr<LiveInstance>& instance)
                               { return instance->Instance != nullptr && instance->OwnerLabel == id; });
}

std::vector<std::string> Pod::PluginIds() const
{
    std::vector<std::string> ids;
    ids.reserve(Instances.size());
    for (const std::unique_ptr<LiveInstance>& instance : Instances)
    {
        if (instance->Instance != nullptr)
        {
            ids.push_back(instance->OwnerLabel);
        }
    }
    return ids;
}

const std::vector<FailedPluginRecord>& Pod::Failures() const { return FailureRecords; }

void Pod::TeardownInstancesAndRoot()
{
    // 逆序拆：后加载的先拆，与 §5.4 的关停序一致。Effect 先于实例回收——Effect 可能
    // 引用实例成员（HelloPlugin 的订阅就指向自己）。std::views::reverse 与 rbegin/rend
    // 同序同开销，只是把裸迭代器换成范围循环。
    for (const std::unique_ptr<LiveInstance>& entry : std::views::reverse(Instances))
    {
        LiveInstance& instance = *entry;
        if (instance.Instance == nullptr)
        {
            continue; // §5.2：失败路径已即时回收，这里不重复
        }
        if (Ledger != nullptr)
        {
            // §5.6 规则②：边随实例销毁消失。先摘边再拆镜像——回收动作里任何查账都看得见一致状态。
            Ledger->RemoveByInstance(instance.Instance);
#ifndef NDEBUG
            // 「活集合→空」的正查：摘完两边都该是空的，非空即框架自己漏摘（Debug-only 的免费断言）。
            assert(Ledger->EdgesFrom(instance.Instance).empty() && Ledger->EdgesTo(instance.Instance).empty());
#endif
        }
        instance.Scope->Dispose();
        instance.Desc->Destroy(instance.Instance);
        instance.Instance = nullptr;
        --Counters.PluginInstances;
    }

    // 根 Scope 最后回收：注册最早 → 回收最晚。这条顺序是「一切皆 Effect」换来的（§5.4）。
    RootScope->Dispose();

    // **不卸载二进制**：拆局不卸货，货留在架子上等下一局或等 Eject（§8.1）。
    // 卸货只发生在 Eject（T10）与宿主关闭（~PluginHost 逐个 Unload 驻留表）。
}

} // namespace vase
