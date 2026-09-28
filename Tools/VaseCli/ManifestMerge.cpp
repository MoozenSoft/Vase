#include "ManifestMerge.h"

#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Detail/Result.h"

#include <filesystem>
#include <system_error>
#include <utility>

namespace tools::cli
{

MergeOutcome MergeWithExisting(const std::filesystem::path& manifestFile, vase::ManifestEntry fromBinary)
{
    MergeOutcome out;
    out.Entry = std::move(fromBinary);
    std::error_code ec;
    if (!std::filesystem::exists(manifestFile, ec) && !ec)
    {
        return out; // 首次生成：enabledByDefault 走缺省（true ⇒ 省略，D122）
    }
    out.HadManifest = true; // ec 置位按存在处理：让解析出面报真错，不静默跳过合并
    const vase::Result<vase::ManifestEntry> old = vase::ParseManifestFile(manifestFile, out.Entry.Subdirectory);
    if (!old.IsOk())
    {
        // 旧清单读不出：以二进制为准（它就是权威）；归因经 ParseError 交给调用方打印（D134 逐条报）。
        out.ParseError = old.GetError().Message();
        return out;
    }
    out.Entry.EnabledByDefault = old.Value().EnabledByDefault; // 唯一保真的字段（D122）
    if (old.Value().Binary != out.Entry.Binary)
    {
        out.PreviousBinary = old.Value().Binary;
    }
    return out;
}

} // namespace tools::cli
