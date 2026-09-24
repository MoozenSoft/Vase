// 第二消费者探针（T9）：与 EdgeConsumer 抢同一个提供方——点名的拒绝必须是清单，不是单点。
#include "Vase/Plugin.h"

#include "../SharedCommon.h"

namespace
{

class SharedConsumer2Plugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        // strict 声明 + 解析 → 落一条入边（凭声明执法，§5.6 规则①）。
        Shared = &ctx.Get<samples_fixture::ISharedService>();
        return vase::Result<void>::Ok();
    }

private:
    const samples_fixture::ISharedService* Shared = nullptr; // 只承担消费者身份；指针不外用
};

} // namespace

VASE_PLUGIN(SharedConsumer2Plugin){
    .Id = "Vase.SharedConsumer2",
    .DisplayName = "第二消费者探针",
    .Version = "0.0.1",
    .Requires = {{.Name = "Vase.Test.Shared", .Version = 1}},
    .Provides = {},
};
