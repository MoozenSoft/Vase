// DependentPlugin.cpp —— 依赖链演示位（spec §5.2）：消费端复用 Hello 的 IGreeter，
// 自己再经 IFarewell 接口提供 Vase.Farewell（Provide 走接口 = Hello 的教学面）。
#include "Farewell.h"
#include "Greeter.h"
#include "Vase/Plugin.h"

#include <string_view>

namespace
{

class FarewellImpl final : public samples::IFarewell
{
public:
    [[nodiscard]] std::string_view Farewell() const override { return "farewell via Vase.Hello"; }
};

class DependentPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<samples::IFarewell>(FarewellText);
        return vase::Result<void>::Ok();
    }

    vase::Result<void> OnStart(vase::Context& ctx) override
    {
        // 凭声明解析（Requires 在场，预检放行）；Get 失败在本项目是 terminate，
        // 所以「活着返回」本身就是链通的证人。
        const samples::IGreeter& greeter = ctx.Get<samples::IGreeter>();
        static_cast<void>(greeter.Greet());
        return vase::Result<void>::Ok();
    }

private:
    FarewellImpl FarewellText;
};

} // namespace

VASE_PLUGIN(DependentPlugin){
    .Id = "Vase.Dependent",
    .DisplayName = "依赖演示",
    .Version = "0.1.0",
    .Requires = {{.Name = "Vase.Hello.Greeter", .Version = 1}},
    .Provides = {{.Name = "Vase.Farewell", .Version = 1}},
};
