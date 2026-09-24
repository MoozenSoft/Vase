#include "Vase/Host/PluginHost.h"

#include "../Integration/fixtures/SharedCommon.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Evidence.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Pod/Context.h"
#include "Vase/Pod/Pod.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace
{

// 宿主标记服务：与 T9 的 LedgerSemanticsTests 同形、同理由的**文件内**副本——
// per-file 测试助手不抽公共头（预检裁定 R2）。
class HostMarker final : public samples_fixture::IHostOnlyService
{
public:
    [[nodiscard]] int Marker() const override { return 7; }
};

vase::LoadPlan Plan(std::initializer_list<std::pair<std::string_view, std::filesystem::path>> items)
{
    vase::LoadPlan plan;
    for (const auto& [id, path] : items)
    {
        plan.Ordered.push_back({.Id = id, .BinaryPath = path});
    }
    return plan;
}

TEST(Eject, LeafPluginLeavesWithFullEvidence)
{
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}})).Value();
    const vase::Result<vase::EjectReport> r = host.EjectPlugin(h, "Vase.Hello");
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    const vase::EjectReport& rep = r.Value();
    EXPECT_TRUE(rep.LedgerHadNoIncomingEdges && rep.ScopeEmptied && rep.CrossPodInstancesZeroed); // 档一全绿
    EXPECT_TRUE(rep.BinaryActuallyUnloaded);
#ifdef _WIN32
    EXPECT_TRUE(rep.ReopenWritableIsMeaningful && rep.ReopenWritable); // 档二 · Win 主判（辅助地位）
#else
    EXPECT_TRUE(rep.MappingRemovalIsObservable && rep.MappingRemoved); // 档二 · Linux 主判
#endif
    EXPECT_EQ(host.Resolve(h)->PluginCount(), 0U);
    const vase::PodReport podReport = host.DestroyPod(h);
    EXPECT_TRUE(podReport.Clean()); // Eject 没给 §9.2 留任何尾巴
    EXPECT_EQ(podReport.HotSwapLog.size(), 1U);
    EXPECT_NE(podReport.HotSwapLog.front().find("eject:Vase.Hello"), std::string::npos);
}

TEST(Eject, RefusedWithConsumersNamedInStructuredReport)
{
    // 3b 的 M2a 兑现（D21）：执法拒绝走 Ok + Status + 逐条点名，两个消费者都要出现。
    vase::PluginHost host;
    vase::LoadPlan plan;
    plan.Ordered.push_back({.Id = "Vase.SharedProvider", .BinaryPath = VASE_FIXTURE_SHAREDPROVIDER});
    plan.Ordered.push_back({.Id = "Vase.EdgeConsumer", .BinaryPath = VASE_FIXTURE_EDGECONSUMER});
    plan.Ordered.push_back({.Id = "Vase.SharedConsumer2", .BinaryPath = VASE_FIXTURE_SHAREDCONSUMER2});
    vase::PodOptions options;
    HostMarker marker;
    options.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::IHostOnlyService>(marker); };
    const vase::PodHandle h = host.CreatePod(plan, options).Value();

    const vase::Result<vase::EjectReport> r = host.EjectPlugin(h, "Vase.SharedProvider");
    ASSERT_TRUE(r.IsOk()); // 执法拒绝从此不是 Err（D21）；bad handle / not-in-pod 那族留在 Err
    const vase::EjectReport& report = r.Value();
    EXPECT_EQ(report.Status, vase::EjectStatus::kRejectedConsumers);
    ASSERT_EQ(report.Consumers.size(), 2U);
    bool seenEdge = false;
    bool seenSecond = false;
    for (const vase::LedgerEdgeRef& consumer : report.Consumers)
    {
        EXPECT_EQ(consumer.ProviderId, "Vase.SharedProvider");
        EXPECT_EQ(consumer.Service, "Vase.Test.Shared");
        EXPECT_EQ(consumer.Version, 1U);
        seenEdge = seenEdge || consumer.ConsumerId == "Vase.EdgeConsumer";
        seenSecond = seenSecond || consumer.ConsumerId == "Vase.SharedConsumer2";
    }
    EXPECT_TRUE(seenEdge);
    EXPECT_TRUE(seenSecond);
    EXPECT_EQ(host.Resolve(h)->PluginCount(), 3U); // 被拒 = 什么都没发生
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(Eject, RemovedEdgesSnapshotRecordsLedgerTruth)
{
    // 「解析记录」Eject 侧：拆 EdgeConsumer，出边（Edge→Shared）逐条进报告。
    vase::PluginHost host;
    vase::LoadPlan plan;
    plan.Ordered.push_back({.Id = "Vase.SharedProvider", .BinaryPath = VASE_FIXTURE_SHAREDPROVIDER});
    plan.Ordered.push_back({.Id = "Vase.EdgeConsumer", .BinaryPath = VASE_FIXTURE_EDGECONSUMER});
    vase::PodOptions options;
    HostMarker marker;
    options.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::IHostOnlyService>(marker); };
    const vase::PodHandle h = host.CreatePod(plan, options).Value();
    const vase::Result<vase::EjectReport> r = host.EjectPlugin(h, "Vase.EdgeConsumer");
    ASSERT_TRUE(r.IsOk());
    EXPECT_EQ(r.Value().Status, vase::EjectStatus::kEjected);
    ASSERT_EQ(r.Value().RemovedEdges.size(), 1U);
    EXPECT_EQ(r.Value().RemovedEdges.begin()->ConsumerId, "Vase.EdgeConsumer");
    EXPECT_EQ(r.Value().RemovedEdges.begin()->ProviderId, "Vase.SharedProvider");
    host.DestroyPod(h);
}

