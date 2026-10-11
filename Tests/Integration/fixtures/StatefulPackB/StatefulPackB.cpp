// 组合库成员 B（VasePack 静态链入波证人 #7，spec §2.5/D236）：与 A 同住一张组合库、
// 两者都登记进程级状态。Id、状态名与 Reset 位点都与 A 各一份，让证人能钉住「两个都该重置」。
// 形态照 Tests/HotSwap/fixtures/StatefulPlugin/StatefulPlugin.cpp 复制改造。
#include "Vase/Plugin.h"

namespace
{

// 进程级状态：与 A 同机制、各持一份实体（§9.1），随镜像卸载而灭。
int& StatefulPackBLoads()
{
    static int statefulPackBLoads = 0;
    return statefulPackBLoads;
}

void ResetLoads() { StatefulPackBLoads() = 0; } // 幂等（§9.1 契约）

// 类名必须等于 target 名（D198，同 A）。
class StatefulPackB final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);                    // 本探针只数装载次数，不取服务、不落账
        static_cast<void>(++StatefulPackBLoads()); // 每次装载一次
        return vase::Result<void>::Ok();
    }
};

} // namespace

VASE_PLUGIN(StatefulPackB){
    .Id = "Vase.Test.StatefulPackB",
    .DisplayName = "组合库进程级状态探针 B",
    .Version = "1.0.0",
    .Requires = {},
    .Provides = {},
    .ProcessStates = {{.Name = "Vase.Test.StatefulPackB.Loads", .Reset = &ResetLoads}},
};
