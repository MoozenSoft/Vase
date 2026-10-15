#pragma once

// Catalog 层（spec §2/§3）：唯一 JSON 消费者、唯一插件目录扫描者；站 VaseHost 之上
// 产定形 LoadPlan（D45）。宿主/工具自持的普通对象：不在 PluginHost 的线程绑定契约
// （§1.4）范围内，无内部锁，契约随宿主用法（spec §3「线程」段原文）。

#include "Vase/Catalog/Analysis.h"
#include "Vase/Catalog/LoadRequest.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"

#include <filesystem>
#include <span>
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
    // 来源表（T4）：按一级目录名顺序产出，非 Id 序；借用窗与 Warnings 同（下一次成功 Refresh）。
    [[nodiscard]] const std::vector<CatalogSource>& Sources() const { return SnapshotSources; }
    // 插件 → 所属来源。未收录的 Id → nullptr。
    [[nodiscard]] const CatalogSource* SourceFor(std::string_view pluginId) const;

    // 事务性（D58+D64a）：成功才整体换快照、来源表与 Warnings；失败三者都不换。
    // 目录不存在 → Err；子目录两类清单均缺 → 跳过 + Warnings（D50）；
    // 两类清单同处一目录 / 坏清单 / 重复 Id / 重复 sourceId → Err（D50）。入参绝对化后存为 Directory()。
    Result<void> Refresh(const std::filesystem::path& pluginDirectory);

    // 纯数据分析（T5，spec §4.1）：与 Solve 共用同一套参与判定、依赖闭包、冲突检查、拓扑排序、
    // 配置合并与归因算法；不装载库、不调用入口、不依赖静态绑定。结果全自持、可安全 move。
    // 首扫前调用 → Err（D63）；确定性、错误/跳过区分与 Notes 去重键契约不变。
    Result<AnalysisOutcome> Analyze(const LoadRequest& request) const;

    // 可执行计划（T5，spec §4.2）：Analyze 之后按来源表物化，静态来源取 bindings 的入口。
    // 单参形态保留、委派空绑定集合；Plan 里余下唯一借用 = Ordered[i].Id 的快照窗
    // （D61，窗 = 到下一次 Refresh）；Expected 自 T9 起为拥有值形、逐条目深拷（D84），必有值。
    Result<SolveOutcome> Solve(const LoadRequest& request) const;
    Result<SolveOutcome> Solve(const LoadRequest& request, std::span<const InProcessBinding> bindings) const;

private:
    std::filesystem::path SnapshotDirectory;    // 首次 Refresh 后为绝对路径
    std::vector<ManifestEntry> SnapshotEntries; // Id 字典序（Find 二分与 D53 同源）
    std::vector<CatalogSource> SnapshotSources; // 一级目录名序
    std::vector<CatalogWarning> SnapshotWarnings;
    bool HasScanned = false; // 首次成功 Refresh 后置位（D63），T5 的 Solve 前置读它
};

// 单文件清单解析（spec §4 全执法线；unknown 字段拒 + D64 硬闸）。subdirectoryName
// 只参与 binary 缺省与 Subdirectory 字段，不校验与 Id 的关系。
VASE_CATALOG_API Result<ManifestEntry> ParseManifestFile(const std::filesystem::path& file,
                                                         std::string_view subdirectoryName);

} // namespace vase
