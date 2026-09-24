#include "Vase/Host/PluginHost.h"

#include "Vase/Detail/Result.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Pod/Pod.h"
#include "fixtures/SharedCommon.h"

#include <gtest/gtest.h>

namespace
{

// 宿主标记服务：Stage0 在根 Context 上以 IHostOnlyService 的标识注册它。插件端在**另一个
// DLL** 里用同一个标识取回——解析走字符串 kName，不走 type_index（§6.1 的兑现点）。
class HostMarker final : public samples_fixture::IHostOnlyService
{
public:
    [[nodiscard]] int Marker() const override { return 7; }
};

vase::LoadPlan SharedAndConsumerPlan()
{
    // 手写拓扑：提供方在前、消费方在后——§5.3 阶段 1「依赖就绪」的前提。
    vase::LoadPlan plan;
    plan.Ordered.push_back({.Id = "Vase.SharedProvider", .BinaryPath = VASE_FIXTURE_SHAREDPROVIDER});
    plan.Ordered.push_back({.Id = "Vase.EdgeConsumer", .BinaryPath = VASE_FIXTURE_EDGECONSUMER});
    return plan;
}

vase::PodOptions Stage0WithHostMarker(HostMarker& marker)
{
    vase::PodOptions options;
    options.Stage0 = [&marker](vase::Context& root) { root.Provide<samples_fixture::IHostOnlyService>(marker); };
    return options;
}

TEST(LedgerSemantics, PluginResolutionRecordsHostResolutionDoesNot)
{
    // §5.6 规则②：插件的解析逐条落账，宿主提供方向不落。EdgeConsumer 取 Shared（插件提供，
    // 落一条）与 HostOnly（宿主提供，不落）各一次——账本上只该有前者那一条。
    vase::PluginHost host;
    HostMarker marker;
    const vase::Result<vase::PodHandle> created = host.CreatePod(SharedAndConsumerPlan(), Stage0WithHostMarker(marker));
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
    const vase::Pod* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);

    // 两个 OnLoad 都得成功：Consumer 取不到宿主服务会 Err → Failed → 它已落的边被即时摘掉，
    // 下面那个 1 就成 0。这条同时证明「宿主提供、插件消费」跨 DLL 真的走通了。
    ASSERT_EQ(pod->Failures().size(), 0U);
    EXPECT_EQ(pod->PluginCount(), 2U);

    // 1 = 只有 plugin→plugin 那条。0 = 解析根本没落账；2 = 宿主提供方向也落了边
    // （§5.6「宿主的解析不落边」被破坏）。
    EXPECT_EQ(host.ForTestLedgerEdgeCount(), 1U);
    host.DestroyPod(created.Value());
}

TEST(LedgerSemantics, DestroyPodClearsLedger)
{
    // §5.6「活集合→空」：拆局即归零。不清则槽位复用让上一局的边拦下一局的 Eject。
    vase::PluginHost host;
    HostMarker marker;
    const vase::Result<vase::PodHandle> created = host.CreatePod(SharedAndConsumerPlan(), Stage0WithHostMarker(marker));
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
    ASSERT_EQ(host.ForTestLedgerEdgeCount(), 1U); // 先证明这一局真落了账，否则下面的 0 是废话
    host.DestroyPod(created.Value());
    EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U);
}

TEST(LedgerSemantics, FailedConsumerDropsItsEdgesImmediately)
{
    // §5.2 即时回收 × §5.6 规则②：失败实例的边当场摘净。判据必须在 **DestroyPod 之前**
    // 取——ClearPod 到拆局才生效，兜不住这一条（T9 实施者用临时探针实测：摘掉那处
    // RemoveByInstance，这里的 count 停在 1）。死边不摘会以「还有消费者指着你」的
    // 形态拦住 T10 的 Eject 反查，所以这是承重覆盖，不是补白。
    vase::PluginHost host;
    // 消费者换成落边即败探针（R5-2）：T5 预检后，HostOnly 缺席的形态在 OnLoad 之前就被拦成跳过，
    // 造不出「边已落、随后败」的现场——这根证桩必须自带这条路径。
    vase::LoadPlan plan;
    plan.Ordered.push_back({.Id = "Vase.SharedProvider", .BinaryPath = VASE_FIXTURE_SHAREDPROVIDER});
    plan.Ordered.push_back({.Id = "Vase.FailingEdgeConsumer", .BinaryPath = VASE_FIXTURE_FAILINGEDGECONSUMER});
    const vase::Result<vase::PodHandle> created = host.CreatePod(plan);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
    const vase::Pod* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(pod->Failures().size(), 1U);        // 消费者 OnLoad 失败（§5.2 记录保留）
    EXPECT_EQ(pod->PluginCount(), 1U);            // 提供方仍在
    EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U); // 那条 consumer→provider 边随实例一起死
    // 再补一次拆局：Debug 的 ~PluginHost 断言「活 Pod 表为空 + 五项计数归零」，不拆就是硬失败；
    // 它顺带把「失败实例没留半个 Scope」也钉住（Clean）。
    EXPECT_TRUE(host.DestroyPod(created.Value()).Clean());
}
// 覆盖的是 OnLoad 失败那处；OnStart 失败那处（PluginHost.cpp 阶段 2）是同一形状的镜像，
// 要单独覆盖得再加一个「先解析成功、再 OnStart 失败」的 fixture——M1 不做，登记在案。

TEST(LedgerSemanticsDeath, UndeclaredResolutionTerminates)
{
    // §5.6 规则① + §6.2：没在 Requires 里声明就 Get = 编程错误，两个构建都终止。
    EXPECT_DEATH(
        {
            vase::PluginHost host;
            vase::LoadPlan plan;
            plan.Ordered.push_back({.Id = "Vase.UndeclaredGet", .BinaryPath = VASE_FIXTURE_UNDECLAREDGET});
            static_cast<void>(host.CreatePod(plan));
        },
        // 锚 T5 的实发消息（Context.cpp ReportUndeclaredResolution）：三要素齐全，且尾部留一个
        // 空格——否则 `.*1` 那类松尾会命中 ProgrammerError 追加的 ` (file:line)`（R86 教训）。
        "plugin 'Vase\\.UndeclaredGet' resolved service 'Vase\\.Test\\.Shared' v1 ");
}

} // namespace
