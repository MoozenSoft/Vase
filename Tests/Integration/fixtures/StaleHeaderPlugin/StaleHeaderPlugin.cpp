// 旧头探针（12 节 #12 的执法对象）：**手写描述符**，不走 VASE_PLUGIN。
// 它模拟「用旧 Vase 头编译出来的插件」——§8.3 那条「插件与宿主包含同一份头」的
// 前提，唯一的执行点就是 HeaderVersion 相等性。
#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"
#include "Vase/PluginDescriptor.h"
#include "Vase/Pod/Context.h"

#include <memory>
#include <string_view>

namespace
{

class StubPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Ok();
    }
};

// 指定初始化式而非位置初始化：modernize-use-designated-initializers 对本仓库开启。
const vase::PluginMeta kMeta{
    .Id = "Vase.StaleHeader",
    .DisplayName = "旧头探针",
    .Version = "0.0.0",
    .Requires = {},
    .Provides = {},
};

// 创建/销毁端与 VASE_PLUGIN 展开同一形态：make_unique 交出所有权、unique_ptr 接住再析构，
// 全程不出现裸 new / delete 表达式（否则 cppcoreguidelines-owning-memory 当场报）。
vase::Plugin* CreateStub() { return std::make_unique<StubPlugin>().release(); }

void DestroyStub(vase::Plugin* raw) { const std::unique_ptr<vase::Plugin> owning{raw}; }

const vase::PluginDescriptor kDesc{
    .HeaderVersion = 2U,
    .Meta = &kMeta,
    .Create = &CreateStub,
    .Destroy = &DestroyStub,
};
// HeaderVersion=2：钉的是上一代——v2 二进制配 v3 宿主；§3.1 的判据是相等性，
// 真实漂移形比 999 占位更响（顺带仍证明这不是「二进制更新就放行」的方向性检查）。

} // namespace

// 符号名是 §8.1 的 ABI 契约（Loader 按字面 "VasePlugin_GetPlugin" 查），不能改名。
// VASE_PLUGIN 里同名函数因落在宏展开中而不被 readability-identifier-naming 看到，
// 本文件手写、没有那层豁免，故就地抑制。
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" VASE_EXPORT const vase::PluginDescriptor* VasePlugin_GetPlugin(const char* id)
{
    return std::string_view{id} == kMeta.Id ? &kDesc : nullptr;
}
