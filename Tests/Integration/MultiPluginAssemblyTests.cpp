#include "AdoptExpectations.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Pod.h"

#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

vase::LoadPlan Plan(std::initializer_list<std::pair<std::string_view, std::filesystem::path>> items)
{
    vase::LoadPlan plan;
    for (const auto& [id, path] : items)
    {
        plan.Ordered.push_back({.Id = id, .BinaryPath = path});
    }
    return plan;
}

// 服务标识与 fixture 同源（测试材料，不是公开 API）：本任务只用到 SharedCommon 的两个。
#include "../Integration/fixtures/SharedCommon.h"

// 跳过记录按处理序落账，但环用例要按 Id 分别校验——小查找器，找不到给 nullptr 由调用方 ASSERT。
const vase::SkippedRecord* FindSkip(const vase::Pod& pod, std::string_view id)
{
    const auto found = std::find_if(pod.Skips().begin(), pod.Skips().end(),
                                    [id](const vase::SkippedRecord& record) { return record.Id == id; });
    return found == pod.Skips().end() ? nullptr : &*found;
}

class HostMarker final : public samples_fixture::IHostOnlyService
{
public:
    [[nodiscard]] int Marker() const override { return 1; }
};

TEST(MultiPluginAssembly, DuplicateIdRejectedBeforeAnySideEffect)
{
    vase::PluginHost host;
    const vase::Result<vase::PodHandle> r =
        host.CreatePod(Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}, {"Vase.Hello", VASE_FIXTURE_SHAREDPROVIDER}}));
    ASSERT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("duplicate plugin id"), std::string::npos);
    const auto& counters = host.ForTestCounters(); // 真·零副作用：五项全 0，没有半个镜像被读、没有半条注册
    EXPECT_EQ(counters.PluginInstances, 0U);
    EXPECT_EQ(counters.Effects, 0U);
    EXPECT_EQ(counters.Services, 0U);
    EXPECT_EQ(host.Resolve(vase::PodHandle{.Index = 0, .Generation = 1}), nullptr); // 槽根本没被占
}

TEST(MultiPluginAssembly, StaticSkipRecordedAndAdoptableLater)
{
    // kSkip 条目：不进局、如实记进报告、路径照注册（D30）——之后可被 Adopt 捞回。
    vase::PluginHost host;
    vase::LoadPlan plan = Plan({{"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER}});
    plan.Ordered.push_back({
        .Id = "Vase.EdgeConsumer",
        .BinaryPath = VASE_FIXTURE_EDGECONSUMER,
        .Decision = vase::LoadDecision::kSkip,
        .Reason = vase::SkipReason::kDisabled,
    });
    HostMarker marker;
    vase::PodOptions options;
    options.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::IHostOnlyService>(marker); };
    const vase::PodHandle h = host.CreatePod(plan, options).Value();

    vase::Pod* pod = host.Resolve(h);
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(pod->PluginCount(), 1U); // 只进局了 provider
    ASSERT_EQ(pod->Skips().size(), 1U);
    EXPECT_EQ(pod->Skips().begin()->Class, vase::SkipClass::kStatic);
    EXPECT_EQ(pod->Skips().begin()->Id, "Vase.EdgeConsumer");
    EXPECT_NE(pod->Skips().begin()->Cause.find("disabled"), std::string::npos);

    const vase::ManifestExpectation edge = testing_support::MakeEdgeConsumerExpectation();
    vase::AdoptRequest edgeRequest;
    edgeRequest.Id = edge.Id;
    edgeRequest.BinaryPath = VASE_FIXTURE_EDGECONSUMER; // 路径随请求自带（D71），不再查计划注册
    edgeRequest.Expected = &edge;
    ASSERT_TRUE(host.AdoptPlugin(h, edgeRequest).IsOk()); // 当初为什么没进局，Adopt 不关心（D30）
    EXPECT_EQ(host.Resolve(h)->PluginCount(), 2U);
    const vase::PodReport report = host.DestroyPod(h);
    EXPECT_TRUE(report.Clean());        // D38：Skips 不进 Clean
    ASSERT_EQ(report.Skips.size(), 1U); // 静态记录随报告带出
    EXPECT_EQ(report.Skips.begin()->Class, vase::SkipClass::kStatic);
}

TEST(MultiPluginAssembly, StrictIgnoresStaticSkips)
{
    // D36 的静态侧：Strict 只看 Failed。kSkip 不触发整局失败（运行时侧在 T5 补）。
    vase::PluginHost host;
    vase::LoadPlan plan = Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}});
    plan.Ordered.push_back({
        .Id = "Vase.SharedProvider",
        .BinaryPath = VASE_FIXTURE_SHAREDPROVIDER,
        .Decision = vase::LoadDecision::kSkip,
    });
    vase::PodOptions options;
    options.Strict = true;
    const vase::Result<vase::PodHandle> r = host.CreatePod(plan, options);
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    host.DestroyPod(r.Value());
}

