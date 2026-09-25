#include "Vase/Catalog/PluginCatalog.h"

#include "Vase/Catalog/LoadRequest.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Config/Value.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/ConfigBlob.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/PluginDescriptor.h"

#include "Detail/LibraryFileName.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace vase
{
namespace
{

Error SolveError(std::string_view detail) { return Error("catalog solve: " + std::string(detail)); }

// 并行数组（participating/reasons/adjacency/inDegree/emitted）按下标寻址是算法本体。
// 非常量下标 operator[] 过不了 cppcoreguidelines-pro-bounds-avoid-unchecked-container-access，
// .at() 本仓库禁用——用 std::next，惯例同 EffectScope::SlotAt；界由外层
// `index < SnapshotEntries.size()` 循环条件与 size 同源的初始化担保。
template <typename T>
[[nodiscard]] const T& At(const std::vector<T>& vec, std::size_t index)
{
    return *std::next(vec.begin(), static_cast<std::ptrdiff_t>(index));
}

template <typename T>
[[nodiscard]] T& At(std::vector<T>& vec, std::size_t index)
{
    return *std::next(vec.begin(), static_cast<std::ptrdiff_t>(index));
}

std::string_view KindName(ValueKind kind)
{
    switch (kind)
    {
    case ValueKind::kBool:
        return "bool";
    case ValueKind::kInt32:
        return "int32";
    case ValueKind::kInt64:
        return "int64";
    case ValueKind::kFloat:
        return "float";
    case ValueKind::kDouble:
        return "double";
    case ValueKind::kString:
        return "string";
    default:
        return "none";
    }
}

// D59 原样形（bool/int64/double/string；也容忍 typed 面的 int32/float）→ 字段声明型。
// widened 只构造不重赋：Value 的隐式拷贝赋值被 tidy 建模为联合成员访问（探针实测：
// 拷贝构造不报，赋值报在类声明行），构造式无赋值即无痕。
std::optional<Value> Coerce(const Value& raw, ValueKind kind)
{
    const auto widen = [](const Value& v) -> Value
    {
        if (v.Kind == ValueKind::kInt32)
        {
            return Value::From(static_cast<std::int64_t>(v.GetAs<std::int32_t>()));
        }
        if (v.Kind == ValueKind::kFloat)
        {
            return Value::From(static_cast<double>(v.GetAs<float>()));
        }
        return v;
    };
    const Value widened = widen(raw);

    const auto asInt64 = [&widened](std::int64_t& out)
    {
        if (widened.Kind != ValueKind::kInt64)
        {
            return false;
        }
        out = widened.GetAs<std::int64_t>();
        return true;
    };

    switch (kind)
    {
    case ValueKind::kBool:
        if (widened.Kind == ValueKind::kBool)
        {
            return widened;
        }
        break;
    case ValueKind::kInt32:
    {
        std::int64_t i = 0;
        if (asInt64(i) && i >= std::numeric_limits<std::int32_t>::min() &&
            i <= std::numeric_limits<std::int32_t>::max())
        {
            return Value::From(static_cast<std::int32_t>(i));
        }
        break;
    }
    case ValueKind::kInt64:
        if (widened.Kind == ValueKind::kInt64)
        {
            return widened;
        }
        break;
    case ValueKind::kFloat:
    {
        std::int64_t i = 0;
        if (asInt64(i))
        {
            return Value::From(static_cast<float>(i));
        }
        if (widened.Kind == ValueKind::kDouble)
        {
            return Value::From(static_cast<float>(widened.GetAs<double>()));
        }
        break;
    }
    case ValueKind::kDouble:
    {
        std::int64_t i = 0;
        if (asInt64(i))
        {
            return Value::From(static_cast<double>(i));
        }
        if (widened.Kind == ValueKind::kDouble)
        {
            return widened;
        }
        break;
    }
    case ValueKind::kString:
        if (widened.Kind == ValueKind::kString)
        {
            return widened;
        } // blob.Set 深拷入
        break;
    default:
        break;
    }
    return std::nullopt;
}

// bool/string 无 min/max（解析期保证）→ 数值四型比较。
bool OutOfRange(const ManifestConfigField& field, const Value& value)
{
    const auto less = [&field](std::uint64_t a, std::uint64_t b)
    {
        switch (field.Kind)
        {
        case ValueKind::kInt32:
            return static_cast<std::int32_t>(static_cast<std::uint32_t>(a)) <
                   static_cast<std::int32_t>(static_cast<std::uint32_t>(b));
        case ValueKind::kInt64:
            return static_cast<std::int64_t>(a) < static_cast<std::int64_t>(b);
        case ValueKind::kFloat:
            return std::bit_cast<float>(static_cast<std::uint32_t>(a)) <
                   std::bit_cast<float>(static_cast<std::uint32_t>(b));
        default:
            return std::bit_cast<double>(a) < std::bit_cast<double>(b);
        }
    };
    // Bits 只在 HasMin/HasMax 为真后经 && 短路求值：bool/string 无 min/max（数值型才有），
    // 无条件读会让 kString 值碰一次非活动联合成员（结果用不上，但形式上是 UB）。
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) 读已 Coerce 数值位形（D24 同机制）
    if (field.HasMin && less(value.Bits, field.MinBits))
    {
        return true;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) 同上
    if (field.HasMax && less(field.MaxBits, value.Bits))
    {
        return true;
    }
    return false;
}

Value FromStorage(const ConfigBlob::Storage& stored)
{
    return std::visit(
        [](const auto& v) -> Value
        {
            using T = std::remove_cvref_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>)
            {
                return Value::From(v.c_str()); // 借用；调用方在同一次 Solve 内 Set 深拷
            }
            else
            {
                return Value::From(v);
            }
        },
        stored);
}

