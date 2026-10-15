#pragma once

#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Detail/Result.h"
#include "Vase/PluginDescriptor.h"

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace tools::cli
{

struct DiscoveredPlugin
{
    std::filesystem::path Directory;  // <插件目录>/<子目录>
    std::filesystem::path BinaryPath; // 该子目录里的那个库
    std::string Stem;                 // LibraryStem 的结果 = 清单 binary 该写的值（D122）
};

// 已有 pack.json 的一级子目录（spec §5.3）：来源信息取自盘上那份文档，不数库、不猜路径。
struct DiscoveredPackage
{
    std::filesystem::path Directory;    // <插件目录>/<包子目录>
    std::filesystem::path ManifestPath; // <包目录>/pack.json
    std::filesystem::path BinaryPath;   // 动态来源的库文件；进程内来源为空（普通 CLI 枚举不了它）
    vase::PackageManifest Manifest;     // 已解析：动态来源的库文件名由它的 binary 拼
};

struct Discovery
{
    std::vector<DiscoveredPlugin> Found;
    std::vector<DiscoveredPackage> Packages; // 包目录：动态来源按来源事实更新，静态来源如实报缺枚举能力
    std::vector<std::string> Skipped;        // 既无库也无清单的子目录名（打印用，不影响退出码）
    std::vector<std::string> Failed;         // 真歧义与坏树（响亮，影响退出码）
};

// 目录不存在 / 不是目录 → Err。**零插件树不是 Err**：由调用方按 D135 判退出码 2
// （这里只如实报告「一个都没有」，不替调用方决定那算不算用法错误）。
[[nodiscard]] vase::Result<Discovery> DiscoverPlugins(const std::filesystem::path& pluginDirectory);

// 描述符元信息 → 清单值（M5/D122）。**借用 → 拥有的唯一物化点**：返回的每个字符串都已深拷，
// 调用方可在 Loader::Unload 之后安全使用（spec §2.2 的寿命纪律）。
// 清单独有字段（enabledByDefault）不在这里定——那是合并的职责（Task 10）。
[[nodiscard]] vase::ManifestEntry ManifestEntryFromMeta(const vase::PluginMeta& meta, const std::string& subdirectory,
                                                        const std::string& binaryStem);

// 生成清单：遍历一级子目录 → 每个子目录一个库 → 生成/回写 plugin.json。
// 退出码见 Cli.h。
int RunScan(const std::string& pluginDirectory, std::ostream& out, std::ostream& err);

// 显式包扫描（T7，spec §5.2 命令矩阵）：调用者给出那一个动态库与来源标识，工具**不猜目录**——
// 固定写 BinaryPath.parent_path()/pack.json，其他库不参与成员发现。
struct PackageScanOptions
{
    std::filesystem::path BinaryPath;
    std::string SourceId;
    bool Regenerate = false;
};

// 整份包文档为单位物化（成员按 Id 排序），成功才原子替换；失败保留原文件。
int ScanPackage(const PackageScanOptions& options, std::ostream& out, std::ostream& err);

// argv 腿（形状固定：scan --package <动态库文件> --source-id <标识> [--regenerate]）：
// 重名 / 缺值 / 多余 flag 一律用法档，不猜意图。
int RunPackageScan(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

} // namespace tools::cli
