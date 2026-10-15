// T6 的逐格判据（spec §4.3）：参与包在建 Stage0、Pod、插件/配置实例、OnLoad/OnStart 之前，
// 用**随后装配消费的同一份镜像**枚举并核对全部成员；结构自相矛盾与镜像不一致都整局 Err，
// 一个实例都不建。全跳过包不碰 Loader；原始手写计划（无 PackageSources）保持逐条目原语义。

#include "Vase/Detail/Counters.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/Loader.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/Host/PackagePlan.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Pod.h"

#include "AdoptExpectations.h"
#include "PackageDescriptorProbe.h"

#include <cstddef>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <utility>

namespace
{

using testing_support::MakeHelloExpectation;
using testing_support::MakeProbePackagePlan;
using testing_support::ProbeEntries;
using testing_support::ProbeExpectations;
using testing_support::ReadPackageProbeCounts;
using testing_support::ResetPackageProbe;
using testing_support::SetProbeFailure;
using vase::LoadDecision;
using vase::LoadPlan;
using vase::LoadPlanEntry;
using vase::ManifestExpectation;
using vase::PackagePlanSource;
using vase::PackageSourceKind;
using vase::PluginHost;
using vase::PodOptions;
using vase::SkipReason;

// 手写条目的 Id 只借不拥有：字面量正是计划要的那种静态串（LoadPlan.h 的借用窗契约）。
constexpr std::string_view kProbeAId = "Probe.A";

// 失败局的五项实例计数必须全零（§4.3「不交付部分装配的 Pod」）。
void ExpectNoInstances(PluginHost& host)
{
    const vase::detail::DiagnosticCounters& counters = host.ForTestCounters();
    EXPECT_EQ(counters.Effects, 0U);
    EXPECT_EQ(counters.Services, 0U);
    EXPECT_EQ(counters.Subscriptions, 0U);
    EXPECT_EQ(counters.PluginInstances, 0U);
    EXPECT_EQ(counters.Scopes, 0U);
}

// 纯结构检查不过时一个镜像都不许加载：入口零调用、驻留表空。
void ExpectLoaderUntouched(PluginHost& host)
{
    EXPECT_EQ(ReadPackageProbeCounts().Lookup, 0U);
    EXPECT_EQ(ReadPackageProbeCounts().Enumerate, 0U);
    EXPECT_EQ(host.ForTestResidentBinaryCount(), 0U);
}

} // namespace

// brief 给定用例（逐字保留）：镜像不一致在 Stage0 之前就整局 Err，不留账本边、不建实例。
TEST(PackagePreflight, RejectsMismatchBeforeStage0)
{
    ResetPackageProbe();
    const auto plan = MakeProbePackagePlan();
    SetProbeFailure("lookup-mismatch");
    std::size_t stage0Calls = 0;
    vase::PodOptions options;
    options.Stage0 = [&stage0Calls](vase::Context&) { ++stage0Calls; };
    vase::PluginHost host;
    EXPECT_FALSE(host.CreatePod(plan, options).IsOk());
    EXPECT_EQ(stage0Calls, 0U);
    EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U);
    EXPECT_EQ(ReadPackageProbeCounts().Create, 0U);
    // 同一份用例的追加判据：一个配置对象、一次 OnLoad、一次进程级状态都不得发生。
    EXPECT_EQ(ReadPackageProbeCounts().CreateConfig, 0U);
    EXPECT_EQ(ReadPackageProbeCounts().OnLoad, 0U);
    EXPECT_EQ(ReadPackageProbeCounts().Reset, 0U);
    EXPECT_EQ(ReadPackageProbeCounts().Enumerate, 1U); // 参与源只枚举一次
    ExpectNoInstances(host);
    EXPECT_EQ(host.ForTestResidentBinaryCount(), 1U); // 进程内来源已登记：失败留架，不卸载
}

