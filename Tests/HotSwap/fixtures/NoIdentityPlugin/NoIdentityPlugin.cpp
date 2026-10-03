// 档三「特征缺失」探针（T11）：与 LoadProbe 同源，唯一的差别是**不带**自己的身份特征（三个平台
// 各摘各的，见 Tests/CMakeLists.txt）。CreatePod 不设身份闸（v2 语义），所以它照常装载；
// Adopt 的档三一读到「没有特征」就得拒绝并指路补标志。
//
// M4/D109：跨平台化并改名——它的存在理由是平台中性的「身份特征缺失」（Linux 无 .note.gnu.build-id /
// Windows 无 CodeView / macOS 无 LC_UUID）。
#include "Vase/Plugin.h"

#include <cstdint>
#include <string_view>

namespace
{

// 与 LoadProbe 同由来的真实 Pod 动作：让产物里留下 VasePod 的导入条目（Context::Emit<E>
// 调到 out-of-line 的 EmitRaw）——「真实插件二进制」这条前提对本探针同样承重，
// §8.7 的导入解析要有东西可读。
struct NoIdentityEvent
{
    static constexpr std::string_view kName = "Vase.NoIdentity.Loaded";
    static constexpr std::uint32_t kVersion = 1;
};

class NoIdentityPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Emit(NoIdentityEvent{});
        return vase::Result<void>::Ok();
    }
};

} // namespace

VASE_PLUGIN(NoIdentityPlugin){
    .Id = "Vase.NoIdentity",
    .DisplayName = "无身份特征探针",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {},
};
