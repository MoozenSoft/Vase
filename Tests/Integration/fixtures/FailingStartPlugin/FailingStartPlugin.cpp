// 启动失败探针：OnLoad 成功（注册一个 Provide + 一个 On），OnStart 返回 Err。
// 验的是 §5.2 的「失败即回收自己的 Scope」——OnStart 败也要当场拆干净，
// 否则判据 #18/#1 会被这个 fixture 自己污染（半个 Scope 留在计数里）。
#include "Vase/Plugin.h"

#include <cstdint>
#include <string_view>

namespace
{

class StartService
{
public:
    static constexpr std::string_view kName = "Vase.FailingStart.Service";
    static constexpr std::uint32_t kVersion = 1;
};

struct StartEvent
{
    static constexpr std::string_view kName = "Vase.FailingStart.Event";
    static constexpr std::uint32_t kVersion = 1;
};

class FailingStartPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<StartService>(Service);
        ctx.On<StartEvent>([](const StartEvent&) {});
        return vase::Result<void>::Ok();
    }

    vase::Result<void> OnStart(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Err(vase::Error{"intentional start failure"});
    }

private:
    StartService Service;
};

} // namespace

VASE_PLUGIN(FailingStartPlugin){
    .Id = "Vase.FailingStart",
    .DisplayName = "启动失败探针",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {},
};
