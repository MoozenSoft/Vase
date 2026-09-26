#include "Vase/Config/Value.h"
#include "Vase/Host/ConfigBlob.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <type_traits>

namespace
{

constexpr vase::Value kProbeInt{vase::Value::From<std::int32_t>(-7)};
constexpr vase::Value kProbeFloat{vase::Value::From<float>(2.5F)};

static_assert(kProbeInt.Kind == vase::ValueKind::kInt32);
static_assert(kProbeInt.GetAs<std::int32_t>() == -7);
static_assert(kProbeFloat.Kind == vase::ValueKind::kFloat);
static_assert(kProbeFloat.GetAs<float>() == 2.5F);
static_assert(std::is_trivially_copyable_v<vase::Value>);

TEST(ConfigValue, RoundTripsEveryKind)
{
    EXPECT_TRUE(vase::Value::From<bool>(true).GetAs<bool>());
    EXPECT_EQ(vase::Value::From<std::int64_t>(-1234567890123LL).GetAs<std::int64_t>(), -1234567890123LL);
    EXPECT_DOUBLE_EQ(vase::Value::From<double>(1.25).GetAs<double>(), 1.25);
    const char* greeting = "你好"; // UTF-8 字面量：/utf-8 在 VaseBuildOptions 里钉着
    EXPECT_STREQ(vase::Value::From<const char*>(greeting).GetAs<const char*>(), greeting);
}

TEST(ConfigValue, EqualityIsKindSensitive)
{
    // 1 (int32) 与 1.0 (float) 位形可能相同——Kind 不参与相等就是静默串型。
    EXPECT_NE(vase::Value::From<std::int32_t>(1), vase::Value::From<float>(1.0F));
    EXPECT_EQ(vase::Value::From<std::int32_t>(1), vase::Value::From<std::int32_t>(1));
    EXPECT_EQ(vase::Value{}, vase::Value{}); // kNone 默认值可比较
}

TEST(ConfigValue, DefaultIsNone)
{
    EXPECT_EQ(vase::Value{}.Kind, vase::ValueKind::kNone);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) 测的就是默认位形，无代码级出路。
    EXPECT_EQ(vase::Value{}.Bits, 0U);
}

// NOLINTNEXTLINE(performance-enum-size) D75 的闸就是 underlying 必须 int32——缩基型即失去被测契约本身。
enum class ET : std::int32_t
{
    kA = 0,
    kB = -5, // 负值位形（0xFFFFFFFB）：零扩展链的钉（T1/T4 handoff）
};

// 编不过的形状反例不进编译面（本仓纪律，T2 Step 2 同此）：underlying ≠ int32 的
// static_assert（D75）与枚举缺 choices 的配对闸（T4）由编译期评审守，不设运行期用例。

TEST(EnumValue, RoundTripsViaInt32Bits)
{
    const vase::Value v = vase::Value::From<ET>(ET::kB);
    EXPECT_EQ(v.Kind, vase::ValueKind::kEnum);
    EXPECT_EQ(v.GetAs<ET>(), ET::kB);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) 测的就是位形本身，无代码级出路。
    EXPECT_EQ(static_cast<std::int32_t>(static_cast<std::uint32_t>(v.Bits)), -5); // 位形低 32 位（负）
    EXPECT_TRUE(v == vase::Value::From<ET>(ET::kB));
    EXPECT_FALSE(v == vase::Value::From<std::int32_t>(-5)); // Kind 参与相等
}

TEST(EnumValue, NegativeBitFormSurvivesBlob)
{
    // D75/D78 零扩展链：int32 -5 → EnumStored{-5} → 位形回装，解码仍恰为 -5。
    vase::ConfigBlob blob;
    blob.Set("e", vase::Value::From<ET>(ET::kB));
    const vase::Value e = blob.Find("e").value_or(vase::Value{}); // miss 落 kNone，Kind 断言即红（禁 .value()）
    EXPECT_EQ(e.Kind, vase::ValueKind::kEnum);
    EXPECT_EQ(e.GetAs<ET>(), ET::kB);
    EXPECT_EQ(e.GetAs<std::int32_t>(), -5);
}

} // namespace
