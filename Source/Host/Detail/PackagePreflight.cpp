#include "PackagePreflight.h"

#include "Vase/Detail/Result.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/Loader.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/Host/PackageInspect.h"
#include "Vase/Host/PackagePlan.h"
#include "Vase/PluginDescriptor.h"

#include <cstddef>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

// §0.2：每条拒绝都要带「谁、哪一步」（与 PluginHost.cpp 的 Refusal 同形，指定初始化不容省略字段）。
[[nodiscard]] vase::Error Refusal(std::string_view pluginId, std::string message)
{
    vase::ErrorContext context;
    context.PluginId = std::string(pluginId);
    context.Stage = vase::Phase::kLoad;
    return vase::Error{std::move(message), std::move(context)};
}

// 非常量下标 operator[] 过不了 cppcoreguidelines-pro-bounds-*，.at() 本仓库禁用——用 std::next
// （惯例同 Solve.cpp 的 At）；界由「index < size()」的循环条件与同源的 size 担保。
template <typename T>
[[nodiscard]] const T& At(const std::vector<T>& values, std::size_t index)
{
    return *std::next(values.begin(), static_cast<std::ptrdiff_t>(index));
}

template <typename T>
[[nodiscard]] T& At(std::vector<T>& values, std::size_t index)
{
    return *std::next(values.begin(), static_cast<std::ptrdiff_t>(index));
}

} // namespace

