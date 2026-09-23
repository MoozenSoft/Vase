// HelloPlugin.cpp —— 与 HelloPlugin 同一个 Id、同一个服务标识，只有行为不同：
// 它就是 spec §12.1 里那个「A′」。file install 把它覆盖到 Vase.Hello 的登记路径上，
// adopt 之后 get 必须打出不同的串——这就是「行为真的换了」的判据。
#include "Greeter.h"
#include "Vase/Plugin.h"

#include <string_view>

namespace
{

class GreeterImpl final : public samples::IGreeter
{
public:
    [[nodiscard]] std::string_view Greet() const override { return "hello from Vase.Hello prime v2"; }
};

class HelloPluginPrime final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<samples::IGreeter>(Greeter);
        ctx.On<samples::GreetEvent>(&HelloPluginPrime::OnGreet, this);
        return vase::Result<void>::Ok();
    }

    vase::Result<void> OnStart(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Ok();
    }

private:
    void OnGreet(const samples::GreetEvent& event)
    {
        static_cast<void>(event);
        ++GreetCount;
    }

    GreeterImpl Greeter;
    int GreetCount = 0;
};

} // namespace

VASE_PLUGIN(HelloPluginPrime){
    .Id = "Vase.Hello",
    .DisplayName = "示例插件（prime 版）",
    .Version = "0.1.0",
    .Requires = {},
    .Provides = {{.Name = "Vase.Hello.Greeter", .Version = 1}},
};
