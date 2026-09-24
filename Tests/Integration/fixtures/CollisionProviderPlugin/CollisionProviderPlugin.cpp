// 与 SharedProviderPlugin 提供同一个 Vase.Test.Shared：计划结构性碰撞的执法对象（D33 ②）。
#include "Vase/Plugin.h"

#include "../SharedCommon.h"

namespace
{
class CollisionService final : public samples_fixture::ISharedService
{
public:
    [[nodiscard]] int Value() const override { return -1; } // 与正主的 42 区分（本任务不取到它就该被拒）
};
class CollisionProviderPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<samples_fixture::ISharedService>(Instance); // 正常路径根本到不了（碰撞先拒）
        return vase::Result<void>::Ok();
    }

private:
    CollisionService Instance;
};
} // namespace

VASE_PLUGIN(CollisionProviderPlugin){
    .Id = "Vase.CollisionProvider",
    .DisplayName = "重复提供者探针",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.Shared", .Version = 1}},
};
