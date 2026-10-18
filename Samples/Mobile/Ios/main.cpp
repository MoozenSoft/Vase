#include "BundleResources.h"
#include "MobileScenario.h"
#include "Vase/Catalog/PackageManifest.h"

#include <vector>

// Link probe: contains the actual resource→Catalog→Solve→Pod path.
// This cross-linked iPhoneOS executable is not run on macOS as a fake device.
int main()
{
    auto loaded = vase_mobile::ReadBundlePackageResources();
    if (!loaded.IsOk())
    {
        return 2;
    }
    std::vector<vase::PackageResource> borrowed;
    borrowed.reserve(loaded.Value().size());
    for (const auto& item : loaded.Value())
    {
        borrowed.push_back(vase::PackageResource{.Location = item.Location, .Contents = item.Contents});
    }
    const auto report = vase_mobile::RunPackageScenario(borrowed);
    if (!report.IsOk() || !report.Value().Clean || report.Value().CompletedRounds != 50U)
    {
        return 3;
    }
    return 0;
}
