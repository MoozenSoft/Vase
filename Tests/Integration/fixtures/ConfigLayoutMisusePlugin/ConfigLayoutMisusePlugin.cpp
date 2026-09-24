// 作者契约反例：读配置用错结构体（尺寸不符）→ ProgrammerError（D25 的布局校验）。
#include "Vase/Plugin.h"

#include <cstdint>

namespace
{

VASE_CONFIG(RealConfig, (std::int32_t, Echo, 1, vase::Meta{}));

struct WrongShape
{
    std::int64_t First = 0;
    std::int64_t Second = 0; // sizeof 与 RealConfig 不等
};

class ConfigLayoutMisusePlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx.Config<WrongShape>()); // 必炸：layout mismatch
        return vase::Result<void>::Ok();
    }
};

} // namespace

VASE_PLUGIN(ConfigLayoutMisusePlugin){
    .Id = "Vase.ConfigLayoutMisuse",
    .DisplayName = "配置布局误用探针",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {},
    .Config = vase::FieldsOf<RealConfig>(),
};
