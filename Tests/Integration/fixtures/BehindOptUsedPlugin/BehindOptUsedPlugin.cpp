// optional 声明 + 解析过 → 账本边命中波及（D28：指针已经交出去了）。
#include "Vase/Plugin.h"

#include "../M2Common.h"

namespace
{
class BehindOptUsedPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        Consumed = ctx.TryGet<m2_fixture::IBehind>(); // 注册表里有 → 命中即落账本边
        return vase::Result<void>::Ok();
    }

private:
    const m2_fixture::IBehind* Consumed = nullptr; // 只承担消费者身份；指针不外用
};
} // namespace

VASE_PLUGIN(BehindOptUsedPlugin){
    .Id = "Vase.BehindOptUsed",
    .DisplayName = "optional 声明且用过",
    .Version = "0.0.1",
    .Requires = {},
    .OptionalRequires = {{.Name = "Vase.Test.Behind", .Version = 1}},
    .Provides = {},
};
