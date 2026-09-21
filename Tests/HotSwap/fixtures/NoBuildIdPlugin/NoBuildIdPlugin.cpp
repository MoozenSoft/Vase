// 档三「特征缺失」探针（T11，Linux only）：与 LoadProbe 同源，唯一的差别是**不带**
// .note.gnu.build-id（见 Tests/CMakeLists.txt 的 -Wl,--build-id=none）。CreatePod 不设
// 身份闸（v2 语义），所以它照常装载；Adopt 的档三一读到「没有特征」就得拒绝并指路补标志。
#include "Vase/Plugin.h"

#include <cstdint>
#include <string_view>

namespace
{

// 与 LoadProbe 同由来的真实 Pod 动作：让产物里留下 VasePod 的导入条目（Context::Emit<E>
// 调到 out-of-line 的 EmitRaw）——「真实插件二进制」这条前提对本探针同样承重，
// §8.7 的导入解析要有东西可读。
struct NoBuildIdEvent
{
    static constexpr std::string_view kName = "Vase.NoBuildId.Loaded";
    static constexpr std::uint32_t kVersion = 1;
};

class NoBuildIdPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Emit(NoBuildIdEvent{});
        return vase::Result<void>::Ok();
    }
};

} // namespace

VASE_PLUGIN(NoBuildIdPlugin){
    .Id = "Vase.NoBuildId",
    .DisplayName = "无建置 ID 探针",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {},
};
