#include "PackageBuild.h"

#include "Scan.h"
#include "Vase/Catalog/LibraryFileName.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PackageManifest.h"
#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Config/FieldInfo.h"
#include "Vase/Detail/MetaArray.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Loader.h"
#include "Vase/Host/PackageInspect.h"
#include "Vase/Host/PackagePlan.h"
#include "Vase/PluginDescriptor.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace
{

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

// 成员描述符的双读法：静态直接调用调用者给的入口，动态用同一个已加载包按 DescriptorSymbol 解析。
// 借用窗止于调用方的 Unload——调用方保证在卸下之前读完返回的描述符。
vase::Result<const vase::PluginDescriptor*> ResolveMemberDescriptor(const tools::cli::PackageMemberInput& input,
                                                                    vase::PackageSourceKind kind,
                                                                    const vase::detail::BinaryRecord* record)
{
    using Result = vase::Result<const vase::PluginDescriptor*>;
    if (kind == vase::PackageSourceKind::kInProcess)
    {
        if (input.Descriptor == nullptr)
        {
            return Result::Err(vase::Error{"inProcess package member requires a descriptor entry point"});
        }
        return Result::Ok(input.Descriptor());
    }
    const auto symbol = vase::detail::Loader::Symbol(*record, input.DescriptorSymbol);
    if (!symbol.IsOk())
    {
        return Result::Err(vase::Error{"package member descriptor symbol not found: " + input.DescriptorSymbol});
    }
    // 平台符号边界返回 void*，转契约函数指针只有 reinterpret_cast 这一条路（与 PackageInspect 同律）。
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto entry = reinterpret_cast<tools::cli::DescriptorEntryPoint>(symbol.Value());
    return Result::Ok(entry());
}

vase::Result<vase::PackageManifest> Materialize(const tools::cli::PackageBuildOptions& options,
                                                const std::vector<const vase::PluginDescriptor*>& descriptors,
                                                const vase::detail::BinaryRecord* record, std::string binaryStem)
{
    using Result = vase::Result<vase::PackageManifest>;
    // 成员 Id 一律取自描述符（非调用者手写）：逐输入成员解析描述符，再用它的 Id 去关联。
    std::map<std::string, const tools::cli::PackageMemberInput*> inputs;
    for (const auto& input : options.Members)
    {
        const auto resolved = ResolveMemberDescriptor(input, options.Kind, record);
        if (!resolved.IsOk())
        {
            return Result::Err(resolved.GetError());
        }
        const vase::PluginDescriptor* descriptor = resolved.Value();
        // 该描述符必须属于包枚举（指针级）：否则调用者点名的描述符与包里那个即使同 Id 也不是同一份。
        if (std::ranges::find(descriptors, descriptor) == descriptors.end())
        {
            const std::string id = descriptor != nullptr && descriptor->Meta != nullptr
                                       ? std::string(descriptor->Meta->Id)
                                       : std::string{};
            return Result::Err(vase::Error{"member descriptor not in package enumeration: " + id});
        }
        const std::string id{descriptor->Meta->Id};
        if (id.empty() || !inputs.emplace(id, &input).second)
        {
            return Result::Err(vase::Error{"duplicate or empty package input member Id: " + id});
        }
    }
    vase::PackageManifest manifest;
    manifest.SourceId = options.SourceId;
    manifest.Kind = options.Kind;
    manifest.Binary = std::move(binaryStem);
    manifest.Plugins.reserve(descriptors.size());
    for (const vase::PluginDescriptor* descriptor : descriptors)
    {
        const std::string id{descriptor->Meta->Id};
        const auto input = inputs.find(id);
        if (input == inputs.end())
        {
            return Result::Err(vase::Error{"package member missing in input: " + id});
        }
        vase::ManifestEntry entry = tools::cli::ManifestEntryFromMeta(*descriptor->Meta, options.SourceId, {});
        const auto& sourcePath = input->second->SourceManifest;
        if (!sourcePath.empty())
        {
            const auto source = vase::ParseManifestFile(sourcePath, options.SourceId);
            if (!source.IsOk())
            {
                return Result::Err(vase::Error{"source manifest for " + id + ": " + source.GetError().Message()});
            }
            if (source.Value().Id != id)
            {
                return Result::Err(vase::Error{"source manifest Id mismatch for " + id + ": " + source.Value().Id});
            }
            entry.EnabledByDefault = source.Value().EnabledByDefault;
        }
        manifest.Plugins.push_back(std::move(entry));
        inputs.erase(input);
    }
    if (!inputs.empty())
    {
        return Result::Err(vase::Error{"package input member missing in source: " + inputs.begin()->first});
    }
    return Result::Ok(std::move(manifest));
}

} // namespace

namespace tools::cli
{

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

vase::Result<vase::PackageManifest> GeneratePackageManifest(const PackageBuildOptions& options)
{
    using Result = vase::Result<vase::PackageManifest>;
    if (options.Kind == vase::PackageSourceKind::kInProcess)
    {
        if (!options.BinaryPath.empty())
        {
            return Result::Err(vase::Error{"inProcess package rejects binary path"});
        }
        const auto members = vase::InspectPackageMembers(options.Entries);
        if (!members.IsOk())
        {
            return Result::Err(members.GetError());
        }
        return Materialize(options, members.Value(), nullptr, {});
    }
    if (options.Kind != vase::PackageSourceKind::kDynamic || options.BinaryPath.empty() ||
        options.Entries.Lookup != nullptr || options.Entries.Enumerate != nullptr)
    {
        return Result::Err(vase::Error{"invalid dynamic package source"});
    }
    const auto stem = vase::catalog_detail::LibraryStem(options.BinaryPath.filename().string());
    if (!stem.has_value())
    {
        return Result::Err(vase::Error{"invalid dynamic package library file name"});
    }
    vase::detail::Loader loader;
    const auto resident = loader.EnsureResident(options.BinaryPath);
    if (!resident.IsOk())
    {
        return Result::Err(resident.GetError());
    }
    const auto members = vase::InspectPackageMembers(*resident.Value());
    if (!members.IsOk())
    {
        loader.Unload(*resident.Value());
        return Result::Err(members.GetError());
    }
    auto manifest = Materialize(options, members.Value(), resident.Value(), *stem);
    loader.Unload(*resident.Value()); // 所有描述符借用止于此；manifest 内只剩深拷的值。
    return manifest;
}

} // namespace tools::cli
