#include "Scan.h"

#include "Cli.h"
#include "ManifestMerge.h"
#include "Vase/Catalog/LibraryFileName.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Inspect.h"
#include "Vase/Host/Loader.h"
#include "Vase/Host/PackageInspect.h"
#include "Vase/Host/PackagePlan.h"
#include "Vase/PluginDescriptor.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

using tools::cli::ManifestEntryFromMeta;
using tools::cli::PackageMergeOutcome;
using tools::cli::PackageSourceFacts;

// 两类清单的固定名字（spec §2.1/§2.2）：包名与 Catalog 侧同字面（Source/Catalog/PluginCatalog.cpp 的私有常量）。
constexpr const char* kPluginManifestName = "plugin.json";
constexpr const char* kPackageManifestName = "pack.json";

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

// 一个来源（单插件或包）的完整处理结果。收集后再打印：输出确定性由工具保证（D136），故不能边扫边打。
struct ScanResult
{
    std::string Key; // 单插件 = 插件 Id；包 = sourceId（各自原本的排序键）
    std::string Subdirectory;
    // 两枚 NSDMI 为指定初始化省略豁免所必需（clang 的 -Wmissing-designated-field-initializers，同 LoadPlan.h）。
    // NOLINTNEXTLINE(readability-redundant-member-init) 见上条。
    std::optional<std::string> PreviousBinary = {}; // 单插件的 binary 漂移（打印用）
    // NOLINTNEXTLINE(readability-redundant-member-init) 同前。
    std::optional<PackageMergeOutcome> Package = {}; // 有值 = 包：成员数与变更明细都在里面
};

std::string_view KindName(vase::PackageSourceKind kind)
{
    return kind == vase::PackageSourceKind::kInProcess ? "inProcess" : "dynamic";
}

std::string SourceFactsText(const PackageSourceFacts& facts)
{
    std::string text = "\"" + facts.SourceId + "\" (" + std::string{KindName(facts.Kind)};
    if (!facts.Binary.empty())
    {
        text += ", binary \"" + facts.Binary + "\"";
    }
    return text + ")";
}

// 包文档的变更明细（spec §5.3）：成员增删与来源变化逐条打——「wrote / regenerated」那一行由调用方给。
void PrintPackageChanges(std::ostream& out, const PackageMergeOutcome& merged)
{
    for (const std::string& id : merged.Added)
    {
        out << "  added member: " << id << '\n';
    }
    for (const std::string& id : merged.Removed)
    {
        out << "  removed member: " << id << '\n';
    }
    if (merged.PreviousSource.has_value())
    {
        const PackageSourceFacts current{
            .SourceId = merged.Manifest.SourceId,
            .Kind = merged.Manifest.Kind,
            .Binary = merged.Manifest.Binary,
        };
        out << "  source: " << SourceFactsText(*merged.PreviousSource) << " -> " << SourceFactsText(current) << '\n';
    }
}

// 一个包目录的库文件名由来源事实拼（T4 同律）：目录模式与显式模式共用，免得两处各拼一份。
std::filesystem::path PackageLibraryPath(const std::filesystem::path& directory, const std::string& binaryStem)
{
    return directory / vase::catalog_detail::LibraryFileName(binaryStem);
}

