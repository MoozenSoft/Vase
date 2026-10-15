// 本波 spec §3/§2.2 的发现面（T4）：两类清单一趟一级目录枚举产出、来源表与快照同事务替换。
// 全部走真文件系统（temp 沙箱），不碰二进制——静态包目录可以只有 pack.json。

#include "Vase/Catalog/LibraryFileName.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Host/PackagePlan.h"

#include "CatalogSandbox.h"

#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace
{

using testing_support::CatalogSandbox;
using testing_support::RefreshOrFail;
using vase::CatalogSource;
using vase::PluginCatalog;

void PutSingle(const CatalogSandbox& sandbox, const std::string& dir, const std::string& id)
{
    sandbox.WriteFile(dir + "/plugin.json", R"({"schemaVersion":1,"id":")" + id + R"("})");
}

std::string DynamicPack(const std::string& sourceId, const std::string& binary, const std::string& members)
{
    return R"({"schemaVersion":1,"sourceId":")" + sourceId + R"(","source":{"kind":"dynamic","binary":")" + binary +
           R"("},"plugins":[)" + members + "]}";
}

std::string StaticPack(const std::string& sourceId, const std::string& members)
{
    return R"({"schemaVersion":1,"sourceId":")" + sourceId + R"(","source":{"kind":"inProcess"},"plugins":[)" +
           members + "]}";
}

std::string Member(const std::string& id, bool enabled)
{
    return R"({"id":")" + id + R"(","enabledByDefault":)" + (enabled ? "true" : "false") + "}";
}

// 来源表按目录名顺序产出（不按 Id 排），故一律按 Id 查而不用下标。
const CatalogSource* FindSource(const PluginCatalog& catalog, const std::string& id)
{
    const auto it = std::find_if(catalog.Sources().begin(), catalog.Sources().end(),
                                 [&id](const CatalogSource& source) { return source.Id == id; });
    if (it == catalog.Sources().end())
    {
        ADD_FAILURE() << "source not found: " << id;
        return nullptr;
    }
    return &*it;
}

std::vector<std::string> Sorted(std::vector<std::string> ids)
{
    std::ranges::sort(ids);
    return ids;
}

TEST(PackageCatalog, MixedTreeDiscoversSinglePluginAndBothPackageKinds)
{
    CatalogSandbox sandbox{"package-catalog-mixed"};
    PutSingle(sandbox, "solo", "Vase.Solo");
    sandbox.WriteFile("dyn-dir/pack.json",
                      DynamicPack("DynPack", "DynLib", Member("Vase.Dyn.A", true) + "," + Member("Vase.Dyn.B", true)));
    // 静态包目录只有 pack.json：没有库文件也必须是合法快照（spec §2.1）。
    sandbox.WriteFile("stat-dir/pack.json", StaticPack("StatPack", Member("Vase.Stat.A", true)));

    PluginCatalog catalog;
    RefreshOrFail(catalog, sandbox);

    EXPECT_EQ(Sorted(catalog.Ids()),
              (std::vector<std::string>{"Vase.Dyn.A", "Vase.Dyn.B", "Vase.Solo", "Vase.Stat.A"}));
    EXPECT_TRUE(catalog.Warnings().empty());
    ASSERT_EQ(catalog.Sources().size(), 3U);

    const CatalogSource* single = FindSource(catalog, "@single/solo");
    ASSERT_NE(single, nullptr);
    EXPECT_FALSE(single->IsPackage);
    EXPECT_EQ(single->Kind, vase::PackageSourceKind::kDynamic);
    EXPECT_EQ(single->ManifestPath, sandbox.Root / "solo" / "plugin.json");
    EXPECT_EQ(single->BinaryPath, sandbox.Root / "solo" / vase::catalog_detail::LibraryFileName("solo"));
    EXPECT_EQ(single->MemberIds, (std::vector<std::string>{"Vase.Solo"}));

    const CatalogSource* dynamic = FindSource(catalog, "DynPack");
    ASSERT_NE(dynamic, nullptr);
    EXPECT_TRUE(dynamic->IsPackage);
    EXPECT_EQ(dynamic->Kind, vase::PackageSourceKind::kDynamic);
    EXPECT_EQ(dynamic->ManifestPath, sandbox.Root / "dyn-dir" / "pack.json");
    EXPECT_EQ(dynamic->BinaryPath, sandbox.Root / "dyn-dir" / vase::catalog_detail::LibraryFileName("DynLib"));
    EXPECT_EQ(dynamic->MemberIds, (std::vector<std::string>{"Vase.Dyn.A", "Vase.Dyn.B"}));

    const CatalogSource* inProcess = FindSource(catalog, "StatPack");
    ASSERT_NE(inProcess, nullptr);
    EXPECT_TRUE(inProcess->IsPackage);
    EXPECT_EQ(inProcess->Kind, vase::PackageSourceKind::kInProcess);
    EXPECT_EQ(inProcess->ManifestPath, sandbox.Root / "stat-dir" / "pack.json");
    EXPECT_TRUE(inProcess->BinaryPath.empty()); // 进程内来源不拼磁盘路径
    EXPECT_EQ(inProcess->MemberIds, (std::vector<std::string>{"Vase.Stat.A"}));
}

