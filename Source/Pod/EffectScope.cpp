#include "Vase/Effect/EffectScope.h"

#include "Vase/Detail/Counters.h"
#include "Vase/Detail/Fail.h"
#include "Vase/Detail/ScopePool.h"
#include "Vase/Effect/EffectHandle.h"
#include "Vase/Effect/IEffect.h"

#include <cstddef>
#include <cstdint>

namespace vase
{

bool EffectHandle::IsValid() const { return Scope != nullptr && Scope->SlotAlive(Slot, Generation); }

void EffectHandle::Release() const
{
    if (Scope != nullptr && Slot != kInvalidSlot)
    {
        Scope->ReleaseSlot(Slot, Generation); // 内部校验代际，过期即无副作用（D9）
    }
}

EffectScope::EffectScope(detail::ScopePool& pool, detail::DiagnosticCounters* counters, const char* ownerLabel)
    : Pool(pool)
    , Counters(counters)
    , Label(ownerLabel)
{
    if (Counters != nullptr)
    {
        ++Counters->Scopes;
    }
}

EffectScope::~EffectScope()
{
    Dispose(); // 幂等，已 Dispose 则无副作用
}

EffectHandle EffectScope::CreateRaw(std::size_t size, void (*construct)(void*, void*, IEffect**),
                                    void (*destroy)(IEffect*), void* arg)
{
    if (Disposed)
    {
        // §7.2「回收后不得再注册」要求 Debug 断言；这里两个构建都终止——比最低要求严。
        detail::ProgrammerError("disposed scope rejects new effects");
    }

    std::uint32_t slot = kNone;
    if (FreeHead != kNone)
    {
        slot = FreeHead;
        FreeHead = SlotAt(slot).Next;
    }
    else
    {
        slot = static_cast<std::uint32_t>(Slots.size());
        Slots.emplace_back();
    }

    Slot& s = SlotAt(slot);
    void* mem = Pool.Acquire(size);
    IEffect* obj = nullptr;
    construct(mem, arg, &obj); // 构造回调把对象指针写进 out（不用返回值：那是 owning-memory 的所有权语义）
    s.Memory = mem;
    s.Object = obj;
    s.Destroy = destroy;
    s.Size = size;
    s.Live = true;
    s.Prev = Tail;
    s.Next = kNone;
    if (Tail != kNone)
    {
        SlotAt(Tail).Next = slot;
    }
    else
    {
        Head = slot;
    }
    Tail = slot;
    if (Counters != nullptr)
    {
        ++Counters->Effects;
    }
    return EffectHandle{.Scope = this, .Slot = slot, .Generation = s.Generation};
}

void EffectScope::ReleaseSlot(std::uint32_t slot, std::uint32_t generation)
{
    if (slot >= Slots.size())
    {
        return;
    }
    Slot& s = SlotAt(slot);
    if (!s.Live || s.Generation != generation)
    {
        return; // 陈旧/重复句柄：静默无副作用（§7.2 幂等 + D9）
    }

    s.Object->Recycle(); // 逻辑撤销（注册链此刻仍含本槽：Recycle 里允许读兄弟 Effect）
    // 派生析构由 thunk 跑。写成 s.Object->~IEffect() 只销毁基类子对象（~IEffect 非虚），
    // 派生成员全漏；限定名写法只是压掉 -Wdelete-abstract-non-virtual-dtor，不解决问题。
    s.Destroy(s.Object);
    Pool.Release(s.Memory, s.Size);

    Unlink(slot);
    s.Memory = nullptr;
    s.Object = nullptr;
    s.Live = false;
    ++s.Generation; // 复用即推进：旧句柄从此永远解不开新槽（D9）
    s.Next = FreeHead;
    FreeHead = slot;
    if (Counters != nullptr)
    {
        --Counters->Effects;
    }
}

void EffectScope::Unlink(std::uint32_t slot)
{
    const Slot& s = SlotAt(slot);
    if (s.Prev != kNone)
    {
        SlotAt(s.Prev).Next = s.Next;
    }
    else
    {
        Head = s.Next;
    }
    if (s.Next != kNone)
    {
        SlotAt(s.Next).Prev = s.Prev;
    }
    else
    {
        Tail = s.Prev;
    }
}

void EffectScope::Dispose()
{
    if (Disposed)
    {
        return; // §7.2 幂等
    }
    while (Tail != kNone)
    {
        const std::uint32_t current = Tail;
        ReleaseSlot(current, SlotAt(current).Generation); // 后注册先销毁（§5.4 的顺序保证）
    }
    Disposed = true;
    if (Counters != nullptr)
    {
        --Counters->Scopes;
    }
}

std::size_t EffectScope::EffectCount() const
{
    std::size_t count = 0;
    for (std::uint32_t i = Head; i != kNone; i = SlotAt(i).Next)
    {
        ++count;
    }
    return count;
}

bool EffectScope::SlotAlive(std::uint32_t slot, std::uint32_t generation) const
{
    return slot < Slots.size() && SlotAt(slot).Live && SlotAt(slot).Generation == generation;
}

} // namespace vase
