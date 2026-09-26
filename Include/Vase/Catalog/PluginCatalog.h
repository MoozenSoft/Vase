#pragma once

// Catalog 层（spec §2/§3）：唯一 JSON 消费者、唯一插件目录扫描者；站 VaseHost 之上
// 产定形 LoadPlan（D45）。宿主/工具自持的普通对象：不在 PluginHost 的线程绑定契约
// （§1.4）范围内，无内部锁，契约随宿主用法（spec §3「线程」段原文）。

#include "Vase/Catalog/LoadRequest.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace vase
{

class VASE_CATALOG_API PluginCatalog
{
public:
    PluginCatalog(); // 定义在 cpp：Windows 类级 dllexport 要求全部声明的成员有出定义
    ~PluginCatalog();

    // 宿主自持、不转让所有权；拷贝还会让 D61 的「Plan 借快照」语义当场悬垂——四件全禁。
    PluginCatalog(const PluginCatalog&) = delete;
    PluginCatalog& operator=(const PluginCatalog&) = delete;
    PluginCatalog(PluginCatalog&&) = delete;
    PluginCatalog& operator=(PluginCatalog&&) = delete;

    [[nodiscard]] std::vector<std::string> Ids() const;                 // 值拷贝，字典序
    [[nodiscard]] const ManifestEntry* Find(std::string_view id) const; // 借用快照，下次 Refresh 失效（D61）
    [[nodiscard]] const std::vector<CatalogWarning>& Warnings() const { return SnapshotWarnings; }
    [[nodiscard]] const std::filesystem::path& Directory() const { return SnapshotDirectory; }

    // 事务性（D58+D64a）：成功才整体换快照与 Warnings；失败两者都不换。
    // 目录不存在 → Err；子目录缺 plugin.json → 跳过 + Warnings（D50）；
    // 坏清单 / 重复 Id → Err（D50）。入参绝对化后存为 Directory()。
    Result<void> Refresh(const std::filesystem::path& pluginDirectory);

    // 纯函数，吃当前快照；spec §6 全语义。返回 Plan 里余下唯一借用 = Ordered[i].Id 的快照窗
    // （D61，窗 = 到下一次 Refresh）；Expected 自 T9 起为拥有值形、逐条目深拷（D84），必有值。
    // 首扫前调用 → Err（D63）。
    Result<SolveOutcome> Solve(const LoadRequest& request) const;

private:
    std::filesystem::path SnapshotDirectory;    // 首次 Refresh 后为绝对路径
    std::vector<ManifestEntry> SnapshotEntries; // Id 字典序（Find 二分与 D53 同源）
    std::vector<CatalogWarning> SnapshotWarnings;
    bool HasScanned = false; // 首次成功 Refresh 后置位（D63），T5 的 Solve 前置读它
};

// 单文件清单解析（spec §4 全执法线；unknown 字段拒 + D64 硬闸）。subdirectoryName
// 只参与 binary 缺省与 Subdirectory 字段，不校验与 Id 的关系。
VASE_CATALOG_API Result<ManifestEntry> ParseManifestFile(const std::filesystem::path& file,
                                                         std::string_view subdirectoryName);

} // namespace vase
