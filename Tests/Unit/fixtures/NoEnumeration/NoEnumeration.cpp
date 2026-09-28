// 缺枚举入口探针（M5/D118）：手写描述符，除「没有 VasePlugin_Descriptors」外一切正常。
// HeaderVersion 刻意取正确值——与 StaleHeaderPlugin 的区别就在这里：那条撞的是版本闸，
// 本条要撞的是枚举闸，单变量才判得出是哪一道闸在响（spec 事实取证注⑬/§2.1）。
#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"
#include "Vase/PluginDescriptor.h"
#include "Vase/Pod/Context.h"

#include <memory>
#include <string_view>

namespace
{

class NoEnumerationPluginImpl final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Ok();
    }
};

const vase::PluginMeta kMeta{
    .Id = "Vase.NoEnumeration",
    .DisplayName = "缺枚举入口探针",
    .Version = "0.0.0",
    .Requires = {},
    .Provides = {},
};

vase::Plugin* CreateStub() { return std::make_unique<NoEnumerationPluginImpl>().release(); }

void DestroyStub(vase::Plugin* raw) { const std::unique_ptr<vase::Plugin> owning{raw}; }

const vase::PluginDescriptor kDesc{
    .HeaderVersion = vase::kHeaderVersion, // ← 正确值：本条不该撞版本闸
    .Meta = &kMeta,
    .Create = &CreateStub,
    .Destroy = &DestroyStub,
};

} // namespace

// 只导出装载跳（§8.1）；**刻意不导出** VasePluginDesc_* 与 VasePlugin_Descriptors。
// 符号名是 ABI 契约（Loader 按字面查），不能改名——手写处没有宏展开那层豁免，故就地抑制。
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" VASE_EXPORT const vase::PluginDescriptor* VasePlugin_GetPlugin(const char* id)
{
    return std::string_view{id} == kMeta.Id ? &kDesc : nullptr;
}
