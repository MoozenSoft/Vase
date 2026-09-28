#include "Scan.h"

#include "Cli.h"
#include "ManifestMerge.h"
#include "Vase/Catalog/LibraryFileName.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Config/FieldInfo.h"
#include "Vase/Detail/MetaArray.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Inspect.h"
#include "Vase/Host/Loader.h"
#include "Vase/PluginDescriptor.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <optional>
#include <ostream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

bool IsNativeLibraryFile(const std::filesystem::path& path)
{
    return vase::catalog_detail::LibraryStem(path.filename().string()).has_value();
}

// 一级子目录按名字排序：输出确定性由工具保证（D136），不让文件系统枚举序漏进报告。
// 枚举中断（构造或 increment 出错）不再静默 break——原样上抛，交 DiscoverPlugins 归成 Err：根目录连列
// 都列不全，下游一切分类皆不可信（D134/D135 响亮口径）。
vase::Result<std::vector<std::filesystem::path>> Subdirectories(const std::filesystem::path& root)
{
    std::vector<std::filesystem::path> out;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(root, ec), end; it != end; it.increment(ec))
    {
        if (ec)
        {
            break;
        }
        std::error_code probe;
        if (it->is_directory(probe) && !probe)
        {
            out.push_back(it->path());
        }
    }
    if (ec)
    {
        return vase::Result<std::vector<std::filesystem::path>>::Err(
            vase::Error{"cannot enumerate directory \"" + root.string() + "\": " + ec.message()});
    }
    std::ranges::sort(out);
    return vase::Result<std::vector<std::filesystem::path>>::Ok(std::move(out));
}

std::vector<vase::ManifestDependency> ConvertDeps(const vase::MetaArray<vase::ServiceRef, 16>& declared)
{
    std::vector<vase::ManifestDependency> out;
    out.reserve(declared.Size());
    for (std::size_t index = 0; index < declared.Size(); ++index)
    {
        const vase::ServiceRef& ref = *std::next(declared.Begin(), static_cast<std::ptrdiff_t>(index));
        out.push_back(vase::ManifestDependency{.Service = std::string(ref.Name), .Version = ref.Version});
    }
    return out;
}

// 一个插件的完整处理结果。收集后再打印：输出按 Id 排序（D136），故不能边扫边打。
struct ScanResult
{
    std::string Id;
    std::string Subdirectory;
    std::optional<std::string> PreviousBinary;
};

} // namespace

