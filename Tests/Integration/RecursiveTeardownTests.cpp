// §5.2 判据 5（T7）：OnStart 失败的递归拆除——波及集四形态、传递闭包、D42 空壳与 Adopt 零级联。
#include "AdoptExpectations.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Evidence.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Pod.h"

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

vase::LoadPlan BehindPlan() // 拓扑序：提供者 → 四下游 → 第二跳。
{
    vase::LoadPlan plan;
    plan.Ordered.push_back({.Id = "Vase.StartFailProvider", .BinaryPath = VASE_FIXTURE_STARTFAILPROVIDER});
    plan.Ordered.push_back({.Id = "Vase.BehindStrictUsed", .BinaryPath = VASE_FIXTURE_BEHINDSTRICTUSED});
    plan.Ordered.push_back({.Id = "Vase.BehindStrictUnused", .BinaryPath = VASE_FIXTURE_BEHINDSTRICTUNUSED});
    plan.Ordered.push_back({.Id = "Vase.BehindOptUsed", .BinaryPath = VASE_FIXTURE_BEHINDOPTUSED});
    plan.Ordered.push_back({.Id = "Vase.BehindOptUnused", .BinaryPath = VASE_FIXTURE_BEHINDOPTUNUSED});
    plan.Ordered.push_back({.Id = "Vase.BehindFar", .BinaryPath = VASE_FIXTURE_BEHINDFAR});
    return plan;
}

const vase::SkippedRecord* FindSkip(const vase::Pod& pod, std::string_view id)
{
    for (const vase::SkippedRecord& record : pod.Skips())
    {
        if (record.Id == id)
        {
            return &record;
        }
    }
    return nullptr;
}

TEST(RecursiveTeardown, FourMorphologiesTransitiveChainAndTeardownOrder)
{
    // §5.2 判据 5 / D28 的四形态 + 传递闭包。过不了的读法见 spec §13（epoch 请回来那条）。
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(BehindPlan()).Value();
    const vase::Pod* pod = host.Resolve(h);
    ASSERT_NE(pod, nullptr);

    EXPECT_EQ(pod->PluginCount(), 1U); // 只剩 BehindOptUnused（只声明没用，合法幸存）
    EXPECT_TRUE(pod->HasPlugin("Vase.BehindOptUnused"));
    ASSERT_EQ(pod->Failures().size(), 1U);
    EXPECT_EQ(pod->Failures().begin()->Id, "Vase.StartFailProvider");
    EXPECT_EQ(pod->Failures().begin()->Stage, vase::Phase::kStart);

    ASSERT_EQ(pod->Skips().size(), 4U); // StrictUsed / StrictUnused / OptUsed / Far
    for (const std::string_view id :
         {"Vase.BehindStrictUsed", "Vase.BehindStrictUnused", "Vase.BehindOptUsed", "Vase.BehindFar"})
    {
        const vase::SkippedRecord* record = FindSkip(*pod, id);
        ASSERT_NE(record, nullptr) << id;
        EXPECT_EQ(record->Class, vase::SkipClass::kRuntime);
        EXPECT_EQ(record->CausedBy, "Vase.StartFailProvider"); // 多层都记最初失败者（4.3）
        EXPECT_NE(record->Cause.find("cascade from Vase.StartFailProvider"), std::string::npos);
    }

    // 出场序 = 拆除序 = 数组逆序（拓扑逆）——§7.6-2「拆谁之前其服务还活着」的行为证人（R7-1）：
    // TearDownLiveInstances 的正/逆序任何一处被改，这里当场翻红；上面的 FindSkip 循环对序失明，故两者都留。
    std::vector<std::string> tornOrder;
    for (const vase::SkippedRecord& record : pod->Skips())
    {
        tornOrder.push_back(record.Id);
    }
    EXPECT_EQ(tornOrder, std::vector<std::string>({"Vase.BehindFar", "Vase.BehindOptUsed", "Vase.BehindStrictUnused",
                                                   "Vase.BehindStrictUsed"}));
    EXPECT_TRUE(host.DestroyPod(h).Clean()); // 空壳与驻留镜像全收干净
}

