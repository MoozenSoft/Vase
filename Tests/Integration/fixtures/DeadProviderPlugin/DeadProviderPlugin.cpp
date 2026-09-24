// 声明 Provides 但 OnLoad 必败：下游按声明登记账归因到它（D41），按注册表预检跳过（D27）。
#include "Vase/Plugin.h"

namespace
{
class DeadProviderPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Err(vase::Error{"intentional provider failure"});
    }
};
} // namespace

VASE_PLUGIN(DeadProviderPlugin){
    .Id = "Vase.DeadProvider",
    .DisplayName = "失败提供者探针",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.Dead", .Version = 1}},
};
