// D57 端到端证人 + D65 staging：Solve 产的计划喂 CreatePod，Host 一行未改。
// 「Solve 产同形计划、Host 无感」= D12 零返工的直接证据（spec §8）。

#include "Vase/Catalog/PluginCatalog.h"

#include "Vase/Catalog/LoadRequest.h"
#include "Vase/Catalog/Preset.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/PluginDescriptor.h"
#include "Vase/Pod/Context.h"
#include "Vase/Pod/Pod.h"

#include "CatalogSandbox.h"
#include "PodTestPeer.h"
#include "fixtures/SharedCommon.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

namespace
{

using testing_support::CatalogSandbox;
using vase::Context;
using vase::LoadRequest;
using vase::PluginCatalog;
using vase::PluginHost;
using vase::PodOptions;
using vase::ServiceRef;

struct HostOnly final : samples_fixture::IHostOnlyService
{
    [[nodiscard]] int Marker() const override { return 1; }
};

void ProvideHostOnly(Context& ctx)
{
    static HostOnly instance; // Stage0 借用注册：实例须活过整局（AdoptTests 同法）
    ctx.Provide<samples_fixture::IHostOnlyService>(instance);
}

class AssemblyFromSolve : public ::testing::Test
{
public:
    // 沙箱声明最先 → 析构最后：DLL 文件要活到 Host 拆完局（HotSwapLoop 的声明序契约同因）。
    CatalogSandbox Sandbox{"assembly-solve"};
    PluginCatalog Catalog;

    void StageAll()
    {
        const std::filesystem::path manifests = VASE_FIXTURE_MANIFESTS;
        struct Roster
        {
            const char* Dir;
            const char* BinaryMacro;
        };
        const std::array<Roster, 4> roster{
            {
                Roster{.Dir = "hello", .BinaryMacro = VASE_FIXTURE_HELLO},
                Roster{.Dir = "shared_provider", .BinaryMacro = VASE_FIXTURE_SHAREDPROVIDER},
                Roster{.Dir = "edge_consumer", .BinaryMacro = VASE_FIXTURE_EDGECONSUMER},
                Roster{.Dir = "shared_consumer2", .BinaryMacro = VASE_FIXTURE_SHAREDCONSUMER2},
            },
        };
        for (const Roster& item : roster)
        {
            const std::filesystem::path binary{item.BinaryMacro};
            Sandbox.CopyFile(std::string(item.Dir) + "/plugin.json", manifests / item.Dir / "plugin.json");
            Sandbox.CopyFile(std::string(item.Dir) + "/" + binary.filename().string(), binary);
        }
        const auto refreshed = Catalog.Refresh(Sandbox.Root);
        ASSERT_TRUE(refreshed.IsOk()) << refreshed.GetError().Message();
    }

    // T11 沙箱：Samples 三件的任意子集逐个入格（manifest 与 DLL 同源 staging，StageAll 同法）。
    struct StagedPlugin
    {
        std::string Dir;
        std::string Binary;
    };

    void Stage(const std::vector<StagedPlugin>& items)
    {
        const std::filesystem::path manifests = VASE_FIXTURE_MANIFESTS;
        for (const StagedPlugin& item : items)
        {
            const std::filesystem::path binary{item.Binary};
            Sandbox.CopyFile(item.Dir + "/plugin.json", manifests / item.Dir / "plugin.json");
            Sandbox.CopyFile(item.Dir + "/" + binary.filename().string(), binary);
        }
        const auto refreshed = Catalog.Refresh(Sandbox.Root);
        ASSERT_TRUE(refreshed.IsOk()) << refreshed.GetError().Message();
    }

