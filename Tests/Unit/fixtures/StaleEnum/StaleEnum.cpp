// 枚举路径版本闸探针（M5/D123 的可证伪证人）：**两个入口都导出**，唯一变量是描述符 HeaderVersion = k+1。
// 缺符号闸因此被排除在外——它响不了，测试红/绿只跟 CheckHeaderVersion 那一句走（spec §5.2）。
#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"
#include "Vase/PluginDescriptor.h"
#include "Vase/Pod/Context.h"

#include <cstdint>
#include <memory>
#include <string_view>

namespace
{

class StaleEnumPluginImpl final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Ok();
    }
};

const vase::PluginMeta kMeta{
    .Id = "Vase.StaleEnum",
    .DisplayName = "枚举路径版本闸探针",
    .Version = "0.0.0",
    .Requires = {},
    .Provides = {},
};

vase::Plugin* CreateStub() { return std::make_unique<StaleEnumPluginImpl>().release(); }

void DestroyStub(vase::Plugin* raw) { const std::unique_ptr<vase::Plugin> owning{raw}; }

const vase::PluginDescriptor kDesc{
    .HeaderVersion = vase::kHeaderVersion + 1U, // ← 唯一变量：故意比宿主新
    .Meta = &kMeta,
    .Create = &CreateStub,
    .Destroy = &DestroyStub,
};

} // namespace

// 符号名是 ABI 契约（Loader 按字面查），不能改名——手写处没有宏展开那层豁免，故就地抑制。
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" VASE_EXPORT const vase::PluginDescriptor* VasePlugin_GetPlugin(const char* id)
{
    return std::string_view{id} == kMeta.Id ? &kDesc : nullptr;
}

// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" VASE_EXPORT const vase::PluginDescriptor* const* VasePlugin_Descriptors(std::uint32_t* outCount)
{
    static const vase::PluginDescriptor* const kAll[] = {&kDesc};
    *outCount = 1;
    return static_cast<const vase::PluginDescriptor* const*>(kAll);
}
