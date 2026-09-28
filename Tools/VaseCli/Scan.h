#pragma once

#include "Vase/Catalog/ManifestView.h"
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

struct Discovery
{
    std::vector<DiscoveredPlugin> Found;
    std::vector<std::string> Skipped; // 既无库也无清单的子目录名（打印用，不影响退出码）
    std::vector<std::string> Failed;  // 真歧义与坏树（响亮，影响退出码）
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

} // namespace tools::cli
