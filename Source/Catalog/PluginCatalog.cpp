#include "Vase/Catalog/PluginCatalog.h"

#include "Vase/Catalog/ManifestView.h"
#include "Vase/Detail/Result.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace vase
{

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

} // namespace vase
