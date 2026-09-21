// 双版本探针 A（T12 §12.1）：与 VersionedAPrime.cpp 只差一处服务返回值——
// 「同名、不同目录、运行时覆盖」的两个构建就由这一行区分。
#include "Vase/Plugin.h"

#include "../BCommon.h"

namespace
{

class CounterImpl final : public samples_fixture::ICounter
{
public:
    [[nodiscard]] int Value() const override { return 1; } // ← A′ 的唯一差异行：return 2;
};

class VersionedAPlugin final : public vase::Plugin
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

// VASE_PLUGIN 必须在**文件全局作用域**：它的导出符号是 extern "C"，放进 namespace 会
// 破掉 C 链接（上面的类进匿名命名空间即可）。
VASE_PLUGIN(VersionedAPlugin){
    .Id = "Vase.VersionedA",
    .DisplayName = "双版本探针",
    .Version = "1.0.0",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.Counter", .Version = 1}},
};
