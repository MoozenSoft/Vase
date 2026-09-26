#include "Vase/Config/ConfigInfo.h"
#include "Vase/Config/ConfigMacros.h"
#include "Vase/Config/FieldInfo.h"
#include "Vase/Detail/Result.h"
#include "Vase/PluginDescriptor.h"

#include <gtest/gtest.h>
#include <iterator>
#include <type_traits>

// 探针类放匿名命名空间：tidy 的 misc-use-internal-linkage 要求「只在本 TU 使用的类型」
// 就该有内部链接。类的名字只在 VASE_PLUGIN 展开里用一次，放进匿名命名空间不改变任何语义
// （全局作用域的非限定查找仍能找到它），却省掉一条 NOLINT。ProbeConfig 同此处置。
namespace
{

VASE_CONFIG(ProbeConfig, (float, Volume, 2.5F, vase::Meta{.Label = "音量"}));

class DescriptorProbePlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Ok();
    }
};
} // namespace

VASE_PLUGIN(DescriptorProbePlugin){
    .Id = "Vase.DescriptorProbe",
    .DisplayName = "描述符探针",
    .Version = "0.0.1",
    .Requires = {{.Name = "Vase.World", .Version = 1}, {.Name = "Vase.Audio", .Version = 2}},
    .OptionalRequires = {{.Name = "Vase.Optional", .Version = 1}},
    .Provides = {{.Name = "Vase.Probe.Service", .Version = 1}},
    .Config = vase::FieldsOf<ProbeConfig>(),
};

namespace
{

TEST(Descriptor, MetaPopulatedThroughBraceBlock)
{
    const vase::PluginDescriptor* d = VasePluginDesc_DescriptorProbePlugin();
    EXPECT_EQ(d->HeaderVersion, vase::kHeaderVersion);
    EXPECT_EQ(vase::kHeaderVersion, 3U); // 钉字面值：自比对拦不住常量被误改，而它是 §8.3 的描述符 ABI 闸
    EXPECT_EQ(d->Meta->Id, "Vase.DescriptorProbe");
    ASSERT_EQ(d->Meta->Requires.Size(), 2U);
    // 迭代器而非 operator[]：非常量下标过不了 cppcoreguidelines-pro-bounds-*（计划「tidy 形态约束」）。
    EXPECT_EQ(std::next(d->Meta->Requires.Begin())->Name, "Vase.Audio");
    EXPECT_EQ(std::next(d->Meta->Requires.Begin())->Version, 2U);
    ASSERT_EQ(d->Meta->Provides.Size(), 1U);
    EXPECT_EQ(d->Meta->Provides.Begin()->Name, "Vase.Probe.Service");
    ASSERT_EQ(d->Meta->OptionalRequires.Size(), 1U);
    EXPECT_EQ(d->Meta->OptionalRequires.Begin()->Name, "Vase.Optional");
    EXPECT_EQ(d->Meta->Config.Count, 1U);
    EXPECT_EQ(d->Meta->Config.StructSize, sizeof(ProbeConfig));
    ASSERT_NE(d->Meta->Config.Fields, nullptr);
    EXPECT_STREQ(d->Meta->Config.Fields->Name, "Volume");
}

TEST(Descriptor, GetPluginRoutesByIdentity)
{
    const vase::PluginDescriptor* d = VasePlugin_GetPlugin("Vase.DescriptorProbe");
    EXPECT_EQ(d, VasePluginDesc_DescriptorProbePlugin());
    EXPECT_EQ(VasePlugin_GetPlugin("Vase.Nope"), nullptr); // §12 判据 #12 之外的路由正确性
    EXPECT_EQ(VasePlugin_GetPlugin(nullptr), nullptr);     // 入口的 null 守卫（VASE_PLUGIN 宏体）
}

TEST(Descriptor, CreateDestroyRoundTrips)
{
    const vase::PluginDescriptor* d = VasePluginDesc_DescriptorProbePlugin();
    vase::Plugin* p = d->Create();
    // EXPECT 而非 ASSERT：ASSERT_NE 的失败早退在 clang-analyzer 眼里是「p 泄漏路径」，
    // 而 p 为 null 时销毁端（unique_ptr 接住空指针）本来就无害，无需早退。
    EXPECT_NE(p, nullptr);
    d->Destroy(p); // 虚析构路径（§3.1 的「Delete 在镜像内」性质；测试 exe 内等价演示）
}

TEST(Descriptor, DefaultOnStartReturnsOk)
{
    // OnStart 默认实现在无 Context 内容时也能被编译进（取引用不触碰）。
    static_assert(std::is_polymorphic_v<vase::Plugin>, "Plugin 必须多态（虚析构路径的前提）");
    SUCCEED();
}

} // namespace
