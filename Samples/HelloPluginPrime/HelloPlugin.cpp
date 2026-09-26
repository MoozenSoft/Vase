// HelloPlugin.cpp —— 与 HelloPlugin 同一个 Id、同一个服务标识，只有行为不同：
// 它就是 spec §12.1 里那个「A′」。file install 把它覆盖到 Vase.Hello 的登记路径上，
// adopt 之后 get 必须打出不同的串——这就是「行为真的换了」的判据。
#include "Greeter.h"
#include "Mood.h"
#include "Vase/Plugin.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace
{

// 与 HelloPlugin 的结构体**逐字段一致**——swapdemo 的前提：换件后布局校验要过（D25）。
// 唯一差的是 Mood 默认值（D87：换件后 play 肉眼可见 label 从「安静」翻到「响亮」）。
VASE_CONFIG(HelloConfig, (std::int32_t, Repeats, 1, vase::Meta{.Label = "问候次数"}),
            (samples::Mood, MoodValue, samples::Mood::kLoud,
             vase::Meta{.Label = "情绪", .Choices = vase::ChoicesOf(samples::kMoodChoices)}));

class GreeterImpl final : public samples::IGreeter
{
public:
    void SetGreeting(std::string text) { Greeting = std::move(text); }
    [[nodiscard]] std::string_view Greet() const override { return Greeting; }

private:
    std::string Greeting = "hello from Vase.Hello prime v2 x0"; // OnLoad 立刻被配置真值覆写
};

class HelloPluginPrime final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        const auto& cfg = ctx.Config<HelloConfig>();
        // "prime v2" 由 VaseConsoleAdoptManifestSyncedReason 与 VaseConsoleAdoptManifestSyncedReplayReason
        // 两族共钉，前缀一字不动，尾缀只在 x<N> 后接。
        Greeter.SetGreeting("hello from Vase.Hello prime v2 x" + std::to_string(cfg.Repeats) + " [" +
                            samples::MoodLabel(cfg.MoodValue) + "]");
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
    .OptionalRequires = {}, // 与 HelloPlugin 同形（§3.4 作者侧证人）
    .Provides = {{.Name = "Vase.Hello.Greeter", .Version = 1}},
    .Config = vase::FieldsOf<HelloConfig>(),
};
