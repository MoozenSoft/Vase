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
// 两平台对 std::error_code 的归属判定不同（MSVC STL 的映射认 <filesystem> 也提供它，
// libc++ 只认 <system_error>）：本行留着，Windows 报「未被直接使用」、Linux 报「没有头
// 提供它」；摘掉则反过来。只有「留着 + 在 Windows 抑制」能同时过两条 debug 线。
#include <system_error> // NOLINT(misc-include-cleaner)
#include <utility>

namespace
{

// 宿主标记服务：与 T9/T10 同形、同理由的**文件内**副本——per-file 测试助手不抽公共头（预检裁定 R2）。
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

TEST(Adopt, UnknownIdAndAlreadyInPodRejected)
{
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}})).Value();
    // 先断 IsOk() 再取错误消息，且必须是 **ASSERT_**：Result::GetError() 在 Ok 上会走
    // ProgrammerError 终止进程，EXPECT_FALSE 只记录失败、继续往下走，照样撞上它——
    // 那就把「一条断言失败」变成了「整条进程挂掉」。
    const vase::Result<vase::AdoptReport> unknown = host.AdoptPlugin(h, "Vase.NeverRegistered");
    ASSERT_FALSE(unknown.IsOk());
    EXPECT_NE(unknown.GetError().Message().find("unknown plugin id"), std::string::npos);
    const vase::Result<vase::AdoptReport> duplicate = host.AdoptPlugin(h, "Vase.Hello");
    ASSERT_FALSE(duplicate.IsOk());
    EXPECT_NE(duplicate.GetError().Message().find("already in pod"), std::string::npos);
    host.DestroyPod(h);
}

