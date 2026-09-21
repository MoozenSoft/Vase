// 共享服务提供方探针（T9）：在插件子 Context 上 Provide 一个 kPlugin 来源的服务——
// 账本要记的正是「插件消费者 → 插件提供方」这一类边（§5.6 规则②）。
//
// 注册走**拥有型**重载（unique_ptr）：移交式注册的跨边界 delete 只在这一支上发生
// （外层适配器的 Recycle → DestroyHolder → 删除插件镜像内的实例），其余 fixture 走的
// 借用型重载根本不碰它。本 fixture 被 LedgerSemantics / Eject / Adopt 三条套件反复
// 跑到，把它挂在这里是让那条删除路径有常驻证人。
#include "Vase/Plugin.h"

#include <memory>

#include "../SharedCommon.h"

namespace
{

class SharedService final : public samples_fixture::ISharedService
{
public:
    [[nodiscard]] int Value() const override { return 42; }
};

class SharedProviderPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<samples_fixture::ISharedService>(std::make_unique<SharedService>());
        return vase::Result<void>::Ok();
    }
};

} // namespace

VASE_PLUGIN(SharedProviderPlugin){
    .Id = "Vase.SharedProvider",
    .DisplayName = "共享服务提供方探针",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.Shared", .Version = 1}},
};