TEST(MultiPluginAssembly, SkipsDoNotEnterLiveSetOrClean)
{
    // D38 钉死：跳过既不留资源也不进活集合——Clean() 与 PluginIds 都不为它变脸。
    vase::PluginHost host;
    vase::LoadPlan plan = Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}});
    plan.Ordered.push_back(
        {.Id = "Vase.StaleHeader", .BinaryPath = VASE_FIXTURE_STALEHEADER, .Decision = vase::LoadDecision::kSkip});
    const vase::PodHandle h = host.CreatePod(plan).Value();
    const vase::Pod* pod = host.Resolve(h);
    EXPECT_EQ(pod->PluginIds(), std::vector<std::string>{"Vase.Hello"});
    EXPECT_FALSE(pod->HasPlugin("Vase.StaleHeader"));
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(MultiPluginAssembly, DuplicateIdRejectsSkipMixed)
{
    // D39「含 kSkip」那一半：查重看的是**整个计划**，不按 Decision 豁免——同 Id 的
    // kLoad + kSkip 一样是路径覆盖 + 反查歧义，命中即整局 Err、真·零副作用。
    vase::PluginHost host;
    vase::LoadPlan plan = Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}});
    plan.Ordered.push_back(
        {.Id = "Vase.Hello", .BinaryPath = VASE_FIXTURE_SHAREDPROVIDER, .Decision = vase::LoadDecision::kSkip});
    const vase::Result<vase::PodHandle> r = host.CreatePod(plan);
    ASSERT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("duplicate plugin id"), std::string::npos);
    EXPECT_EQ(host.ForTestCounters().PluginInstances, 0U);                          // kSkip 条目也没被豁免出去加载
    EXPECT_EQ(host.Resolve(vase::PodHandle{.Index = 0, .Generation = 1}), nullptr); // 槽根本没被占
}