// 描述符物化 → 拥有形包文档（借用 → 拥有必须发生在 Unload 之前；调用方负责 Unload）。
vase::PackageManifest MaterializePackage(const std::vector<const vase::PluginDescriptor*>& members,
                                         std::string sourceId, vase::PackageSourceKind kind, std::string binaryStem,
                                         const std::string& subdirectory)
{
    vase::PackageManifest manifest;
    manifest.SourceId = std::move(sourceId);
    manifest.Kind = kind;
    manifest.Binary = std::move(binaryStem);
    for (const vase::PluginDescriptor* descriptor : members)
    {
        // 包成员的 Subdirectory 保留目录展示值、Binary 不进 JSON（T2 的 kPackageMember 模式不读它）。
        manifest.Plugins.push_back(ManifestEntryFromMeta(*descriptor->Meta, subdirectory, std::string{}));
    }
    return manifest;
}

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
        const std::string name = subdirectory.filename().string();
        // 两类清单先查（与 Catalog 的 D50 同口径）：stat 不出来不猜，按 §2.3 响亮口径记 Failed
        // （异常重载在本仓是 terminate，规矩 4）。
        std::error_code pluginEc;
        const bool hasPluginManifest = std::filesystem::exists(subdirectory / kPluginManifestName, pluginEc);
        if (pluginEc)
        {
            found.Failed.push_back(name + ": cannot stat " + kPluginManifestName + ": " + pluginEc.message());
            continue;
        }
        std::error_code packageEc;
        const bool hasPackageManifest = std::filesystem::exists(subdirectory / kPackageManifestName, packageEc);
        if (packageEc)
        {
            found.Failed.push_back(name + ": cannot stat " + kPackageManifestName + ": " + packageEc.message());
            continue;
        }
        if (hasPluginManifest && hasPackageManifest)
        {
            // 不隐式设优先级（D50）：同一目录两类清单并存是坏树。
            found.Failed.push_back(name + ": " + kPluginManifestName + " and " + kPackageManifestName +
                                   " are both present (one directory holds one manifest kind)");
            continue;
        }
        if (hasPackageManifest)
        {
            DiscoveredPackage package;
            package.Directory = subdirectory;
            package.ManifestPath = subdirectory / kPackageManifestName;
            const vase::Result<vase::PackageManifest> parsed = vase::ParsePackageManifestFile(package.ManifestPath);
            if (!parsed.IsOk())
            {
                // 坏清单默认保持原文件（spec §5.3）：不沿用单插件那条自动恢复，归因点名显式重建。
                found.Failed.push_back(
                    name + ": " + kPackageManifestName + " unreadable (" + parsed.GetError().Message() +
                    ") — rebuild it with: scan --package <动态库文件> --source-id <标识> --regenerate");
                continue;
            }
            package.Manifest = parsed.Value();
            if (package.Manifest.Kind == vase::PackageSourceKind::kInProcess)
            {
                // 命令矩阵（spec §5.2）：静态来源的成员枚举要宿主的显式入口，普通 CLI 做不到——如实报，不当成功。
                found.Failed.push_back(name +
                                       ": inProcess package — the plain CLI cannot enumerate its members (spec §5.2)");
                continue;
            }
            package.BinaryPath = PackageLibraryPath(subdirectory, package.Manifest.Binary);
            std::error_code libraryEc;
            if (!std::filesystem::is_regular_file(package.BinaryPath, libraryEc) || libraryEc)
            {
                found.Failed.push_back(name + ": dynamic package library not found: " + package.BinaryPath.string());
                continue;
            }
            found.Packages.push_back(std::move(package));
            continue;
        }

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
            // 有清单却没库是坏树（清单在指一个不存在的文件）；两样都没有则只是不是插件。
            if (hasPluginManifest)
            {
                found.Failed.push_back(name + ": " + kPluginManifestName + " present but no shared library");
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
    if (discovery.Found.empty() && discovery.Packages.empty() && discovery.Failed.empty())
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
            // 组合库（一库 N 插件）：目录模型装不下它，但**只**引导到显式模式——不因枚举出多员就把
            // 单插件目录改写成包目录（spec §5.3 命令矩阵）。
            err << "error: " << subdirectory << ": combined library (" << descriptors.Value().size()
                << " descriptors in one binary) — generate its manifest with: VaseCli scan --package \""
                << plugin.BinaryPath.string() << "\" --source-id <标识>\n";
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
            .Key = merged.Entry.Id,
            .Subdirectory = subdirectory,
            .PreviousBinary = merged.PreviousBinary,
        });
    }

    for (const DiscoveredPackage& package : discovery.Packages)
    {
        const std::string subdirectory = package.Directory.filename().string();
        vase::Result<vase::detail::BinaryRecord*> record = loader.EnsureResident(package.BinaryPath);
        if (!record.IsOk())
        {
            err << "error: " << subdirectory << ": " << record.GetError().Message() << '\n';
            failed = true;
            continue;
        }
        const vase::Result<std::vector<const vase::PluginDescriptor*>> members =
            vase::InspectPackageMembers(*record.Value());
        if (!members.IsOk())
        {
            err << "error: " << subdirectory << ": " << members.GetError().Message() << '\n';
            loader.Unload(*record.Value());
            failed = true;
            continue;
        }
        // 借用 → 拥有必须发生在 Unload 之前：整份包文档在这里重新物化（spec §5.3）。
        vase::PackageManifest fromBinary = MaterializePackage(
            members.Value(), package.Manifest.SourceId, package.Manifest.Kind, package.Manifest.Binary, subdirectory);
        loader.Unload(*record.Value());

        // 来源事实取自盘上那份 pack.json（不是目录名）：显式指定的 sourceId 才是显式模式的写出口径。
        const PackageMergeOutcome merged = MergePackageWithExisting(package.ManifestPath, std::move(fromBinary));
        if (merged.ParseError.has_value())
        {
            // 目录模式没有 --regenerate：坏清单保持原文件，并点名显式重建的路。
            err << "error: " << subdirectory << ": " << kPackageManifestName << " unreadable (" << *merged.ParseError
                << ") — rebuild it with: scan --package \"" << package.BinaryPath.string()
                << "\" --source-id <标识> --regenerate\n";
            failed = true;
            continue;
        }
        const vase::Result<void> written = vase::WritePackageManifestFile(package.ManifestPath, merged.Manifest);
        if (!written.IsOk())
        {
            err << "error: " << subdirectory << ": " << written.GetError().Message() << '\n';
            failed = true;
            continue;
        }
        results.push_back(ScanResult{
            .Key = merged.Manifest.SourceId,
            .Subdirectory = subdirectory,
            .Package = merged,
        });
    }

    // 输出确定性由工具保证（D136）：单插件按插件 Id、包按 sourceId（两类各自原本的排序键），
    // 同键再按目录名——不把文件系统枚举序漏进报告。
    std::ranges::sort(results,
                      [](const ScanResult& left, const ScanResult& right)
                      {
                          if (left.Key != right.Key)
                          {
                              return left.Key < right.Key;
                          }
                          return left.Subdirectory < right.Subdirectory;
                      });
    for (const ScanResult& result : results)
    {
        if (result.Package.has_value())
        {
            out << "wrote " << result.Subdirectory << '/' << kPackageManifestName << " sourceId=" << result.Key
                << " members=" << result.Package->Manifest.Plugins.size() << '\n';
            PrintPackageChanges(out, *result.Package);
            continue;
        }
        out << "wrote " << result.Subdirectory << '/' << kPluginManifestName << " id=" << result.Key << '\n';
        if (result.PreviousBinary.has_value())
        {
            out << "  binary: \"" << *result.PreviousBinary << "\" -> actual file name stem (D122)\n";
        }
    }
    return failed ? kExitCheckFailed : kExitOk;
}

