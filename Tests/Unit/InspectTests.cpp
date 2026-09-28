// Host 侧枚举读法（M5/D118/D123）：读二进制里的全部描述符，两条失败面各有单变量证人。
#include "Vase/Host/Inspect.h"

#include "Vase/Detail/Result.h"
#include "Vase/Host/Loader.h"
#include "Vase/PluginDescriptor.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace
{

std::filesystem::path FixturePath(const char* defineValue) { return std::filesystem::path{defineValue}; }

TEST(Inspect, ReadsEveryDescriptorFromARealBinary)
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> record = loader.EnsureResident(FixturePath(VASE_FIXTURE_LOADPROBE));
    ASSERT_TRUE(record.IsOk()) << record.GetError().Message();

    const vase::Result<std::vector<const vase::PluginDescriptor*>> all = vase::InspectDescriptors(*record.Value());
    ASSERT_TRUE(all.IsOk()) << all.GetError().Message();
    ASSERT_EQ(all.Value().size(), 1U);
    EXPECT_EQ(all.Value().front()->Meta->Id, "Vase.LoadProbe"); // 借用镜像，仍在驻留期故可解引用
    loader.Unload(*record.Value());
}

TEST(Inspect, MissingEnumerationEntryIsLoudAndNamed)
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> record = loader.EnsureResident(FixturePath(VASE_FIXTURE_NOENUMERATION));
    ASSERT_TRUE(record.IsOk()) << record.GetError().Message();

    const vase::Result<std::vector<const vase::PluginDescriptor*>> all = vase::InspectDescriptors(*record.Value());
    ASSERT_FALSE(all.IsOk());
    // 响亮且点名原因：不许静默返回「零个插件」（D118 的静默陷阱）。钉整段独有文案，
    // 裸符号名会被三条共享同 token 的报错共用，缺入口那一条便失去唯一性。
    EXPECT_NE(all.GetError().Message().find("binary has no VasePlugin_Descriptors"), std::string::npos);
    loader.Unload(*record.Value());
}

TEST(Inspect, StaleHeaderVersionIsRejectedOnTheEnumerationPath)
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> record = loader.EnsureResident(FixturePath(VASE_FIXTURE_STALEENUM));
    ASSERT_TRUE(record.IsOk()) << record.GetError().Message();

    const vase::Result<std::vector<const vase::PluginDescriptor*>> all = vase::InspectDescriptors(*record.Value());
    ASSERT_FALSE(all.IsOk());
    // StaleEnum 两个入口齐备、只差 HeaderVersion —— 缺符号闸被排除，这一红必出自
    // 枚举路径上的 CheckHeaderVersion（spec §5.2：与加载期同一段闸、同一份文案）。
    EXPECT_NE(all.GetError().Message().find("HeaderVersion mismatch"), std::string::npos);
    loader.Unload(*record.Value());
}

} // namespace
