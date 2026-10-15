#pragma once

#include "Vase/Catalog/ManifestView.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/PackagePlan.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace vase
{

struct PackageManifest
{
    std::string SourceId;
    PackageSourceKind Kind = PackageSourceKind::kDynamic;
    std::string Binary;
    std::vector<ManifestEntry> Plugins;
};

// 来源表条目（T4，spec §3）：来源事实的唯一维护处；快照条目经 ManifestEntry::SourceId 关联到它。
// 单插件来源（Id = `@single/<子目录>`）与包来源共用同一形状，包 sourceId 无 `@`、两界不冲突。
struct CatalogSource
{
    std::string Id;
    bool IsPackage = false;
    PackageSourceKind Kind = PackageSourceKind::kDynamic;
    std::filesystem::path ManifestPath; // 绝对路径，自持；借用窗随下一次成功 Refresh（失败保留）
    std::filesystem::path BinaryPath;   // 动态来源的库文件；进程内来源为空
    std::vector<std::string> MemberIds; // 本来源的插件 Id，字典序
};

// 独立于 plugin.json 的包格式；Catalog 只读清单，不装载来源。
VASE_CATALOG_API Result<PackageManifest> ParsePackageManifestFile(const std::filesystem::path& file);
// 先验证整份值形，再原子替换；成员按 Id 写出，choices 等展示顺序保留。
VASE_CATALOG_API Result<void> WritePackageManifestFile(const std::filesystem::path& file,
                                                       const PackageManifest& manifest);

namespace catalog_detail
{

// 包 sourceId 的合法字符集（包格式规则）：非空、非 "."/".."、只含 [A-Za-z0-9._-]。写盘侧与
// CLI 的用法档共用这一处判定——header-only 内联、不挂导出宏（纯字符串、无 ABI 面，同 LibraryFileName.h）。
inline bool IsValidPackageSourceId(std::string_view id)
{
    if (id.empty() || id == "." || id == "..")
    {
        return false;
    }
    return std::ranges::all_of(id,
                               [](char value)
                               {
                                   return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
                                          (value >= '0' && value <= '9') || value == '.' || value == '_' ||
                                          value == '-';
                               });
}

} // namespace catalog_detail

} // namespace vase
