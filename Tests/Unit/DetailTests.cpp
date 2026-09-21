#include "Vase/Detail/Fail.h"
#include "Vase/Detail/MetaArray.h"
#include "Vase/Detail/Result.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace
{

struct MoveOnly
{
    std::string S;

    MoveOnly() = default;
    explicit MoveOnly(std::string s)
        : S(std::move(s))
    {
    }
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) noexcept = default;
    MoveOnly& operator=(MoveOnly&&) noexcept = default;
    ~MoveOnly() = default;
};

// 描述符里的条目就是这个形状（名字 + 主版本），拿来测非平凡元素类型的运行期构造。
struct Service
{
    std::string_view Name;
    std::uint32_t Version = 0;
};

TEST(Result, OkCarriesMoveOnlyValue)
{
    vase::Result<MoveOnly> r = vase::Result<MoveOnly>::Ok(MoveOnly{"vase"});
    EXPECT_TRUE(r.IsOk());
    EXPECT_EQ(r.Value().S, "vase");
}

TEST(Result, ErrCarriesMessageAndContext)
{
    const vase::ErrorContext context{
        .PluginId = "Vase.Combat",
        .ServiceName = "Vase.World",
        .ServiceVersion = 1,
        .Stage = vase::Phase::kAdopt,
    };
    const vase::Result<int> r = vase::Result<int>::Err(vase::Error{"no such service", context});
    ASSERT_FALSE(r.IsOk());
    EXPECT_EQ(r.GetError().Message(), "no such service");
    EXPECT_EQ(r.GetError().Context().PluginId, "Vase.Combat");
    EXPECT_EQ(r.GetError().Context().ServiceVersion, 1U);
    EXPECT_EQ(r.GetError().Context().Stage, vase::Phase::kAdopt);
}

TEST(Result, VoidOkAndErr)
{
    EXPECT_TRUE(vase::Result<void>::Ok().IsOk());
    const vase::Result<void> e = vase::Result<void>::Err(vase::Error{"boom"});
    ASSERT_FALSE(e.IsOk());
    EXPECT_EQ(e.GetError().Message(), "boom");
}

TEST(Error, DefaultIsUnsetWithLoadPhase)
{
    const vase::Error e;
    EXPECT_FALSE(e.IsSet());
    EXPECT_EQ(e.Context().Stage, vase::Phase::kLoad);
}

TEST(MetaArray, DefaultIsEmpty)
{
    const vase::MetaArray<int, 4> a;
    EXPECT_TRUE(a.Empty());
    EXPECT_EQ(a.Size(), 0U);
}

TEST(MetaArray, InitializerPopulatesInOrder)
{
    const vase::MetaArray<int, 4> a{1, 2, 3};
    ASSERT_EQ(a.Size(), 3U);
    // 下标访问是本类型的契约之一（越界由 Capacity 在编译期拦截，无 .at() 可替代），故就地抑制该检查。
    EXPECT_EQ(a[0], 1); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    EXPECT_EQ(a[2], 3); // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    EXPECT_EQ(std::distance(a.Begin(), a.End()), 3);
}

TEST(MetaArray, CapacityBoundaryIsExact)
{
    const vase::MetaArray<int, 2> a{7, 8}; // 恰好满：合法。超出的反例在 Step 6 手动验证。
    EXPECT_EQ(a.Size(), 2U);
}

TEST(MetaArray, RuntimeConstructionPopulatesAndReads)
{
    // 运行期（非 constexpr）构造非平凡元素：这是「只声明不定义」的 MetaArrayCapacityExceeded
    // 会让整个 TU 链接失败的形态（-O0 实测 undefined symbol），本用例即那条失效模式的守卫。
    // 注意 -O2 下 clang 会把常量初值的构造整体折叠掉（const 与否都一样），故这条守卫的有效范围是 debug 线。
    const vase::MetaArray<Service, 2> services{
        {.Name = "Vase.World", .Version = 1},
        {.Name = "Vase.Audio", .Version = 2},
    };
    ASSERT_EQ(services.Size(), 2U);
    EXPECT_EQ(std::distance(services.Begin(), services.End()), 2);
    EXPECT_EQ(services.Begin()->Name, "Vase.World");
    EXPECT_EQ(std::next(services.Begin())->Version, 2U);
}

TEST(Fail, ProgrammerErrorTerminates)
{
    // 终止类断言只能用 death test——EXPECT_THROW 族在本仓库是编译期硬失败（CLAUDE.md 规矩 2）。
    EXPECT_DEATH(vase::detail::ProgrammerError("death probe"), "Vase programmer error: death probe");
}

} // namespace