namespace tools::cli
{

vase::Result<Discovery> DiscoverPlugins(const std::filesystem::path& pluginDirectory)
{
    std::error_code ec;
    if (!std::filesystem::is_directory(pluginDirectory, ec) || ec)
    {
        return vase::Result<Discovery>::Err(vase::Error{"not a plugin directory: " + pluginDirectory.string()});
    }

    Discovery found;
    const vase::Result<std::vector<std::filesystem::path>> subdirectories = Subdirectories(pluginDirectory);
    if (!subdirectories.IsOk())
    {
        return vase::Result<Discovery>::Err(subdirectories.GetError()); // 根枚举不可信 = 环境错（D135）
    }
    for (const std::filesystem::path& subdirectory : subdirectories.Value())
    {
        std::vector<std::filesystem::path> libraries;
        std::error_code walk;
        for (std::filesystem::directory_iterator it(subdirectory, walk), end; it != end; it.increment(walk))
        {
            if (walk)
            {
                break;
            }
            std::error_code probe;
            if (it->is_regular_file(probe) && !probe && IsNativeLibraryFile(it->path()))
            {
                libraries.push_back(it->path());
            }
        }

        const std::string name = subdirectory.filename().string();
        // 枚举中断（构造或 increment 出错）：部分树不可信，点名该子目录 + ec 原因，不落入 skip/零库分类。
        if (walk)
        {
            found.Failed.push_back(name + ": cannot enumerate subdirectory: " + walk.message());
            continue;
        }
        if (libraries.size() > 1U)
        {
            found.Failed.push_back(name + ": more than one shared library in the subdirectory");
            continue;
        }
        if (libraries.empty())
        {
            std::error_code mec;
            const bool hasManifest = std::filesystem::exists(subdirectory / "plugin.json", mec);
            // stat 不出来不猜 hasManifest：按 §2.3 响亮口径记 Failed（异常重载在本仓是 terminate，规矩 4）。
            if (mec)
            {
                found.Failed.push_back(name + ": cannot stat plugin.json: " + mec.message());
            }
            else if (hasManifest)
            {
                // 有清单却没库是坏树（清单在指一个不存在的文件）；两样都没有则只是不是插件。
                found.Failed.push_back(name + ": plugin.json present but no shared library");
            }
            else
            {
                found.Skipped.push_back(name);
            }
            continue;
        }

        const std::optional<std::string> stem =
            vase::catalog_detail::LibraryStem(libraries.front().filename().string());
        found.Found.push_back(DiscoveredPlugin{
            .Directory = subdirectory,
            .BinaryPath = libraries.front(),
            .Stem = stem.has_value() ? *stem : std::string{},
        });
    }
    return vase::Result<Discovery>::Ok(std::move(found));
}

vase::ManifestEntry ManifestEntryFromMeta(const vase::PluginMeta& meta, const std::string& subdirectory,
                                          const std::string& binaryStem)
{
    vase::ManifestEntry entry;
    entry.Id = std::string(meta.Id);
    entry.DisplayName = std::string(meta.DisplayName);
    entry.Version = std::string(meta.Version);
    entry.Subdirectory = subdirectory;
    entry.Binary = binaryStem;
    entry.Requires = ConvertDeps(meta.Requires);
    entry.OptionalRequires = ConvertDeps(meta.OptionalRequires);
    entry.Provides = ConvertDeps(meta.Provides);
    entry.Config.reserve(meta.Config.Count);
    for (std::uint32_t index = 0; index < meta.Config.Count; ++index)
    {
        const vase::FieldInfo& field = *std::next(meta.Config.Fields, static_cast<std::ptrdiff_t>(index));
        entry.Config.push_back(vase::MakeManifestConfigField(field));
    }
    // MetaArray 只有 Begin()/Size()（无 begin/end），故按仓内既有惯用法迭代（PluginHost 同款）。
    for (std::size_t index = 0; index < meta.ProcessStates.Size(); ++index)
    {
        const vase::ProcessStateDesc& state =
            *std::next(meta.ProcessStates.Begin(), static_cast<std::ptrdiff_t>(index));
        entry.ProcessStates.emplace_back(state.Name);
    }
    return entry;
}

int RunScan(const std::string& pluginDirectory, std::ostream& out, std::ostream& err)
{
    const vase::Result<Discovery> discovered = DiscoverPlugins(pluginDirectory);
    if (!discovered.IsOk())
    {
        err << discovered.GetError().Message() << '\n';
        return kExitUsage; // 环境错误（D135 第三档）
    }
    const Discovery& discovery = discovered.Value();
    for (const std::string& skipped : discovery.Skipped)
    {
        out << "skip " << skipped << " (neither a library nor a manifest)\n";
    }
    if (discovery.Found.empty() && discovery.Failed.empty())
    {
        err << "no plugins found under " << pluginDirectory << '\n';
        return kExitUsage; // 「什么都没做」不是成功（D135）
    }

    vase::detail::Loader loader;
    std::vector<ScanResult> results;
    bool failed = false;
    for (const std::string& problem : discovery.Failed)
    {
        err << "error: " << problem << '\n';
        failed = true;
    }

    for (const DiscoveredPlugin& plugin : discovery.Found)
    {
        const std::string subdirectory = plugin.Directory.filename().string();
        vase::Result<vase::detail::BinaryRecord*> record = loader.EnsureResident(plugin.BinaryPath);
        if (!record.IsOk())
        {
            err << "error: " << subdirectory << ": " << record.GetError().Message() << '\n';
            failed = true;
            continue;
        }
        const vase::Result<std::vector<const vase::PluginDescriptor*>> descriptors =
            vase::InspectDescriptors(*record.Value());
        if (!descriptors.IsOk())
        {
            err << "error: " << subdirectory << ": " << descriptors.GetError().Message() << '\n';
            loader.Unload(*record.Value());
            failed = true;
            continue;
        }
        if (descriptors.Value().size() != 1U)
        {
            // 组合库（一库 N 插件）本波不做（D126）：目录模型装不下它，响亮拒绝而不是猜。
            err << "error: " << subdirectory
                << ": combined libraries (N plugins in one binary) are not supported yet\n";
            loader.Unload(*record.Value());
            failed = true;
            continue;
        }
        // 借用 → 拥有必须发生在 Unload 之前（寿命纪律）。
        const vase::PluginDescriptor* single = *descriptors.Value().begin();
        const MergeOutcome merged = MergeWithExisting(plugin.Directory / "plugin.json",
                                                      ManifestEntryFromMeta(*single->Meta, subdirectory, plugin.Stem));
        loader.Unload(*record.Value());

        const vase::Result<void> written = vase::WriteManifestFile(plugin.Directory / "plugin.json", merged.Entry);
        if (!written.IsOk())
        {
            err << "error: " << subdirectory << ": " << written.GetError().Message() << '\n';
            failed = true;
            continue;
        }
        // 只在重写成功后说 "regenerated"：写失败时旧文件还在盘上，那句话就是谎话。
        if (merged.ParseError.has_value())
        {
            out << "warning: " << subdirectory << ": existing plugin.json unreadable (" << *merged.ParseError
                << ") — regenerated from binary\n";
        }
        results.push_back(ScanResult{
            .Id = merged.Entry.Id,
            .Subdirectory = subdirectory,
            .PreviousBinary = merged.PreviousBinary,
        });
    }

    std::ranges::sort(results, [](const ScanResult& left, const ScanResult& right) { return left.Id < right.Id; });
    for (const ScanResult& result : results)
    {
        out << "wrote " << result.Subdirectory << "/plugin.json id=" << result.Id << '\n';
        if (result.PreviousBinary.has_value())
        {
            out << "  binary: \"" << *result.PreviousBinary << "\" -> actual file name stem (D122)\n";
        }
    }
    return failed ? kExitCheckFailed : kExitOk;
}

} // namespace tools::cli