int ScanPackage(const PackageScanOptions& options, std::ostream& out, std::ostream& err)
{
    // sourceId 字符集是包格式规则（Catalog 的 IsValidPackageSourceId）：畸形值属用法档（D135），
    // 在装载/物化之前就拒——否则拖到 WritePackageManifestFile 才报，且那时库已经装过了。
    if (!vase::catalog_detail::IsValidPackageSourceId(options.SourceId))
    {
        err << "scan --package: --source-id must be a non-empty ASCII identifier: \"" << options.SourceId << "\"\n";
        return kExitUsage;
    }
    // 固定写出口径（spec §5.2/§5.3）：BinaryPath.parent_path()/pack.json——不数库、不猜目录。
    const std::filesystem::path directory = options.BinaryPath.parent_path();
    const std::filesystem::path manifestPath = directory / kPackageManifestName;

    std::error_code fileEc;
    if (!std::filesystem::is_regular_file(options.BinaryPath, fileEc) || fileEc)
    {
        err << "scan --package: not a readable library file: \"" << options.BinaryPath.string() << "\"\n";
        return kExitUsage;
    }
    // 静态归档（.a/.lib）进不了动态加载器（spec §1）：本平台库名形态就是这道闸。
    const std::optional<std::string> stem = vase::catalog_detail::LibraryStem(options.BinaryPath.filename().string());
    if (!stem.has_value())
    {
        err << "scan --package: not a dynamic library for this platform: \"" << options.BinaryPath.string()
            << "\" (static archives are not scannable by the plain CLI, spec §5.2)\n";
        return kExitUsage;
    }
    std::error_code pluginEc;
    const bool hasPluginManifest = std::filesystem::exists(directory / kPluginManifestName, pluginEc);
    if (pluginEc)
    {
        err << "scan --package: cannot stat \"" << (directory / kPluginManifestName).string()
            << "\": " << pluginEc.message() << '\n';
        return kExitUsage;
    }
    if (hasPluginManifest)
    {
        // 一目录一清单类（spec §5.3）：遇到 plugin.json 即拒，绝不产生两种清单并存的目录。
        err << "scan --package: \"" << directory.string() << "\" already holds " << kPluginManifestName
            << " — one directory holds one manifest kind, refusing to write " << kPackageManifestName
            << " next to it\n";
        return kExitCheckFailed;
    }

    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> record = loader.EnsureResident(options.BinaryPath);
    if (!record.IsOk())
    {
        err << "error: " << options.BinaryPath.string() << ": " << record.GetError().Message() << '\n';
        return kExitCheckFailed;
    }
    const vase::Result<std::vector<const vase::PluginDescriptor*>> members =
        vase::InspectPackageMembers(*record.Value());
    if (!members.IsOk())
    {
        // 入口/版本/成员一致性三闸都在 InspectPackageMembers 里：--regenerate 也绕不过（spec §5.3 末段）。
        err << "error: " << options.BinaryPath.string() << ": " << members.GetError().Message() << '\n';
        loader.Unload(*record.Value());
        return kExitCheckFailed;
    }
    // 借用 → 拥有必须发生在 Unload 之前：整份包文档先物化，再谈写盘（spec §5.1）。
    vase::PackageManifest fromBinary = MaterializePackage(
        members.Value(), options.SourceId, vase::PackageSourceKind::kDynamic, *stem, directory.filename().string());
    loader.Unload(*record.Value());

    const PackageMergeOutcome merged = MergePackageWithExisting(manifestPath, std::move(fromBinary));
    const bool regenerating = merged.ParseError.has_value();
    if (regenerating && !options.Regenerate)
    {
        // 坏 pack.json 默认报错并保持原文件（spec §5.3）——单插件那条自动恢复不适用于包。
        err << "error: " << manifestPath.string() << ": " << *merged.ParseError
            << " — add --regenerate to rebuild it from the binary\n";
        return kExitCheckFailed;
    }
    const vase::Result<void> written = vase::WritePackageManifestFile(manifestPath, merged.Manifest);
    if (!written.IsOk())
    {
        // 写失败时旧文件还在盘上：一个字都不报（与单插件路径同律——那句话否则就是谎话）。
        err << "error: " << written.GetError().Message() << '\n';
        return kExitCheckFailed;
    }
    if (regenerating)
    {
        out << "warning: " << manifestPath.string() << " unreadable (" << *merged.ParseError
            << ") — regenerated from binary\n";
        out << "regenerated " << manifestPath.string() << " sourceId=" << merged.Manifest.SourceId
            << " members=" << merged.Manifest.Plugins.size() << " (enabledByDefault reset to default)\n";
    }
    else
    {
        out << "wrote " << manifestPath.string() << " sourceId=" << merged.Manifest.SourceId
            << " members=" << merged.Manifest.Plugins.size() << '\n';
    }
    PrintPackageChanges(out, merged);
    return kExitOk;
}

