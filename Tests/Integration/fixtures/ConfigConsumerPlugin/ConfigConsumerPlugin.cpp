// 配置应用探针（spec 4.1/D32/D34/D35 的现场）：OnLoad 读 ctx.Config<T>() 并把 Echo 注册成服务。
#include "Vase/Plugin.h"

#include "../M2Common.h"

#include <array>
#include <cstdint>
#include <memory>

namespace
{

// D79 装配点成员执法的材料：域 {0,1}，手写 blob 灌域外 kEnum 值 → Err；
// 域内值 → Apply 落成员、MoodLabel 现场读回（正反两半共用这一个 fixture）。
// NOLINTNEXTLINE(performance-enum-size) D75 钉 int32 基型（与 ConfigMacroTests 的 Mood 同因）。
enum class Emotion : std::int32_t
{
    kQuiet = 0,
    kLoud = 1,
};
inline constexpr std::array<vase::ChoiceInfo, 2> kMoodChoices = {
    {
        {.Value = 0, .Label = "安静"},
        {.Value = 1, .Label = "响亮"},
    },
};

VASE_CONFIG(ConsumerConfig,
            (std::int32_t, Echo, 1,
             vase::Meta{.Label = "回声",
                        .Min = vase::Value::From<std::int32_t>(0),
                        .Max = vase::Value::From<std::int32_t>(10)}),
            (const char*, Banner, "default", vase::Meta{}),
            (Emotion, Mood, Emotion::kQuiet, vase::Meta{.Label = "情绪", .Choices = vase::ChoicesOf(kMoodChoices)}));

class EchoImpl final : public m2_fixture::IConfigEcho
{
public:
    EchoImpl(int value, const ConsumerConfig& config)
        : Stored(value)
        , Config(config)
    {
    } // 成员不带下划线后缀（tidy 命名表）
    [[nodiscard]] int Value() const override { return Stored; }
    // 调用现场读配置结构体——正是 R-F1 修复前会悬空的那个读法（供体曾随计划先死）。
    [[nodiscard]] const char* Banner() const override { return Config.Banner; }
    [[nodiscard]] const char* MoodLabel() const override
    {
        // 现场读已应用的成员再查作者侧表——不是重放 blob：证的是 Apply 写对了形。
        const auto [quiet, loud] = kMoodChoices; // 解构取件，同 Mood.h 的理由
        return Config.Mood == Emotion::kLoud ? loud.Label : quiet.Label;
    }

private:
    int Stored;
    const ConsumerConfig& Config; // 借用：结构体由本实例持有，销毁序晚于插件
};

class ConfigConsumerPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        const auto& config = ctx.Config<ConsumerConfig>();
        Echo = std::make_unique<EchoImpl>(config.Echo, config);
        ctx.Provide<m2_fixture::IConfigEcho>(*Echo);
        return vase::Result<void>::Ok();
    }

private:
    std::unique_ptr<EchoImpl> Echo;
};

} // namespace

VASE_PLUGIN(ConfigConsumerPlugin){
    .Id = "Vase.ConfigConsumer",
    .DisplayName = "配置应用探针",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.ConfigEcho", .Version = 1}},
    .Config = vase::FieldsOf<ConsumerConfig>(),
};
