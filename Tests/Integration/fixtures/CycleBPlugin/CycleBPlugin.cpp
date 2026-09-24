// 环的另一边：strict 需 A、提供 B。与 CycleAPlugin 对偶。
#include "Vase/Plugin.h"

#include "../M2Common.h"

namespace
{
class CycleBImpl final : public m2_fixture::ICycleB
{
};
class CycleBPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<m2_fixture::ICycleB>(Instance);
        return vase::Result<void>::Ok();
    }

private:
    CycleBImpl Instance;
};
} // namespace

VASE_PLUGIN(CycleBPlugin){
    .Id = "Vase.CycleB",
    .DisplayName = "环探针 B",
    .Version = "0.0.1",
    .Requires = {{.Name = "Vase.Test.CycleA", .Version = 1}},
    .Provides = {{.Name = "Vase.Test.CycleB", .Version = 1}},
};
