// 第二只无依赖邻居探针（M3/D90 §12.1 的「其余实例」复数形）：提供脉冲服务并订阅 TickEvent。
// A 的进出与它无关——50 轮循环里 B 与 C 的计数都必须纹丝不动。
#include "Vase/Plugin.h"

#include "../BCommon.h"

namespace
{

class PulseImpl final : public samples_fixture::IPulse
{
public:
    [[nodiscard]] int Pulses() const override { return PulseCount; }
    void Pulse() { ++PulseCount; }

private:
    int PulseCount = 0;
};

class NeighborCPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<samples_fixture::IPulse>(Pulse);
        ctx.On<samples_fixture::TickEvent>(&NeighborCPlugin::OnTick, this);
        return vase::Result<void>::Ok();
    }

private:
    void OnTick(const samples_fixture::TickEvent& event)
    {
        static_cast<void>(event); // 只数次数
        Pulse.Pulse();
    }

    PulseImpl Pulse;
};

} // namespace

VASE_PLUGIN(NeighborCPlugin){
    .Id = "Vase.NeighborC",
    .DisplayName = "第二只无依赖邻居探针",
    .Version = "1.0.0",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.Pulse", .Version = 1}},
};
