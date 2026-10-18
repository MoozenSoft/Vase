#include "VaseMobileResources.h"

#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Host/PackagePlan.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

namespace
{
int Fail(std::string_view reason)
{
    std::cerr << "VaseMobileManifestVerifier: " << reason << '\n';
    return 1;
}
} // namespace

int main(int argc, const char* const* argv)
{
    if (argc != 2)
    {
        return Fail("expected candidate resource directory");
    }

    vase::PluginCatalog catalog;
    const auto refreshed = catalog.Refresh(std::filesystem::path{*std::next(argv)});
    if (!refreshed.IsOk())
    {
        std::cerr << refreshed.GetError().Message() << '\n';
        return Fail("candidate package catalog failed to refresh");
    }
    const auto& sources = catalog.Sources();
    if (sources.size() != vase_mobile::kPackageResources.size() ||
        sources.size() != vase_mobile::kPackageBindings.size() || sources.size() != 2)
    {
        return Fail("wrong candidate source or generated table count");
    }

    for (const auto& resource : vase_mobile::kPackageResources)
    {
        const auto found = std::ranges::find_if(sources, [&](const vase::CatalogSource& source)
                                                { return source.Id == resource.SourceId; });
        if (found == sources.end() || !found->IsPackage || found->Kind != vase::PackageSourceKind::kInProcess ||
            resource.AssetName != std::string{"vase/"} + found->Id + "/pack.json")
        {
            return Fail("candidate source differs from generated resource table");
        }
    }

    const auto* provider = catalog.Find("Vase.PackageProvider");
    const auto* consumer = catalog.Find("Vase.PackageConsumer");
    if (provider == nullptr || provider->EnabledByDefault || consumer == nullptr || consumer->SourceId != "Consumer" ||
        provider->SourceId != "Provider")
    {
        return Fail("provider/consumer policy or source relation mismatch");
    }
    const auto requirement = std::ranges::find_if(
        consumer->Requires, [](const vase::ManifestDependency& dependency)
        { return dependency.Service == "Vase.PackageProvider.Counter" && dependency.Version == 1; });
    if (requirement == consumer->Requires.end())
    {
        return Fail("consumer requires Counter v1");
    }
    return 0;
}
