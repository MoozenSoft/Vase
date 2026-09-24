// 判据 5 主角：OnLoad 成功注册 Behind（声明与注册一致），OnStart 必败——下游已在跑，只能拆。
#include "Vase/Plugin.h"

#include "../M2Common.h"

namespace
{
class BehindImpl final : public m2_fixture::IBehind
{
};
class StartFailProviderPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<m2_fixture::IBehind>(Instance);
        return vase::Result<void>::Ok();
    }
    vase::Result<void> OnStart(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Err(vase::Error{"behind failure surfaces at start"});
    }

private:
    BehindImpl Instance;
};
} // namespace

VASE_PLUGIN(StartFailProviderPlugin){
    .Id = "Vase.StartFailProvider",
    .DisplayName = "启动失败的提供者",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.Behind", .Version = 1}},
};
