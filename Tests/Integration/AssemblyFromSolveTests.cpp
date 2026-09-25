// D57 端到端证人 + D65 staging：Solve 产的计划喂 CreatePod，Host 一行未改。
// 「Solve 产同形计划、Host 无感」= D12 零返工的直接证据（spec §8）。

#include "Vase/Catalog/PluginCatalog.h"

#include "Vase/Catalog/LoadRequest.h"
#include "Vase/Catalog/Preset.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/PluginDescriptor.h"
#include "Vase/Pod/Context.h"
#include "Vase/Pod/Pod.h"

#include "CatalogSandbox.h"
#include "PodTestPeer.h"
#include "fixtures/SharedCommon.h"

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
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message(); // 跳过条目照进局（D30 KnownBinaries 语义不变）
    auto* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(vase::PodTestPeer::InstanceOrder(*pod), std::vector<std::string>{"Vase.Hello"});
    const auto report = host.DestroyPod(created.Value());
    EXPECT_TRUE(report.Clean());
}

} // namespace