// 枚举面多出一员（清单只列两员、实际包三员）：整包核对在枚举处现形，带来源与成员归因。
TEST(PackagePreflight, RejectsBinaryMemberMissingFromManifest)
{
    ResetPackageProbe();
    const auto plan = MakeProbePackagePlan();
    SetProbeFailure("extra-member");
    std::size_t stage0Calls = 0;
    vase::PodOptions options;
    options.Stage0 = [&stage0Calls](vase::Context&) { ++stage0Calls; };
    vase::PluginHost host;
    const vase::Result<vase::PodHandle> refused = host.CreatePod(plan, options);
    ASSERT_FALSE(refused.IsOk());
    const std::string& message = refused.GetError().Message();
    EXPECT_NE(message.find("probe.package"), std::string::npos) << message;
    EXPECT_NE(message.find("Probe.Extra"), std::string::npos) << message;
    EXPECT_NE(message.find("missing in manifest"), std::string::npos) << message;
    EXPECT_EQ(stage0Calls, 0U);
    EXPECT_EQ(ReadPackageProbeCounts().Create, 0U);
    ExpectNoInstances(host);
}

// 清单多列一员、二进制里没有：Id 集双向等值的另一半。
TEST(PackagePreflight, RejectsManifestMemberMissingFromBinary)
{
    ResetPackageProbe();
    auto plan = MakeProbePackagePlan();
    plan.PackageSources.front().Members.push_back(ManifestExpectation{.Id = "Probe.Ghost"});
    const ManifestExpectation& ghost = plan.PackageSources.front().Members.back();
    LoadPlanEntry ghostEntry;
    ghostEntry.Id = ghost.Id;
    ghostEntry.InProcessEntry = ProbeEntries().Lookup;
    ghostEntry.Expected = ghost;
    ghostEntry.PackageSourceId = plan.PackageSources.front().SourceId;
    plan.Ordered.push_back(std::move(ghostEntry));

    std::size_t stage0Calls = 0;
    vase::PodOptions options;
    options.Stage0 = [&stage0Calls](vase::Context&) { ++stage0Calls; };
    vase::PluginHost host;
    const vase::Result<vase::PodHandle> refused = host.CreatePod(plan, options);
    ASSERT_FALSE(refused.IsOk());
    const std::string& message = refused.GetError().Message();
    EXPECT_NE(message.find("Probe.Ghost"), std::string::npos) << message;
    EXPECT_NE(message.find("missing in binary"), std::string::npos) << message;
    EXPECT_EQ(stage0Calls, 0U);
    EXPECT_EQ(ReadPackageProbeCounts().Create, 0U);
    ExpectNoInstances(host);
}

// 被禁用的成员照样全量核对：跳过不豁免整包比对（spec §4.3「参与包内被禁用的成员仍全部检查」）。
TEST(PackagePreflight, RejectsDisabledMemberDrift)
{
    ResetPackageProbe();
    auto plan = MakeProbePackagePlan();
    for (ManifestExpectation& member : plan.PackageSources.front().Members)
    {
        if (member.Id == "Probe.B")
        {
            member.Version = "9.9.9";
        }
    }
    for (LoadPlanEntry& entry : plan.Ordered)
    {
        if (entry.Id != "Probe.B")
        {
            continue;
        }
        entry.Decision = LoadDecision::kSkip;
        entry.Reason = SkipReason::kDisabled;
        if (entry.Expected.has_value())
        {
            entry.Expected->Version = "9.9.9";
        }
    }

    std::size_t stage0Calls = 0;
    vase::PodOptions options;
    options.Stage0 = [&stage0Calls](vase::Context&) { ++stage0Calls; };
    vase::PluginHost host;
    const vase::Result<vase::PodHandle> refused = host.CreatePod(plan, options);
    ASSERT_FALSE(refused.IsOk());
    const std::string& message = refused.GetError().Message();
    EXPECT_NE(message.find("Probe.B"), std::string::npos) << message;
    EXPECT_NE(message.find("version"), std::string::npos) << message;
    EXPECT_EQ(stage0Calls, 0U);
    EXPECT_EQ(ReadPackageProbeCounts().Create, 0U);
    ExpectNoInstances(host);
}

// kLoad 条目挂着计划里不存在的来源键：关联缺失 = 计划误用，Loader 一个记录都不许建。
TEST(PackagePreflight, RejectsEntryWithUnknownSourceBeforeLoading)
{
    ResetPackageProbe();
    auto plan = MakeProbePackagePlan();
    plan.PackageSources.clear();
    std::size_t stage0Calls = 0;
    vase::PodOptions options;
    options.Stage0 = [&stage0Calls](vase::Context&) { ++stage0Calls; };
    vase::PluginHost host;
    ASSERT_FALSE(host.CreatePod(plan, options).IsOk());
    EXPECT_EQ(stage0Calls, 0U);
    ExpectLoaderUntouched(host);
}