namespace vase::detail
{

Result<std::vector<BinaryRecord*>> PreflightPackages(Loader& loader, const LoadPlan& plan)
{
    // ① 归属表：来源键 → 下标，以及每个来源的**全部**归属条目（含 kSkip——Members 也含被跳过的成员）。
    //    悬空的关联键只许出现在 kSkip 条目上：全跳过包按 Solve 规则仍带键而不进 PackageSources；
    //    kLoad 条目挂不到参与源上就没人替它做整包校验，那是计划误用。
    std::map<std::string_view, std::size_t> sourceIndex;
    for (std::size_t index = 0; index < plan.PackageSources.size(); ++index)
    {
        const PackagePlanSource& source = At(plan.PackageSources, index);
        if (source.SourceId.empty())
        {
            return Result<std::vector<BinaryRecord*>>::Err(
                Refusal({}, "plan rejected: package source with an empty sourceId"));
        }
        if (!sourceIndex.emplace(source.SourceId, index).second)
        {
            return Result<std::vector<BinaryRecord*>>::Err(
                Refusal(source.SourceId, "plan rejected: duplicate package source \"" + source.SourceId + "\""));
        }
    }
    std::vector<std::vector<const LoadPlanEntry*>> attributed(plan.PackageSources.size());
    for (const LoadPlanEntry& entry : plan.Ordered)
    {
        if (entry.PackageSourceId.empty())
        {
            continue; // 单插件来源与原始手写条目：不伪装成包（T5 契约）
        }
        const auto found = sourceIndex.find(entry.PackageSourceId);
        if (found == sourceIndex.end())
        {
            if (entry.Decision == LoadDecision::kLoad)
            {
                return Result<std::vector<BinaryRecord*>>::Err(Refusal(
                    entry.Id, "plan rejected: plugin \"" + std::string(entry.Id) + "\" claims package source \"" +
                                  entry.PackageSourceId + "\" that the plan does not carry"));
            }
            continue;
        }
        At(attributed, found->second).push_back(&entry);
    }

    // ② 纯结构检查（一个镜像都不加载）：来源字段自洽、Members 与归属条目 Id 集双向等值、
    //    每个 kLoad 包条目带与自身同 Id 的期望。自相矛盾的输入连读盘都不该发生。
    std::vector<char> participating(plan.PackageSources.size(), 0);
    for (std::size_t index = 0; index < plan.PackageSources.size(); ++index)
    {
        const PackagePlanSource& source = At(plan.PackageSources, index);
        const std::vector<const LoadPlanEntry*>& members = At(attributed, index);
        if (source.Kind == PackageSourceKind::kDynamic)
        {
            if (source.BinaryPath.empty() || source.Entries.Lookup != nullptr || source.Entries.Enumerate != nullptr)
            {
                return Result<std::vector<BinaryRecord*>>::Err(
                    Refusal(source.SourceId, "plan rejected: dynamic package \"" + source.SourceId +
                                                 "\" needs a binary path and no static entries"));
            }
        }
        else if (source.Entries.Lookup == nullptr || source.Entries.Enumerate == nullptr || !source.BinaryPath.empty())
        {
            return Result<std::vector<BinaryRecord*>>::Err(
                Refusal(source.SourceId, "plan rejected: in-process package \"" + source.SourceId +
                                             "\" needs both entries and no binary path"));
        }
        std::map<std::string_view, const ManifestExpectation*> expectations;
        for (const ManifestExpectation& member : source.Members)
        {
            if (member.Id.empty() || !expectations.emplace(member.Id, &member).second)
            {
                return Result<std::vector<BinaryRecord*>>::Err(
                    Refusal(source.SourceId, "plan rejected: package \"" + source.SourceId +
                                                 "\" has an empty or duplicate member expectation"));
            }
        }
        for (const LoadPlanEntry* entry : members)
        {
            if (!expectations.contains(entry->Id))
            {
                return Result<std::vector<BinaryRecord*>>::Err(
                    Refusal(entry->Id, "plan rejected: entry \"" + std::string(entry->Id) +
                                           "\" is not a member of package \"" + source.SourceId + "\""));
            }
        }
        if (members.size() != expectations.size())
        {
            return Result<std::vector<BinaryRecord*>>::Err(
                Refusal(source.SourceId, "plan rejected: package \"" + source.SourceId + "\" lists " +
                                             std::to_string(expectations.size()) + " members but the plan attributes " +
                                             std::to_string(members.size()) + " entries"));
        }
        for (const LoadPlanEntry* entry : members)
        {
            if (entry->Decision != LoadDecision::kLoad)
            {
                continue;
            }
            At(participating, index) = 1;
            if (!entry->Expected.has_value() || entry->Expected->Id != entry->Id)
            {
                return Result<std::vector<BinaryRecord*>>::Err(
                    Refusal(entry->Id, "plan rejected: package entry \"" + std::string(entry->Id) +
                                           "\" needs a matching declaration expectation"));
            }
        }
    }

    // ③ 参与源：各 Ensure 一次并枚举全部成员，与 Members 整包比对。全跳过包在这一段之前出圈
    //    （participating 为 0）——不加载、不要求绑定、不执行入口。记录随返回值交给装配复用。
    std::vector<BinaryRecord*> records(plan.PackageSources.size(), nullptr);
    for (std::size_t index = 0; index < plan.PackageSources.size(); ++index)
    {
        if (At(participating, index) == 0)
        {
            continue;
        }
        const PackagePlanSource& source = At(plan.PackageSources, index);
        const Result<BinaryRecord*> resident = source.Kind == PackageSourceKind::kInProcess
                                                   ? loader.EnsureInProcess(source.Entries.Lookup)
                                                   : loader.EnsureResident(source.BinaryPath);
        if (!resident.IsOk())
        {
            return Result<std::vector<BinaryRecord*>>::Err(
                Refusal(source.SourceId,
                        "plan rejected: package \"" + source.SourceId + "\": " + resident.GetError().Message()));
        }
        const Result<std::vector<const PluginDescriptor*>> actual = source.Kind == PackageSourceKind::kInProcess
                                                                        ? InspectPackageMembers(source.Entries)
                                                                        : InspectPackageMembers(*resident.Value());
        if (!actual.IsOk())
        {
            return Result<std::vector<BinaryRecord*>>::Err(Refusal(
                source.SourceId, "plan rejected: package \"" + source.SourceId + "\": " + actual.GetError().Message()));
        }
        const Result<void> compared = ComparePackage(source.Members, actual.Value());
        if (!compared.IsOk())
        {
            return Result<std::vector<BinaryRecord*>>::Err(
                Refusal(source.SourceId,
                        "plan rejected: package \"" + source.SourceId + "\": " + compared.GetError().Message()));
        }
        At(records, index) = resident.Value();
    }
    return Result<std::vector<BinaryRecord*>>::Ok(std::move(records));
}

} // namespace vase::detail
