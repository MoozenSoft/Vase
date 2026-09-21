// HelloPlugin.cpp —— §3.1 的最小形态：提供并消费，注册两个可数的 Effect。
#include "Greeter.h"
#include "Vase/Plugin.h"

#include <string_view>

namespace
{

class GreeterImpl final : public samples::IGreeter
{
public:
    [[nodiscard]] std::string_view Greet() const override { return "hello from Vase.Hello"; }
};

class HelloPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<samples::IGreeter>(Greeter);                  // Effect #1：服务
        ctx.On<samples::GreetEvent>(&HelloPlugin::OnGreet, this); // Effect #2：订阅
        return vase::Result<void>::Ok();
    }

    vase::Result<void> OnStart(vase::Context& ctx) override
    {
        static_cast<void>(ctx); // 不是所有插件都需要启动动作（§3.1）
        return vase::Result<void>::Ok();
    }

private:
    void OnGreet(const samples::GreetEvent& event)
    {
        static_cast<void>(event); // 示例只数次数：事件载荷（Seq）在这里用不到
        ++GreetCount;
    }

    GreeterImpl Greeter;
    int GreetCount = 0;
};

} // namespace

// VASE_PLUGIN 必须在**文件全局作用域**：它的导出符号是 extern "C"，放进 namespace 会
// 破掉 C 链接。上面的类进匿名命名空间即可——同一 TU 内全局作用域照样看得见它们，
// 而工厂 / 销毁端由此收成内部链接（misc-use-internal-linkage）。
VASE_PLUGIN(HelloPlugin){
    .Id = "Vase.Hello",
    .DisplayName = "示例插件",
    .Version = "0.1.0",
    .Requires = {}, // 不依赖任何服务：任何局都能进（§4.1 增量语义的插件侧镜像）
    .Provides = {{.Name = "Vase.Hello.Greeter", .Version = 1}},
};
