// 组合库成员 A（VasePack 静态链入波证人 #7，spec §2.5/D236）：与 B 同住一张组合库、两者都登记
// 进程级状态——后者是 MakeInstance「共享 BinaryRecord 上后写覆盖先写」的可证伪现场（Eject 只重置
// 最后装配那一个成员）。形态照 Tests/HotSwap/fixtures/StatefulPlugin/StatefulPlugin.cpp 复制改造。
#include "Vase/Plugin.h"

namespace
{

// 进程级状态：跨 Pod、跨装卸存活（§9.1），随镜像卸载而灭。
// 实体是函数局部 static 而非文件作用域变量（同 EdgeConsumerPlugin.cpp），
// 这样包一层以避 cppcoreguidelines-avoid-non-const-global-variables。
int& StatefulPackALoads()
{
    static int statefulPackALoads = 0;
    return statefulPackALoads;
}

void ResetLoads() { StatefulPackALoads() = 0; } // 幂等（§9.1 契约）

// 类名必须等于 target 名：vase_add_plugin_fixture 无条件设 VASE_PLUGIN_CLASS "${name}"，
// 分发表据此拼 VasePluginDesc_<类名>（D198）。
class StatefulPackA final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);                    // 本探针只数装载次数，不取服务、不落账
        static_cast<void>(++StatefulPackALoads()); // 每次装载一次
        return vase::Result<void>::Ok();
    }
};

} // namespace

VASE_PLUGIN(StatefulPackA){
    .Id = "Vase.Test.StatefulPackA",
    .DisplayName = "组合库进程级状态探针 A",
    .Version = "1.0.0",
    .Requires = {},
    .Provides = {},
    .ProcessStates = {{.Name = "Vase.Test.StatefulPackA.Loads", .Reset = &ResetLoads}},
};
