#pragma once

// Android JNI 与 iOS 应用共用的单局证人；资源拥有值只在平台适配层使用。
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Detail/Result.h"

#include <cstdint>
#include <span>
#include <string>

namespace vase_mobile
{
struct OwnedPackageResource
{
    std::string Location;
    std::string Contents;
};

struct ScenarioReport
{
    std::uint32_t CompletedRounds = 0;
    bool Clean = false;
};

[[nodiscard]] vase::Result<ScenarioReport> RunPackageScenario(std::span<const vase::PackageResource> resources);
} // namespace vase_mobile
