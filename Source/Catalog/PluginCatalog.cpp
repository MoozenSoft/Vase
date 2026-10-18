#include "Vase/Catalog/PluginCatalog.h"

#include "Vase/Catalog/CatalogAdopt.h"
#include "Vase/Catalog/LibraryFileName.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Config/FieldInfo.h"
#include "Vase/Config/Value.h"
#include "Vase/Detail/Fail.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/ConfigBlob.h"
#include "Vase/Host/Evidence.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/Host/PackagePlan.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Pod.h"

#include "Detail/CatalogSnapshot.h"
#include "Detail/ChoiceCoerce.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace vase
{
namespace
{

// 清单 Value（位形/借用文本）→ 期望拥有形；kNone = 未设 → nullopt（P-3 对位）。kString 在此
// 深拷——期望树零借用窗（ManifestExpectation.h 头注）。
std::optional<ConfigBlob::Storage> StorageOf(const Value& view)
{
    switch (view.Kind)
    {
    case ValueKind::kNone:
        return std::nullopt;
    case ValueKind::kBool:
        return ConfigBlob::Storage{view.GetAs<bool>()};
    case ValueKind::kInt32:
        return ConfigBlob::Storage{view.GetAs<std::int32_t>()};
    case ValueKind::kInt64:
        return ConfigBlob::Storage{view.GetAs<std::int64_t>()};
    case ValueKind::kFloat:
        return ConfigBlob::Storage{view.GetAs<float>()};
    case ValueKind::kDouble:
        return ConfigBlob::Storage{view.GetAs<double>()};
    case ValueKind::kString:
        return ConfigBlob::Storage{std::string{view.GetAs<const char*>()}};
    case ValueKind::kEnum:
        return ConfigBlob::Storage{ConfigBlob::EnumStored{.Value = view.GetAs<std::int32_t>()}};
    }
    return std::nullopt;
}

// enum 的 default 是 label 中间形（D80），在此换 value——唯一转换位仍是 ChoiceCoerce。
// 成员资格解析期已闸（D80 硬闸），换不到 = 编程错误，防御与 Solve 的 buildLoadEntry 同形。
std::optional<ConfigBlob::Storage> DefaultStorageOf(const ManifestConfigField& field)
{
    if (field.Kind != ValueKind::kEnum)
    {
        return StorageOf(field.DefaultValue());
    }
    const std::optional<std::int32_t> mapped = catalog_detail::LabelToValue(field.DefaultStr, field.Choices);
    if (!mapped.has_value())
    {
        detail::ProgrammerError("BuildExpectation: enum default label not in choices");
    }
    return ConfigBlob::Storage{ConfigBlob::EnumStored{.Value = *mapped}};
}

ExpectedConfigField ConvertField(const ManifestConfigField& field)
{
    ExpectedConfigField out;
    out.Key = field.Key;
    out.Kind = field.Kind;
    out.Default = DefaultStorageOf(field);
    out.Min = StorageOf(field.MinValue());
    out.Max = StorageOf(field.MaxValue());
    out.Label = field.DisplayName;
    out.Choices.reserve(field.Choices.size());
    for (const ManifestChoice& choice : field.Choices)
    {
        out.Choices.push_back(ExpectedChoice{.Value = choice.Value, .Label = choice.Label});
    }
    return out;
}

std::vector<ExpectedService> ConvertServices(const std::vector<ManifestDependency>& deps)
{
    std::vector<ExpectedService> out;
    out.reserve(deps.size());
    for (const ManifestDependency& dep : deps)
    {
        out.push_back(ExpectedService{.Name = dep.Service, .Version = dep.Version});
    }
    return out;
}

// Value → ManifestConfigField 的位形编码（DefaultBits/MinBits/MaxBits）：与 ManifestView.h 的
// Decode* 访问器是同一套编码，两者必须同源——三条 round-trip 用例守它（本文件与 ManifestJsonTests）。
std::uint64_t EncodeBits(const Value& value)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) 与 Value::From 同机制：按 Kind 互斥取用
    return value.Bits;
}

// 一级子目录里的两类清单名（spec §3）：只在此处出现。
constexpr std::string_view kSingleManifestName = "plugin.json";
constexpr std::string_view kPackageManifestName = "pack.json";

// 单插件来源的保留命名空间（T4）：包 sourceId 限 ASCII 标识符、不含 @，两界不冲突。
constexpr std::string_view kSingleSourcePrefix = "@single/";

