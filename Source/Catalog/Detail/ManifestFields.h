#pragma once

#include "Vase/Catalog/ManifestView.h"
#include "Vase/Detail/Result.h"

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>

namespace vase::catalog_detail
{

enum class ManifestFieldMode : std::uint8_t
{
    kSinglePlugin,
    kPackageMember,
};

Error ManifestError(const std::filesystem::path& file, std::string_view detail);
Result<nlohmann::json> ReadJsonFile(const std::filesystem::path& file);
bool OnlyKeys(const nlohmann::json& object, const std::initializer_list<std::string_view>& allowed,
              std::string& offender);
Result<ManifestEntry> ParsePluginFields(const nlohmann::json& root, const std::filesystem::path& file,
                                        std::string_view subdirectoryName, ManifestFieldMode mode);
nlohmann::json SerializePluginFields(const ManifestEntry& entry, ManifestFieldMode mode);
Result<void> ValidatePluginValues(const ManifestEntry& entry, const std::filesystem::path& file);
Result<void> WriteJsonAtomically(const std::filesystem::path& file, const nlohmann::json& root);

} // namespace vase::catalog_detail
