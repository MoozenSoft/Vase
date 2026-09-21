#include "Vase/Pod/Context.h"

#include "Vase/Detail/Counters.h"
#include "Vase/Detail/Fail.h"
#include "Vase/Detail/RegistryBus.h"
#include "Vase/Effect/EffectHandle.h"
#include "Vase/Effect/EffectScope.h"
#include "Vase/Effect/IEffect.h"
#include "Vase/Event/Event.h"
#include "Vase/PluginDescriptor.h"
#include "Vase/Pod/DependencyLedger.h"
#include "Vase/Service/Service.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>

// —— 零开销条约登记（.claude/skills/cpp20-zero-overhead 第 8 组）——
// 本文件（含它包含的 Context.h）引入的付费点全在「注册」上：每次 Provide / On 一次堆
// 分配（外壳对象），不在任何稳态重复路径里（条约 4.2 的临界路径是每帧 / 每对外事件 /
// 稳态每入口）。未做阶梯 1/2/3/4 的任何测量，故不主张任何性能结论；下面写的是
// 「付了什么」与「为什么值」。
//
//   位置                       付了什么                为什么值                测量点          复核触发
//   Context.h:Context::On      每条订阅 1 次堆分配     handler 类型编译期不可  未做阶梯测量    订阅进每帧或每
//                              （std::function 外壳）  知，类型擦除必须落地                    事件
//   Context.h:Context::        每次移交式注册 1 次     跨边界 delete 要留在    未做阶梯测量    移交式注册进
//   Provide(unique_ptr)        堆分配（unique_ptr      插件镜像内，外壳是载体                  热路径
//                              外壳）
//   RegistryBus.h:             容器增长：超容时才重    注册项以十计，摊还成    未做阶梯测量    注册项进入四
//   ServiceRegistry::Add /     分配（否则 0 次）；     本可忽略；EffectScope                   位数
//   EventBus::Add /            EffectScope.cpp 的      的槽位增长与它同类
//   EffectScope.cpp:           Slots.emplace_back 同形
//   EffectScope::CreateRaw
//   ScopePool.h:ScopePool::    首次按 chunk 向系统要   反复 play/stop 的稳态   未做阶梯测量    稳态循环里出
//   Acquire                    内存；释放后再 Acquire  零分配（D10）在这里                     现新分配点
//                              走空闲链，0 次分配      实现
//
// 表里最后两行是**别的文件**的点：skill 的完成判据是全仓每个付费点都有归属，而容器增长
// 与池分配此前没有登记处，收到本块。

namespace
{

// 「撤销登记」也是 Effect（§7.1 唯一通道）：Provide/On 登记什么，Recycle 就撤什么。
// 只在本 TU 内构造，故留匿名命名空间（misc-use-internal-linkage）。
class RegistrationEffect final : public vase::IEffect
{
public:
    RegistrationEffect(vase::detail::ServiceRegistry* registry, vase::ServiceKey key, std::uint64_t id,
                       vase::detail::DiagnosticCounters* counters)
        : Registry(registry)
        , Key(key)
        , Id(id)
        , Counters(counters)
    {
    }

    void Recycle() override
    {
        // 真从表里删掉了才退计数：重复回收被双键匹配挡住，计数不再动（§7.2 幂等）。
        if (Registry->Remove(Key, Id) && Counters != nullptr)
        {
            --Counters->Services;
        }
    }

private:
    vase::detail::ServiceRegistry* Registry;
    vase::ServiceKey Key;
    std::uint64_t Id;
    vase::detail::DiagnosticCounters* Counters;
};

class SubscriptionEffect final : public vase::IEffect
{
public:
    SubscriptionEffect(vase::detail::EventBus* bus, vase::EventKey key, std::uint64_t id,
                       vase::detail::DiagnosticCounters* counters)
        : Bus(bus)
        , Key(key)
        , Id(id)
        , Counters(counters)
    {
    }

    void Recycle() override
    {
        if (Bus->Remove(Key, Id) && Counters != nullptr)
        {
            --Counters->Subscriptions;
        }
    }

private:
    vase::detail::EventBus* Bus;
    vase::EventKey Key;
    std::uint64_t Id;
    vase::detail::DiagnosticCounters* Counters;
};

// 服务是否在本插件的 Requires 上（§5.6 规则①）。主版本也算身份的一部分（§6.1），
// 所以比对的是 (name, version) 整对，不是只看名字。
bool DeclaredInRequires(const vase::PluginMeta& meta, std::string_view name, std::uint32_t version)
{
    for (std::size_t index = 0; index < meta.Requires.Size(); ++index)
    {
        // 用 std::next 而非 operator[]：非常量下标过不了 cppcoreguidelines-pro-bounds-*。
        // DescriptorTests 迭代 Requires 用的是同一写法（subscript 那一处无代码级出路的才点名豁免）。
        const vase::ServiceRef& ref = *std::next(meta.Requires.Begin(), static_cast<std::ptrdiff_t>(index));
        if (ref.Name == name && ref.Version == version)
        {
            return true;
        }
    }
    return false;
}

// ProgrammerError 只收 string_view，三要素（谁的解析、哪个服务、哪个版本）先在栈上拼好。
// 这条路径必然终止，拼串那次分配无所谓。
[[noreturn]] void ReportUndeclaredResolution(std::string_view pluginId, std::string_view serviceName,
                                             std::uint32_t version)
{
    std::string message{"plugin '"};
    message.append(pluginId);
    message.append("' resolved service '");
    message.append(serviceName);
    message.append("' v");
    message.append(std::to_string(version));
    message.append(" without declaring it in Requires");
    vase::detail::ProgrammerError(message);
}

[[noreturn]] void ReportMissingRequiredService(std::string_view ownerId, std::string_view serviceName,
                                               std::uint32_t version)
{
    std::string message{ownerId};
    message.append(" required service '");
    message.append(serviceName);
    message.append("' v");
    message.append(std::to_string(version));
    message.append(" but no implementation is registered");
    vase::detail::ProgrammerError(message);
}

} // namespace

