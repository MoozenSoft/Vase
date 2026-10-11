#include "Vase/Plugin.h"

namespace
{

// 保活探针（spec §5.2）：**不列入** vase_pack_plugins 的 PLUGINS——它的对象因此不该被静态链接器
// 提取，描述符符号在见证可执行里应当是**未定义**的。这正是 §8.6「静态库按需提取、无人引用即
// 丢弃」那句可证伪的一半。类住匿名命名空间取内部链接（同 StatefulPackA），只在本 TU 内被按名取用。
class KeepAliveProbePlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Ok();
    }
};

} // namespace

VASE_PLUGIN(KeepAliveProbePlugin){
    .Id = "Vase.Test.KeepAliveProbe",
    .DisplayName = "KeepAliveProbe",
    .Version = "1.0.0",
    // Requires 与 Provides 都无 NSDMI，必须显式写（-Wmissing-designated-field-initializers = /WX 下 error）。
    .Requires = {},
    .Provides = {},
};
