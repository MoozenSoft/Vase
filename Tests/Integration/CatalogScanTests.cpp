// spec §3 的扫描面：枚举/跳过/查重/事务/借用窗/目录闸（D50/D58/D61/D64a）。
// 全部走真文件系统（temp 沙箱），不碰二进制。

#include "Vase/Catalog/PluginCatalog.h"

#include "Vase/Catalog/LoadRequest.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Config/Value.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/ManifestExpectation.h"

#include "CatalogSandbox.h"

#include <gtest/gtest.h>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

namespace
{

using testing_support::CatalogSandbox;
using vase::ManifestEntry;
using vase::PluginCatalog;

void PutManifest(const CatalogSandbox& sandbox, const std::string& dir, const std::string& id)
{
    sandbox.WriteFile(dir + "/plugin.json", R"({"schemaVersion":1,"id":")" + id + R"("})");
}

// bugprone-unchecked-optional-access 不认 ASSERT/EXPECT 级 has_value 断言（同 SolveTests 的 Unwrap 裁定）。
template <typename T>
const T* Expect(const std::optional<T>& found)
{
    if (!found.has_value())
    {
        ADD_FAILURE() << "optional empty";
        return nullptr;
    }
    return &*found;
}

TEST(CatalogScan, EmptyDirIsLegalEmptySnapshot)
{
    CatalogSandbox sandbox{"scan-empty"};
    sandbox.CreateDir("stray-dir"); // 唯一的子目录，且没有清单 → warn + 跳过（D50）
    PluginCatalog catalog;
    const auto refreshed = catalog.Refresh(sandbox.Root);
    ASSERT_TRUE(refreshed.IsOk()) << refreshed.GetError().Message();
    EXPECT_TRUE(catalog.Ids().empty());
    ASSERT_EQ(catalog.Warnings().size(), 1U);
    EXPECT_EQ(catalog.Warnings().begin()->Subdirectory, "stray-dir");
}

TEST(CatalogScan, NonexistentDirRejects)
{
    CatalogSandbox sandbox{"scan-nodir"};
    PluginCatalog catalog;
    EXPECT_FALSE(catalog.Refresh(sandbox.Root / "missing").IsOk());
}

TEST(CatalogScan, StrayRootFileIgnored)
{
    CatalogSandbox sandbox{"scan-stray"};
    sandbox.WriteFile("README.md", "not a plugin"); // 根散文件：忽略，连 warn 都不记
    PluginCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    EXPECT_TRUE(catalog.Ids().empty());
    EXPECT_TRUE(catalog.Warnings().empty());
}

TEST(CatalogScan, IdsSortedAndFindWorks)
{
    CatalogSandbox sandbox{"scan-order"};
    PutManifest(sandbox, "zeta", "Vase.Z");
    PutManifest(sandbox, "alpha", "Vase.A");
    PluginCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    const auto ids = catalog.Ids();
    ASSERT_EQ(ids.size(), 2U);
    EXPECT_EQ(*ids.begin(), "Vase.A"); // Id 字典序（D53 同源）
    EXPECT_EQ(*std::next(ids.begin(), 1), "Vase.Z");
    const ManifestEntry* found = catalog.Find("Vase.Z");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->Subdirectory, "zeta"); // binary 缺省来自子目录名
    EXPECT_EQ(found->Binary, "zeta");
    EXPECT_EQ(catalog.Find("Vase.Missing"), nullptr);
}

TEST(CatalogScan, DuplicateIdRejectsWholeRefresh)
{
    CatalogSandbox sandbox{"scan-dup"};
    PutManifest(sandbox, "one", "Vase.Same");
    PutManifest(sandbox, "two", "Vase.Same");
    PluginCatalog catalog;
    const auto first = catalog.Refresh(sandbox.Root);
    ASSERT_FALSE(first.IsOk());
    const std::string& msg = first.GetError().Message();
    EXPECT_NE(msg.find("duplicate plugin id"), std::string::npos); // 最小区分子串
    EXPECT_NE(msg.find("\"one\""), std::string::npos);             // 两个子目录都要点名（D50 可诊断）
    EXPECT_NE(msg.find("\"two\""), std::string::npos);
    EXPECT_TRUE(catalog.Ids().empty()); // D58：失败不留半成品
}