TEST(PackageCatalog, SourceForMapsEveryPluginToItsOwningSource)
{
    CatalogSandbox sandbox{"package-catalog-source-for"};
    PutSingle(sandbox, "solo", "Vase.Solo");
    sandbox.WriteFile("pack-dir/pack.json", StaticPack("PackA", Member("Vase.Pack.A", true)));

    PluginCatalog catalog;
    RefreshOrFail(catalog, sandbox);

    const CatalogSource* single = catalog.SourceFor("Vase.Solo");
    ASSERT_NE(single, nullptr);
    EXPECT_EQ(single->Id, "@single/solo"); // 保留命名空间：包 sourceId 不含 @，两界不冲突
    EXPECT_FALSE(single->IsPackage);

    const CatalogSource* member = catalog.SourceFor("Vase.Pack.A");
    ASSERT_NE(member, nullptr);
    EXPECT_EQ(member->Id, "PackA");
    EXPECT_TRUE(member->IsPackage);

    EXPECT_EQ(catalog.SourceFor("Vase.Missing"), nullptr);
}

TEST(PackageCatalog, PackageDirectoryNameMayDifferFromSourceId)
{
    CatalogSandbox sandbox{"package-catalog-dir-name"};
    sandbox.WriteFile("folder-name/pack.json", DynamicPack("LogicalPack", "PackLib", Member("Vase.X", true)));

    PluginCatalog catalog;
    RefreshOrFail(catalog, sandbox);

    const CatalogSource* source = FindSource(catalog, "LogicalPack");
    ASSERT_NE(source, nullptr);
    // 库路径按包目录（不是 sourceId）解析——目录名不参与来源标识。
    EXPECT_EQ(source->BinaryPath, sandbox.Root / "folder-name" / vase::catalog_detail::LibraryFileName("PackLib"));
    const vase::ManifestEntry* entry = catalog.Find("Vase.X");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->SourceId, "LogicalPack");
    EXPECT_EQ(entry->Subdirectory, "folder-name");
}

TEST(PackageCatalog, AllDisabledMembersStillRejectCrossPackageDuplicateId)
{
    CatalogSandbox sandbox{"package-catalog-duplicate"};
    // 先成功刷新一份合法来源：让「失败不换快照」有非空基准可比（否则空表区分不了「没刷新」）。
    sandbox.WriteFile("zz-base/pack.json", StaticPack("Base", Member("Vase.Base", true)));
    PluginCatalog catalog;
    RefreshOrFail(catalog, sandbox);
    ASSERT_EQ(catalog.Sources().size(), 1U);

    sandbox.WriteFile("pack-one/pack.json", StaticPack("One", Member("Vase.Dup", false)));
    sandbox.WriteFile("pack-two/pack.json", StaticPack("Two", Member("Vase.Dup", false)));

    const auto refreshed = catalog.Refresh(sandbox.Root);
    ASSERT_FALSE(refreshed.IsOk());
    const std::string& message = refreshed.GetError().Message();
    EXPECT_NE(message.find("duplicate plugin id"), std::string::npos);
    EXPECT_NE(message.find("\"pack-one\""), std::string::npos); // 两个目录都要点名（D50 可诊断）
    EXPECT_NE(message.find("\"pack-two\""), std::string::npos);
    EXPECT_EQ(catalog.Sources().size(), 1U); // 失败不留半成品来源表：基准原封不动
    EXPECT_EQ(catalog.Sources().begin()->Id, "Base");
    EXPECT_EQ(Sorted(catalog.Ids()), (std::vector<std::string>{"Vase.Base"}));
}

