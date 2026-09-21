#include "Vase/Detail/Counters.h"
#include "Vase/Detail/ScopePool.h"
#include "Vase/Effect/EffectHandle.h"
#include "Vase/Effect/EffectScope.h"
#include "Vase/Effect/IEffect.h"

#include <gtest/gtest.h>
#include <vector>

namespace
{

std::vector<int>& RecycleLog()
{
    static std::vector<int> log;
    return log;
}

// final：IEffect 是保护非虚析构（销毁权归 Scope），派生类不该被拿去多态 delete；
// 标了 final 才不招 cppcoreguidelines-virtual-class-destructor。
class Tagged final : public vase::IEffect
{
public:
    explicit Tagged(int tag)
        : Tag(tag)
    {
    }
    void Recycle() override { RecycleLog().push_back(Tag); }

    int Tag;
};

std::vector<int>& DestroyLog()
{
    static std::vector<int> log;
    return log;
}

// Finding 1 的守卫装置：Tagged 只持一个 int（析构平凡），回收路径漏没漏跑派生析构看不出来。
// 这个的析构往静态日志记一笔——若回收只销毁 IEffect 基类子对象（限定名析构那种写法），
// 日志必为空，用例确定性红灯，无需 ASan。
class NonTrivial final : public vase::IEffect
{
public:
    explicit NonTrivial(int tag)
        : Tag(tag)
    {
    }
    ~NonTrivial() { DestroyLog().push_back(Tag); }
    void Recycle() override {}

    // 与 IEffect 一致：Effect 就地构造、由 Scope 回收，从不拷贝也从不移动。
    // 显式写全也是 cppcoreguidelines-special-member-functions 要的（声明了析构就得处置其余四个）。
    NonTrivial(const NonTrivial&) = delete;
    NonTrivial& operator=(const NonTrivial&) = delete;
    NonTrivial(NonTrivial&&) = delete;
    NonTrivial& operator=(NonTrivial&&) = delete;

    int Tag;
};

class Fixture
{
public:
    vase::detail::DiagnosticCounters Counters;
    vase::detail::ScopePool Pool;
    vase::EffectScope Scope{Pool, &Counters, "fixture"};
};

TEST(EffectScope, DisposeRecyclesInReverseOrder)
{
    Fixture f;
    RecycleLog().clear();
    f.Scope.Create<Tagged>(1);
    f.Scope.Create<Tagged>(2);
    f.Scope.Create<Tagged>(3);
    f.Scope.Dispose();
    EXPECT_EQ(RecycleLog(), (std::vector<int>{3, 2, 1}));
    EXPECT_EQ(f.Counters.Effects, 0U);
    EXPECT_EQ(f.Counters.Scopes, 0U);
}

TEST(EffectScope, DisposeIsIdempotent)
{
    Fixture f;
    RecycleLog().clear();
    f.Scope.Create<Tagged>(1);
    f.Scope.Dispose();
    f.Scope.Dispose();
    EXPECT_EQ(RecycleLog().size(), 1U);
    EXPECT_EQ(f.Counters.Scopes, 0U); // 第二次 Dispose 不得再动计数
}

TEST(EffectScope, DisposeRunsDerivedDestructor)
{
    Fixture f;
    DestroyLog().clear();
    f.Scope.Create<NonTrivial>(11);
    f.Scope.Create<NonTrivial>(12);
    f.Scope.Dispose();
    EXPECT_EQ(DestroyLog(), (std::vector<int>{12, 11})); // 派生析构真跑了，且与回收同序（Finding 1 的守卫）
}

TEST(EffectHandle, ReleaseExecutesAndUnregistersAtomically)
{
    Fixture f;
    RecycleLog().clear();
    const vase::EffectHandle h = f.Scope.Create<Tagged>(42);
    EXPECT_TRUE(h.IsValid()); // 导出面 EffectHandle::IsValid 的正反两态（此前零覆盖）
    EXPECT_EQ(f.Scope.EffectCount(), 1U);
    h.Release();
    EXPECT_FALSE(h.IsValid());
    EXPECT_EQ(RecycleLog(), (std::vector<int>{42})); // 动作执行了……
    EXPECT_EQ(f.Scope.EffectCount(), 0U);            // ……账也划了（§7.3 两件事一起）
    f.Scope.Dispose();
    EXPECT_EQ(RecycleLog().size(), 1U); // 不再二次回收
}

TEST(EffectHandle, SecondReleaseIsNoOp)
{
    Fixture f;
    RecycleLog().clear();
    const vase::EffectHandle h = f.Scope.Create<Tagged>(7);
    h.Release();
    h.Release();
    EXPECT_EQ(RecycleLog().size(), 1U);
}

TEST(EffectHandle, StaleHandleCannotTouchReusedSlot)
{
    Fixture f;
    RecycleLog().clear(); // 与其他用例一致：日志是静态的，单进程整跑时不先清会串味
    const vase::EffectHandle a = f.Scope.Create<Tagged>(1);
    a.Release();                                            // 槽进自由链，代际推进
    const vase::EffectHandle b = f.Scope.Create<Tagged>(2); // 大概率复用同一槽
    a.Release();                                            // 陈旧句柄：必须无副作用（D9）
    EXPECT_EQ(f.Scope.EffectCount(), 1U);                   // b 仍活着
    EXPECT_EQ(RecycleLog().size(), 1U);                     // 只回收了 a 那一次
    b.Release();
    EXPECT_EQ(f.Scope.EffectCount(), 0U);
}

TEST(ScopePool, FreedBlockIsReusedAtSameAddress)
{
    vase::detail::ScopePool pool;
    void* first = pool.Acquire(64);
    pool.Release(first, 64);
    EXPECT_EQ(pool.Acquire(64), first); // 稳态零分配的直接证据（D10）
}

TEST(ScopePool, DistinctSizesGetDistinctBlocks)
{
    vase::detail::ScopePool pool;
    void* a = pool.Acquire(16);
    void* b = pool.Acquire(32);
    EXPECT_NE(a, b);
}

#ifndef NDEBUG
TEST(EffectScopeDeath, CreateAfterDisposeTerminates)
{
    Fixture f;
    f.Scope.Dispose();
    EXPECT_DEATH(f.Scope.Create<Tagged>(1), "disposed scope rejects new effects");
}
#endif

} // namespace
