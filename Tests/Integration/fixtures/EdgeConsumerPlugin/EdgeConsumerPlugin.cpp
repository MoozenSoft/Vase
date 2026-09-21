// 账本消费者探针（T9）：取一个插件提供的服务（应落一条边）+ 一个宿主提供的服务
// （宿主提供方，账本不记边）。宿主若没在 Stage0 注册 HostOnly，OnLoad 返回 Err——
// 那条「取到了插件服务、随后才失败」的路径正是 Failed 即时拆账的现场。
#include "Vase/Plugin.h"

#include "../SharedCommon.h"

namespace
{

class EdgeConsumerPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        // 插件 → 插件：命中即落账（§5.6 规则②）。引用由本实例持有到关停。
        Shared = &ctx.Get<samples_fixture::ISharedService>();
        // 宿主提供方：取得到即成功，但账本不记这条边（ProviderInstance 为空，§5.6）。
        HostOnly = ctx.TryGet<samples_fixture::IHostOnlyService>();
        if (HostOnly == nullptr)
        {
            return vase::Result<void>::Err(vase::Error{"host service missing"});
        }
        return vase::Result<void>::Ok();
    }

    // 两个引用只承担「消费者身份」的语义：真插件会拿它们干活，探针的判据落在账本上
    // （Get/TryGet 命中即落账），不在调用服务方法。
private:
    const samples_fixture::ISharedService* Shared = nullptr;
    const samples_fixture::IHostOnlyService* HostOnly = nullptr;
};

} // namespace

VASE_PLUGIN(EdgeConsumerPlugin){
    .Id = "Vase.EdgeConsumer",
    .DisplayName = "账本消费者探针",
    .Version = "0.0.1",
    .Requires = {{.Name = "Vase.Test.Shared", .Version = 1}, {.Name = "Vase.Test.HostOnly", .Version = 1}},
    .Provides = {},
};
