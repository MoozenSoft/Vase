// 缺依赖消费者探针：声明了 Requires 却全场无人提供 → 走 §6.2 的 required-miss
// 终止路径（消息含插件 Id + 服务名 + 版本）。M1 无求解器，这类「依赖不满足」本该在
// 求解期被拒；这里证明的是**运行期最后一道墙**是响的（§12 #3b 的「缺失要响」那一半）。
#include "Vase/Plugin.h"

#include <cstdint>
#include <string_view>

namespace
{

// 接口只声明标识：本探针从不 Provide 它，故不需要虚函数（加了虚析构反而要
// 补全四个特殊成员——cppcoreguidelines-special-member-functions）。
struct IGhostService
{
    static constexpr std::string_view kName = "Vase.Ghost";
    static constexpr std::uint32_t kVersion = 1;
};

class RequiresMissingConsumer final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx.Get<IGhostService>()); // 声明过（见下 Requires），但没有人 Provide
        return vase::Result<void>::Ok();
    }
};

} // namespace

VASE_PLUGIN(RequiresMissingConsumer){
    .Id = "Vase.RequiresMissingConsumer",
    .DisplayName = "缺依赖消费者探针",
    .Version = "0.0.1",
    .Requires = {{.Name = "Vase.Ghost", .Version = 1}},
    .Provides = {},
};
