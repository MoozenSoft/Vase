// strict 声明且解析过（T7 四形态之一）：OnLoad 的 Get 落账本边，并中继提供 Behind2（传递链第一跳）。
#include "Vase/Plugin.h"

#include "../M2Common.h"

namespace
{
class Behind2Impl final : public m2_fixture::IBehind2
{
};
class BehindStrictUsedPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        Consumed = &ctx.Get<m2_fixture::IBehind>(); // 落一条账本边（也是声明边，双保险命中）
        ctx.Provide<m2_fixture::IBehind2>(Second);
        return vase::Result<void>::Ok();
    }

private:
    Behind2Impl Second;
    const m2_fixture::IBehind* Consumed = nullptr; // 只承担消费者身份；指针不外用
};
} // namespace

VASE_PLUGIN(BehindStrictUsedPlugin){
    .Id = "Vase.BehindStrictUsed",
    .DisplayName = "strict 声明且用过",
    .Version = "0.0.1",
    .Requires = {{.Name = "Vase.Test.Behind", .Version = 1}},
    .Provides = {{.Name = "Vase.Test.Behind2", .Version = 1}},
};
