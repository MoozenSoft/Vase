#pragma once

// Context——Pod 内的服务访问入口（§2.1/§2.3/§6.2）。四条边界（§2.3）在类型上的
// 投影：无 Proxy、无惰性解析（Get 是显式的）、查找是**一张扁平面**（子 Context
// 不是查找链的一环——它只记录 Effect 归属与诊断说话的身份）、一个服务标识
// 一个实现（重复 Provide 在 M1 运行期就终止、两个构建一致；M2a 已把它提前到装配层硬拒——
// CreatePod 结构性校验 + Adopt 双向执法，求解期那一半随 M2b 清单到位）。
//
// 堆壳的所有权写法：make_unique 造壳 + release() 显式移交，销毁端用 unique_ptr<Cell>
// 接住再析构——全程不出现裸 new/delete 表达式。

#include "Vase/Detail/Counters.h"
#include "Vase/Detail/Export.h"
#include "Vase/Detail/Fail.h"
#include "Vase/Detail/Result.h"
#include "Vase/Effect/EffectHandle.h"
#include "Vase/Effect/EffectScope.h"
#include "Vase/Event/Event.h"
#include "Vase/PluginDescriptor.h"
#include "Vase/Service/Service.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

namespace vase::detail
{
class ServiceRegistry;
class EventBus;
class DependencyLedger; // T9；VasePod 类型（放在 Pod/ 下，Host 持有实例）
struct ServiceEntry;
} // namespace vase::detail

namespace vase
{

class Pod;
class PluginHost;
class PodTestPeer; // 仅测试装配用（T8）

class VASE_POD_API Context
{
public:
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;
    ~Context() = default;

    template <typename T>
    EffectHandle Provide(T& instance)
    {
        static_assert(HasServiceIdentity<T>,
                      "服务接口必须声明 kName 与 kVersion。参见 Vase/Service/Service.h 的示例。");
        return ProvideRaw(ServiceKey{T::kName, T::kVersion}, &instance, nullptr, nullptr);
    }

    template <typename T>
    EffectHandle Provide(std::unique_ptr<T> owned)
    {
        static_assert(HasServiceIdentity<T>,
                      "服务接口必须声明 kName 与 kVersion。参见 Vase/Service/Service.h 的示例。");
        if (owned == nullptr)
        {
            detail::ProgrammerError("Provide(unique_ptr) got null");
        }
        void* instance = owned.get();
        // 外壳：unique_ptr<unique_ptr<T>> 承载「移交进来的那个 unique_ptr」。
        // make_unique 之后 release()，所有权转给注册表项的 HeapHolder 字段。
        std::unique_ptr<std::unique_ptr<T>> shell = std::make_unique<std::unique_ptr<T>>(std::move(owned));
        return ProvideRaw(ServiceKey{T::kName, T::kVersion}, instance, shell.release(),
                          &DestroyShell<std::unique_ptr<T>>);
    }

    template <typename T>
    T& Get()
    {
        static_assert(HasServiceIdentity<T>,
                      "服务接口必须声明 kName 与 kVersion。参见 Vase/Service/Service.h 的示例。");
        void* found = ResolveRaw(T::kName, T::kVersion, /*required=*/true);
        return *static_cast<T*>(found);
    }

    template <typename T>
    T* TryGet()
    {
        static_assert(HasServiceIdentity<T>,
                      "服务接口必须声明 kName 与 kVersion。参见 Vase/Service/Service.h 的示例。");
        return static_cast<T*>(ResolveRaw(T::kName, T::kVersion, /*required=*/false));
    }

    template <typename E, typename C>
    EffectHandle On(void (C::*method)(const E&), C* self)
    {
        return On<E>([self, method](const E& event) { (self->*method)(event); });
    }

