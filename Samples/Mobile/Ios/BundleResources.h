#pragma once

#include "MobileScenario.h"
#include "Vase/Detail/Result.h"

#include <vector>

namespace vase_mobile
{
// Load resources from the main app bundle only; no runtime staging, writing or file loader.
[[nodiscard]] vase::Result<std::vector<OwnedPackageResource>> ReadBundlePackageResources();
} // namespace vase_mobile