TEST(CatalogScan, BadManifestRejectsAndOldSnapshotAndWarningsStand)
{
    CatalogSandbox sandbox{"scan-bad"};
    PutManifest(sandbox, "good", "Vase.Good");
    sandbox.CreateDir("zz-ghost"); // 缺清单：让 scan 1 的旧 Warnings 非空（D64a 摆脱空转断言）
    PluginCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    ASSERT_EQ(catalog.Warnings().size(), 1U);

    // 坏清单放 mm-broken：排在 good 之后、zz-ghost 之前 —— Err 点处 nextWarnings 为空，
    // 非事务实现会把旧的非空 Warnings 换成空，下面的断言当场变红。
    sandbox.WriteFile("mm-broken/plugin.json", "{\"schemaVersion\":1,,}"); // 坏清单（D50）
    const auto second = catalog.Refresh(sandbox.Root);
    EXPECT_FALSE(second.IsOk());
    const auto ids = catalog.Ids(); // 旧快照与旧 Warnings 都不换（D58+D64a）
    ASSERT_EQ(ids.size(), 1U);
    EXPECT_EQ(*ids.begin(), "Vase.Good");
    ASSERT_EQ(catalog.Warnings().size(), 1U);
    EXPECT_EQ(catalog.Warnings().begin()->Subdirectory, "zz-ghost");
}

TEST(CatalogScan, BorrowWindowFollowsRefresh)
{
    CatalogSandbox sandbox{"scan-borrow"};
    PutManifest(sandbox, "a", "Vase.Old");
    PluginCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    ASSERT_NE(catalog.Find("Vase.Old"), nullptr);

    PutManifest(sandbox, "a", "Vase.New"); // 同一文件换 Id 再扫
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    EXPECT_EQ(catalog.Find("Vase.Old"), nullptr); // 旧快照随 Refresh 作废（D61 借用窗的另一半）
    EXPECT_NE(catalog.Find("Vase.New"), nullptr);
}

// §3.3/D61+D84：Refresh(A)→Solve→Refresh(B)→Solve 后新快照全量再绑定；旧 plan 的 Expected 是拥有深拷，
// 跨窗完好——旧 plan 唯余的借用人 Id 在此不被触碰（纪律断言：证人自身遵守即成立）。
TEST(CatalogWindow, RebindAfterSecondRefresh)
{
    CatalogSandbox sandbox{"scan-rebind"};
    sandbox.WriteFile(
        "snap-a/alpha/plugin.json",
        R"({"schemaVersion":1,"id":"Vase.Alpha","version":"1.0.0","config":[{"key":"n","type":"int32","default":5}]})");
    sandbox.WriteFile("snap-b/beta/plugin.json", R"({"schemaVersion":1,"id":"Vase.Beta","version":"9.9.9"})");
    PluginCatalog catalog;
    const vase::LoadRequest request;

    ASSERT_TRUE(catalog.Refresh(sandbox.Root / "snap-a").IsOk());
    auto first = catalog.Solve(request);
    ASSERT_TRUE(first.IsOk()) << first.GetError().Message();
    const vase::SolveOutcome planA = std::move(first.Value());

    ASSERT_TRUE(catalog.Refresh(sandbox.Root / "snap-b").IsOk());
    auto second = catalog.Solve(request);
    ASSERT_TRUE(second.IsOk()) << second.GetError().Message();
    const vase::SolveOutcome planB = std::move(second.Value());

    ASSERT_EQ(planB.Plan.Ordered.size(), 1U); // 新 Plan 的 Id/内容全按快照 B
    EXPECT_EQ(planB.Plan.Ordered.begin()->Id, "Vase.Beta");
    EXPECT_EQ(catalog.Directory(), sandbox.Root / "snap-b");
    EXPECT_EQ(catalog.Find("Vase.Alpha"), nullptr); // Find 只命中 B
    EXPECT_NE(catalog.Find("Vase.Beta"), nullptr);

    ASSERT_EQ(planA.Plan.Ordered.size(), 1U);
    const vase::LoadPlanEntry& oldEntry = *planA.Plan.Ordered.begin();
    const vase::ManifestExpectation* expected = Expect(oldEntry.Expected);
    ASSERT_NE(expected, nullptr);
    EXPECT_EQ(expected->Id, "Vase.Alpha"); // 旧 plan 的期望不随第二次 Refresh 改变（拥有形，D84）
    EXPECT_EQ(expected->Version, "1.0.0");
    ASSERT_EQ(expected->Config.size(), 1U);
    EXPECT_EQ(expected->Config.begin()->Key, "n");
    EXPECT_EQ(expected->Config.begin()->Kind, vase::ValueKind::kInt32);
}

} // namespace
