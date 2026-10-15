#include "ManifestMerge.h"

#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Detail/Result.h"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <utility>
#include <vector>

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

PackageMergeOutcome MergePackageWithExisting(const std::filesystem::path& manifestFile,
                                             vase::PackageManifest fromBinary)
{
    PackageMergeOutcome out;
    // 成员序先归一为 Id 序（写出口径，T2）：Added 于是与写出的文档同序，不随描述符枚举序漂移。
    std::ranges::sort(fromBinary.Plugins, [](const vase::ManifestEntry& left, const vase::ManifestEntry& right)
                      { return left.Id < right.Id; });
    out.Manifest = std::move(fromBinary);
    std::error_code ec;
    if (!std::filesystem::exists(manifestFile, ec) && !ec)
    {
        return out; // 首次生成：成员一律默认启用（spec §5.3）——「新增」名单到读到旧文档才有意义
    }
    const vase::Result<vase::PackageManifest> old = vase::ParsePackageManifestFile(manifestFile);
    if (!old.IsOk())
    {
        // 旧清单读不出：以二进制为准，但归因经 ParseError 交调用方——默认写失败，显式 --regenerate 才重建。
        out.ParseError = old.GetError().Message();
        return out;
    }
    const PackageSourceFacts previous{
        .SourceId = old.Value().SourceId,
        .Kind = old.Value().Kind,
        .Binary = old.Value().Binary,
    };
    if (previous.SourceId != out.Manifest.SourceId || previous.Kind != out.Manifest.Kind ||
        previous.Binary != out.Manifest.Binary)
    {
        out.PreviousSource = previous;
    }
    for (vase::ManifestEntry& member : out.Manifest.Plugins)
    {
        const auto found = std::ranges::find(old.Value().Plugins, member.Id, &vase::ManifestEntry::Id);
        if (found == old.Value().Plugins.end())
        {
            out.Added.push_back(member.Id); // 新成员：缺键 ≡ true（D122），即默认启用
            continue;
        }
        // enabledByDefault 是包成员里唯一没有描述符背书的字段（与单插件同律，D73/D122），按 Id 保真。
        member.EnabledByDefault = found->EnabledByDefault;
    }
    for (const vase::ManifestEntry& member : old.Value().Plugins)
    {
        if (std::ranges::find(out.Manifest.Plugins, member.Id, &vase::ManifestEntry::Id) == out.Manifest.Plugins.end())
        {
            out.Removed.push_back(member.Id); // 旧文件序可能不是 Id 序：排序留给下面的统一排序
        }
    }
    std::ranges::sort(out.Removed);
    return out;
}

} // namespace tools::cli
