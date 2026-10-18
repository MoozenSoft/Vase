#pragma once

#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Detail/Result.h"

#include <vector>

namespace vase::catalog_detail
{

struct PendingCatalogSnapshot
{
    std::vector<ManifestEntry> Entries;
    std::vector<CatalogSource> Sources;
    std::vector<CatalogWarning> Warnings;
};

Result<void> AppendCatalogEntry(PendingCatalogSnapshot& pending, ManifestEntry entry, const CatalogSource& origin);
Result<void> AppendCatalogPackage(PendingCatalogSnapshot& pending, PackageManifest package, CatalogSource source);
void SortCatalogEntries(PendingCatalogSnapshot& pending);

} // namespace vase::catalog_detail