// 清单存在性：区分「不存在」（warn + 跳过，D50）与「stat 失败」（权限/IO，响亮 Err）。MSVC STL 的
// status 连 ENOENT 也写 ec——not_found 必须先判，否则「无清单」被误读成 stat 失败（Linux 恰好不写）。
Result<bool> IsRegularFile(const std::filesystem::path& file)
{
    std::error_code statEc;
    const std::filesystem::file_status st = std::filesystem::status(file, statEc);
    if (st.type() == std::filesystem::file_type::not_found || !statEc)
    {
        return Result<bool>::Ok(st.type() == std::filesystem::file_type::regular);
    }
    return Result<bool>::Err(
        Error("catalog refresh: cannot stat manifest \"" + file.string() + "\": " + statEc.message()));
}

} // namespace

PluginCatalog::PluginCatalog() = default;
PluginCatalog::~PluginCatalog() = default;

std::vector<std::string> PluginCatalog::Ids() const
{
    std::vector<std::string> out;
    out.reserve(SnapshotEntries.size());
    for (const ManifestEntry& entry : SnapshotEntries)
    {
        out.push_back(entry.Id);
    }
    return out;
}

const ManifestEntry* PluginCatalog::Find(std::string_view id) const
{
    const auto it = std::ranges::lower_bound(SnapshotEntries, id, {}, &ManifestEntry::Id);
    if (it != SnapshotEntries.end() && it->Id == id)
    {
        return &*it;
    }
    return nullptr;
}

const CatalogSource* PluginCatalog::SourceFor(std::string_view pluginId) const
{
    const ManifestEntry* entry = Find(pluginId);
    if (entry == nullptr)
    {
        return nullptr;
    }
    const auto it = std::ranges::find(SnapshotSources, entry->SourceId, &CatalogSource::Id);
    return it == SnapshotSources.end() ? nullptr : &*it;
}

Result<void> PluginCatalog::Refresh(const std::filesystem::path& pluginDirectory)
{
    std::error_code ec;
    std::filesystem::path abs = std::filesystem::absolute(pluginDirectory, ec);
    if (ec)
    {
        return Result<void>::Err(
            Error("catalog refresh: cannot absolutize \"" + pluginDirectory.string() + "\": " + ec.message()));
    }
    if (!std::filesystem::is_directory(abs, ec))
    {
        return Result<void>::Err(Error("catalog refresh: not a directory: \"" + abs.string() + "\""));
    }

    std::vector<std::filesystem::path> subdirectories;
    for (std::filesystem::directory_iterator it(abs, std::filesystem::directory_options::skip_permission_denied, ec),
         end;
         !ec && it != end; it.increment(ec))
    {
        if (it->is_directory(ec))
        {
            subdirectories.push_back(it->path());
        }
    }
    if (ec)
    {
        return Result<void>::Err(Error("catalog refresh: cannot enumerate \"" + abs.string() + "\": " + ec.message()));
    }
    std::ranges::sort(subdirectories, [](const std::filesystem::path& a, const std::filesystem::path& b)
                      { return a.filename().string() < b.filename().string(); });

    catalog_detail::PendingCatalogSnapshot pending;
    for (const std::filesystem::path& dir : subdirectories)
    {
        const std::string name = dir.filename().string();
        const std::filesystem::path singleManifest = dir / std::filesystem::path(kSingleManifestName);
        const std::filesystem::path packageManifest = dir / std::filesystem::path(kPackageManifestName);
        const Result<bool> hasSingle = IsRegularFile(singleManifest);
        if (!hasSingle.IsOk())
        {
            return Result<void>::Err(hasSingle.GetError());
        }
        const Result<bool> hasPackage = IsRegularFile(packageManifest);
        if (!hasPackage.IsOk())
        {
            return Result<void>::Err(hasPackage.GetError());
        }
        if (hasSingle.Value() && hasPackage.Value())
        {
            // 不隐式设优先级：一个目录一类文档（spec §3）。
            return Result<void>::Err(Error("catalog refresh: \"" + name + "\" holds both plugin.json and pack.json"));
        }
        if (!hasSingle.Value() && !hasPackage.Value())
        {
            pending.Warnings.push_back(
                CatalogWarning{.Subdirectory = name, .Message = "no plugin.json or pack.json — skipped"});
            continue;
        }

        CatalogSource source;
        if (hasSingle.Value())
        {
            Result<ManifestEntry> parsed = ParseManifestFile(singleManifest, name);
            if (!parsed.IsOk())
            {
                // D50：坏清单整体响；失败不换快照/Warnings/来源表（D58/D64a）。
                return Result<void>::Err(parsed.GetError());
            }
            ManifestEntry entry = std::move(parsed.Value());
            source.Id = std::string(kSingleSourcePrefix) + name;
            source.ManifestPath = singleManifest;
            source.BinaryPath = dir / std::filesystem::path(catalog_detail::LibraryFileName(entry.Binary));
            source.MemberIds.push_back(entry.Id);
            const Result<void> appended = catalog_detail::AppendCatalogEntry(pending, std::move(entry), source);
            if (!appended.IsOk())
            {
                return Result<void>::Err(appended.GetError());
            }
            pending.Sources.push_back(std::move(source));
            continue;
        }

        Result<PackageManifest> parsed = ParsePackageManifestFile(packageManifest);
        if (!parsed.IsOk())
        {
            return Result<void>::Err(parsed.GetError());
        }
        PackageManifest package = std::move(parsed.Value());
        source.ManifestPath = packageManifest;
        if (package.Kind == PackageSourceKind::kDynamic)
        {
            // 库文件与 pack.json 同处包目录；目录名不参与来源标识（spec §2.1/§3）。
            source.BinaryPath = dir / std::filesystem::path(catalog_detail::LibraryFileName(package.Binary));
        }
        const auto appended = catalog_detail::AppendCatalogPackage(pending, std::move(package), std::move(source));
        if (!appended.IsOk())
        {
            return Result<void>::Err(appended.GetError());
        }
    }

    catalog_detail::SortCatalogEntries(pending);

    SnapshotDirectory = std::move(abs);
    SnapshotEntries = std::move(pending.Entries);
    SnapshotSources = std::move(pending.Sources);
    SnapshotWarnings = std::move(pending.Warnings);
    HasScanned = true;
    return Result<void>::Ok();
}

