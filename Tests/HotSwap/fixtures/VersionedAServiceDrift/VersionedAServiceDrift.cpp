// 换件谱 rung3（M4/D106）：与 rung2 只差 `Provides` 的服务版本（v1 → v2），行为同为 return 2。
// 注册的是 ICounterV2——**声明与实注册同版本**（理由见 BCommon.h 的注释）。
#include "Vase/Plugin.h"

#include "../BCommon.h"

namespace
{

class CounterV2Impl final : public samples_fixture::ICounterV2
{
public:
    [[nodiscard]] int Value() const override { return 2; }
};

class VersionedAServiceDriftPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<samples_fixture::ICounterV2>(Counter);
        return vase::Result<void>::Ok();
    }

private:
    CounterV2Impl Counter;
};

} // namespace

VASE_PLUGIN(VersionedAServiceDriftPlugin){
    .Id = "Vase.VersionedA",
    .DisplayName = "双版本探针",
    .Version = "1.1.0",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.Counter", .Version = 2}}, // ← 与 rung2 的唯一差异
};