// Members 与归属条目 Id 集不一致（条目少一员）：计划自相矛盾，一个镜像都不加载。
TEST(PackagePreflight, RejectsMemberSetDriftBeforeLoading)
{
    ResetPackageProbe();
    auto plan = MakeProbePackagePlan();
    plan.Ordered.pop_back();
    vase::PluginHost host;
    ASSERT_FALSE(host.CreatePod(plan).IsOk());
    ExpectLoaderUntouched(host);
}

// 来源字段缺失：动态来源没有库路径、进程内来源没有双入口——各自拒绝且不碰 Loader。
TEST(PackagePreflight, RejectsMissingSourceFieldsBeforeLoading)
{
    ResetPackageProbe();
    {
        auto plan = MakeProbePackagePlan();
        plan.PackageSources.front().Kind = PackageSourceKind::kDynamic;
        plan.PackageSources.front().Entries = {};
        vase::PluginHost host;
        ASSERT_FALSE(host.CreatePod(plan).IsOk());
        ExpectLoaderUntouched(host);
    }
    {
        auto plan = MakeProbePackagePlan();
        plan.PackageSources.front().Entries = {}; // 进程内来源的双入口缺席
        vase::PluginHost host;
        ASSERT_FALSE(host.CreatePod(plan).IsOk());
        ExpectLoaderUntouched(host);
    }
}

// 包条目没有期望：整包校验无从谈起（关联/Expected 缺失即计划误用）。
TEST(PackagePreflight, RejectsPackageEntryWithoutExpectation)
{
    ResetPackageProbe();
    auto plan = MakeProbePackagePlan();
    plan.Ordered.front().Expected.reset();
    vase::PluginHost host;
    ASSERT_FALSE(host.CreatePod(plan).IsOk());
    ExpectLoaderUntouched(host);
}

// 全跳过包：不加载、不要求绑定、不执行入口，成员如实进报告的 Skips。
TEST(PackagePreflight, FullySkippedPackageNeverTouchesLoader)
{
    ResetPackageProbe();
    auto plan = MakeProbePackagePlan();
    for (LoadPlanEntry& entry : plan.Ordered)
    {
        entry.Decision = LoadDecision::kSkip;
        entry.Reason = SkipReason::kDisabled;
    }
    std::size_t stage0Calls = 0;
    vase::PodOptions options;
    options.Stage0 = [&stage0Calls](vase::Context&) { ++stage0Calls; };
    vase::PluginHost host;
    const vase::Result<vase::PodHandle> created = host.CreatePod(plan, options);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
    EXPECT_EQ(stage0Calls, 1U); // 全跳过不是失败：建局照常走完
    ExpectLoaderUntouched(host);
    vase::Pod* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(pod->Skips().size(), 2U);
    EXPECT_EQ(pod->PluginCount(), 0U);
    host.DestroyPod(created.Value());
}

// 原始手写计划（无 PackageSources）保留逐条目原语义：同一 mismatch 材料在整包预检下是整局 Err，
// 这里只落该条目的 Failed 记录、Pod 照常建起（宽容语义 §0.3-4）。
TEST(PackagePreflight, HandwrittenPlanKeepsPerEntrySemantics)
{
    ResetPackageProbe();
    SetProbeFailure("lookup-mismatch");
    LoadPlan plan;
    LoadPlanEntry entry;
    entry.Id = kProbeAId;
    entry.InProcessEntry = ProbeEntries().Lookup;
    entry.Expected = ProbeExpectations().front();
    plan.Ordered.push_back(std::move(entry));

    std::size_t stage0Calls = 0;
    vase::PodOptions options;
    options.Stage0 = [&stage0Calls](vase::Context&) { ++stage0Calls; };
    vase::PluginHost host;
    const vase::Result<vase::PodHandle> created = host.CreatePod(plan, options);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
    EXPECT_EQ(stage0Calls, 1U);
    vase::Pod* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);
    ASSERT_EQ(pod->Failures().size(), 1U);
    EXPECT_EQ(pod->Failures().front().Stage, vase::Phase::kLoad);
    EXPECT_EQ(pod->PluginCount(), 0U);
    EXPECT_EQ(host.ForTestResidentBinaryCount(), 1U); // 逐条目的 EnsureInProcess 照旧
    host.DestroyPod(created.Value());
}

