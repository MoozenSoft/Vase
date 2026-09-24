// 落边即败探针（T5 R5-2）：strict 需 Shared（装配期可满足、预检放行），OnLoad 先 Get 落边再 Err——
// FailedConsumerDropsItsEdgesImmediately 的承重现场：边先落、失败后当场摘（§5.2 × §5.6 规则②）。
#include "Vase/Plugin.h"

#include "../SharedCommon.h"

namespace
{

class FailingEdgeConsumerPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        Shared = &ctx.Get<samples_fixture::ISharedService>(); // 声明过且在场：落一条边
        return vase::Result<void>::Err(vase::Error{"edge taken then failure"});
    }

private:
    const samples_fixture::ISharedService* Shared = nullptr; // 只承担消费者身份，判据在账本上
};

} // namespace

VASE_PLUGIN(FailingEdgeConsumerPlugin){
    .Id = "Vase.FailingEdgeConsumer",
    .DisplayName = "落边即败探针",
    .Version = "0.0.1",
    .Requires = {{.Name = "Vase.Test.Shared", .Version = 1}},
    .Provides = {},
};
