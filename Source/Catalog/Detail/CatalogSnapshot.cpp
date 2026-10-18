#include "CatalogSnapshot.h"

#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Detail/Result.h"

#include <algorithm>
#include <string>
#include <utility>

namespace vase::catalog_detail
{
namespace
{

std::string OriginLabel(const CatalogSource& source)
{
    return source.ResourceLocation.empty() ? source.ManifestPath.parent_path().filename().string()
                                           : source.ResourceLocation;
}

} // namespace

Result<void> AppendCatalogEntry(PendingCatalogSnapshot& pending, ManifestEntry entry, const CatalogSource& origin)
{
    for (const ManifestEntry& held : pending.Entries)
    {
        if (held.Id == entry.Id)
        {
            const auto source = std::ranges::find(pending.Sources, held.SourceId, &CatalogSource::Id);
            const std::string previous = source == pending.Sources.end() ? held.Subdirectory : OriginLabel(*source);
            return Result<void>::Err(Error("catalog refresh: duplicate plugin id \"" + entry.Id + "\" in \"" +
                                           previous + "\" and \"" + OriginLabel(origin) + "\" (sources \"" +
                                           held.SourceId + "\" and \"" + origin.Id + "\")"));
        }
    }
    entry.SourceId = origin.Id;
    pending.Entries.push_back(std::move(entry));
    return Result<void>::Ok();
}

Result<void> AppendCatalogPackage(PendingCatalogSnapshot& pending, PackageManifest package, CatalogSource source)
{
    for (const CatalogSource& held : pending.Sources)
    {
        if (held.Id == package.SourceId)
        {
            return Result<void>::Err(Error("catalog refresh: duplicate source id \"" + package.SourceId + "\" in \"" +
                                           OriginLabel(held) + "\" and \"" + OriginLabel(source) + "\""));
        }
    }
    source.Id = package.SourceId;
    source.IsPackage = true;
    source.Kind = package.Kind;
    source.MemberIds.reserve(package.Plugins.size());
    for (ManifestEntry& member : package.Plugins)
    {
        source.MemberIds.push_back(member.Id);
        const auto appended = AppendCatalogEntry(pending, std::move(member), source);
        if (!appended.IsOk())
        {
            return Result<void>::Err(appended.GetError());
        }
    }
    std::ranges::sort(source.MemberIds);
    pending.Sources.push_back(std::move(source));
    return Result<void>::Ok();
}

void SortCatalogEntries(PendingCatalogSnapshot& pending) { std::ranges::sort(pending.Entries, {}, &ManifestEntry::Id); }

} // namespace vase::catalog_detail
