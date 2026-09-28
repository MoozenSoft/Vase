#pragma once

// scan 的回写合并（M5/D122）：描述符背书字段来自二进制，清单独有字段按类处置。
// 单独成文件是因为它是全仓唯一知道「哪些字段是清单独有」的地方——与 D73 同源。

#include "Vase/Catalog/ManifestView.h"

#include <filesystem>
#include <optional>
#include <string>

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

} // namespace tools::cli
