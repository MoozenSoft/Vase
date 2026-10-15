#pragma once

// scan 的回写合并（M5/D122）：描述符背书字段来自二进制，清单独有字段按类处置。
// 单独成文件是因为它是全仓唯一知道「哪些字段是清单独有」的地方——与 D73 同源。

#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PackageManifest.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace tools::cli
{

struct MergeOutcome
{
    vase::ManifestEntry Entry;
    std::optional<std::string> PreviousBinary; // 有值 = 旧清单存在且 binary 与之不同（打印用）
    std::optional<std::string> ParseError;     // 有值 = 旧清单在但读不出；调用方打警告（D134 失败逐条报）
    bool HadManifest = false;
};

// 以 fromBinary 为底，只从既有清单取回 enabledByDefault（人手语义，描述符无从推断，D73）。
// 不存在 → 原样返回 fromBinary（首次生成走缺省）；读不出 → 同，但带出 ParseError 供调用方打警告（D134）。
MergeOutcome MergeWithExisting(const std::filesystem::path& manifestFile, vase::ManifestEntry fromBinary);

// 旧包清单的来源三元组（spec §5.3「来源标识变化都明确报告」）：只读快照，格式化留给打印层。
struct PackageSourceFacts
{
    std::string SourceId;
    vase::PackageSourceKind Kind = vase::PackageSourceKind::kDynamic;
    std::string Binary;
};

struct PackageMergeOutcome
{
    vase::PackageManifest Manifest;                   // 以描述符物化为底（成员按 Id 序）
    std::vector<std::string> Added;                   // 二进制有、旧清单没有的成员 Id（读到旧文档才算）
    std::vector<std::string> Removed;                 // 旧清单有、二进制没有的成员 Id
    std::optional<PackageSourceFacts> PreviousSource; // 有值 = 来源三元组与写出值不同
    std::optional<std::string> ParseError;            // 有值 = 旧清单在但读不出：调用方决定写失败还是重建
};

// 以 fromBinary 为底：按 Id 取回旧清单的 enabledByDefault（包成员里唯一没有描述符背书的字段，同 D73），
// 并报出成员增删与来源变化。不存在 → 原样返回（首次生成，成员一律默认启用）；读不出 → 带出 ParseError。
PackageMergeOutcome MergePackageWithExisting(const std::filesystem::path& manifestFile,
                                             vase::PackageManifest fromBinary);

} // namespace tools::cli
