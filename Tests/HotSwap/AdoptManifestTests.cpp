// T9/D81/D84 端到端证人：AdoptInto 从快照目录**就地重读**清单、装成期望形、喂给 Host 的
// 清单轨——「清单 → 期望 → 比对」链条的最后一环。套件名前缀 Adopt，自动进规矩 6 选择子。

#include "Vase/Catalog/CatalogAdopt.h"

#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Evidence.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Pod.h"

#include "CatalogSandbox.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <iterator>
#include <string>
// std::error_code 的归属头两平台判定不同（同 AdoptTests.cpp 头部注释），留着并抑制。
#include <system_error> // NOLINT(misc-include-cleaner)

namespace
{

using testing_support::CatalogSandbox;
using testing_support::RefreshOrFail;

// 声明序 = 析构逆序：Sandbox 的 temp 树要活到 Host 拆完结局（AssemblyFromSolve 同因）。
class AdoptManifest : public ::testing::Test
{
public:
    CatalogSandbox Sandbox{"adopt-manifest"};
    vase::PluginHost Host;
    vase::PluginCatalog Catalog;

    [[nodiscard]] std::filesystem::path HelloDll() const
    {
        return Sandbox.Root / "VaseHello" / std::filesystem::path{VASE_FIXTURE_HELLO}.filename();
    }

    [[nodiscard]] std::filesystem::path HelloManifest() const { return Sandbox.Root / "VaseHello" / "plugin.json"; }

    // 真 HelloPlugin DLL + T6 校准过的 hello 清单落 VaseHello/，刷快照。
    void StageHello()
    {
        const std::filesystem::path hello{VASE_FIXTURE_HELLO};
        Sandbox.CopyFile("VaseHello/" + hello.filename().string(), hello);
        Sandbox.CopyFile("VaseHello/plugin.json",
                         std::filesystem::path{VASE_FIXTURE_MANIFESTS} / "hello" / "plugin.json");
        RefreshOrFail(Catalog, Sandbox);
    }

    // 手写裸计划（无 Expected——pod 创建侧的 M2a 通道仍合法）入局并弹掉 Hello。
    vase::PodHandle EjectedHelloPod()
    {
        vase::LoadPlan plan;
        plan.Ordered.push_back({.Id = "Vase.Hello", .BinaryPath = HelloDll()});
        const vase::PodHandle handle = Host.CreatePod(plan).Value();
        EXPECT_TRUE(Host.EjectPlugin(handle, "Vase.Hello").IsOk());
        return handle;
    }
};

TEST_F(AdoptManifest, AdoptIntoHappyPath)
{
    StageHello();
    const vase::PodHandle h = EjectedHelloPod();
    const vase::Result<vase::AdoptReport> adopted = vase::AdoptInto(Catalog, Host, h, "Vase.Hello");
    ASSERT_TRUE(adopted.IsOk()) << adopted.GetError().Message();
    EXPECT_EQ(adopted.Value().Status, vase::AdoptStatus::kAdopted);
    EXPECT_TRUE(adopted.Value().ManifestVerified); // 清单轨比对跑过且通过（D69 新轨成功态）
    EXPECT_TRUE(Host.DestroyPod(h).Clean());
}

TEST_F(AdoptManifest, FreshManifestNotFromSnapshot)
{
    // 快照建立后改盘上清单 → 拒——证 AdoptInto 读的是磁盘现值，不是快照条目。
    StageHello();
    const vase::PodHandle h = EjectedHelloPod();
    std::ifstream in(HelloManifest(), std::ios::binary);
    ASSERT_TRUE(static_cast<bool>(in));
    const std::string text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    const auto at = text.find("示例插件"); // hello 清单里恰一次（两个 config displayName 不带它）
    ASSERT_NE(at, std::string::npos);
    std::string tampered = text;
    tampered.insert(at + std::string{"示例插件"}.size(), "X");
    Sandbox.WriteFile("VaseHello/plugin.json", tampered);

    const vase::Result<vase::AdoptReport> refused = vase::AdoptInto(Catalog, Host, h, "Vase.Hello");
    ASSERT_FALSE(refused.IsOk()); // ASSERT_：下面取 GetError()，Ok 上取会终止进程
    EXPECT_NE(refused.GetError().Message().find("manifest/binary mismatch"), std::string::npos)
        << refused.GetError().Message();
    EXPECT_TRUE(Host.DestroyPod(h).Clean());
}

TEST_F(AdoptManifest, MissingManifestInSnapshotDirErr)
{
    StageHello();
    const vase::PodHandle h = EjectedHelloPod();
    std::error_code ec;
    std::filesystem::remove(HelloManifest(), ec);
    ASSERT_FALSE(ec);
    const vase::Result<vase::AdoptReport> refused = vase::AdoptInto(Catalog, Host, h, "Vase.Hello");
    ASSERT_FALSE(refused.IsOk());
    EXPECT_NE(refused.GetError().Message().find("plugin.json"), std::string::npos); // 读失败响亮、点名文件
    EXPECT_TRUE(Host.DestroyPod(h).Clean());
}

TEST_F(AdoptManifest, IdNotInSnapshotErr)
{
    StageHello();
    const vase::PodHandle h = EjectedHelloPod();
    const vase::Result<vase::AdoptReport> refused = vase::AdoptInto(Catalog, Host, h, "Vase.Nowhere");
    ASSERT_FALSE(refused.IsOk());
    EXPECT_NE(refused.GetError().Message().find("not in catalog snapshot"), std::string::npos); // D81 归因
    EXPECT_TRUE(Host.DestroyPod(h).Clean());
}

} // namespace