TEST(MultiPluginAssembly, LoadCascadeAttributesToFailedProvider)
{
    // D41「提供者已死」支：DeadProvider Failed → DeadConsumer 预检跳过且点名它。
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(Plan({
                                                 {"Vase.DeadProvider", VASE_FIXTURE_DEADPROVIDER},
                                                 {"Vase.DeadConsumer", VASE_FIXTURE_DEADCONSUMER},
                                             }))
                                  .Value();
    const vase::Pod* pod = host.Resolve(h);
    EXPECT_EQ(pod->PluginCount(), 0U);
    ASSERT_EQ(pod->Failures().size(), 1U);
    EXPECT_EQ(pod->Failures().begin()->Id, "Vase.DeadProvider");
    ASSERT_EQ(pod->Skips().size(), 1U);
    const vase::SkippedRecord& skipped = *pod->Skips().begin();
    EXPECT_EQ(skipped.Class, vase::SkipClass::kRuntime);
    EXPECT_EQ(skipped.CausedBy, "Vase.DeadProvider"); // 级联有名（§4.4 编辑器形态）
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(MultiPluginAssembly, BadOrderSkipsWithNoCauseNamed)
{
    // 序缺陷支（R5-1）：消费者在前、健康提供者在后 → 预检跳过且 CausedBy 空（不能赖给没轮到的人）。
    // Stage0 在场 HostOnly，miss 因此落在 Requires[0] 的 Vase.Test.Shared 上——纯序缺陷形态。
    vase::PluginHost host;
    HostMarker marker;
    vase::PodOptions options;
    options.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::IHostOnlyService>(marker); };
    const vase::PodHandle h = host.CreatePod(Plan({
                                                 {"Vase.EdgeConsumer", VASE_FIXTURE_EDGECONSUMER},
                                                 {"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER},
                                             }),
                                             options)
                                  .Value();
    const vase::Pod* pod = host.Resolve(h);
    EXPECT_EQ(pod->PluginCount(), 1U); // provider 自己无依赖，照常进局
    ASSERT_EQ(pod->Skips().size(), 1U);
    EXPECT_EQ(pod->Skips().begin()->Id, "Vase.EdgeConsumer");
    EXPECT_NE(pod->Skips().begin()->Cause.find("Vase.Test.Shared"), std::string::npos);
    EXPECT_TRUE(pod->Skips().begin()->CausedBy.empty());
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(MultiPluginAssembly, CycleDegradesToMutualSkips)
{
    // D40：环不检测——互缺互跳、不挂不炸；报告里没有「cycle」字样（点名归 M2b Solve）。
    vase::PluginHost host;
    const vase::PodHandle h =
        host.CreatePod(Plan({{"Vase.CycleA", VASE_FIXTURE_CYCLEA}, {"Vase.CycleB", VASE_FIXTURE_CYCLEB}})).Value();
    const vase::Pod* pod = host.Resolve(h);
    EXPECT_EQ(pod->PluginCount(), 0U);
    EXPECT_EQ(pod->Failures().size(), 0U);
    EXPECT_EQ(pod->Skips().size(), 2U);
    // 归因不对称钉死（R5-4）：环里被点名者同为跳过态——D41 粗粒度归因（已处理即在账上）的
    // 刻意行为，非死亡链。A 先被检：账上唯一的提供者是自己，自豁免 → 空。
    const vase::SkippedRecord* skipA = FindSkip(*pod, "Vase.CycleA");
    const vase::SkippedRecord* skipB = FindSkip(*pod, "Vase.CycleB");
    ASSERT_NE(skipA, nullptr);
    ASSERT_NE(skipB, nullptr);
    EXPECT_TRUE(skipA->CausedBy.empty());
    EXPECT_EQ(skipB->CausedBy, "Vase.CycleA"); // B 后跳：A 已在账（跳过态也算）
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(MultiPluginAssembly, StrictIgnoresRuntimeSkipsButFailsOnRealFailure)
{
    // D36 运行时侧：Strict 下「只有跳过」照常交付（上面 BadOrder 的 Strict 化断言）；
    // 「有 Failed」才整局 Err——两个形态钉在同一条用例里，读的人不必在两条测试之间拼语义。
    {
        vase::PluginHost host;
        HostMarker marker;
        vase::PodOptions options;
        options.Strict = true;
        options.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::IHostOnlyService>(marker); };
        const vase::Result<vase::PodHandle> r = host.CreatePod(Plan({
                                                                   {"Vase.EdgeConsumer", VASE_FIXTURE_EDGECONSUMER},
                                                                   {"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER},
                                                               }),
                                                               options);
        ASSERT_TRUE(r.IsOk()) << r.GetError().Message(); // 健康对只撞序缺陷：跳过不是失败
        EXPECT_TRUE(host.DestroyPod(r.Value()).Clean());
    }
    {
        vase::PluginHost host;
        vase::PodOptions options;
        options.Strict = true;
        const vase::Result<vase::PodHandle> r = host.CreatePod(
            Plan({{"Vase.DeadProvider", VASE_FIXTURE_DEADPROVIDER}, {"Vase.DeadConsumer", VASE_FIXTURE_DEADCONSUMER}}),
            options);
        ASSERT_FALSE(r.IsOk()); // provider Failed → 整局 Err（预检跳过随半局一起丢）
        EXPECT_NE(r.GetError().Message().find("intentional provider failure"), std::string::npos);
    }
}

TEST(MultiPluginAssembly, ProvidesCollisionKillsWholePodWithoutRegistering)
{
    // D33 ②：两个 kLoad 条目抢同一服务 → 整局 Err。注册表/实例双清零（镜像驻留残留 = Strict 先例，
    // 计数为 0 即「Pod 视角什么都没发生」）。两个方向各钉一次：检出在第二个被装载者。
    for (const bool firstIsShared : {true, false})
    {
        vase::PluginHost host;
        const vase::LoadPlan plan = Plan({
            firstIsShared ? std::pair{"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER}
                          : std::pair{"Vase.CollisionProvider", VASE_FIXTURE_COLLISIONPROVIDER},
            firstIsShared ? std::pair{"Vase.CollisionProvider", VASE_FIXTURE_COLLISIONPROVIDER}
                          : std::pair{"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER},
        });
        const vase::Result<vase::PodHandle> r = host.CreatePod(plan);
        ASSERT_FALSE(r.IsOk()) << "order firstIsShared=" << firstIsShared;
        EXPECT_NE(r.GetError().Message().find("provides collision Vase.Test.Shared@1"), std::string::npos);
        // 归因点名的是插件（登记账臂先行），不是注册表臂的兜底 "host"——两臂的分工从此可分辨。
        EXPECT_NE(r.GetError().Message().find("claimed by Vase."), std::string::npos);
        const auto& counters = host.ForTestCounters();
        EXPECT_EQ(counters.PluginInstances, 0U); // 被拆干净：实例层零残留
        EXPECT_EQ(counters.Services, 0U);
        EXPECT_EQ(counters.Effects, 0U);
        EXPECT_EQ(counters.Scopes, 0U);
    }
}

TEST(MultiPluginAssembly, HostProvidedServiceIsAlsoCollisionTarget)
{
    // 碰撞登记账 ∪ 注册表双查的后半：宿主 Stage0 提供的同名服务也是碰撞对象。
    struct SharedMarker final : public samples_fixture::ISharedService
    {
        [[nodiscard]] int Value() const override { return 0; }
    };

    vase::PluginHost host;
    SharedMarker marker; // 借用注册：对象必须活过整局（§5.3 阶段 0 的既有契约）
    vase::PodOptions options;
    options.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::ISharedService>(marker); };
    const vase::Result<vase::PodHandle> r =
        host.CreatePod(Plan({{"Vase.CollisionProvider", VASE_FIXTURE_COLLISIONPROVIDER}}), options);
    ASSERT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("claimed by host"), std::string::npos);
    EXPECT_EQ(host.ForTestCounters().Services, 0U); // 半局的宿主注册随根 Scope 一起拆净
}

TEST(MultiPluginAssembly, StaticSkippedCollisionFixtureIsNotACollisionTarget)
{
    // D39 分治的另一半：Id 唯一含 kSkip（上面已钉），Provides 碰撞只查 kLoad——跳过者不 inspect、
    // 不在账，与它抢同一服务的健康计划照常交付（§4.2 ②「第二个从未轮到」支的静态形态）。
    vase::PluginHost host;
    vase::LoadPlan plan = Plan({{"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER}});
    plan.Ordered.push_back({
        .Id = "Vase.CollisionProvider",
        .BinaryPath = VASE_FIXTURE_COLLISIONPROVIDER,
        .Decision = vase::LoadDecision::kSkip,
    });
    const vase::Result<vase::PodHandle> r = host.CreatePod(plan);
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    vase::Pod* pod = host.Resolve(r.Value());
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(pod->PluginCount(), 1U); // 只有正主进局：-1 实现无从可观测
    ASSERT_EQ(pod->Skips().size(), 1U);
    EXPECT_EQ(pod->Skips().begin()->Id, "Vase.CollisionProvider");
    EXPECT_EQ(host.ForTestCounters().Services, 1U); // 在册的 Shared@1 只有 42 那一份
    EXPECT_TRUE(host.DestroyPod(r.Value()).Clean());
}

TEST(MultiPluginAssembly, RuntimeSkippedProviderStillCollides)
{
    // R8-1 正形证人：DeadProvider2 在 (a') 入账、(b) 被永缺依赖运行时跳过（从不在注册表）——
    // 声明级不变量不看存活态，后到的 DeadProvider 仍撞它，整局 Err 且点名跳过者。
    vase::PluginHost host;
    const vase::Result<vase::PodHandle> r = host.CreatePod(
        Plan({{"Vase.DeadProvider2", VASE_FIXTURE_DEADPROVIDER2}, {"Vase.DeadProvider", VASE_FIXTURE_DEADPROVIDER}}));
    ASSERT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("provides collision Vase.Test.Dead@1 claimed by Vase.DeadProvider2"),
              std::string::npos);
    EXPECT_EQ(host.ForTestCounters().PluginInstances, 0U); // 两站都没建实例：整局真·零残留
    EXPECT_EQ(host.ForTestCounters().Services, 0U);
}

} // namespace
