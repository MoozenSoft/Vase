#include "Vase/Host/PluginHost.h"

#include "../Integration/fixtures/SharedCommon.h"
#include "AdoptExpectations.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Evidence.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/Pod/Context.h"
#include "Vase/Pod/Pod.h"
#include "fixtures/BCommon.h"

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

// 3d 的行为证人（M3/D91/D99）：钉「登记→报告点名→闸放行」这条账面链。本条是单局全卸，
// re-Adopt 落在全新镜像上、static 自行归零——Loads()==1 对 state.Reset() 不敏感；
// 调用点的可证伪证人是下面的 ProcessStatesResetCoexistsWithKeptResidentImage（镜像驻留腿）。
TEST(Eject, ProcessStatesResetMakesReloadLikeFirstTime)
{
    // 期望用 AdoptExpectations 的手写形（raw 计划走 Adopt 需自带期望，T12 单轨）。
    const vase::ManifestExpectation stateful = testing_support::MakeStatefulExpectation();
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(Plan({{"Vase.Stateful", VASE_FIXTURE_STATEFUL}})).Value();
    vase::Pod* pod = host.Resolve(h);
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(pod->Root().Get<samples_fixture::IStateProbe>().Loads(), 1); // 首次装载

    const vase::Result<vase::EjectReport> ejected = host.EjectPlugin(h, "Vase.Stateful");
    ASSERT_TRUE(ejected.IsOk());
    EXPECT_EQ(ejected.Value().Status, vase::EjectStatus::kEjected);
    ASSERT_EQ(ejected.Value().ProcessStatesReset.size(), 1U);
    EXPECT_EQ(*ejected.Value().ProcessStatesReset.begin(), "Vase.Test.StateProbe.Loads");

    vase::AdoptRequest request;
    request.Id = stateful.Id;
    request.BinaryPath = VASE_FIXTURE_STATEFUL;
    request.Expected = &stateful;
    const vase::Result<vase::AdoptReport> adopted = host.AdoptPlugin(h, request);
    ASSERT_TRUE(adopted.IsOk()) << adopted.GetError().Message();
    // 「如同首次」（§9.1）——但注意：本条镜像已全卸、重装落在全新镜像上，该读数对
    // state.Reset() 不敏感（可证伪的调用点证人见 CoexistsWithKeptResidentImage）。
    EXPECT_EQ(host.Resolve(h)->Root().Get<samples_fixture::IStateProbe>().Loads(), 1);
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(Eject, ProcessStatesKeptWhenOtherPodHoldsLiveInstance)
{
    // D91 的反半句：别局有**活实例** → 不重置（那是别局正在用的共享状态）。
    vase::PluginHost host;
    const vase::PodHandle first = host.CreatePod(Plan({{"Vase.Stateful", VASE_FIXTURE_STATEFUL}})).Value();
    const vase::PodHandle second = host.CreatePod(Plan({{"Vase.Stateful", VASE_FIXTURE_STATEFUL}})).Value();
    EXPECT_EQ(host.Resolve(second)->Root().Get<samples_fixture::IStateProbe>().Loads(), 2); // 第二局再装一次

    const vase::Result<vase::EjectReport> ejected = host.EjectPlugin(first, "Vase.Stateful");
    ASSERT_TRUE(ejected.IsOk());
    EXPECT_TRUE(ejected.Value().ProcessStatesReset.empty()); // 没重置
    EXPECT_FALSE(ejected.Value().BinaryActuallyUnloaded);    // 镜像留在架上（别局持有）
    // 别局读数不动：2 → 2（被重置的话这里会是 0）。
    EXPECT_EQ(host.Resolve(second)->Root().Get<samples_fixture::IStateProbe>().Loads(), 2);

    // 最后一局 Eject → 活实例归零 → 重置。
    const vase::Result<vase::EjectReport> last = host.EjectPlugin(second, "Vase.Stateful");
    ASSERT_TRUE(last.IsOk());
    ASSERT_EQ(last.Value().ProcessStatesReset.size(), 1U);
    // 不判 Clean：差分基线是**进程级**计数在各局创建时的快照，两局错峰进出后必有一局的
    // 差分含别局的存量（本仓库既有两局证人 KeptResident/OtherPodFailedRecord 同此不判）。
    host.DestroyPod(first);
    host.DestroyPod(second);
}

TEST(Eject, ProcessStatesResetCoexistsWithKeptResidentImage)
{
    // spec §3.4 的合法组合首次被钉死（D91/R-F-2）：别局只剩 Failed 记录 → **不卸货但照重置**，
    // 且这一位的重置是**可证伪的**——镜像驻留，re-Adopt 走复用分支再跑 OnLoad，删掉
    // state.Reset() 这里就成 2（上面的单局证人全卸后重装全新镜像，看不见这一位）。
    vase::PluginHost host;
    const vase::PodHandle first = host.CreatePod(Plan({{"Vase.Stateful", VASE_FIXTURE_STATEFUL}})).Value();
    // 第二局同文件喂错 Id：GetPlugin 落空 = 普通加载失败 → 只留 FailedBinaries 指向同一记录。
    const vase::PodHandle second = host.CreatePod(Plan({{"Vase.StatefulX", VASE_FIXTURE_STATEFUL}})).Value();
    EXPECT_EQ(host.Resolve(second)->PluginCount(), 0U);

    const vase::Result<vase::EjectReport> ejected = host.EjectPlugin(first, "Vase.Stateful");
    ASSERT_TRUE(ejected.IsOk()) << ejected.GetError().Message();
    EXPECT_EQ(ejected.Value().Status, vase::EjectStatus::kEjected);
    // 并存两半：重置非空（活实例归零，D91）+ 不卸货（别局 Failed 记录指着，§8.1）。
    ASSERT_EQ(ejected.Value().ProcessStatesReset.size(), 1U);
    EXPECT_EQ(*ejected.Value().ProcessStatesReset.begin(), "Vase.Test.StateProbe.Loads");
    EXPECT_FALSE(ejected.Value().BinaryActuallyUnloaded);
    EXPECT_NE(ejected.Value().HotSwapNote.find("kept resident"), std::string::npos);
    // 而这一位不置位：判据是「进程内还有没有**活实例**」（别局只剩 Failed 记录、本局被卸者已摘除），
    // 不是「镜像有没有留下」——kept-resident ≠ 置位，这正是这一位与 Reset 闸「问持有者」的分野。
    EXPECT_FALSE(ejected.Value().SemanticDependencyPossible);

    // 镜像还在架上，复用腿重装——这条读数才是 state.Reset() 的证人。
    const vase::ManifestExpectation stateful = testing_support::MakeStatefulExpectation();
    vase::AdoptRequest request;
    request.Id = stateful.Id;
    request.BinaryPath = VASE_FIXTURE_STATEFUL;
    request.Expected = &stateful;
    const vase::Result<vase::AdoptReport> adopted = host.AdoptPlugin(first, request);
    ASSERT_TRUE(adopted.IsOk()) << adopted.GetError().Message();
    EXPECT_TRUE(adopted.Value().ReusedResidentImage); // kept-resident 的正面凭证（旧闸下此支全新=false）
    EXPECT_EQ(host.Resolve(first)->Root().Get<samples_fixture::IStateProbe>().Loads(), 1);
    host.DestroyPod(first); // 错峰双局差分必含别局存量，Clean 不判（本文件两局证人惯例）
    host.DestroyPod(second);
}

TEST(Eject, ProcessStatesResetCoexistsWithKeptResidentShell)
{
    // C1 的行为证人（R-F-1）：③ 闸的第三类持有者=级联空壳——不计它，最后一手活实例的 Eject
    // 会把别局残条目还指着的记录 Unload 成悬垂（D94 起再 Eject 即读它）。造壳：Pod2 的
    // SharedFailProvider 于 OnStart 失败，同局 EdgeConsumer 被级联拆成空壳（自带 ProcessStates）。
    vase::PluginHost host;
    vase::PodOptions options;
    HostMarker marker;
    options.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::IHostOnlyService>(marker); };
    const vase::LoadPlan live =
        Plan({{"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER}, {"Vase.EdgeConsumer", VASE_FIXTURE_EDGECONSUMER}});
    const vase::LoadPlan torn = Plan({
        {"Vase.SharedFailProvider", VASE_FIXTURE_SHAREDFAILPROVIDER},
        {"Vase.EdgeConsumer", VASE_FIXTURE_EDGECONSUMER},
    });
    const vase::PodHandle first = host.CreatePod(live, options).Value();
    const vase::PodHandle second = host.CreatePod(torn, options).Value();
    EXPECT_EQ(host.Resolve(second)->PluginCount(), 0U); // EdgeConsumer 此刻是空壳、不在 FailedBinaries

    const vase::Result<vase::EjectReport> ejected = host.EjectPlugin(first, "Vase.EdgeConsumer");
    ASSERT_TRUE(ejected.IsOk()) << ejected.GetError().Message();
    EXPECT_TRUE(ejected.Value().CrossPodInstancesZeroed); // 活实例确实归零（Reset 闸口径）
    // 并存两半：重置非空 + 因空壳计数而不卸货（删去 crossPodShellRefs 这支即红——C1 的反证位）。
    ASSERT_EQ(ejected.Value().ProcessStatesReset.size(), 1U);
    EXPECT_EQ(*ejected.Value().ProcessStatesReset.begin(), "Vase.Test.EdgeConsumer.Loads");
    EXPECT_FALSE(ejected.Value().BinaryActuallyUnloaded);
    EXPECT_NE(ejected.Value().HotSwapNote.find("kept resident"), std::string::npos);
    // spec §6 预判的组合（kept-resident 而仍该置位）在此取得证人：重置发生了 ∧ 进程内还有活实例
    // （本局的 SharedProvider 活着；别局 EdgeConsumer 虽成了空壳，但那是「镜像留不留」的口径）。
    EXPECT_TRUE(ejected.Value().SemanticDependencyPossible);

    // 镜像因此还在架上：re-Adopt 复用旧记录。旧闸下这里已全卸——ReusedResidentImage 成 false。
    const vase::ManifestExpectation edge = testing_support::MakeEdgeConsumerExpectation();
    vase::AdoptRequest request;
    request.Id = edge.Id;
    request.BinaryPath = VASE_FIXTURE_EDGECONSUMER;
    request.Expected = &edge;
    const vase::Result<vase::AdoptReport> adopted = host.AdoptPlugin(first, request);
    ASSERT_TRUE(adopted.IsOk()) << adopted.GetError().Message();
    EXPECT_EQ(adopted.Value().Status, vase::AdoptStatus::kAdopted);
    EXPECT_TRUE(adopted.Value().ReusedResidentImage);
    host.DestroyPod(first);
    host.DestroyPod(second);
}

TEST(Eject, SemanticDependencyPossibleWhenResetAndOthersLive)
{
    // M4/D110 正例：重置发生 ∧ 进程内仍有活实例 → 置位。B 还活着——它可能攥着从
    // StateProbe 派生的值，而那条边账本看不见（§9.1）。
    vase::PluginHost host;
    const vase::PodHandle h =
        host.CreatePod(Plan({{"Vase.Stateful", VASE_FIXTURE_STATEFUL}, {"Vase.NeighborB", VASE_FIXTURE_NEIGHBORB}}))
            .Value();
    const vase::Result<vase::EjectReport> r = host.EjectPlugin(h, "Vase.Stateful");
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    EXPECT_FALSE(r.Value().ProcessStatesReset.empty());
    EXPECT_TRUE(r.Value().SemanticDependencyPossible);
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(Eject, SemanticDependencyAbsentWhenNothingElseLive)
{
    // M4/D110 反例 a：重置确实跑了，但全进程再无活实例 → 不置位（没人可能拿着派生值）。
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(Plan({{"Vase.Stateful", VASE_FIXTURE_STATEFUL}})).Value();
    const vase::Result<vase::EjectReport> r = host.EjectPlugin(h, "Vase.Stateful");
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    EXPECT_FALSE(r.Value().ProcessStatesReset.empty());
    EXPECT_FALSE(r.Value().SemanticDependencyPossible);
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(Eject, SemanticDependencyAbsentWithoutProcessStateReset)
{
    // M4/D110 反例 b：有人活着，但被卸者根本没有进程级状态 → 不置位。
    vase::PluginHost host;
    const vase::PodHandle h =
        host.CreatePod(Plan({{"Vase.NeighborB", VASE_FIXTURE_NEIGHBORB}, {"Vase.NeighborC", VASE_FIXTURE_NEIGHBORC}}))
            .Value();
    const vase::Result<vase::EjectReport> r = host.EjectPlugin(h, "Vase.NeighborB");
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    // 下面两条读数在 kRejectedConsumers 的报告（默认构造）上同样成立——先钉住这一条真来自 kEjected。
    EXPECT_EQ(r.Value().Status, vase::EjectStatus::kEjected);
    EXPECT_TRUE(r.Value().ProcessStatesReset.empty());
    EXPECT_FALSE(r.Value().SemanticDependencyPossible);
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

} // namespace