Result<void> PluginCatalog::RefreshResources(std::span<const PackageResource> resources)
{
    std::vector<PackageResource> ordered(resources.begin(), resources.end());
    for (const PackageResource& resource : ordered)
    {
        if (resource.Location.empty() || resource.Location.find('\0') != std::string_view::npos)
        {
            return Result<void>::Err(Error("catalog resources: location must be non-empty and contain no NUL: \"" +
                                           std::string(resource.Location) + "\""));
        }
    }
    std::ranges::sort(ordered, {}, &PackageResource::Location);
    catalog_detail::PendingCatalogSnapshot pending;
    std::string_view previous;
    for (const PackageResource& resource : ordered)
    {
        if (resource.Location == previous)
        {
            return Result<void>::Err(
                Error("catalog resources: duplicate location \"" + std::string(resource.Location) + "\""));
        }
        previous = resource.Location;
        auto parsed = ParsePackageManifest(resource.Contents, resource.Location);
        if (!parsed.IsOk())
        {
            return Result<void>::Err(parsed.GetError());
        }
        if (parsed.Value().Kind != PackageSourceKind::kInProcess)
        {
            return Result<void>::Err(Error("catalog resources: \"" + std::string(resource.Location) + "\" source \"" +
                                           parsed.Value().SourceId + "\" must be inProcess"));
        }
        CatalogSource source;
        source.ResourceLocation = resource.Location;
        const auto appended =
            catalog_detail::AppendCatalogPackage(pending, std::move(parsed.Value()), std::move(source));
        if (!appended.IsOk())
        {
            return Result<void>::Err(appended.GetError());
        }
    }
    catalog_detail::SortCatalogEntries(pending);
    SnapshotDirectory.clear();
    SnapshotEntries = std::move(pending.Entries);
    SnapshotSources = std::move(pending.Sources);
    SnapshotWarnings = std::move(pending.Warnings);
    HasScanned = true;
    return Result<void>::Ok();
}

ManifestExpectation BuildExpectation(const ManifestEntry& entry)
{
    ManifestExpectation expected;
    expected.Id = entry.Id;
    expected.DisplayName = entry.DisplayName;
    expected.Version = entry.Version;
    expected.Requires = ConvertServices(entry.Requires);
    expected.OptionalRequires = ConvertServices(entry.OptionalRequires);
    expected.Provides = ConvertServices(entry.Provides);
    expected.Config.reserve(entry.Config.size());
    for (const ManifestConfigField& field : entry.Config)
    {
        expected.Config.push_back(ConvertField(field));
    }
    expected.ProcessStates = entry.ProcessStates; // M3/D93：清单名字数组逐字进期望（D84 深拷）
    return expected;
}

