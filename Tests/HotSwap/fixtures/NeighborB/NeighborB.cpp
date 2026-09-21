// 无依赖邻居探针 B（T12 §12.1）：提供心跳服务并订阅 TickEvent。A 的进出与它无关——
// 「B 的实例从第一步起没被碰过」正是主循环要断言的那件事，而 B 心跳不断是它的活体证据。
#include "Vase/Plugin.h"

#include "../BCommon.h"

namespace
{

class HeartImpl final : public samples_fixture::IHeart
{
public:
    [[nodiscard]] int Beats() const override { return BeatCount; }
    void Beat() { ++BeatCount; }

private:
    int BeatCount = 0;
};

class NeighborBPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<samples_fixture::IHeart>(Heart);
        ctx.On<samples_fixture::TickEvent>(&NeighborBPlugin::OnTick, this);
        return vase::Result<void>::Ok();
    }

private:
    void OnTick(const samples_fixture::TickEvent& event)
    {
        static_cast<void>(event); // 心跳只数次数，载荷（Seq）在这里用不到
        Heart.Beat();
    }

    HeartImpl Heart;
};

} // namespace

VASE_PLUGIN(NeighborBPlugin){
    .Id = "Vase.NeighborB",
    .DisplayName = "无依赖邻居探针",
    .Version = "1.0.0",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.Heart", .Version = 1}},
};