// 手写计划的合法路径同样原样成立：预检对无关联键的计划是空操作。
TEST(PackagePreflight, HandwrittenPlanWithoutPackageSourcesStillAssembles)
{
    ResetPackageProbe();
    LoadPlan plan;
    LoadPlanEntry entry;
    entry.Id = kProbeAId;
    entry.InProcessEntry = ProbeEntries().Lookup;
    entry.Expected = ProbeExpectations().front();
    plan.Ordered.push_back(std::move(entry));

    vase::PluginHost host;
    const vase::Result<vase::PodHandle> created = host.CreatePod(plan);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
    EXPECT_EQ(ReadPackageProbeCounts().Enumerate, 0U); // 原路径不枚举
    EXPECT_EQ(ReadPackageProbeCounts().Create, 1U);
    EXPECT_EQ(ReadPackageProbeCounts().OnLoad, 1U);
    vase::Pod* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(pod->PluginCount(), 1U);
    host.DestroyPod(created.Value());
}

// 两局之间改探针元信息：第二局必须重新枚举并改用新事实——没有「上一局校验通过」的缓存。
TEST(PackagePreflight, ChecksEveryCreatePodInsteadOfCaching)
{
    ResetPackageProbe();
    const auto plan = MakeProbePackagePlan();
    {
        vase::PluginHost host;
        const vase::Result<vase::PodHandle> created = host.CreatePod(plan);
        ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
        EXPECT_EQ(ReadPackageProbeCounts().Enumerate, 1U);
        EXPECT_EQ(ReadPackageProbeCounts().Create, 2U); // 两名成员各建一次实例
        host.DestroyPod(created.Value());
    }
    SetProbeFailure("extra-member");
    {
        vase::PluginHost host;
        const vase::Result<vase::PodHandle> refused = host.CreatePod(plan);
        ASSERT_FALSE(refused.IsOk());
        EXPECT_NE(refused.GetError().Message().find("Probe.Extra"), std::string::npos);
        EXPECT_EQ(ReadPackageProbeCounts().Enumerate, 2U); // 第二局重新枚举
        EXPECT_EQ(host.ForTestResidentBinaryCount(), 1U);
    }
}

// 真实动态包（组合库 VasePlugins 三成员）配一份只列一员的清单：整局 Err，已装载镜像留架
// （不自动卸载、不重置进程级状态），宿主关闭时收走——平台证据从全新 Loader 读。
TEST(PackagePreflight, DynamicPackageMismatchKeepsResidentImageAndNoReset)
{
    ResetPackageProbe();
    const std::filesystem::path library{VASE_FIXTURE_VASEPLUGINS};
    LoadPlan plan;
    PackagePlanSource source;
    source.SourceId = "vase.plugins";
    source.Kind = PackageSourceKind::kDynamic;
    source.BinaryPath = library;
    source.Members.push_back(ManifestExpectation{.Id = "Vase.Hello"});
    plan.PackageSources.push_back(std::move(source));
    LoadPlanEntry entry;
    entry.Id = plan.PackageSources.front().Members.front().Id;
    entry.BinaryPath = library;
    entry.Expected = ManifestExpectation{.Id = "Vase.Hello"};
    entry.PackageSourceId = plan.PackageSources.front().SourceId;
    plan.Ordered.push_back(std::move(entry));

    std::size_t stage0Calls = 0;
    vase::PodOptions options;
    options.Stage0 = [&stage0Calls](vase::Context&) { ++stage0Calls; };
    {
        vase::PluginHost host;
        const vase::Result<vase::PodHandle> refused = host.CreatePod(plan, options);
        ASSERT_FALSE(refused.IsOk());
        const std::string& message = refused.GetError().Message();
        EXPECT_NE(message.find("vase.plugins"), std::string::npos) << message;
        EXPECT_NE(message.find("missing in manifest"), std::string::npos) << message;
        EXPECT_EQ(stage0Calls, 0U);
        EXPECT_EQ(ReadPackageProbeCounts().Reset, 0U);    // 失败局没有 Pod 可 Eject，无 Reset
        EXPECT_EQ(host.ForTestResidentBinaryCount(), 1U); // 真实镜像仍驻留（留架）
        ExpectNoInstances(host);
    }
    // 宿主关闭收走动态镜像：泄漏的映射会挡住「可写开」（Win）或留在映射清单里（POSIX）。
    vase::detail::Loader probe;
    const vase::Result<vase::detail::BinaryRecord*> resident = probe.EnsureResident(library);
    ASSERT_TRUE(resident.IsOk()) << resident.GetError().Message();
    const vase::detail::BinaryRecord copy = *resident.Value(); // 先复制：Unload 会摘表
    const vase::detail::UnloadEvidence evidence = probe.Unload(copy);
#ifdef _WIN32
    EXPECT_TRUE(evidence.ReopenWritableIsMeaningful);
    EXPECT_TRUE(evidence.ReopenWritable);
#else
    EXPECT_TRUE(evidence.MappingRemovalIsObservable);
    EXPECT_TRUE(evidence.MappingRemoved);
#endif
}

