// 环的一边：strict 需 B、提供 A。装配预检让双方都「缺对方」→ 双双运行时跳过（D40）。
#include "Vase/Plugin.h"

#include "../M2Common.h"

namespace
{
class CycleAImpl final : public m2_fixture::ICycleA
{
};
class CycleAPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<m2_fixture::ICycleA>(Instance);
        return vase::Result<void>::Ok();
    }

private:
    CycleAImpl Instance;
};
} // namespace

VASE_PLUGIN(CycleAPlugin){
    .Id = "Vase.CycleA",
    .DisplayName = "环探针 A",
    .Version = "0.0.1",
    .Requires = {{.Name = "Vase.Test.CycleB", .Version = 1}},
    .Provides = {{.Name = "Vase.Test.CycleA", .Version = 1}},
};