ManifestConfigField MakeManifestConfigField(const FieldInfo& field)
{
    ManifestConfigField out;
    out.Key = field.Name;
    out.Kind = field.Kind;
    out.DisplayName = field.Label;

    if (field.Kind == ValueKind::kEnum)
    {
        const std::optional<std::string_view> label =
            catalog_detail::ValueToLabel(field.Default.GetAs<std::int32_t>(), field.Choices, field.ChoiceCount);
        if (!label.has_value())
        {
            detail::ProgrammerError("MakeManifestConfigField: enum default not in choices");
        }
        out.DefaultStr = std::string(*label); // D80 的 label 中间形
        out.Choices.reserve(field.ChoiceCount);
        for (std::uint32_t index = 0; index < field.ChoiceCount; ++index)
        {
            const ChoiceInfo& choice = *std::next(field.Choices, static_cast<std::ptrdiff_t>(index));
            out.Choices.push_back(ManifestChoice{.Value = choice.Value, .Label = choice.Label});
        }
        return out;
    }

    if (field.Kind == ValueKind::kString)
    {
        const char* text = field.Default.GetAs<const char*>();
        out.DefaultStr = text == nullptr ? std::string{} : std::string(text);
    }
    else if (field.Default.Kind != ValueKind::kNone)
    {
        out.DefaultBits = EncodeBits(field.Default);
    }

    // min/max 仅数值型可出现（解析期保证）；描述符侧用 kNone 表示未设。
    if (field.Min.Kind != ValueKind::kNone)
    {
        out.HasMin = true;
        out.MinBits = EncodeBits(field.Min);
    }
    if (field.Max.Kind != ValueKind::kNone)
    {
        out.HasMax = true;
        out.MaxBits = EncodeBits(field.Max);
    }
    return out;
}

Result<AdoptReport> AdoptInto(const PluginCatalog& catalog, PluginHost& host, PodHandle handle, std::string_view id)
{
    const ManifestEntry* located = catalog.Find(id);
    if (located == nullptr)
    {
        return Result<AdoptReport>::Err(Error("adopt: id not in catalog snapshot"));
    }
    const CatalogSource* source = catalog.SourceFor(id);
    if (source == nullptr)
    {
        return Result<AdoptReport>::Err(Error("adopt: source missing for plugin " + std::string(id)));
    }
    if (source->Kind == PackageSourceKind::kInProcess)
    {
        return Result<AdoptReport>::Err(Error("adopt: in-process source cannot be adopted from disk: " + source->Id));
    }

    ManifestExpectation expected;
    if (source->IsPackage)
    {
        Result<PackageManifest> parsed = ParsePackageManifestFile(source->ManifestPath);
        if (!parsed.IsOk())
        {
            return Result<AdoptReport>::Err(parsed.GetError());
        }
        const auto member = std::ranges::find(parsed.Value().Plugins, id, &ManifestEntry::Id);
        if (member == parsed.Value().Plugins.end())
        {
            return Result<AdoptReport>::Err(Error("adopt: package manifest \"" + source->ManifestPath.string() +
                                                  "\" no longer contains plugin " + std::string(id)));
        }
        expected = BuildExpectation(*member);
    }
    else
    {
        Result<ManifestEntry> parsed = ParseManifestFile(source->ManifestPath, located->Subdirectory);
        if (!parsed.IsOk())
        {
            return Result<AdoptReport>::Err(parsed.GetError()); // 就地重读失败响亮（CatalogAdopt.h 注）
        }
        expected = BuildExpectation(parsed.Value());
    }

    std::vector<std::string> siblings;
    for (const CatalogSource& other : catalog.Sources())
    {
        if (other.Kind != PackageSourceKind::kDynamic || other.BinaryPath == source->BinaryPath)
        {
            continue;
        }
        const std::string name = other.BinaryPath.filename().string();
        if (std::ranges::find(siblings, name) == siblings.end())
        {
            siblings.push_back(name);
        }
    }

    AdoptRequest request;
    request.Id = std::string(id);
    request.BinaryPath = source->BinaryPath;
    request.Expected = &expected;
    request.SiblingBinaries = std::move(siblings);
    return host.AdoptPlugin(handle, request);
}

} // namespace vase
