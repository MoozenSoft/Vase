// 「Shared 的启动失败提供者」（M3 终审修复 / C1 证人机器）：OnLoad 正常注册 Vase.Test.Shared、
// OnStart 必败——级联把同局的 EdgeConsumer 拆成空壳（受害者沿 strict 声明边咬消费者，见
// ComputeDownstreamClosure）。EdgeConsumer 自带 ProcessStates，空壳那条持有路径因此可被报告点名。
#include "Vase/Plugin.h"

#include "../SharedCommon.h"

namespace
{
class SharedService final : public samples_fixture::ISharedService
{
public:
    [[nodiscard]] int Value() const override { return 42; }
};
class SharedFailProviderPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<samples_fixture::ISharedService>(Instance);
        return vase::Result<void>::Ok();
    }
    vase::Result<void> OnStart(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Err(vase::Error{"shared provider fails at start"});
    }

private:
    SharedService Instance;
};
} // namespace

VASE_PLUGIN(SharedFailProviderPlugin){
    .Id = "Vase.SharedFailProvider",
    .DisplayName = "启动失败的共享提供方",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.Shared", .Version = 1}},
};
