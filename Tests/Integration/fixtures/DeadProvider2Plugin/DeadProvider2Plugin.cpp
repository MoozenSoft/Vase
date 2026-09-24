// 第二个 Vase.Test.Dead 提供者（T8 补充）：声明在 (a') 入账、(b) 处因永缺依赖被运行时跳过——
// R8-1「跳过者也算碰撞对象」（声明级不变量）的证人。
#include "Vase/Plugin.h"

namespace
{
class DeadProvider2Plugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Ok(); // 正常路径根本到不了（预检先跳）
    }
};
} // namespace

VASE_PLUGIN(DeadProvider2Plugin){
    .Id = "Vase.DeadProvider2",
    .DisplayName = "跳过提供者探针",
    .Version = "0.0.1",
    .Requires = {{.Name = "Vase.Nowhere", .Version = 1}},
    .Provides = {{.Name = "Vase.Test.Dead", .Version = 1}},
};
