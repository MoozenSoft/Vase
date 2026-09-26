// 比对材料：两条 Provides 且作者序刻意逆序（序不敏感证人的镜像侧），无 .Config（D89 全零静默点侧）。
#include "Vase/Plugin.h"

namespace
{
class TwoProvidersPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx); // 只提供声明；服务注册与否不是本探针的判据
        return vase::Result<void>::Ok();
    }
};
} // namespace

VASE_PLUGIN(TwoProvidersPlugin){
    .Id = "Vase.Test.TwoProviders",
    .DisplayName = "双服务比对材料",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.Second", .Version = 2}, {.Name = "Vase.Test.First", .Version = 1}},
};
