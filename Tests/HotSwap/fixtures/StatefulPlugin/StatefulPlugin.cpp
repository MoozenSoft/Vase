// 进程级状态探针（M3/D91）：OnLoad 递增一个**跨 Pod 存活**的静态计数，Reset 归零。
// 它是 3d 的行为证人——「Eject 后再 Adopt 读数回到 1」只有在 Reset 真被调用时才成立。
#include "Vase/Plugin.h"

#include "../BCommon.h"

namespace
{

// 进程级状态：跨 Pod、跨装卸存活（§9.1），随镜像卸载而灭。
// 实体是函数局部 static 而非文件作用域变量：同 EdgeConsumerPlugin.cpp，
// 这样包一层以避 cppcoreguidelines-avoid-non-const-global-variables。
int& StatefulLoads()
{
    static int statefulLoads = 0;
    return statefulLoads;
}

void ResetLoads() { StatefulLoads() = 0; } // 幂等（§9.1 契约）

class StateProbeImpl final : public samples_fixture::IStateProbe
{
public:
    [[nodiscard]] int Loads() const override { return StatefulLoads(); }
};

class StatefulPluginImpl final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(++StatefulLoads()); // 每次装载一次
        ctx.Provide<samples_fixture::IStateProbe>(Probe);
        return vase::Result<void>::Ok();
    }

private:
    StateProbeImpl Probe;
};

} // namespace

VASE_PLUGIN(StatefulPluginImpl){
    .Id = "Vase.Stateful",
    .DisplayName = "进程级状态探针",
    .Version = "1.0.0",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.StateProbe", .Version = 1}},
    .ProcessStates = {{.Name = "Vase.Test.StateProbe.Loads", .Reset = &ResetLoads}},
};
