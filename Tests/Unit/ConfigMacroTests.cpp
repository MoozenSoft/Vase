#include "Vase/Config/ConfigMacros.h"
#include "Vase/Plugin.h"

#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <iterator>
#include <string_view>
#include <type_traits>

namespace
{

// 作者侧形态：元组紧贴、Meta 的 brace-init 在括号保护壳内（spec 3.2）。
// clang-format off
VASE_CONFIG(CombatConfig,
    (float, CriticalMultiplier, 2.0F, vase::Meta{.Label = "暴击倍率",
                                                 .Min = vase::Value::From<float>(1.0F),
                                                 .Max = vase::Value::From<float>(10.0F)}),
    (bool, FriendlyFire, false, vase::Meta{.Label = "友军伤害"}));

VASE_CONFIG(StringProbeConfig,
    (const char*, Banner, "hello", vase::Meta{.Label = "横幅"}));
// clang-format on

// D76 的两个编译期反例（enum 无 choices / 非 enum 带 choices）与 D80 侧补的第三个（enum 默认值
// ∉ choices，账①）由 VASE_CONFIG 的 CHECK static_assert 执法——形状反例不进编译面，
// 其存在性以本注释记录（与本仓「反例不编」纪律同格）。
// NOLINTNEXTLINE(performance-enum-size) 与 ConfigValueTests 的配置枚举同因（D75 钉 int32 基型）。
enum class Mood : std::int32_t
{
    kQuiet = 0,
    kLoud = 1,
};
// clang-format off
inline constexpr std::array<vase::ChoiceInfo, 2> kMoodChoices = {
    {
        {.Value = 0, .Label = "安静"},
        {.Value = 1, .Label = "响亮"},
    },
};
// 反例表（只含「安静」）：kLoud 在它之外——取件器语义的另一半靠它见证。
inline constexpr std::array<vase::ChoiceInfo, 1> kQuietOnly = {
    {
        {.Value = 0, .Label = "安静"},
    },
};
VASE_CONFIG(EmotionConfig,
    (std::int32_t, Repeats, 1, vase::Meta{.Label = "次数"}),
    (Mood, Feel, Mood::kQuiet, vase::Meta{.Label = "情绪", .Choices = vase::ChoicesOf(kMoodChoices)}));
// clang-format on

static_assert(
    std::is_trivially_copyable_v<CombatConfig>); // 标量成员的类型化结构体——kFields 的 POD 要求由 FieldInfo 自查
static_assert(std::is_trivially_copyable_v<vase::FieldInfo>);

TEST(ConfigMacro, MembersAndFieldsComeFromOneExpansion)
{
    const CombatConfig cfg; // 成员默认值 = 元组第 3 项
    EXPECT_FLOAT_EQ(cfg.CriticalMultiplier, 2.0F);
    EXPECT_FALSE(cfg.FriendlyFire);

    constexpr const auto& kFields = CombatConfig::kFields;
    static_assert(kFields.size() == 2);
    static_assert(kFields.front().Name == std::string_view{"CriticalMultiplier"});
    static_assert(kFields.front().Kind == vase::ValueKind::kFloat);
    static_assert(kFields.front().Default.GetAs<float>() == 2.0F); // 「同一次展开」的一致性：成员默认 == 表默认
    static_assert(kFields.front().Min.GetAs<float>() == 1.0F);
    static_assert(kFields.front().Max.GetAs<float>() == 10.0F);
    static_assert(kFields.front().Label == std::string_view{"暴击倍率"});
    static_assert((*std::next(kFields.begin(), 1)).Name == std::string_view{"FriendlyFire"});
    static_assert((*std::next(kFields.begin(), 1)).Min.Kind == vase::ValueKind::kNone); // Meta 未写 = kNone（D35）

    EXPECT_NE(kFields.front().Apply, nullptr);
}

TEST(ConfigMacro, ApplyWritesTypedMember)
{
    CombatConfig cfg;
    (*std::next(CombatConfig::kFields.begin(), 1)).Apply(&cfg, vase::Value::From<bool>(true));
    EXPECT_TRUE(cfg.FriendlyFire);
    CombatConfig::kFields.front().Apply(&cfg, vase::Value::From<float>(3.5F));
    EXPECT_FLOAT_EQ(cfg.CriticalMultiplier, 3.5F); // D35：越过 Meta.Min 的 0.5f 照写不误——库内无执法
}

TEST(ConfigMacro, FieldsOfDescribesTheStruct)
{
    constexpr vase::ConfigInfo kInfo = vase::FieldsOf<CombatConfig>();
    static_assert(kInfo.Count == 2);
    static_assert(kInfo.StructSize == sizeof(CombatConfig));
    static_assert(kInfo.StructAlign == alignof(CombatConfig));
    EXPECT_NE(kInfo.Fields, nullptr);

    void* store = kInfo.CreateConfig(); // 镜像内配对工厂（测试 exe = 本 TU 的镜像）
    ASSERT_NE(store, nullptr);
    std::next(kInfo.Fields, 1)->Apply(store, vase::Value::From<bool>(true));
    EXPECT_TRUE(static_cast<CombatConfig*>(store)->FriendlyFire);
    kInfo.DestroyConfig(store);
}

TEST(ConfigMacro, StringKindWorksEndToEnd)
{
    constexpr vase::ConfigInfo kInfo = vase::FieldsOf<StringProbeConfig>();
    static_assert(kInfo.Count == 1);
    static_assert(kInfo.Fields->Default.GetAs<const char*>() != nullptr);
    EXPECT_STREQ(kInfo.Fields->Default.GetAs<const char*>(), "hello"); // 运行期回读：走的就是 Apply 那条 GetAs
    void* store = kInfo.CreateConfig();
    kInfo.DestroyConfig(store);
}

// 指针/数组下标一律走 -> 与 std::next（pro-bounds 门禁，同 DescriptorTests 纪律）。
TEST(ConfigMacroEnum, FieldInfoCarriesChoicesTable)
{
    const auto& fields = EmotionConfig::kFields;
    EXPECT_EQ(fields.back().Kind, vase::ValueKind::kEnum);
    ASSERT_EQ(fields.back().ChoiceCount, 2U);
    ASSERT_NE(fields.back().Choices, nullptr);
    EXPECT_EQ(fields.back().Choices->Value, 0);
    EXPECT_STREQ(std::next(fields.back().Choices, 1)->Label, "响亮");
    EXPECT_EQ(fields.back().Default.GetAs<Mood>(), Mood::kQuiet);
    EXPECT_EQ(fields.front().Choices, nullptr); // 非枚举字段全零槽
    EXPECT_EQ(fields.front().ChoiceCount, 0U);
}

// 账①：闸本身是 static_assert（形状反例不编，见文件头），此处钉取件器的语义——正例、反例各一。
// 非 enum 一支的真证人是本 TU 已有的 CombatConfig / StringProbeConfig 展开（真转 int32 早编不过）。
TEST(ConfigMacroEnum, DefaultInChoicesIsTheMembershipGate)
{
    EXPECT_TRUE(vase::DefaultInChoices<Mood>(Mood::kQuiet, vase::ChoicesOf(kMoodChoices)));
    EXPECT_FALSE(vase::DefaultInChoices<Mood>(Mood::kLoud, vase::ChoicesOf(kQuietOnly)));
    EXPECT_TRUE(vase::DefaultInChoices<std::int32_t>(7, vase::ChoiceView{}));
}

} // namespace
