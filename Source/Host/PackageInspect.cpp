#include "Vase/Host/PackageInspect.h"

#include "Detail/InspectInternal.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Inspect.h"
#include "Vase/Host/Loader.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/PluginDescriptor.h"

#include <map>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using MemberMap = std::map<std::string_view, const vase::PluginDescriptor*>;

vase::Result<MemberMap> CheckMembers(std::span<const vase::PluginDescriptor* const> members)
{
    MemberMap byId;
    for (const vase::PluginDescriptor* desc : members)
    {
        if (desc == nullptr)
        {
            return vase::Result<MemberMap>::Err(vase::Error{"package contains a null descriptor"});
        }
        const auto version = vase::detail::CheckHeaderVersion(*desc);
        if (!version.IsOk())
        {
            return vase::Result<MemberMap>::Err(version.GetError());
        }
        if (desc->Meta == nullptr)
        {
            return vase::Result<MemberMap>::Err(vase::Error{"package descriptor Meta is null"});
        }
        const std::string_view id = desc->Meta->Id;
        if (id.empty())
        {
            return vase::Result<MemberMap>::Err(vase::Error{"package descriptor Id is empty"});
        }
        if (!byId.emplace(id, desc).second)
        {
            return vase::Result<MemberMap>::Err(vase::Error{"duplicate package member Id: " + std::string{id}});
        }
    }
    return vase::Result<MemberMap>::Ok(std::move(byId));
}

} // namespace

namespace vase
{

Result<std::vector<const PluginDescriptor*>> InspectPackageMembers(PackageEntryPoints entries)
{
    if (entries.Lookup == nullptr)
    {
        return Result<std::vector<const PluginDescriptor*>>::Err(Error{"VasePlugin_GetPlugin entry is null"});
    }
    auto descriptors = InspectDescriptors(entries.Enumerate);
    if (!descriptors.IsOk())
    {
        return descriptors;
    }
    const auto members = CheckMembers(descriptors.Value());
    if (!members.IsOk())
    {
        return Result<std::vector<const PluginDescriptor*>>::Err(members.GetError());
    }
    std::vector<const PluginDescriptor*> sorted;
    sorted.reserve(members.Value().size());
    for (const auto& [id, desc] : members.Value())
    {
        const std::string zeroTerminated{id};
        if (entries.Lookup(zeroTerminated.c_str()) != desc)
        {
            return Result<std::vector<const PluginDescriptor*>>::Err(
                Error{"package lookup/enumeration mismatch for Id: " + zeroTerminated});
        }
        sorted.push_back(desc);
    }
    return Result<std::vector<const PluginDescriptor*>>::Ok(std::move(sorted));
}

Result<std::vector<const PluginDescriptor*>> InspectPackageMembers(const detail::BinaryRecord& record)
{
    const auto lookup = detail::Loader::Symbol(record, "VasePlugin_GetPlugin");
    if (!lookup.IsOk())
    {
        return Result<std::vector<const PluginDescriptor*>>::Err(lookup.GetError());
    }
    const auto enumerate = detail::Loader::Symbol(record, "VasePlugin_Descriptors");
    if (!enumerate.IsOk())
    {
        return Result<std::vector<const PluginDescriptor*>>::Err(enumerate.GetError());
    }
    // 平台符号边界返回 void*，转为契约函数指针只能 reinterpret_cast（与 InspectDescriptors 同律）。
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    const PackageEntryPoints entries{
        .Lookup = reinterpret_cast<PluginEntryPoint>(lookup.Value()),
        .Enumerate = reinterpret_cast<PluginEnumerationEntryPoint>(enumerate.Value()),
    };
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
    return InspectPackageMembers(entries);
}

Result<void> ComparePackage(std::span<const ManifestExpectation> expected,
                            std::span<const PluginDescriptor* const> actual)
{
    const auto members = CheckMembers(actual);
    if (!members.IsOk())
    {
        return Result<void>::Err(members.GetError());
    }
    std::map<std::string_view, const ManifestExpectation*> expectations;
    for (const ManifestExpectation& member : expected)
    {
        if (member.Id.empty())
        {
            return Result<void>::Err(Error{"package expectation Id is empty"});
        }
        if (!expectations.emplace(member.Id, &member).second)
        {
            return Result<void>::Err(Error{"duplicate package expectation Id: " + member.Id});
        }
        if (!members.Value().contains(member.Id))
        {
            return Result<void>::Err(Error{"package member missing in binary: " + member.Id});
        }
    }
    for (const auto& id : members.Value() | std::views::keys)
    {
        if (!expectations.contains(id))
        {
            return Result<void>::Err(Error{"package member missing in manifest: " + std::string{id}});
        }
    }
    for (const auto& [id, desc] : members.Value())
    {
        const auto found = expectations.find(id);
        const auto compared = CompareDescriptor(*found->second, *desc);
        if (!compared.IsOk())
        {
            return compared;
        }
    }
    return Result<void>::Ok();
}

} // namespace vase
