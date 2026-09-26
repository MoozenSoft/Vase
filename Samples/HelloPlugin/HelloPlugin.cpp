// HelloPlugin.cpp —— §3.1 的最小形态：提供并消费，注册两个可数的 Effect。
#include "Greeter.h"
#include "Mood.h"
#include "Vase/Plugin.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace
{

// 作者侧配置形态证人（§14）：宿主不写 JSON 也能把值灌进来，play 肉眼可见 x<N>；
// D87 起加七型中的 enum——默认「安静」，play 串尾缀直接打 label。
VASE_CONFIG(HelloConfig, (std::int32_t, Repeats, 1, vase::Meta{.Label = "问候次数"}),
            (samples::Mood, MoodValue, samples::Mood::kQuiet,
             vase::Meta{.Label = "情绪", .Choices = vase::ChoicesOf(samples::kMoodChoices)}));

class GreeterImpl final : public samples::IGreeter
{
public:
    void SetGreeting(std::string text) { Greeting = std::move(text); }
    [[nodiscard]] std::string_view Greet() const override { return Greeting; }

private:
    std::string Greeting = "hello from Vase.Hello x0"; // OnLoad 立刻被配置真值覆写
};

class HelloPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        const auto& cfg = ctx.Config<HelloConfig>();
        Greeter.SetGreeting("hello from Vase.Hello x" + std::to_string(cfg.Repeats) + " [" +
                            samples::MoodLabel(cfg.MoodValue) + "]");
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
    .Requires = {},         // 不依赖任何服务：任何局都能进（§4.1 增量语义的插件侧镜像）
    .OptionalRequires = {}, // 示例形态：显式写出来，作者看得见这槽存在（§3.4）
    .Provides = {{.Name = "Vase.Hello.Greeter", .Version = 1}},
    .Config = vase::FieldsOf<HelloConfig>(),
};
