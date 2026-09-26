#include "Vase/Catalog/PluginCatalog.h"

#include "Vase/Catalog/CatalogAdopt.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Config/Value.h"
#include "Vase/Detail/Fail.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/ConfigBlob.h"
#include "Vase/Host/Evidence.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Pod.h"

#include "Detail/ChoiceCoerce.h"
#include "Detail/LibraryFileName.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
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

    std::vector<ManifestEntry> nextEntries;
    std::vector<CatalogWarning> nextWarnings;
    for (const std::filesystem::path& dir : subdirectories)
    {
        const std::string name = dir.filename().string();
        const std::filesystem::path manifest = dir / "plugin.json";
        // status 显式取型而非 is_regular_file 布尔：区分「清单不存在」（warn+跳过，D50）与
        // 「stat 失败」（权限/IO 等，响亮 Err）。MSVC STL 的 status 连 ENOENT 也写 ec——
        // not_found 必须先判，否则「无清单」被误读成 stat 失败（Linux 恰好不写，两边不对称）。
        // stat 失败无便携构造手段，此分支不立用例、止于本注释。
        std::error_code statEc;
        const std::filesystem::file_status st = std::filesystem::status(manifest, statEc);
        if (st.type() != std::filesystem::file_type::not_found && statEc)
        {
            return Result<void>::Err(
                Error("catalog refresh: cannot stat manifest \"" + manifest.string() + "\": " + statEc.message()));
        }
        if (st.type() != std::filesystem::file_type::regular)
        {
            nextWarnings.push_back(CatalogWarning{.Subdirectory = name, .Message = "no plugin.json — skipped"});
            continue;
        }
        Result<ManifestEntry> parsed = ParseManifestFile(manifest, name);
        if (!parsed.IsOk())
        {
            return Result<void>::Err(parsed.GetError()); // D50：坏清单整体响；失败不换快照/Warnings（D58/D64a）
        }
        ManifestEntry entry = std::move(parsed.Value());
        for (const ManifestEntry& held : nextEntries)
        {
            if (held.Id == entry.Id)
            {
                return Result<void>::Err(Error("catalog refresh: duplicate plugin id \"" + entry.Id + "\" in \"" +
                                               held.Subdirectory + "\" and \"" + entry.Subdirectory + "\""));
            }
        }
        nextEntries.push_back(std::move(entry));
    }

    std::ranges::sort(nextEntries, [](const ManifestEntry& a, const ManifestEntry& b) { return a.Id < b.Id; });

    SnapshotDirectory = std::move(abs);
    SnapshotEntries = std::move(nextEntries);
    SnapshotWarnings = std::move(nextWarnings);
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
    return expected;
}

Result<AdoptReport> AdoptInto(const PluginCatalog& catalog, PluginHost& host, PodHandle handle, std::string_view id)
{
    const ManifestEntry* located = catalog.Find(id);
    if (located == nullptr)
    {
        return Result<AdoptReport>::Err(Error("adopt: id not in catalog snapshot"));
    }
    const std::filesystem::path manifest = catalog.Directory() / located->Subdirectory / "plugin.json";
    Result<ManifestEntry> parsed = ParseManifestFile(manifest, located->Subdirectory);
    if (!parsed.IsOk())
    {
        return Result<AdoptReport>::Err(parsed.GetError()); // 就地重读失败响亮（CatalogAdopt.h 注）
    }
    const ManifestExpectation expected = BuildExpectation(parsed.Value()); // 借用止于下方同步调用

    std::vector<std::string> siblings;
    for (const std::string& otherId : catalog.Ids())
    {
        if (otherId != id)
        {
            if (const ManifestEntry* other = catalog.Find(otherId); other != nullptr)
            {
                siblings.push_back(catalog_detail::LibraryFileName(other->Binary));
            }
        }
    }

    AdoptRequest request;
    request.Id = std::string(id);
    request.BinaryPath = catalog.Directory() / located->Subdirectory / catalog_detail::LibraryFileName(located->Binary);
    request.Expected = &expected;
    request.SiblingBinaries = std::move(siblings);
    return host.AdoptPlugin(handle, request);
}

} // namespace vase