TEST(Eject, UnknownOrStaleRejected)
{
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}})).Value();
    EXPECT_FALSE(host.EjectPlugin(h, "Vase.Nowhere").IsOk());                                          // 不在局中 → 拒
    EXPECT_FALSE(host.EjectPlugin(vase::PodHandle{.Index = 7, .Generation = 7}, "Vase.Hello").IsOk()); // 失效句柄
    host.DestroyPod(h);
}

TEST(Eject, KeptResidentWhenOtherPodHoldsSamePlugin)
{
    // §8.1「一个镜像」+ 档一全局闸：两局各有实例 → 先 Eject 的那局只拆实例。
    vase::PluginHost host;
    const vase::LoadPlan plan = Plan({{"Vase.LoadProbe", VASE_FIXTURE_LOADPROBE}});
    const vase::PodHandle a = host.CreatePod(plan).Value();
    const vase::PodHandle b = host.CreatePod(plan).Value(); // 同 binary 复用（T6 dedup）
    const vase::Result<vase::EjectReport> first = host.EjectPlugin(a, "Vase.LoadProbe");
    ASSERT_TRUE(first.IsOk());
    EXPECT_FALSE(first.Value().BinaryActuallyUnloaded); // 货留在架上
    EXPECT_NE(first.Value().HotSwapNote.find("other pod"), std::string::npos);
    EXPECT_EQ(host.Resolve(b)->PluginCount(), 1U); // 另一局无恙（#3a 的单点形）
    const vase::Result<vase::EjectReport> second = host.EjectPlugin(b, "Vase.LoadProbe");
    ASSERT_TRUE(second.IsOk());
    EXPECT_TRUE(second.Value().BinaryActuallyUnloaded); // 全局归零 → 真卸
    host.DestroyPod(a);
    host.DestroyPod(b);
}