// 真实动态包的通过面：预检装载并枚举，装配消费的就是那份记录（一份镜像，不重复装）。
TEST(PackagePreflight, DynamicPackagePassesAndAssemblesFromPreflightedRecord)
{
    ResetPackageProbe();
    const std::filesystem::path library{VASE_FIXTURE_HELLO};
    LoadPlan plan;
    PackagePlanSource source;
    source.SourceId = "hello.package";
    source.Kind = PackageSourceKind::kDynamic;
    source.BinaryPath = library;
    source.Members.push_back(MakeHelloExpectation());
    plan.PackageSources.push_back(std::move(source));
    LoadPlanEntry entry;
    entry.Id = plan.PackageSources.front().Members.front().Id;
    entry.BinaryPath = library;
    entry.Expected = plan.PackageSources.front().Members.front();
    entry.PackageSourceId = plan.PackageSources.front().SourceId;
    plan.Ordered.push_back(std::move(entry));

    std::size_t stage0Calls = 0;
    vase::PodOptions options;
    options.Stage0 = [&stage0Calls](vase::Context&) { ++stage0Calls; };
    vase::PluginHost host;
    const vase::Result<vase::PodHandle> created = host.CreatePod(plan, options);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
    EXPECT_EQ(stage0Calls, 1U);
    EXPECT_EQ(host.ForTestResidentBinaryCount(), 1U); // 预检那一份镜像就是装配用的那份
    vase::Pod* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(pod->PluginCount(), 1U);
    host.DestroyPod(created.Value());
}

// 参与包的库加载不了：同样在 Stage0 之前整局 Err，带来源归因，且不留驻留记录。
TEST(PackagePreflight, RejectsUnloadableDynamicSourceBeforeStage0)
{
    ResetPackageProbe();
    const std::filesystem::path missing = std::filesystem::path{VASE_FIXTURE_HELLO}.concat(".absent");
    LoadPlan plan;
    PackagePlanSource source;
    source.SourceId = "missing.package";
    source.Kind = PackageSourceKind::kDynamic;
    source.BinaryPath = missing;
    source.Members.push_back(MakeHelloExpectation());
    plan.PackageSources.push_back(std::move(source));
    LoadPlanEntry entry;
    entry.Id = plan.PackageSources.front().Members.front().Id;
    entry.BinaryPath = missing;
    entry.Expected = plan.PackageSources.front().Members.front();
    entry.PackageSourceId = plan.PackageSources.front().SourceId;
    plan.Ordered.push_back(std::move(entry));

    std::size_t stage0Calls = 0;
    vase::PodOptions options;
    options.Stage0 = [&stage0Calls](vase::Context&) { ++stage0Calls; };
    vase::PluginHost host;
    const vase::Result<vase::PodHandle> refused = host.CreatePod(plan, options);
    ASSERT_FALSE(refused.IsOk());
    EXPECT_NE(refused.GetError().Message().find("missing.package"), std::string::npos) << refused.GetError().Message();
    EXPECT_EQ(stage0Calls, 0U);
    EXPECT_EQ(host.ForTestResidentBinaryCount(), 0U); // 加载失败没有记录可留
    ExpectNoInstances(host);
}