int RunPackageScan(const std::vector<std::string>& args, std::ostream& out, std::ostream& err)
{
    // 形状固定：scan --package <动态库文件> --source-id <标识> [--regenerate]（spec §5.2 命令矩阵）。
    // 重名 / 缺值 / 顺序错 / 多余 flag 一律用法档（D137 现有口径），不猜意图。
    const std::string usage = "usage: VaseCli scan --package <动态库文件> --source-id <标识> [--regenerate]\n";
    const auto at = [&args](std::size_t index) -> const std::string&
    { return *std::next(args.begin(), static_cast<std::ptrdiff_t>(index)); };

    const auto isRequiredValue = [](const std::string& value)
    { return !value.empty() && value != "--package" && value != "--source-id" && value != "--regenerate"; };

    if (args.size() < 3U || at(1U) != "--package" || !isRequiredValue(at(2U)))
    {
        err << usage;
        return kExitUsage;
    }
    PackageScanOptions options;
    options.BinaryPath = at(2U);
    if (args.size() < 5U || at(3U) != "--source-id" || !isRequiredValue(at(4U)))
    {
        err << usage;
        return kExitUsage;
    }
    options.SourceId = at(4U);
    std::size_t consumed = 5U;
    if (args.size() > consumed)
    {
        if (at(consumed) != "--regenerate")
        {
            err << usage;
            return kExitUsage;
        }
        options.Regenerate = true;
        ++consumed;
    }
    if (args.size() != consumed)
    {
        err << usage; // 尾随多余参数（含重复 --regenerate）
        return kExitUsage;
    }
    return ScanPackage(options, out, err);
}

} // namespace tools::cli
