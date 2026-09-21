// 未声明解析探针（T9）：Requires 为空却在 OnLoad 里 Get——§5.6 规则① + §6.2 编程错误，
// 两个构建都终止。它守的是「未声明 → 无账本可查的隐藏边」这条执法线。
#include "Vase/Plugin.h"

#include "../SharedCommon.h"

namespace
{

class UndeclaredGetConsumer final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        // 没在 Requires 里声明就取：步骤①（凭声明）先于步骤②（查表），所以无论有没有
        // 提供方，这里都走 ReportUndeclaredResolution 终止。
        static_cast<void>(ctx.Get<samples_fixture::ISharedService>());
        return vase::Result<void>::Ok();
    }
};

} // namespace

VASE_PLUGIN(UndeclaredGetConsumer){
    .Id = "Vase.UndeclaredGet",
    .DisplayName = "未声明解析探针",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {},
};
