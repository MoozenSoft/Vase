#include "Vase/Config/Value.h"

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

} // namespace