    template <typename E, typename F>
    EffectHandle On(F&& handler)
    {
        static_assert(HasEventIdentity<E>, "事件结构必须声明 kName 与 kVersion。参见 Vase/Event/Event.h 的示例。");
        static_assert(std::is_invocable_v<F, const E&>, "handler 必须可 (const E&) 调用");
        using Fn = std::function<void(const E&)>;
        std::unique_ptr<Fn> shell = std::make_unique<Fn>(std::forward<F>(handler));
        return SubscribeRaw(EventKey{E::kName, E::kVersion}, shell.release(), &InvokeHandler<E>, &DestroyShell<Fn>);
    }

    template <typename E>
    void Emit(const E& event)
    {
        static_assert(HasEventIdentity<E>, "事件结构必须声明 kName 与 kVersion。参见 Vase/Event/Event.h 的示例。");
        EmitRaw(EventKey{E::kName, E::kVersion}, &event);
    }

    // 配置读取端（spec 3.2/D25）：T 必须是作者侧 VASE_CONFIG 的那个结构体。
    // 校验只到布局（Size/Align）：跨 DLL 没有可信的类型身份，同布局张冠李戴是作者契约。
    template <typename T>
    [[nodiscard]] const T& Config() const
    {
        if (ConfigStore == nullptr || ConfigMeta == nullptr)
        {
            detail::ProgrammerError("ctx.Config<T>(): plugin declares no config (.Config = FieldsOf<T> missing?)");
        }
        if (sizeof(T) != ConfigMeta->StructSize || alignof(T) != ConfigMeta->StructAlign)
        {
            detail::ProgrammerError("ctx.Config<T>(): layout mismatch with declared config struct");
        }
        return *static_cast<const T*>(ConfigStore);
    }

    EffectScope& GetScope() { return *Scope; }
    [[nodiscard]] std::string_view OwnerId() const { return SelfMeta == nullptr ? std::string_view{} : SelfMeta->Id; }

private:
    friend class Pod;
    friend class PluginHost;
    friend class PodTestPeer;

    Context(EffectScope& scope, detail::ServiceRegistry& registry, detail::EventBus& bus,
            detail::DiagnosticCounters* counters, const PluginMeta* selfMeta)
        : Scope(&scope)
        , Registry(&registry)
        , Bus(&bus)
        , Counters(counters)
        , SelfMeta(selfMeta)
    {
    }

    // 堆壳的统一销毁端：用 unique_ptr 接住再析构，不出现裸 delete 表达式。
    template <typename Cell>
    static void DestroyShell(void* cell)
    {
        const std::unique_ptr<Cell> owning{static_cast<Cell*>(cell)};
    }

    template <typename E>
    static void InvokeHandler(const void* event, void* cell)
    {
        (*static_cast<std::function<void(const E&)>*>(cell))(*static_cast<const E*>(event));
    }

    EffectHandle ProvideRaw(const ServiceKey& key, void* instance, void* heapHolder, void (*destroyHolder)(void*));
    void* ResolveRaw(std::string_view name, std::uint32_t version, bool required);
    EffectHandle SubscribeRaw(const EventKey& key, void* handler, void (*invoke)(const void*, void*),
                              void (*destroy)(void*));
    void EmitRaw(const EventKey& key, const void* event);

    EffectScope* Scope;
    detail::ServiceRegistry* Registry;
    detail::EventBus* Bus;
    detail::DiagnosticCounters* Counters;
    const PluginMeta* SelfMeta = nullptr; // nullptr == Pod 根（宿主）

    // —— T9 接线点（§5.6）：插件解析落账需要的三个挂钩，由 Host 装配时填充 ——
    detail::DependencyLedger* Ledger = nullptr; // 账本（Host 持有实例）
    const void* ConsumerCookie = nullptr;       // 本 Context 所属插件实例（根 = nullptr）
    std::uint32_t PodIndex = 0;

    // —— M2a（T6 填充）：配置对象的镜像内所有权在 LiveInstance::ConfigObject，这里只借读 ——
    void* ConfigStore = nullptr;
    const ConfigInfo* ConfigMeta = nullptr;
};

} // namespace vase
