// 换件谱 rung2（M4/D106）：与 VersionedAPrime 只差描述符的 `Version` 串（1.0.0 → 1.1.0），
// 行为同为 return 2。它是「描述符变了 ⇒ 期望必须跟着换」那一步的字节。
#include "Vase/Plugin.h"

#include "../BCommon.h"

namespace
{

class CounterImpl final : public samples_fixture::ICounter
{
public:
    [[nodiscard]] int Value() const override { return 2; } // 与 rung1 同行为：本步只漂版本戳
};

class VersionedAStampDriftPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<samples_fixture::ICounter>(Counter);
        return vase::Result<void>::Ok();
    }

private:
    CounterImpl Counter;
};

} // namespace

VASE_PLUGIN(VersionedAStampDriftPlugin){
    .Id = "Vase.VersionedA", // 同一个插件的下一版——Id 不变，换件语义才成立
    .DisplayName = "双版本探针",
    .Version = "1.1.0", // ← 与 rung1 的唯一差异
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.Counter", .Version = 1}},
};
