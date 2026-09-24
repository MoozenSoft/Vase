// 传递链第二跳：StrictUsed 被拆 → 它被拆。
#include "Vase/Plugin.h"

#include "../M2Common.h"

namespace
{
class BehindFarPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        Consumed = &ctx.Get<m2_fixture::IBehind2>(); // 落 StrictUsed → 本实例的账本边
        return vase::Result<void>::Ok();
    }

private:
    const m2_fixture::IBehind2* Consumed = nullptr; // 只承担消费者身份；指针不外用
};
} // namespace

VASE_PLUGIN(BehindFarPlugin){
    .Id = "Vase.BehindFar",
    .DisplayName = "传递链第二跳",
    .Version = "0.0.1",
    .Requires = {{.Name = "Vase.Test.Behind2", .Version = 1}},
    .Provides = {},
};