TEST(Eject, FailedRecordCanBeEjectedAndBinaryReleased)
{
    // §5.6 四条补角：Failed 插件可 Eject（拆记录与驻留镜像，必无入边）。
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(Plan({{"Vase.StaleHeader", VASE_FIXTURE_STALEHEADER}})).Value();
    EXPECT_EQ(host.Resolve(h)->PluginCount(), 0U); // HeaderVersion 拒了实例
    const vase::Result<vase::EjectReport> r = host.EjectPlugin(h, "Vase.StaleHeader");
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message(); // 镜像却驻留着——踢掉它
    EXPECT_TRUE(r.Value().BinaryActuallyUnloaded);
    EXPECT_NE(r.Value().HotSwapNote.find("failed"), std::string::npos);
    // ②' 的另一半：失败**记录**也随 Eject 消失——留着它，下一次 Adopt 会以
    // 「already in pod」把同一个 Id 永久拒之门外（T11 Step 1 的判定）。
    EXPECT_TRUE(host.Resolve(h)->Failures().empty());
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(Eject, OtherPodFailedRecordKeepsBinaryResident)
{
    // §8.1 + 补角：局 A 的**失败记录**（FailedBinaries）也是一类持有者——局 B 先卸，
    // 货必须留在架上；只剩 A 时才是真卸。本条证的是**记录这一支存在**（闸会走 kept）。
    // 「闸按记录判、不按 id 判」归下一条 SameFileUnderTwoIdsKeepsRecordAlive——本条两局
    // 用的是**同一个 id**，两种判据下 contains(id) 都为真，证不了键的选择。
    vase::PluginHost host;
    const vase::LoadPlan plan = Plan({{"Vase.StaleHeader", VASE_FIXTURE_STALEHEADER}});
    const vase::PodHandle a = host.CreatePod(plan).Value(); // HeaderVersion 拒了实例，镜像驻留 + 落 FailedBinaries
    const vase::PodHandle b = host.CreatePod(plan).Value(); // 同 binary 复用（T6 dedup）
    const vase::Result<vase::EjectReport> first = host.EjectPlugin(b, "Vase.StaleHeader");
    ASSERT_TRUE(first.IsOk()) << first.GetError().Message();
    EXPECT_FALSE(first.Value().BinaryActuallyUnloaded); // A 的失败记录还指着它
    EXPECT_NE(first.Value().HotSwapNote.find("other pod"), std::string::npos);
    const vase::Result<vase::EjectReport> second = host.EjectPlugin(a, "Vase.StaleHeader");
    ASSERT_TRUE(second.IsOk()) << second.GetError().Message();
    EXPECT_TRUE(second.Value().BinaryActuallyUnloaded); // 记录也摘净了 → 真卸
    host.DestroyPod(a);
    host.DestroyPod(b);
}

TEST(Eject, SameFileUnderTwoIdsKeepsRecordAlive)
{
    // §8.1 的持有者判定按**记录**、不按 id：同一文件被两条条目引用时，第二条 id 对不上
    // → 只是**普通加载失败**（框架正常受理）→ 但它留下了指向同一记录的 FailedBinaries。
    // 这一步若按 id 判就会误判「无人持有」→ Unload 掉记录 → 那条条目悬垂（再 Eject 即
    // `Unload(*悬垂)`）。本条是**跨键**那条路的常驻证人：其余用例两局/两条都同 id，
    // 把判据「简化」回 contains(id) 它们不会红。
    vase::PluginHost host;
    const vase::LoadPlan plan = Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}, {"Vase.HelloX", VASE_FIXTURE_HELLO}});
    const vase::PodHandle h = host.CreatePod(plan).Value();
    ASSERT_EQ(host.Resolve(h)->Failures().size(), 1U); // 只有 id 对不上那条失败

    const vase::Result<vase::EjectReport> first = host.EjectPlugin(h, "Vase.Hello");
    ASSERT_TRUE(first.IsOk()) << first.GetError().Message();
    EXPECT_FALSE(first.Value().BinaryActuallyUnloaded); // 记录还被第二条条目指着——按 id 判这里会错成 true
    // 持有者是**本局**，故只断 "kept resident"、不断 "other pod"（文案那条 deferred Minor 在案）。
    EXPECT_NE(first.Value().HotSwapNote.find("kept resident"), std::string::npos);

    const vase::Result<vase::EjectReport> second = host.EjectPlugin(h, "Vase.HelloX");
    ASSERT_TRUE(second.IsOk()) << second.GetError().Message();
    EXPECT_TRUE(second.Value().BinaryActuallyUnloaded); // 真没人持有了才卸
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

} // namespace
