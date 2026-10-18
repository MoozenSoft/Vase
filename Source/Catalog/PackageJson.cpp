#include "Vase/Catalog/PackageManifest.h"

#include "Detail/ManifestFields.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/PackagePlan.h"

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#ifdef _WIN32
#include <array>
#endif
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using nlohmann::json;
using vase::Error;
using vase::ManifestEntry;
using vase::PackageManifest;
using vase::PackageSourceKind;
using vase::Result;
using vase::catalog_detail::ManifestFieldMode;

Error PackageError(std::string_view file, std::string_view detail)
{
    return Error("package manifest \"" + std::string(file) + "\": " + std::string(detail));
}

Error PackageError(const std::filesystem::path& file, std::string_view detail)
{
    return PackageError(std::string_view(file.string()), detail);
}

std::string MemberLocation(std::size_t index, const json& member)
{
    std::string location = "plugins[" + std::to_string(index) + "]";
    if (member.is_object())
    {
        const auto id = member.find("id");
        if (id != member.end() && id->is_string())
        {
            location += " id=\"" + id->get_ref<const std::string&>() + '"';
        }
    }
    return location;
}

#ifdef _WIN32
bool IsReservedDeviceStem(std::string_view stem)
{
    // Windows 设备基名不分大小写；扩展名和 COM/LPT 的上标 ¹²³ 形式也受保留规则约束。
    constexpr std::array<std::string_view, 28> kReserved{
        "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1",
        "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9", "COM¹", "COM²", "COM³", "LPT¹", "LPT²", "LPT³",
    };
    std::string base(stem.substr(0, stem.find('.')));
    std::ranges::transform(base, base.begin(), [](char value)
                           { return value >= 'a' && value <= 'z' ? static_cast<char>(value - 'a' + 'A') : value; });
    return std::ranges::find(kReserved, base) != kReserved.end();
}
#endif

bool ValidBinaryStem(std::string_view stem)
{
    return !stem.empty() && stem != "." && stem != ".." && stem.find_first_of("/\\") == std::string_view::npos &&
           stem.find('\0') == std::string_view::npos
#ifdef _WIN32
           && stem.find_first_of("<>:\"|?*") == std::string_view::npos &&
           std::ranges::all_of(stem, [](char value) { return static_cast<unsigned char>(value) >= 32; }) &&
           !IsReservedDeviceStem(stem)
#endif
        ;
}

Result<PackageManifest> ParsePackageDocument(const json& root, std::string_view file, std::string_view subdirectoryName)
{
    if (!root.is_object())
    {
        return Result<PackageManifest>::Err(PackageError(file, "top level must be a JSON object"));
    }
    std::string offender;
    if (!vase::catalog_detail::OnlyKeys(root, {"schemaVersion", "sourceId", "source", "plugins"}, offender))
    {
        return Result<PackageManifest>::Err(PackageError(file, "unknown root field \"" + offender + '"'));
    }
    const auto version = root.find("schemaVersion");
    if (version == root.end() || !version->is_number_integer() || *version != json(1))
    {
        return Result<PackageManifest>::Err(PackageError(file, "schemaVersion must be integer 1"));
    }
    const auto sourceId = root.find("sourceId");
    if (sourceId == root.end() || !sourceId->is_string() ||
        !vase::catalog_detail::IsValidPackageSourceId(sourceId->get_ref<const std::string&>()))
    {
        return Result<PackageManifest>::Err(PackageError(file, "sourceId must be a non-empty ASCII identifier"));
    }
    PackageManifest manifest;
    manifest.SourceId = sourceId->get_ref<const std::string&>();
    const auto source = root.find("source");
    if (source == root.end() || !source->is_object())
    {
        return Result<PackageManifest>::Err(PackageError(file, "source must be an object"));
    }
    if (!vase::catalog_detail::OnlyKeys(*source, {"kind", "binary"}, offender))
    {
        return Result<PackageManifest>::Err(PackageError(file, "unknown source field \"" + offender + '"'));
    }
    const auto kind = source->find("kind");
    if (kind == source->end() || !kind->is_string())
    {
        return Result<PackageManifest>::Err(PackageError(file, "source.kind must be dynamic or inProcess"));
    }
    const auto binary = source->find("binary");
    if (*kind == "dynamic")
    {
        if (binary == source->end() || !binary->is_string() || !ValidBinaryStem(binary->get_ref<const std::string&>()))
        {
            return Result<PackageManifest>::Err(PackageError(file, "dynamic source requires a plain binary stem"));
        }
        manifest.Binary = binary->get_ref<const std::string&>();
    }
    else if (*kind == "inProcess")
    {
        manifest.Kind = PackageSourceKind::kInProcess;
        if (binary != source->end())
        {
            return Result<PackageManifest>::Err(PackageError(file, "inProcess source rejects binary"));
        }
    }
    else
    {
        return Result<PackageManifest>::Err(PackageError(file, "source.kind must be dynamic or inProcess"));
    }
    const auto plugins = root.find("plugins");
    if (plugins == root.end() || !plugins->is_array() || plugins->empty())
    {
        return Result<PackageManifest>::Err(PackageError(file, "plugins must be a non-empty array"));
    }
    std::size_t index = 0;
    for (const json& member : *plugins)
    {
        const std::string location = MemberLocation(index, member);
        auto parsed =
            vase::catalog_detail::ParsePluginFields(member, file, subdirectoryName, ManifestFieldMode::kPackageMember);
        if (!parsed.IsOk())
        {
            return Result<PackageManifest>::Err(PackageError(file, "source \"" + manifest.SourceId + "\": " + location +
                                                                       ": " + parsed.GetError().Message()));
        }
        if (std::ranges::find(manifest.Plugins, parsed.Value().Id, &ManifestEntry::Id) != manifest.Plugins.end())
        {
            return Result<PackageManifest>::Err(
                PackageError(file, "source \"" + manifest.SourceId + "\": " + location + ": duplicate plugin id"));
        }
        manifest.Plugins.push_back(std::move(parsed.Value()));
        ++index;
    }
    return Result<PackageManifest>::Ok(std::move(manifest));
}

} // namespace

