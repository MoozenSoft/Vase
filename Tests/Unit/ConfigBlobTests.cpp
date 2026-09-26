#include "Vase/Host/ConfigBlob.h"

#include "Vase/Config/ConfigInfo.h"
#include "Vase/Config/ConfigMacros.h"
#include "Vase/Config/FieldInfo.h"
#include "Vase/Config/Value.h"

#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <iterator>
#include <optional>
#include <string_view>

namespace
{

constexpr vase::Value Int(int value) { return vase::Value::From<std::int32_t>(value); }

// FromDefaults 的 M2a 消费者（账 M-3）：非空默认层 + Entries() 的插入序判据。
// clang-format off
VASE_CONFIG(DefaultsProbe,
    (std::int32_t, Count, 3, vase::Meta{}),
    (const char*, Tag, "tag-default", vase::Meta{}));
// clang-format on

// bugprone-unchecked-optional-access 不认 EXPECT 级 has_value 断言，显式守卫后解引用。
template <typename T>
T Unwrap(std::optional<T> found)
{
    if (!found.has_value())
    {
        ADD_FAILURE() << "Find miss";
        return T{};
    }
    return *found;
}

TEST(ConfigBlob, EmptyFindIsNullopt)
{
    const vase::ConfigBlob blob;
    EXPECT_FALSE(blob.Find("absent").has_value());
    EXPECT_EQ(blob.Size(), 0U);
}

TEST(ConfigBlob, SetUpsertsAndKindCanChange)
{
    vase::ConfigBlob blob;
    blob.Set("Echo", Int(1));
    blob.Set("Echo", Int(2));
    EXPECT_EQ(blob.Size(), 1U);
    EXPECT_EQ(Unwrap(blob.Find("Echo")), Int(2));
    blob.Set("Echo", vase::Value::From<float>(3.5F)); // 层间合并改类型：blob 无类型可言（D32 的检出点在 Apply）
    EXPECT_EQ(blob.Size(), 1U);
}

TEST(ConfigBlob, StringValueIsOwnedAfterSet)
{
    vase::ConfigBlob blob;
    std::array<char, 10> literal{"temporary"}; // 非 static：拷入后源缓冲报废也不影响 Find
    blob.Set("Banner", vase::Value::From<const char*>(literal.data()));
    literal.front() = 'X';
    EXPECT_STREQ(Unwrap(blob.Find("Banner")).GetAs<const char*>(), "temporary");
}

TEST(ConfigBlob, MergeShallowOverridesAddsKeeps)
{
    vase::ConfigBlob base;
    base.Set("A", Int(1));
    base.Set("B", Int(2));
    vase::ConfigBlob over;
    over.Set("B", Int(20));
    over.Set("C", Int(30));
    base.MergeShallow(over);
    EXPECT_EQ(base.Size(), 3U);
    EXPECT_EQ(Unwrap(base.Find("A")), Int(1));  // 未提及 = 保留
    EXPECT_EQ(Unwrap(base.Find("B")), Int(20)); // 提及 = 覆盖（整体值，无逐字段深合并——§4.2）
    EXPECT_EQ(Unwrap(base.Find("C")), Int(30)); // 新键 = 追加
}

TEST(ConfigBlob, MergeWithEmptyIsIdentity)
{
    vase::ConfigBlob base;
    base.Set("A", Int(1));
    base.MergeShallow(vase::ConfigBlob{});
    EXPECT_EQ(base.Size(), 1U);
    EXPECT_EQ(Unwrap(base.Find("A")), Int(1));
}

TEST(ConfigBlob, FromDefaultsLaysFieldsInDeclarationOrder)
{
    // 非空默认层：铺全部字段、序 = kFields 声明序（M2b 比对复用同一路径）。
    const vase::ConfigBlob blob = vase::ConfigBlob::FromDefaults(vase::FieldsOf<DefaultsProbe>());
    ASSERT_EQ(blob.Size(), 2U);
    const auto& entries = blob.Entries();
    EXPECT_EQ(std::next(entries.begin(), 0)->Key, "Count");
    EXPECT_EQ(std::next(entries.begin(), 1)->Key, "Tag");
    EXPECT_EQ(Unwrap(blob.Find("Count")), Int(3));
    EXPECT_STREQ(Unwrap(blob.Find("Tag")).GetAs<const char*>(), "tag-default");
}

// NOLINTNEXTLINE(performance-enum-size) 与 ConfigValueTests 的 ET 同因（D75 要求 int32 基型）。
enum class ET : std::int32_t
{
    kA = 0,
    kB = 7,
};

TEST(ConfigBlobEnum, SetAndFindRoundTrip)
{
    vase::ConfigBlob blob;
    blob.Set("e", vase::Value::From<ET>(ET::kB));
    const vase::Value e = Unwrap(blob.Find("e"));
    EXPECT_EQ(e.Kind, vase::ValueKind::kEnum);
    EXPECT_EQ(e.GetAs<ET>(), ET::kB);
    blob.Set("i", vase::Value::From<std::int32_t>(7));
    EXPECT_NE(blob.Find("i"), std::nullopt); // 混入 int32 后两者不可混读
}

TEST(ConfigBlobEnum, DistinctFromInt32)
{
    vase::ConfigBlob blob;
    blob.Set("i", vase::Value::From<std::int32_t>(7));
    blob.Set("e", vase::Value::From<ET>(ET::kB));
    EXPECT_EQ(Unwrap(blob.Find("i")).Kind, vase::ValueKind::kInt32);
    EXPECT_EQ(Unwrap(blob.Find("e")).Kind, vase::ValueKind::kEnum); // 折进 int32 则此条红（D78 的第一红测）
}

TEST(ConfigBlob, ThreeLayerStackingOrder)
{
    // §4.3：清单默认 ⊕ Preset ⊕ 本局临时。合并是左折叠。
    vase::ConfigBlob temp;
    temp.Set("Echo", Int(3));
    vase::ConfigBlob preset;
    preset.Set("Echo", Int(2));
    preset.Set("Other", Int(9));
    preset.MergeShallow(temp);
    vase::ConfigBlob resolved = vase::ConfigBlob::FromDefaults(vase::ConfigInfo{}); // 空表 = 空默认层
    resolved.MergeShallow(preset);
    EXPECT_EQ(Unwrap(resolved.Find("Echo")), Int(3));
    EXPECT_EQ(Unwrap(resolved.Find("Other")), Int(9));
}

} // namespace
