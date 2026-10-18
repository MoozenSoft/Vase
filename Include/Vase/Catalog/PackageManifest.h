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

// 只在 Parse/RefreshResources 调用期间借用；成功导入后输入字节即可释放。
struct PackageResource
{
    std::string_view Location;
    std::string_view Contents;
};

// 来源表条目（T4，spec §3）：来源事实的唯一维护处；快照条目经 ManifestEntry::SourceId 关联到它。
// 单插件来源（Id = `@single/<子目录>`）与包来源共用同一形状，包 sourceId 无 `@`、两界不冲突。
struct CatalogSource
{
    std::string Id;
    bool IsPackage = false;
    PackageSourceKind Kind = PackageSourceKind::kDynamic;
    std::filesystem::path ManifestPath; // 文件来源的绝对路径；资源来源为空
    std::filesystem::path BinaryPath;   // 动态来源的库文件；进程内来源为空
    std::vector<std::string> MemberIds; // 本来源的插件 Id，字典序
    std::string ResourceLocation;       // 资源来源的原始 UTF-8 逻辑位置；文件来源为空
};

// Catalog 宿主 API/ABI 增量：PackageResource、内容解析与 CatalogSource 尾槽；描述符 ABI 不变。
// 来源定位字段自持；借用窗到下一次成功的目录/资源导入，失败保留。
VASE_CATALOG_API Result<PackageManifest> ParsePackageManifest(std::string_view contents, std::string_view location);
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