namespace vase
{

Result<PackageManifest> ParsePackageManifest(std::string_view contents, std::string_view location)
{
    const auto read = catalog_detail::ReadJsonContents(contents, location);
    if (!read.IsOk())
    {
        return Result<PackageManifest>::Err(PackageError(location, read.GetError().Message()));
    }
    auto parsed = ParsePackageDocument(read.Value(), location, {});
    if (parsed.IsOk())
    {
        for (ManifestEntry& member : parsed.Value().Plugins)
        {
            member.Subdirectory = parsed.Value().SourceId;
        }
    }
    return parsed;
}

Result<PackageManifest> ParsePackageManifestFile(const std::filesystem::path& file)
{
    const auto read = catalog_detail::ReadJsonFile(file);
    if (!read.IsOk())
    {
        return Result<PackageManifest>::Err(PackageError(file, read.GetError().Message()));
    }
    return ParsePackageDocument(read.Value(), file.string(), file.parent_path().filename().string());
}

Result<void> WritePackageManifestFile(const std::filesystem::path& file, const PackageManifest& manifest)
{
    json source = json::object();
    switch (manifest.Kind)
    {
    case PackageSourceKind::kDynamic:
        source.emplace("kind", "dynamic");
        source.emplace("binary", manifest.Binary);
        break;
    case PackageSourceKind::kInProcess:
        if (!manifest.Binary.empty())
        {
            return Result<void>::Err(PackageError(file, "inProcess source rejects binary"));
        }
        source.emplace("kind", "inProcess");
        break;
    default:
        return Result<void>::Err(PackageError(file, "invalid source kind"));
    }
    std::vector<const ManifestEntry*> members;
    members.reserve(manifest.Plugins.size());
    for (const ManifestEntry& member : manifest.Plugins)
    {
        members.push_back(&member);
    }
    std::ranges::sort(members,
                      [](const ManifestEntry* left, const ManifestEntry* right) { return left->Id < right->Id; });
    json plugins = json::array();
    for (const ManifestEntry* member : members)
    {
        const auto values = catalog_detail::ValidatePluginValues(*member, file);
        if (!values.IsOk())
        {
            return Result<void>::Err(
                PackageError(file, "plugin id=\"" + member->Id + "\": " + values.GetError().Message()));
        }
        plugins.push_back(catalog_detail::SerializePluginFields(*member, ManifestFieldMode::kPackageMember));
    }
    const json root{
        {"schemaVersion", 1},
        {"sourceId", manifest.SourceId},
        {"source", std::move(source)},
        {"plugins", std::move(plugins)},
    };
    const auto checked = ParsePackageDocument(root, file.string(), file.parent_path().filename().string());
    if (!checked.IsOk())
    {
        return Result<void>::Err(checked.GetError());
    }
    return catalog_detail::WriteJsonAtomically(file, root);
}

} // namespace vase
