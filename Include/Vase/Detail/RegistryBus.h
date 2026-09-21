#pragma once

// ServiceRegistry / EventBus 的共享声明（VasePod 内的两个实例级容器）。
// 放 Detail/ 是因为 Context 与单测都要见到同一份布局，而它们都不该包含对方的头。

#include "Vase/Detail/Export.h"
#include "Vase/Event/Event.h"
#include "Vase/Service/Service.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace vase::detail
{

struct ServiceEntry
{
    ServiceKey Key;
    void* Instance = nullptr;
    ServiceOrigin Origin = ServiceOrigin::kHost;
    std::string_view ProviderId;            // kHost 时为空；诊断「谁注册的」（§2.3）
    const void* ProviderInstance = nullptr; // 插件提供方实例 cookie；kHost → nullptr（§5.6 宿主不落边）
    void* HeapHolder = nullptr;             // 移交式注册的 unique_ptr 堆壳；借用式为 nullptr
    void (*DestroyHolder)(void*) = nullptr;
    std::uint64_t Id = 0;
    bool Alive = false;
};

class VASE_POD_API ServiceRegistry
{
public:
    std::uint64_t Add(ServiceEntry entry); // Alive 由 Add 置 true

    // 精确 (name,version)：主版本不同=不同服务（§6.1）
    [[nodiscard]] const ServiceEntry* Find(ServiceKey key) const;

    bool Remove(ServiceKey key, std::uint64_t id); // 双键匹配才删；销毁 HeapHolder

    [[nodiscard]] std::size_t Count() const; // alive 数

    // M1 规模下 tombstone 不压缩（整 Pod 消亡时全清）。重复 Provide 是**编程错误**，
    // 与解析失败同路，经 detail::ProgrammerError **两个构建都终止**——不要用裸 assert：
    // 它会让 Debug 终止、Release 静默后写覆盖，两个构建行为分叉。
    // （§2.3-4 的「一服务一实现」求解期硬拒是 M2，这里是运行期兜底。）

private:
    std::vector<ServiceEntry> Entries;
    std::uint64_t NextId = 1;
};

class VASE_POD_API EventBus
{
public:
    EventBus() = default; // 不接计数：§9.2 的账由**注册通道**（Context）记一处，
                          // 存储容器保持哑存储——与 ServiceRegistry 对称

    std::uint64_t Add(EventKey key, void* handler, void (*invoke)(const void*, void*), void (*destroy)(void*));
    bool Remove(EventKey key, std::uint64_t id);
    void Emit(EventKey key, const void* event); // 同步派发（§2.4）

    [[nodiscard]] std::size_t Count() const;

private:
    struct Subscription
    {
        EventKey Key;
        void* Handler = nullptr;
        void (*Invoke)(const void*, void*) = nullptr;
        void (*Destroy)(void*) = nullptr;
        std::uint64_t Id = 0;
        bool Alive = false;
    };

    std::vector<Subscription> Subs;
    std::vector<Subscription> DeferredDestroy; // Emit 期间的 Remove 先记这里，派完再真删
    std::uint32_t EmitDepth = 0;
    // 订阅 id 由计数器发放，**不是 Subs 的下标**：Remove 走墓碑（不擦元素），
    // 下标今天恰好稳定，但 id 与行位置无关才经得起日后的压缩——与 ServiceRegistry 同形。
    std::uint64_t NextId = 1;
};

} // namespace vase::detail
