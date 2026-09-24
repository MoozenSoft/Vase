// 只声明没用 → 不波及，留下继续跑；此后 TryGet 得 nullptr 合法（§6.3）。
#include "Vase/Plugin.h"

namespace
{
class BehindOptUnusedPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Ok();
    }
};
} // namespace

VASE_PLUGIN(BehindOptUnusedPlugin){
    .Id = "Vase.BehindOptUnused",
    .DisplayName = "optional 声明但没用",
    .Version = "0.0.1",
    .Requires = {},
    .OptionalRequires = {{.Name = "Vase.Test.Behind", .Version = 1}},
    .Provides = {},
};