namespace vase
{

EffectHandle Context::ProvideRaw(const ServiceKey& key, void* instance, void* heapHolder, void (*destroyHolder)(void*))
{
    detail::ServiceEntry entry;
    entry.Key = key;
    entry.Instance = instance;
    entry.HeapHolder = heapHolder;
    entry.DestroyHolder = destroyHolder;
    if (SelfMeta == nullptr)
    {
        // 来源是注册位置，不是接口属性（§6.1）：根 Context 上的注册即宿主提供。
        entry.Origin = ServiceOrigin::kHost;
        entry.ProviderId = std::string_view{};
        entry.ProviderInstance = nullptr;
    }
    else
    {
        entry.Origin = ServiceOrigin::kPlugin;
        entry.ProviderId = SelfMeta->Id;
        // 提供方实例 cookie 由 Pod 建子 Context 时填（T7）；账本的边以它配对消费方（§5.6）。
        entry.ProviderInstance = ConsumerCookie;
    }

    const std::uint64_t id = Registry->Add(entry);
    if (Counters != nullptr)
    {
        ++Counters->Services;
    }
    return Scope->Create<RegistrationEffect>(Registry, key, id, Counters);
}

void* Context::ResolveRaw(std::string_view name, std::uint32_t version, bool required)
{
    // ① 凭声明（§5.6 规则①）：插件的解析必须落在自己声明的 Requires 上。这是 T9 接入
    //    账本前的唯一执法点——未声明的解析会制造账本查不到、因而永远拦不住 Eject 的隐藏边。
    if (SelfMeta != nullptr && !DeclaredInRequires(*SelfMeta, name, version))
    {
        ReportUndeclaredResolution(SelfMeta->Id, name, version);
    }

    // ② 精确 (name,version) 查表：主版本不同即不同服务（§6.1），查不到就是「服务缺失」。
    const detail::ServiceEntry* entry = Registry->Find(ServiceKey{.Name = name, .Version = version});
    if (entry == nullptr)
    {
        if (required)
        {
            ReportMissingRequiredService(SelfMeta == nullptr ? std::string_view{"host"} : SelfMeta->Id, name, version);
        }
        return nullptr;
    }

    // ③ §5.6 规则②：边随解析建立。只记「插件消费者 → 插件提供方」——宿主消费者
    //    （SelfMeta == nullptr，含根 Context）与宿主提供方（ProviderInstance == nullptr）都不落边；
    //    落了又解不开的边会让热卸永久不可用。「调用一次就丢引用」也落账：粗粒度是刻意的，
    //    瞬态解析自动过期是复杂度的黑洞（§5.6）。TryGet 与 Get 同走这里，命中即落账。
    if (Ledger != nullptr && SelfMeta != nullptr && ConsumerCookie != nullptr && entry->ProviderInstance != nullptr)
    {
        Ledger->Record(detail::LedgerEdge{
            .PodIndex = PodIndex,
            .Consumer = ConsumerCookie,
            .Provider = entry->ProviderInstance,
            .ConsumerId = SelfMeta->Id,
            .ProviderId = entry->ProviderId,
            .ServiceName = entry->Key.Name,
            .ServiceVersion = entry->Key.Version,
        });
    }
    return entry->Instance;
}

EffectHandle Context::SubscribeRaw(const EventKey& key, void* handler, void (*invoke)(const void*, void*),
                                   void (*destroy)(void*))
{
    const std::uint64_t id = Bus->Add(key, handler, invoke, destroy);
    if (Counters != nullptr)
    {
        ++Counters->Subscriptions;
    }
    return Scope->Create<SubscriptionEffect>(Bus, key, id, Counters);
}

void Context::EmitRaw(const EventKey& key, const void* event)
{
    // 事件对象是借用：派发同步走完，Emit 返回后它即失效（§2.4）——类型上没有路径能把
    // 这个指针存进任何活对象，除非 handler 自己违规（9.3 契约束）。
    Bus->Emit(key, event);
}

} // namespace vase
