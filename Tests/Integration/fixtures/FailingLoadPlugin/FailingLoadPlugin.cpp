// 装载失败探针：OnLoad 直接返回 Err。验的是 §0.3-4「失败不致命」——
// 坏插件落一条 Failed 记录，同局的其余插件照常 Started（下游级联拆除属 M2）。
#include "Vase/Plugin.h"

namespace
{

class FailingLoadPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Err(vase::Error{"intentional load failure"});
    }
};

} // namespace

VASE_PLUGIN(FailingLoadPlugin){
    .Id = "Vase.FailingLoad",
    .DisplayName = "装载失败探针",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {},
};