    static LoadRequest WithHost()
    {
        static const std::array<ServiceRef, 1> kHost{
            {
                ServiceRef{
                    .Name = samples_fixture::IHostOnlyService::kName,
                    .Version = samples_fixture::IHostOnlyService::kVersion,
                },
            },
        };
        LoadRequest request;
        request.HostProvided = kHost;
        return request;
    }
};

TEST_F(AssemblyFromSolve, SolvePlanAssemblesInTopoOrder)
{
    StageAll();
    const auto solved = Catalog.Solve(WithHost());
    ASSERT_TRUE(solved.IsOk()) << solved.GetError().Message();
    const auto& plan = solved.Value().Plan;
    ASSERT_EQ(plan.Ordered.size(), 4U);
    const std::vector<std::string> expected{
        "Vase.Hello",
        "Vase.SharedProvider",
        "Vase.EdgeConsumer",
        "Vase.SharedConsumer2",
        // 拓扑 + 同层 Id 序（D53）
    };
    std::vector<std::string> planIds;
    planIds.reserve(plan.Ordered.size());
    for (const auto& entry : plan.Ordered)
    {
        planIds.emplace_back(entry.Id);
    }
    EXPECT_EQ(planIds, expected);

    PluginHost host;
    PodOptions options;
    options.Strict = true; // 本局不该有任何 Failed（§5.5）
    options.Stage0 = &ProvideHostOnly;
    const auto created = host.CreatePod(plan, options);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
    auto* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(vase::PodTestPeer::InstanceOrder(*pod), expected); // 实际装载序 == 计划序

    const auto report = host.DestroyPod(created.Value());
    EXPECT_TRUE(report.Clean());
    EXPECT_EQ(host.ForTestCounters().PluginInstances, 0U);
}

TEST_F(AssemblyFromSolve, SkippedEntriesPassThroughHost)
{
    StageAll();
    Sandbox.WriteFile("Client.preset.json",
                      R"({"schemaVersion":1,"overrides":{"Vase.SharedProvider":{"enabled":false}}})");
    auto loaded = vase::LoadPreset(Sandbox.Root / "Client.preset.json");
    ASSERT_TRUE(loaded.IsOk()) << loaded.GetError().Message();
    LoadRequest request = WithHost();
    request.Preset = std::move(loaded.Value());
    const auto solved = Catalog.Solve(request);
    ASSERT_TRUE(solved.IsOk()) << solved.GetError().Message();
    ASSERT_EQ(solved.Value().Plan.Ordered.size(), 4U); // 1 load + 3 skip 全示众（D53）

    PluginHost host;
    PodOptions options;
    options.Stage0 = &ProvideHostOnly;
    const auto created = host.CreatePod(solved.Value().Plan, options);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message(); // 跳过条目照进局（D30 的 kSkip 语义不变）
    auto* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(vase::PodTestPeer::InstanceOrder(*pod), std::vector<std::string>{"Vase.Hello"});
    const auto report = host.DestroyPod(created.Value());
    EXPECT_TRUE(report.Clean());
}

// 判据 4 后半的 Sample 面证人（T11/spec §5.2）：failing 无任何 Provides，OnStart 倒下的
// 闭包炸不到 dependent；后者照常活——「失败不级联、依赖链通」各钉一侧。
TEST_F(AssemblyFromSolve, SampleFailingStartKeepsDependentLive)
{
    Stage({
        StagedPlugin{.Dir = "hello", .Binary = VASE_FIXTURE_HELLO},
        StagedPlugin{.Dir = "dependent", .Binary = VASE_FIXTURE_DEPENDENT},
        StagedPlugin{.Dir = "failing", .Binary = VASE_FIXTURE_FAILING},
    });
    const auto solved = Catalog.Solve(LoadRequest{});
    ASSERT_TRUE(solved.IsOk()) << solved.GetError().Message();
    ASSERT_EQ(solved.Value().Plan.Ordered.size(), 3U); // dependent 的 Greeter 由本局 hello 产出，无人被静态跳

    PluginHost host;
    const auto created = host.CreatePod(solved.Value().Plan); // 宽容模式：Failed 不翻整局（D70 同形）
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
    auto* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);

    ASSERT_EQ(pod->Failures().size(), 1U);
    const vase::FailedPluginRecord& failure = *pod->Failures().begin();
    EXPECT_EQ(failure.Id, "Vase.Failing");
    EXPECT_EQ(failure.Stage, vase::Phase::kStart);
    EXPECT_NE(failure.Message.find("demo: OnStart always fails"), std::string::npos) << failure.Message;
    EXPECT_TRUE(pod->Skips().empty()); // 级联缺席：failing 不提供任何东西
    // 活体点名（评审 Minor-3）：dependent 的 OnStart 凭声明解析到了 Greeter——Get 失败在本
    // 项目是 terminate，HasPlugin 为真本身就是链通的证人。
    EXPECT_TRUE(pod->HasPlugin("Vase.Hello"));
    EXPECT_TRUE(pod->HasPlugin("Vase.Dependent"));
    EXPECT_FALSE(pod->HasPlugin("Vase.Failing"));
    EXPECT_EQ(pod->PluginCount(), 2U);
    EXPECT_EQ(vase::PodTestPeer::InstanceOrder(*pod),
              (std::vector<std::string>{"Vase.Failing", "Vase.Hello", "Vase.Dependent"})); // 层0 同层 Id 序（D53）

    const auto report = host.DestroyPod(created.Value());
    EXPECT_TRUE(report.Clean());
    EXPECT_EQ(host.ForTestCounters().PluginInstances, 0U);
}

// 判据 8 的 Sample 面：hello 缺席时 dependent 的硬需求在 Solve 就静态跳过（kMissingDependency），
// 不进装载循环、不在运行期撞 Get 的 terminate；同局的 failing 照常倒下，互不相干。
TEST_F(AssemblyFromSolve, DependentStaticallySkippedWithoutHello)
{
    Stage({
        StagedPlugin{.Dir = "dependent", .Binary = VASE_FIXTURE_DEPENDENT},
        StagedPlugin{.Dir = "failing", .Binary = VASE_FIXTURE_FAILING},
    });
    const auto solved = Catalog.Solve(LoadRequest{});
    ASSERT_TRUE(solved.IsOk()) << solved.GetError().Message();
    const auto& plan = solved.Value().Plan;
    ASSERT_EQ(plan.Ordered.size(), 2U); // 跳过者照进计划示众（D53）
    const auto dependent = std::ranges::find_if(plan.Ordered, [](const vase::LoadPlanEntry& entry)
                                                { return entry.Id == "Vase.Dependent"; });
    ASSERT_NE(dependent, plan.Ordered.end());
    EXPECT_EQ(dependent->Decision, vase::LoadDecision::kSkip);
    EXPECT_EQ(dependent->Reason, vase::SkipReason::kMissingDependency);

    PluginHost host;
    const auto created = host.CreatePod(plan);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
    auto* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);

    ASSERT_EQ(pod->Skips().size(), 1U);
    const vase::SkippedRecord& skip = *pod->Skips().begin();
    EXPECT_EQ(skip.Id, "Vase.Dependent");
    EXPECT_EQ(skip.Class, vase::SkipClass::kStatic);
    EXPECT_NE(skip.Cause.find("missing dependency"), std::string::npos) << skip.Cause;
    ASSERT_EQ(pod->Failures().size(), 1U); // nobody depends on failing：它倒下，无人陪葬
    EXPECT_EQ(pod->Failures().begin()->Id, "Vase.Failing");
    EXPECT_EQ(pod->PluginCount(), 0U);

    const auto report = host.DestroyPod(created.Value());
    EXPECT_TRUE(report.Clean());
}

} // namespace
