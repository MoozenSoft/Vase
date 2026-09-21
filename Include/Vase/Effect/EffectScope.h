#pragma once

// Effect 的归属容器（§7.2）：逆序回收、Dispose 幂等、回收后不得再注册（终态）。
// Label 是 D15 的最小属主标记：诊断归属与 §1.2 断言用字符串指针就够，
// 完整归属追踪等 M3。（成员不与访问器 OwnerLabel() 撞名——成员叫 Label。）
//
// 构造参数经 std::tuple + std::apply 展开（裸 std::forward<Args>(tup) 带参编不过）；
// 对象指针由两次 static_cast 求出、不接 placement-new 的返回值——所有权自始至终属于本 Scope。

#include "Vase/Detail/Counters.h"
#include "Vase/Detail/Export.h"
#include "Vase/Detail/ScopePool.h"
#include "Vase/Effect/EffectHandle.h"
#include "Vase/Effect/IEffect.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace vase
{

class VASE_POD_API EffectScope
{
public:
    explicit EffectScope(detail::ScopePool& pool, detail::DiagnosticCounters* counters = nullptr,
                         const char* ownerLabel = "");
    ~EffectScope(); // 兜底 Dispose；计数能抓的是「Scope 从未析构」，不是「忘了 Dispose」——析构里补上了

    EffectScope(const EffectScope&) = delete;
    EffectScope& operator=(const EffectScope&) = delete;
    EffectScope(EffectScope&&) = delete;
    EffectScope& operator=(EffectScope&&) = delete;

    template <typename T, typename... Args>
    EffectHandle Create(Args&&... args)
    {
        static_assert(std::is_base_of_v<IEffect, T>, "Effect must derive from vase::IEffect");
        static_assert(alignof(T) <= detail::ScopePool::kAlignment, "over-aligned effects need a pool change first");
        std::tuple<Args&&...> pack(std::forward<Args>(args)...);
        return CreateRaw(
            sizeof(T),
            [](void* where, void* packed, IEffect** out)
            {
                auto& p = *static_cast<std::tuple<Args&&...>*>(packed);
                std::apply([where](auto&&... unpacked)
                           { ::new (where) T(std::forward<decltype(unpacked)>(unpacked)...); }, std::move(p));
                *out = static_cast<IEffect*>(static_cast<T*>(where)); // 多继承下两者可能不同址
            },
            // 析构必须由 T 自己跑：~IEffect 非虚，s.Object->~IEffect() 只销毁基类子对象，
            // 派生类成员全漏。§7.1 的「销毁路径 = Recycle + 显式析构调用」要的正是这一下。
            [](IEffect* object) { static_cast<T*>(object)->~T(); }, &pack);
    }

    void Dispose(); // 逆序、幂等（§7.2）

    [[nodiscard]] std::size_t EffectCount() const;
    [[nodiscard]] bool IsDisposed() const { return Disposed; }
    [[nodiscard]] const char* OwnerLabel() const { return Label; }

private:
    friend struct EffectHandle;

    static constexpr std::uint32_t kNone = 0xFFFFFFFFU;

    struct Slot
    {
        void* Memory = nullptr; // placement-new 的原始块（多继承下 != 对象指针），Release 时原样还给池
        IEffect* Object = nullptr;
        void (*Destroy)(IEffect*) = nullptr; // 由 Create<T> 交下来的派生析构 thunk
        std::size_t Size = 0;
        std::uint32_t Generation = 0;
        std::uint32_t Prev = kNone;
        std::uint32_t Next = kNone; // 活：注册链；死：自由链
        bool Live = false;
    };

    EffectHandle CreateRaw(std::size_t size, void (*construct)(void*, void*, IEffect**), void (*destroy)(IEffect*),
                           void* arg);
    void ReleaseSlot(std::uint32_t slot, std::uint32_t generation);
    void Unlink(std::uint32_t slot);
    [[nodiscard]] bool SlotAlive(std::uint32_t slot, std::uint32_t generation) const;

    // 槽按下标寻址是设计本体（自由链与注册链存的就是下标），故下标访问集中在这两个访问器里。
    // 用 std::next 而非 operator[]：非常量下标过不了 cppcoreguidelines-pro-bounds-avoid-unchecked-container-access。
    [[nodiscard]] Slot& SlotAt(std::uint32_t index)
    {
        return *std::next(Slots.begin(), static_cast<std::ptrdiff_t>(index));
    }
    [[nodiscard]] const Slot& SlotAt(std::uint32_t index) const
    {
        return *std::next(Slots.begin(), static_cast<std::ptrdiff_t>(index));
    }

    detail::ScopePool& Pool;
    detail::DiagnosticCounters* Counters;
    const char* Label;
    std::vector<Slot> Slots;
    std::uint32_t Head = kNone; // 注册序链（Dispose 从 Tail 逆序走）
    std::uint32_t Tail = kNone;
    std::uint32_t FreeHead = kNone;
    bool Disposed = false;
};

} // namespace vase
