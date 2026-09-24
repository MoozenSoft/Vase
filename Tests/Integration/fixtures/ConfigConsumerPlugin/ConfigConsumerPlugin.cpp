// 配置应用探针（spec 4.1/D32/D34/D35 的现场）：OnLoad 读 ctx.Config<T>() 并把 Echo 注册成服务。
#include "Vase/Plugin.h"

#include "../M2Common.h"

#include <cstdint>
#include <memory>

namespace
{

VASE_CONFIG(ConsumerConfig,
            (std::int32_t, Echo, 1,
             vase::Meta{.Label = "回声",
                        .Min = vase::Value::From<std::int32_t>(0),
                        .Max = vase::Value::From<std::int32_t>(10)}),
            (const char*, Banner, "default", vase::Meta{}));

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
