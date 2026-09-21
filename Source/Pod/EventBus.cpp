#include "Vase/Detail/RegistryBus.h"

#include "Vase/Event/Event.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>

namespace vase::detail
{

std::uint64_t EventBus::Add(EventKey key, void* handler, void (*invoke)(const void*, void*), void (*destroy)(void*))
{
    Subscription sub;
    sub.Key = key;
    sub.Handler = handler;
    sub.Invoke = invoke;
    sub.Destroy = destroy;
    const std::uint64_t id = NextId++;
    sub.Id = id;
    sub.Alive = true;
    Subs.push_back(sub);
    return id;
}

bool EventBus::Remove(EventKey key, std::uint64_t id)
{
    for (Subscription& sub : Subs)
    {
        if (sub.Alive && sub.Id == id && sub.Key == key)
        {
            sub.Alive = false;
            if (EmitDepth > 0)
            {
                // 派发中销毁 handler，会让本轮稍后仍会触达它的路径踩空——推到派完再真删。
                DeferredDestroy.push_back(sub);
            }
            else if (sub.Destroy != nullptr)
            {
                sub.Destroy(sub.Handler);
            }
            // 墓碑不留活指针（与 ServiceRegistry::Remove 对称）：回调与 Handler 都已失效，
            // 留着就是「看起来还能调」的死数据。推迟销毁那一支已在上面留了副本。
            sub.Handler = nullptr;
            sub.Invoke = nullptr;
            sub.Destroy = nullptr;
            return true;
        }
    }
    return false;
}

void EventBus::Emit(EventKey key, const void* event)
{
    // 上界在进循环前取定：**本轮只派发进入时已存在的订阅**。派发中 Add 进来的订阅留到下一轮，
    // 否则反复自订阅会在同一轮里成环（且「派到一半插队」的语义无处可查）。
    const std::size_t bound = Subs.size();
    ++EmitDepth;
    // 遍历活表本身：Remove 只置墓碑、不擦元素、不重排，故本轮不会遇到失效行。
    // 用 std::next 定位下标（非常量下标的 operator[] 过不了门禁）。
    for (std::size_t index = 0; index < bound; ++index)
    {
        const Subscription& sub = *std::next(Subs.begin(), static_cast<std::ptrdiff_t>(index));
        if (sub.Alive && sub.Key == key)
        {
            // (event, handler)——与 Context::InvokeHandler 的形参序一致。
            sub.Invoke(event, sub.Handler);
        }
    }
    --EmitDepth;
    if (EmitDepth == 0)
    {
        // 先换出再遍历：某个 Destroy 回调若重入 Emit，嵌套的 Remove 会往 DeferredDestroy 里
        // 追加，就地遍历那个 vector 会踩到失效迭代器。pending 出作用域即析构（DeferredDestroy 已空）。
        std::vector<Subscription> pending;
        pending.swap(DeferredDestroy);
        for (const Subscription& dead : pending)
        {
            if (dead.Destroy != nullptr)
            {
                dead.Destroy(dead.Handler);
            }
        }
    }
}

std::size_t EventBus::Count() const
{
    std::size_t alive = 0;
    for (const Subscription& sub : Subs)
    {
        if (sub.Alive)
        {
            ++alive;
        }
    }
    return alive;
}

} // namespace vase::detail