TEST(RecursiveTeardown, TornShellCanBeEjectedAndThenReAdopted)
{
    // D94（M3 改判）：级联空壳（无实例、无入边、镜像在架、不在 FailedBinaries）**可以**被 Eject——
    // 它与 Failed 记录同是「无实例的账目残留」，待遇从此一致。Eject 后本局不再记得它，
    // 故同 Id 可以再入局（此前被 already-in-pod 挡住）。
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(BehindPlan()).Value();

    const vase::Result<vase::EjectReport> ejected = host.EjectPlugin(h, "Vase.BehindStrictUnused");
    ASSERT_TRUE(ejected.IsOk()) << ejected.GetError().Message();
    EXPECT_EQ(ejected.Value().Status, vase::EjectStatus::kEjected);
    EXPECT_NE(ejected.Value().HotSwapNote.find("torn shell ejected"), std::string::npos);
    // 空壳无实例 ⇒ 必无入边、必无存活 Scope（三档之档一）。
    EXPECT_TRUE(ejected.Value().LedgerHadNoIncomingEdges);
    EXPECT_TRUE(ejected.Value().ScopeEmptied);
    EXPECT_TRUE(ejected.Value().RemovedEdges.empty());

    const vase::ManifestExpectation behind = testing_support::MakeBehindStrictUnusedExpectation();
    vase::AdoptRequest request;
    request.Id = behind.Id;
    request.BinaryPath = VASE_FIXTURE_BEHINDSTRICTUNUSED;
    request.Expected = &behind;
    const vase::Result<vase::AdoptReport> adopted = host.AdoptPlugin(h, request);
    // 空壳已摘，⓪ 的 already-in-pod 不再命中——这次卡在 ③：它声明的 Vase.Test.Behind
    // 由 StartFailProvider 提供，而那一局已经失败（本局没有活提供方）。
    ASSERT_TRUE(adopted.IsOk()) << adopted.GetError().Message();
    EXPECT_EQ(adopted.Value().Status, vase::AdoptStatus::kRejectedDependencies);
    ASSERT_EQ(adopted.Value().Missing.size(), 1U);
    EXPECT_EQ(adopted.Value().Missing.begin()->Service, "Vase.Test.Behind");

    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(RecursiveTeardown, AdoptFailureStaysZeroCascade)
{
    // §5.6 规则②：入局者皆为叶，Adopt 失败零级联——拆的那台机器对这条路径备而不用。
    vase::PluginHost host;
    {
        // 先让 DeadProvider 过一回装载（局照常交付、条目落 Failed）——拆局不卸货（§8.1），
        // 镜像留在架上，Adopt 的 ② 走复用分支；OnLoad 必败的结论与分支无关。
        const vase::PodHandle registrar =
            host.CreatePod(Plan({{"Vase.DeadProvider", VASE_FIXTURE_DEADPROVIDER}})).Value();
        host.DestroyPod(registrar);
    }
    const vase::ManifestExpectation deadExpected = testing_support::MakeDeadProviderExpectation();
    vase::AdoptRequest deadRequest;
    deadRequest.Id = "Vase.DeadProvider";
    deadRequest.BinaryPath = VASE_FIXTURE_DEADPROVIDER;
    deadRequest.Expected = &deadExpected;
    const vase::PodHandle h = host.CreatePod(Plan({{"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER}})).Value();
    const auto dead = host.AdoptPlugin(h, deadRequest); // 一路走到 ⑥ 的 OnLoad 才败
    ASSERT_FALSE(dead.IsOk());
    EXPECT_NE(dead.GetError().Message().find("adopt failed at OnLoad"), std::string::npos);
    const vase::Pod* pod = host.Resolve(h);
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(pod->PluginCount(), 1U);  // 无人受累
    EXPECT_EQ(pod->Skips().size(), 0U); // 级联机器没被接线
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

} // namespace
