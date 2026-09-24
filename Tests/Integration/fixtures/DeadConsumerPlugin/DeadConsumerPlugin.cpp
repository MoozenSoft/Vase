// strict 需要 Dead：提供者死了它就被预检跳过，归因应点名 Vase.DeadProvider。
#include "Vase/Plugin.h"

namespace
{
class DeadConsumerPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx); // 正常路径根本到不了这里（预检先拦）
        return vase::Result<void>::Ok();
    }
};
} // namespace

VASE_PLUGIN(DeadConsumerPlugin){
    .Id = "Vase.DeadConsumer",
    .DisplayName = "死亡依赖消费者探针",
    .Version = "0.0.1",
    .Requires = {{.Name = "Vase.Test.Dead", .Version = 1}},
    .Provides = {},
};