const ManifestConfigField* FindField(const ManifestEntry& entry, std::string_view key)
{
    for (const ManifestConfigField& field : entry.Config)
    {
        if (field.Key == key)
        {
            return &field;
        }
    }
    return nullptr;
}

} // namespace

Result<SolveOutcome> PluginCatalog::Solve(const LoadRequest& request) const
{
    if (!HasScanned)
    {
        return Result<SolveOutcome>::Err(SolveError("no snapshot — call Refresh first (D63)"));
    }

    for (auto outer = request.Overrides.begin(); outer != request.Overrides.end(); ++outer)
    {
        for (auto inner = request.Overrides.begin(); inner != outer; ++inner)
        {
            if (inner->Id == outer->Id)
            {
                return Result<SolveOutcome>::Err(
                    SolveError("duplicate override entry for id \"" + outer->Id + "\" (D64)"));
            }
        }
    }

    std::vector<const PluginOverride*> layers;
    if (request.Preset.has_value())
    {
        for (const PluginOverride& entry : request.Preset->Entries())
        {
            layers.push_back(&entry);
        }
    }
    for (const PluginOverride& overrideEntry : request.Overrides)
    {
        layers.push_back(&overrideEntry);
    }

    SolveOutcome outcome;

    // 验证与归因（D66：跑快照全量，与本局参与与否无关）。
    for (const PluginOverride* layer : layers)
    {
        const ManifestEntry* entry = Find(layer->Id);
        if (entry == nullptr)
        {
            // .Key/.Cause 显式空形：本工具链的 -Wmissing-designated-field-initializers 不容省略字段。
            outcome.Notes.push_back(SolveNote{
                .Kind = SolveNoteKind::kUnknownPluginId,
                .PluginId = layer->Id,
                .Key = {},
                .Cause = {},
                .Message = "override references unknown plugin id",
            });
            continue;
        }
        for (const ConfigBlob::Entry& kv : layer->Config.Entries())
        {
            const ManifestConfigField* field = FindField(*entry, kv.Key);
            if (field == nullptr)
            {
                outcome.Notes.push_back(SolveNote{
                    .Kind = SolveNoteKind::kUnknownConfigKey,
                    .PluginId = entry->Id,
                    .Key = kv.Key,
                    .Cause = {},
                    .Message = "config key not declared in manifest",
                });
                continue;
            }
            const std::optional<Value> coerced = Coerce(FromStorage(kv.Stored), field->Kind);
            if (!coerced.has_value())
            {
                return Result<SolveOutcome>::Err(SolveError("override for \"" + entry->Id + "\" key \"" + kv.Key +
                                                            "\": type mismatch, manifest declares " +
                                                            std::string(KindName(field->Kind))));
            }
            if (OutOfRange(*field, *coerced))
            {
                return Result<SolveOutcome>::Err(SolveError("override for \"" + entry->Id + "\" key \"" + kv.Key +
                                                            "\": value out of manifest range"));
            }
        }
    }

    std::vector<char> participating(SnapshotEntries.size(), 0);
    std::vector<SkipReason> reasons(SnapshotEntries.size(), SkipReason::kDisabled);

    // ① 参与判定：清单默认 → preset → override，后层整体替换该键。
    for (std::size_t index = 0; index < SnapshotEntries.size(); ++index)
    {
        const ManifestEntry& entry = At(SnapshotEntries, index);
        bool enabled = entry.EnabledByDefault;
        for (const PluginOverride* layer : layers)
        {
            if (layer->Id == entry.Id && layer->Enabled.has_value())
            {
                enabled = *layer->Enabled;
            }
        }
        At(participating, index) = enabled ? 1 : 0;
    }

    using ServiceKey = std::pair<std::string, std::uint32_t>;
    std::map<ServiceKey, std::vector<std::size_t>> exactProviders;
    std::map<std::string, std::vector<std::pair<std::uint32_t, std::size_t>>> nameProviders;
    for (std::size_t index = 0; index < SnapshotEntries.size(); ++index)
    {
        for (const ManifestDependency& provided : At(SnapshotEntries, index).Provides)
        {
            exactProviders[{provided.Service, provided.Version}].push_back(index);
            nameProviders[provided.Service].emplace_back(provided.Version, index);
        }
    }

    const auto hostProvides = [&request](std::string_view name, std::uint32_t version)
    {
        return std::ranges::any_of(request.HostProvided, [name, version](const ServiceRef& ref)
                                   { return ref.Name == name && ref.Version == version; });
    };

    // ② 硬需求闭包：不动点迭代，每轮按快照 Id 序扫 → 归因确定性（D53/D62）。
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (std::size_t index = 0; index < SnapshotEntries.size(); ++index)
        {
            if (At(participating, index) == 0)
            {
                continue;
            }
            const ManifestEntry& entry = At(SnapshotEntries, index);
            for (const ManifestDependency& need : entry.Requires)
            {
                if (hostProvides(need.Service, need.Version))
                {
                    continue; // D60：宿主声明恒满足、不入环、不定序
                }
                const auto exactIt = exactProviders.find({need.Service, need.Version});
                bool satisfied = false;
                if (exactIt != exactProviders.end())
                {
                    for (const std::size_t provider : exactIt->second)
                    {
                        if (At(participating, provider) != 0)
                        {
                            satisfied = true;
                            break;
                        }
                    }
                }
                if (satisfied)
                {
                    continue;
                }

                At(participating, index) = 0;
                changed = true;
                At(reasons, index) = SkipReason::kMissingDependency;

                bool versionNamed = false;
                const auto nameIt = nameProviders.find(need.Service);
                if (nameIt != nameProviders.end())
                {
                    for (const auto& [version, provider] : nameIt->second)
                    {
                        if (At(participating, provider) != 0 && version != need.Version)
                        {
                            At(reasons, index) = SkipReason::kVersionMismatch;
                            outcome.Notes.push_back(SolveNote{
                                .Kind = SolveNoteKind::kVersionMismatchProvider,
                                .PluginId = entry.Id,
                                .Key = {},
                                .Cause = At(SnapshotEntries, provider).Id,
                                .Message =
                                    "only major " + std::to_string(version) + " of " + need.Service + " participates",
                            });
                            versionNamed = true;
                            break;
                        }
                    }
                }
                if (!versionNamed && exactIt != exactProviders.end() && !exactIt->second.empty())
                {
                    outcome.Notes.push_back(SolveNote{
                        .Kind = SolveNoteKind::kProviderSkipped,
                        .PluginId = entry.Id,
                        .Key = {},
                        .Cause = At(SnapshotEntries, exactIt->second.front()).Id,
                        .Message = "provider disabled or skipped: " + need.Service,
                    });
                }
                break; // 首个不满足的声明即归因（§6② 注）
            }
        }
    }

    // ③ 碰撞：参与集内 provide 同 {name,ver} 即拒（§6.3「不静默择一」）；撞 HostProvided 同判（D60）。
    std::map<ServiceKey, std::size_t> claimedBy;
    for (std::size_t index = 0; index < SnapshotEntries.size(); ++index)
    {
        if (At(participating, index) == 0)
        {
            continue;
        }
        const ManifestEntry& entry = At(SnapshotEntries, index);
        for (const ManifestDependency& provided : entry.Provides)
        {
            const ServiceKey key{provided.Service, provided.Version};
            if (hostProvides(key.first, key.second))
            {
                return Result<SolveOutcome>::Err(SolveError("service \"" + key.first + "\"@" +
                                                            std::to_string(key.second) + "\" provided by plugin \"" +
                                                            entry.Id + "\" collides with HostProvided (D60)"));
            }
            const auto [it, inserted] = claimedBy.emplace(key, index);
            if (!inserted)
            {
                return Result<SolveOutcome>::Err(SolveError(
                    "service \"" + key.first + "\"@" + std::to_string(key.second) + "\" provided by both \"" +
                    At(SnapshotEntries, it->second).Id + "\" and \"" + entry.Id + "\""));
            }
        }
    }

    // ④⑤ 拓扑 = Kahn（边 = 硬 ∪ optional 的精确提供方对，D52）；ready 集按 Id 字典序弹出（D53）。
    std::vector<std::vector<std::size_t>> adjacency(SnapshotEntries.size());
    std::vector<std::size_t> inDegree(SnapshotEntries.size(), 0);
    std::size_t remaining = 0;
    for (std::size_t index = 0; index < SnapshotEntries.size(); ++index)
    {
        if (At(participating, index) == 0)
        {
            continue;
        }
        ++remaining;
        const ManifestEntry& entry = At(SnapshotEntries, index);
        std::vector<std::size_t> providers;
        const auto collect = [&](const std::vector<ManifestDependency>& deps)
        {
            for (const ManifestDependency& need : deps)
            {
                if (hostProvides(need.Service, need.Version))
                {
                    continue;
                }
                const auto exactIt = exactProviders.find({need.Service, need.Version});
                if (exactIt == exactProviders.end())
                {
                    continue; // optional 缺席不拦；hard 缺席者已在 ② 被跳出局
                }
                for (const std::size_t provider : exactIt->second)
                {
                    if (At(participating, provider) != 0)
                    {
                        providers.push_back(provider);
                    }
                }
            }
        };
        collect(entry.Requires);
        collect(entry.OptionalRequires);
        std::ranges::sort(providers);
        providers.erase(std::ranges::unique(providers).begin(), providers.end());
        for (const std::size_t provider : providers)
        {
            At(adjacency, provider).push_back(index);
            ++At(inDegree, index);
        }
    }

    std::set<std::pair<std::string_view, std::size_t>> ready;
    for (std::size_t index = 0; index < SnapshotEntries.size(); ++index)
    {
        if (At(participating, index) != 0 && At(inDegree, index) == 0)
        {
            ready.insert({At(SnapshotEntries, index).Id, index});
        }
    }
    std::vector<std::size_t> loadOrder;
    loadOrder.reserve(remaining);
    std::vector<char> emitted(SnapshotEntries.size(), 0);
    while (!ready.empty())
    {
        const std::size_t index = ready.begin()->second;
        ready.erase(ready.begin());
        loadOrder.push_back(index);
        At(emitted, index) = 1;
        for (const std::size_t consumer : At(adjacency, index))
        {
            if (--At(inDegree, consumer) == 0)
            {
                ready.insert({At(SnapshotEntries, consumer).Id, consumer});
            }
        }
    }
    if (loadOrder.size() != remaining)
    {
        std::string listed;
        for (std::size_t index = 0; index < SnapshotEntries.size(); ++index)
        {
            if (At(participating, index) != 0 && At(emitted, index) == 0)
            {
                listed += "\"" + At(SnapshotEntries, index).Id + "\" ";
            }
        }
        return Result<SolveOutcome>::Err(
            SolveError("dependency cycle or blocked plugins: " + listed + "(hard & pure-optional edges bite, D52)"));
    }

    // ⑥⑦⑧ 条目装配：kLoad 段 = 拓扑序；kSkip 段尾随、按快照 Id 序。
    const auto buildLoadEntry = [&](std::size_t index)
    {
        const ManifestEntry& entry = At(SnapshotEntries, index);
        LoadPlanEntry planEntry;
        planEntry.Id = entry.Id; // 借快照（D61）
        planEntry.BinaryPath = SnapshotDirectory / entry.Subdirectory / catalog_detail::LibraryFileName(entry.Binary);
        ConfigBlob merged;
        for (const ManifestConfigField& field : entry.Config)
        {
            merged.Set(field.Key, field.DefaultValue());
        }
        for (const PluginOverride* layer : layers)
        {
            if (layer->Id != entry.Id)
            {
                continue;
            }
            for (const ConfigBlob::Entry& kv : layer->Config.Entries())
            {
                const ManifestConfigField* field = FindField(entry, kv.Key);
                if (field == nullptr)
                {
                    continue;
                }
                if (const std::optional<Value> coerced = Coerce(FromStorage(kv.Stored), field->Kind))
                {
                    merged.Set(kv.Key, *coerced);
                }
            }
        }
        planEntry.ResolvedConfig = std::move(merged);
        return planEntry;
    };

    for (const std::size_t index : loadOrder)
    {
        outcome.Plan.Ordered.push_back(buildLoadEntry(index));
    }
    for (std::size_t index = 0; index < SnapshotEntries.size(); ++index)
    {
        if (At(participating, index) != 0)
        {
            continue;
        }
        const ManifestEntry& entry = At(SnapshotEntries, index);
        LoadPlanEntry skip;
        skip.Id = entry.Id;
        skip.BinaryPath = SnapshotDirectory / entry.Subdirectory / catalog_detail::LibraryFileName(entry.Binary);
        skip.Decision = LoadDecision::kSkip;
        skip.Reason = At(reasons, index); // kDisabled / kMissingDependency / kVersionMismatch
        outcome.Plan.Ordered.push_back(std::move(skip));
    }
    return Result<SolveOutcome>::Ok(std::move(outcome));
}

} // namespace vase
