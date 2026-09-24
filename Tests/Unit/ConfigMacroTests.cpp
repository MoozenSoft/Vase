#include "Vase/Config/ConfigMacros.h"
#include "Vase/Plugin.h"

#include <array>
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

} // namespace
