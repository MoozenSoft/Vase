// strict 声明但从没解析——契约上它假定必然拿得到，之后任何一次 Get 都会悬空，故仍被波及（D28 保守半边）。
#include "Vase/Plugin.h"

namespace
{
class BehindStrictUnusedPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Ok();
    }
};
} // namespace

VASE_PLUGIN(BehindStrictUnusedPlugin){
    .Id = "Vase.BehindStrictUnused",
    .DisplayName = "strict 声明但没用",
    .Version = "0.0.1",
    .Requires = {{.Name = "Vase.Test.Behind", .Version = 1}},
    .Provides = {},
};
