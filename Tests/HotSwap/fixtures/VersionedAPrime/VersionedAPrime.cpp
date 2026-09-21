// 双版本探针 A′（T12 §12.1）：与 VersionedA.cpp 只差一处服务返回值——CMake 侧
// OUTPUT_NAME 同为 VersionedA，故两者是「同名不同目录」的两个产物。
#include "Vase/Plugin.h"

#include "../BCommon.h"

namespace
{

class CounterImpl final : public samples_fixture::ICounter
{
public:
    [[nodiscard]] int Value() const override { return 2; } // ← 与 A 的唯一差异行（A 是 return 1）
};

class VersionedAPrimePlugin final : public vase::Plugin
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

VASE_PLUGIN(VersionedAPrimePlugin){
    .Id = "Vase.VersionedA",
    .DisplayName = "双版本探针",
    .Version = "1.0.0",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.Counter", .Version = 1}},
};