TEST(Adopt, ReusedResidentImageStillVerifiesIdentity)
{
    // §5.6②：复用驻留镜像的分支**同样**做档三比对（这里走通 = 比对通过）。
    vase::PluginHost host;
    const vase::LoadPlan pairPlan = Plan({
        {"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER},
        {"Vase.EdgeConsumer", VASE_FIXTURE_EDGECONSUMER},
    });
    HostMarker marker;
    vase::PodOptions options;
    options.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::IHostOnlyService>(marker); };
    host.DestroyPod(host.CreatePod(pairPlan, options).Value()); // 拆局不卸货（§8.1）

    const vase::PodHandle h =
        host.CreatePod(Plan({{"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER}}), options).Value();
    const vase::Result<vase::AdoptReport> r = host.AdoptPlugin(h, "Vase.EdgeConsumer");
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    EXPECT_TRUE(r.Value().ReusedResidentImage); // EdgeConsumer 的镜像还驻留在架上
    EXPECT_TRUE(r.Value().IdentityVerified);
    EXPECT_EQ(r.Value().OutgoingEdges, 1U); // 落账：Shared 那条（HostOnly 不落）
    EXPECT_EQ(host.Resolve(h)->PluginCount(), 2U);
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(Adopt, RequiresMustBindFullyOrNothing)
{
    // 规则①：不带病入局；规则②：Adopt 失败零级联（活人一根毛都不掉）。
    vase::PluginHost host;
    const vase::LoadPlan pairPlan = Plan({
        {"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER},
        {"Vase.EdgeConsumer", VASE_FIXTURE_EDGECONSUMER},
    });
    HostMarker marker;
    vase::PodOptions opts;
    opts.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::IHostOnlyService>(marker); };
    host.DestroyPod(host.CreatePod(pairPlan, opts).Value()); // 只为登记 KnownBinaries

    const vase::PodHandle h = host.CreatePod(Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}})).Value();
    const vase::Result<vase::AdoptReport> r = host.AdoptPlugin(h, "Vase.EdgeConsumer");
    ASSERT_FALSE(r.IsOk()); // ASSERT_：下面立刻取 GetError()，Ok 上取会终止进程
    EXPECT_NE(r.GetError().Message().find("Vase.Test.Shared"), std::string::npos); // 报告缺哪条
    EXPECT_NE(r.GetError().Message().find("Vase.Test.HostOnly"), std::string::npos);
    EXPECT_EQ(host.Resolve(h)->PluginCount(), 1U); // Hello 无恙
    EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U);  // 没留半条边
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

#ifndef _WIN32
// 构建树现场保护：本文件**唯一**会改构建树的用例先把原文件备份出来，析构时放回原位。
// 用 RAII 而不是写在测试末行——中途 ASSERT_* 早退会把构建树留在「该路径指向另一个
// 二进制」的状态，此后每一条装载 LoadProbe 的用例都会拿到错的东西。
// 它同时看住**两个**临时名：`.orig`（备份，放回原位用）与 Staging（换进来的那份）——
// rename 失败时后者会留在构建目录里，只清备份是不够的。
class ProbeSwapGuard final
{
public:
    ProbeSwapGuard(std::filesystem::path target, std::filesystem::path staging)
        : Target(std::move(target))
        , Backup(Target.string() + ".orig")
        , Staging(std::move(staging))
    {
        std::error_code ec;
        std::filesystem::remove(Backup, ec);
        ec.clear();
        std::filesystem::copy_file(Target, Backup, std::filesystem::copy_options::overwrite_existing, ec);
        Armed = !ec;
    }

    ~ProbeSwapGuard()
    {
        std::error_code ec;
        // 暂存文件若还在（rename 没走通那条路），留下它就是污染构建目录。
        std::filesystem::remove(Staging, ec);
        if (!Armed)
        {
            return;
        }
        ec.clear();
        std::filesystem::rename(Backup, Target, ec); // 原子换回：Target 此刻的字节是换进来那份
        if (ec)
        {
            ec.clear();
            std::filesystem::copy_file(Backup, Target, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(Backup, ec);
        }
    }

    ProbeSwapGuard(const ProbeSwapGuard&) = delete;
    ProbeSwapGuard& operator=(const ProbeSwapGuard&) = delete;
    ProbeSwapGuard(ProbeSwapGuard&&) = delete;
    ProbeSwapGuard& operator=(ProbeSwapGuard&&) = delete;

    [[nodiscard]] bool IsArmed() const { return Armed; }

private:
    std::filesystem::path Target;
    std::filesystem::path Backup;
    std::filesystem::path Staging;
    bool Armed = false;
};

TEST(Adopt, RenameReplacementCaughtByTierThree)
{
    // §8.2 的 Linux 现场：改名替换骗过档二，只有特征比对分得出新旧。
    // （Windows 的映射文件覆盖语义另成一题——v3 §12.1「某平台不可行就回来改这节」
    //   同样适用于 Win 的 sharing 规则，该侧端到端验证登记到 M4/M5，不在 M1 赌。）
    vase::PluginHost host;
    const std::filesystem::path probe{VASE_FIXTURE_LOADPROBE};
    const std::filesystem::path tmp = probe.string() + ".swap";
    const ProbeSwapGuard guard{probe, tmp}; // 守卫先立起来：下面任一条 ASSERT_* 早退都不留尾巴
    ASSERT_TRUE(guard.IsArmed());
    host.DestroyPod(host.CreatePod(Plan({{"Vase.LoadProbe", probe}})).Value()); // 驻留 + 登记

    std::error_code ec;
    std::filesystem::copy_file(VASE_FIXTURE_UNLOADPROBE, tmp, std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << ec.message();
    std::filesystem::rename(tmp, probe, ec); // Linux：旧 inode 仍映射，路径已换血——档二全绿现场
    ASSERT_FALSE(ec) << ec.message();

    const vase::PodHandle h = host.CreatePod(vase::LoadPlan{}).Value();
    const vase::Result<vase::AdoptReport> r = host.AdoptPlugin(h, "Vase.LoadProbe");
    ASSERT_FALSE(r.IsOk()); // ASSERT_：下面立刻取 GetError()，Ok 上取会终止进程
    EXPECT_NE(r.GetError().Message().find("differ"), std::string::npos);
    EXPECT_NE(r.GetError().Message().find("rebuild"), std::string::npos); // 逃生门写明（§8.2 政策）
    host.DestroyPod(h);
    // 复原由 guard 负责——本行确实不是唯一的复原点。
}

TEST(Adopt, MissingIdentityFeatureRejectedWithPointer)
{
    vase::PluginHost host;
    const vase::LoadPlan plan = Plan({{"Vase.NoBuildId", VASE_FIXTURE_NOBUILDID}});
    host.DestroyPod(host.CreatePod(plan).Value()); // CreatePod 不设身份闸（v2 语义）——先进过一回拿登记
    const vase::PodHandle h = host.CreatePod(vase::LoadPlan{}).Value();
    const vase::Result<vase::AdoptReport> r = host.AdoptPlugin(h, "Vase.NoBuildId");
    ASSERT_FALSE(r.IsOk()); // ASSERT_：下面立刻取 GetError()，Ok 上取会终止进程
    EXPECT_NE(r.GetError().Message().find("--build-id"), std::string::npos); // 报告指路补链接标志
    host.DestroyPod(h);
}
#endif

TEST(Adopt, FreshLoadBranchAlsoVerifiesAndRecordsEdges)
{
    // 卸载后的再 Adopt：load 分支 + 身份验 + 出边落账一条龙。
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}})).Value();
    ASSERT_TRUE(host.EjectPlugin(h, "Vase.Hello").IsOk());
    const vase::Result<vase::AdoptReport> r = host.AdoptPlugin(h, "Vase.Hello");
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    EXPECT_FALSE(r.Value().ReusedResidentImage);
    EXPECT_TRUE(r.Value().IdentityVerified);
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

} // namespace