TEST(PackageCatalog, RejectsDirectoryHoldingBothManifests)
{
    CatalogSandbox sandbox{"package-catalog-both"};
    PutSingle(sandbox, "both-dir", "Vase.Solo");
    PluginCatalog catalog;
    RefreshOrFail(catalog, sandbox); // 先成功刷新：让「失败不换快照」有非空基准可比
    ASSERT_EQ(catalog.Sources().size(), 1U);

    sandbox.WriteFile("both-dir/pack.json", StaticPack("Pack", Member("Vase.Pack", true))); // 同目录再落一类清单

    const auto refreshed = catalog.Refresh(sandbox.Root);
    ASSERT_FALSE(refreshed.IsOk());
    const std::string& message = refreshed.GetError().Message();
    EXPECT_NE(message.find("both-dir"), std::string::npos);
    EXPECT_NE(message.find("plugin.json"), std::string::npos); // 不隐式设优先级，点名两类清单
    EXPECT_NE(message.find("pack.json"), std::string::npos);
    EXPECT_EQ(catalog.Sources().size(), 1U); // 失败不换快照：仍是首次那份单插件，不是空表
    EXPECT_EQ(catalog.Sources().begin()->Id, "@single/both-dir");
    EXPECT_EQ(Sorted(catalog.Ids()), (std::vector<std::string>{"Vase.Solo"}));
}

TEST(PackageCatalog, RejectsDuplicateSourceId)
{
    CatalogSandbox sandbox{"package-catalog-source-dup"};
    // 先成功刷新一份合法来源：让「失败不换快照」有非空基准可比（否则空表区分不了「没刷新」）。
    sandbox.WriteFile("zz-base/pack.json", StaticPack("Base", Member("Vase.Base", true)));
    PluginCatalog catalog;
    RefreshOrFail(catalog, sandbox);
    ASSERT_EQ(catalog.Sources().size(), 1U);

    sandbox.WriteFile("pack-one/pack.json", StaticPack("Same", Member("Vase.One", true)));
    sandbox.WriteFile("pack-two/pack.json", StaticPack("Same", Member("Vase.Two", true)));

    const auto refreshed = catalog.Refresh(sandbox.Root);
    ASSERT_FALSE(refreshed.IsOk());
    const std::string& message = refreshed.GetError().Message();
    EXPECT_NE(message.find("duplicate source id"), std::string::npos);
    EXPECT_NE(message.find("\"pack-one\""), std::string::npos);
    EXPECT_NE(message.find("\"pack-two\""), std::string::npos);
    EXPECT_EQ(catalog.Sources().size(), 1U); // 失败不留半成品来源表：基准原封不动
    EXPECT_EQ(catalog.Sources().begin()->Id, "Base");
    EXPECT_EQ(Sorted(catalog.Ids()), (std::vector<std::string>{"Vase.Base"}));
}

TEST(PackageCatalog, RootFilesAndLooseLibrariesAreNotScanned)
{
    CatalogSandbox sandbox{"package-catalog-root"};
    // 根目录的清单与库都不参与发现（不递归、不按扩展名猜插件），连 warning 都不记。
    sandbox.WriteFile("plugin.json", R"({"schemaVersion":1,"id":"Vase.Root"})");
    sandbox.WriteFile("pack.json", StaticPack("RootPack", Member("Vase.RootPack", true)));
    sandbox.WriteFile("loose" + std::string(vase::catalog_detail::kLibrarySuffix), "not a library");

    PluginCatalog catalog;
    RefreshOrFail(catalog, sandbox);

    EXPECT_TRUE(catalog.Ids().empty());
    EXPECT_TRUE(catalog.Sources().empty());
    EXPECT_TRUE(catalog.Warnings().empty());
}

TEST(PackageCatalog, FailedRefreshKeepsSourceSnapshot)
{
    CatalogSandbox sandbox{"package-catalog-transaction"};
    sandbox.WriteFile("P/pack.json", StaticPack("P", Member("Vase.P", true)));
    PluginCatalog catalog;
    RefreshOrFail(catalog, sandbox);
    ASSERT_EQ(catalog.Sources().size(), 1U);
    const std::filesystem::path directoryBefore = catalog.Directory();

    // 二次刷新里先放一份**新**合法包（A，目录名序在 P 前）再放坏包（Q）：若失败路径把已构造的
    // next* 提前提交，A 会留在来源表里——「旧快照原封不动」与「坏包前的部分写入」在旧布置下
    // 恰好同形（都只剩 P），加了 A 才可区分（变异取证：临时让失败路径提交 nextSources 即转红）。
    sandbox.WriteFile("A/pack.json", StaticPack("A", Member("Vase.A", true)));
    sandbox.WriteFile("Q/pack.json", "{broken");
    EXPECT_FALSE(catalog.Refresh(sandbox.Root).IsOk());

    ASSERT_EQ(catalog.Sources().size(), 1U);
    EXPECT_EQ(catalog.Sources().begin()->Id, "P"); // 来源表、快照、Warnings、Directory 四件一起保留
    EXPECT_EQ(catalog.Directory(), directoryBefore);
    EXPECT_EQ(Sorted(catalog.Ids()), (std::vector<std::string>{"Vase.P"}));
    EXPECT_EQ(catalog.Find("Vase.A"), nullptr);
    EXPECT_TRUE(catalog.Warnings().empty());
}

} // namespace
