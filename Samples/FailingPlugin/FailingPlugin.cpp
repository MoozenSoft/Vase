// FailingPlugin.cpp —— 启动必败演示位（判据 4 后半的注入材料）：OnLoad 正常、
// OnStart 恒 Err；无 Provides，倒下时闭包炸不到任何人（级联缺席本身就是形态）。
#include "Vase/Plugin.h"

namespace
{

class FailingPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx); // 只提供声明（TwoProvidersPlugin 同法）
        return vase::Result<void>::Ok();
    }

    vase::Result<void> OnStart(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Err(vase::Error{"demo: OnStart always fails"});
    }
};

} // namespace

VASE_PLUGIN(FailingPlugin){
    .Id = "Vase.Failing",
    .DisplayName = "失败演示",
    .Version = "0.1.0",
    .Requires = {},
    .Provides = {},
};
