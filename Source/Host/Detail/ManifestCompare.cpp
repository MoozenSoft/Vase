#include "ManifestCompare.h"

#include "Vase/Config/ConfigInfo.h"
#include "Vase/Config/FieldInfo.h"
#include "Vase/Config/Value.h"
#include "Vase/Detail/MetaArray.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/ConfigBlob.h"
#include "Vase/Host/ManifestExpectation.h"
#include "Vase/PluginDescriptor.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace
{

using vase::ChoiceInfo;
using vase::ConfigBlob;
using vase::Error;
using vase::ExpectedChoice;
using vase::ExpectedConfigField;
using vase::ExpectedService;
using vase::FieldInfo;
using vase::ManifestExpectation;
using vase::MetaArray;
using vase::PluginMeta;
using vase::Result;
using vase::ServiceRef;
using vase::Value;
using vase::ValueKind;

using ServiceSet = std::vector<std::pair<std::string, std::uint32_t>>;
using DiffList = std::vector<std::string>;

// token 与 PluginHost.cpp 的 D32 表逐字同源（kNone..kEnum）：比对消息与 ConfigApply 消息
// 是两个通道，但 Kind 的拼写不开第二套。
std::string_view KindText(ValueKind kind)
{
    switch (kind)
    {
    case ValueKind::kNone:
        return "kNone";
    case ValueKind::kBool:
        return "kBool";
    case ValueKind::kInt32:
        return "kInt32";
    case ValueKind::kInt64:
        return "kInt64";
    case ValueKind::kFloat:
        return "kFloat";
    case ValueKind::kDouble:
        return "kDouble";
    case ValueKind::kString:
        return "kString";
    case ValueKind::kEnum:
        return "kEnum";
    }
    return "kOutOfRange";
}

std::string_view TextOf(const char* text) { return text != nullptr ? std::string_view{text} : std::string_view{}; }

// 描述符侧 Value → 拥有形（D72 比对面）：kNone = 未设 → nullopt；kEnum 走 int32 位形
// 零扩展链（与 Value::GetAs<std::int32_t> 同一条）。
std::optional<ConfigBlob::Storage> StorageFromValue(const Value& value)
{
    switch (value.Kind)
    {
    case ValueKind::kNone:
        return std::nullopt;
    case ValueKind::kBool:
        return ConfigBlob::Storage{value.GetAs<bool>()};
    case ValueKind::kInt32:
        return ConfigBlob::Storage{value.GetAs<std::int32_t>()};
    case ValueKind::kInt64:
        return ConfigBlob::Storage{value.GetAs<std::int64_t>()};
    case ValueKind::kFloat:
        return ConfigBlob::Storage{value.GetAs<float>()};
    case ValueKind::kDouble:
        return ConfigBlob::Storage{value.GetAs<double>()};
    case ValueKind::kString:
        return ConfigBlob::Storage{std::string{TextOf(value.GetAs<const char*>())}};
    case ValueKind::kEnum:
        return ConfigBlob::Storage{ConfigBlob::EnumStored{value.GetAs<std::int32_t>()}};
    }
    return std::nullopt;
}

std::string StorageText(const ConfigBlob::Storage& storage)
{
    if (const auto* flag = std::get_if<bool>(&storage))
    {
        return *flag ? "true" : "false";
    }
    if (const auto* number = std::get_if<std::int32_t>(&storage))
    {
        return std::to_string(*number);
    }
    if (const auto* number = std::get_if<std::int64_t>(&storage))
    {
        return std::to_string(*number);
    }
    if (const auto* number = std::get_if<float>(&storage))
    {
        return std::to_string(*number);
    }
    if (const auto* number = std::get_if<double>(&storage))
    {
        return std::to_string(*number);
    }
    if (const auto* text = std::get_if<std::string>(&storage))
    {
        return *text;
    }
    if (const auto* enumerated = std::get_if<ConfigBlob::EnumStored>(&storage))
    {
        return std::to_string(enumerated->Value);
    }
    return "unknown";
}

// EnumStored 无 ==（POD 纪律），七类分派各自比；index 不等直接 false（kEnum 与 kInt32
// 各存各的，D78——variant 的替代位就是类型本身）。
bool StorageEquals(const ConfigBlob::Storage& left, const ConfigBlob::Storage& right)
{
    if (left.index() != right.index())
    {
        return false;
    }
    return std::visit(
        [](const auto& leftValue, const auto& rightValue)
        {
            using Left = std::remove_cvref_t<decltype(leftValue)>;
            using Right = std::remove_cvref_t<decltype(rightValue)>;
            if constexpr (!std::is_same_v<Left, Right>)
            {
                return false; // visit 为全组合实例化；index 等值检查已挡住异型分支
            }
            else if constexpr (std::is_same_v<Left, ConfigBlob::EnumStored>)
            {
                return leftValue.Value == rightValue.Value; // EnumStored 无 ==（POD 纪律），比成员
            }
            else
            {
                return leftValue == rightValue; // kString 比文本（裁定 P-3），标量按位等值
            }
        },
        left, right);
}

// spec §6 消息形：<字段路径> — manifest "<v1>" != binary "<v2>"。
void AddDiff(DiffList& diffs, std::string_view path, std::string_view manifestValue, std::string_view binaryValue)
{
    diffs.push_back(std::string{path} + " — manifest \"" + std::string{manifestValue} + "\" != binary \"" +
                    std::string{binaryValue} + "\"");
}

ServiceSet ExpectSet(const std::vector<ExpectedService>& services)
{
    ServiceSet set;
    set.reserve(services.size());
    for (const ExpectedService& service : services)
    {
        set.emplace_back(service.Name, service.Version);
    }
    return set;
}

ServiceSet BinarySet(const MetaArray<ServiceRef, 16>& declared)
{
    ServiceSet set;
    set.reserve(declared.Size());
    for (std::size_t index = 0; index < declared.Size(); ++index)
    {
        const ServiceRef& service = *std::next(declared.Begin(), static_cast<std::ptrdiff_t>(index));
        set.emplace_back(std::string{service.Name}, service.Version);
    }
    return set;
}

std::string SetPath(std::string_view label, const std::pair<std::string, std::uint32_t>& service)
{
    return std::string{label} + "[" + service.first + " v" + std::to_string(service.second) + "]";
}

// (name,version) 多重集等值：两侧排序后归并走查——缺员/超员/重复条数各出分歧（D72 含条数）。
void CompareServiceSet(std::string_view label, const ServiceSet& manifestSet, const ServiceSet& binarySet,
                       DiffList& diffs)
{
    ServiceSet left = manifestSet;
    ServiceSet right = binarySet;
    std::ranges::sort(left);
    std::ranges::sort(right);
    auto manifestIt = left.begin();
    auto binaryIt = right.begin();
    while (manifestIt != left.end() || binaryIt != right.end())
    {
        if (binaryIt == right.end() || (manifestIt != left.end() && *manifestIt < *binaryIt))
        {
            AddDiff(diffs, SetPath(label, *manifestIt) + " missing in binary", "declared", "absent");
            ++manifestIt;
        }
        else if (manifestIt == left.end() || *binaryIt < *manifestIt)
        {
            AddDiff(diffs, SetPath(label, *binaryIt) + " missing in manifest", "absent", "declared");
            ++binaryIt;
        }
        else
        {
            ++manifestIt;
            ++binaryIt;
        }
    }
}

// M3/D93：字符串多重集等值（与 CompareServiceSet 同律：排序后归并走查，含条数）。
void CompareStringSet(std::string_view label, std::vector<std::string> manifestSet, std::vector<std::string> binarySet,
                      DiffList& diffs)
{
    std::ranges::sort(manifestSet);
    std::ranges::sort(binarySet);
    auto manifestIt = manifestSet.begin();
    auto binaryIt = binarySet.begin();
    while (manifestIt != manifestSet.end() || binaryIt != binarySet.end())
    {
        if (binaryIt == binarySet.end() || (manifestIt != manifestSet.end() && *manifestIt < *binaryIt))
        {
            AddDiff(diffs, std::string{label} + "[" + *manifestIt + "] missing in binary", "declared", "absent");
            ++manifestIt;
        }
        else if (manifestIt == manifestSet.end() || *binaryIt < *manifestIt)
        {
            AddDiff(diffs, std::string{label} + "[" + *binaryIt + "] missing in manifest", "absent", "declared");
            ++binaryIt;
        }
        else
        {
            ++manifestIt;
            ++binaryIt;
        }
    }
}

// nullopt ↔ 描述符侧 Value.Kind==kNone 互为对位（裁定 P-3）；两侧都有值才谈 StorageEquals。
void CompareBound(DiffList& diffs, std::string_view path, const std::optional<ConfigBlob::Storage>& manifestValue,
                  const Value& binaryValue)
{
    const std::optional<ConfigBlob::Storage> binary = StorageFromValue(binaryValue);
    if (manifestValue.has_value() == binary.has_value() &&
        (!manifestValue.has_value() || StorageEquals(*manifestValue, *binary)))
    {
        return;
    }
    AddDiff(diffs, path, manifestValue.has_value() ? StorageText(*manifestValue) : "unset",
            binary.has_value() ? StorageText(*binary) : "unset");
}

void CompareField(const ExpectedConfigField& manifestField, const FieldInfo& binaryField, DiffList& diffs)
{
    const std::string path = "config \"" + manifestField.Key + "\"";
    if (manifestField.Kind != binaryField.Kind)
    {
        AddDiff(diffs, path + ".kind", KindText(manifestField.Kind), KindText(binaryField.Kind));
    }
    // default 也走 CompareBound——P-3 同律：两侧皆未设 = 相等，不出 diff。
    CompareBound(diffs, path + ".default", manifestField.Default, binaryField.Default);
    CompareBound(diffs, path + ".min", manifestField.Min, binaryField.Min);
    CompareBound(diffs, path + ".max", manifestField.Max, binaryField.Max);
    if (manifestField.Label != TextOf(binaryField.Label))
    {
        AddDiff(diffs, path + ".label", manifestField.Label, TextOf(binaryField.Label));
    }
    if (manifestField.Choices.size() != binaryField.ChoiceCount)
    {
        AddDiff(diffs, path + ".choices", std::to_string(manifestField.Choices.size()),
                std::to_string(binaryField.ChoiceCount));
        return;
    }
    for (std::size_t index = 0; index < manifestField.Choices.size(); ++index)
    {
        const ExpectedChoice& choice = *std::next(manifestField.Choices.begin(), static_cast<std::ptrdiff_t>(index));
        const ChoiceInfo& declared = *std::next(binaryField.Choices, static_cast<std::ptrdiff_t>(index));
        const std::string itemPath = path + ".choices[" + std::to_string(index) + "]";
        if (choice.Value != declared.Value)
        {
            AddDiff(diffs, itemPath + ".value", std::to_string(choice.Value), std::to_string(declared.Value));
        }
        if (choice.Label != TextOf(declared.Label))
        {
            AddDiff(diffs, itemPath + ".label", choice.Label, TextOf(declared.Label));
        }
    }
}

void CompareConfig(const std::vector<ExpectedConfigField>& fields, const PluginMeta& meta, DiffList& diffs)
{
    std::map<std::string_view, const ExpectedConfigField*> manifestFields;
    for (const ExpectedConfigField& field : fields)
    {
        manifestFields.emplace(field.Key, &field);
    }
    std::map<std::string_view, const FieldInfo*> binaryFields;
    for (std::size_t index = 0; index < meta.Config.Count; ++index)
    {
        const FieldInfo* field = std::next(meta.Config.Fields, static_cast<std::ptrdiff_t>(index));
        binaryFields.emplace(TextOf(field->Name), field);
    }
    for (const auto& [key, manifestField] : manifestFields)
    {
        const auto located = binaryFields.find(key);
        if (located == binaryFields.end())
        {
            AddDiff(diffs, "config \"" + std::string{key} + "\" missing in binary", "declared", "absent");
            continue;
        }
        CompareField(*manifestField, *located->second, diffs);
    }
    for (const auto& key : binaryFields | std::views::keys)
    {
        if (!manifestFields.contains(key))
        {
            AddDiff(diffs, "config \"" + std::string{key} + "\" missing in manifest", "absent", "declared");
        }
    }
}

} // namespace

namespace vase::detail
{

Result<void> CompareDescriptor(const ManifestExpectation& expected, const PluginDescriptor& desc)
{
    const PluginMeta& meta = *desc.Meta;
    DiffList diffs;
    // M3/D101 替代：结构化绑定的标识符个数**必须**等于 ManifestExpectation 的非静态数据成员数，
    // 少一个即硬编译错误——给期望形加字段而忘了比对，这一行当场红。逐个接进下方比对（绑定即被用过），
    // 比对那一半因此同样有闸。构造那一半不靠它：BuildExpectation 漏填新字段会让期望恒为空集，
    // 凡清单列了名字的插件当场被加载期拒——响亮。
    const auto& [id, displayName, version, requiresRefs, optionalRequiresRefs, providesRefs, configFields,
                 processStates] = expected;
    if (id != meta.Id)
    {
        AddDiff(diffs, "id", id, meta.Id);
    }
    if (displayName != meta.DisplayName)
    {
        AddDiff(diffs, "displayName", displayName, meta.DisplayName);
    }
    if (version != meta.Version)
    {
        AddDiff(diffs, "version", version, meta.Version);
    }
    CompareServiceSet("requires", ExpectSet(requiresRefs), BinarySet(meta.Requires), diffs);
    CompareServiceSet("optionalRequires", ExpectSet(optionalRequiresRefs), BinarySet(meta.OptionalRequires), diffs);
    CompareServiceSet("provides", ExpectSet(providesRefs), BinarySet(meta.Provides), diffs);
    CompareConfig(configFields, meta, diffs);
    std::vector<std::string> declaredStates;
    declaredStates.reserve(meta.ProcessStates.Size());
    for (std::size_t index = 0; index < meta.ProcessStates.Size(); ++index)
    {
        const ProcessStateDesc& state = *std::next(meta.ProcessStates.Begin(), static_cast<std::ptrdiff_t>(index));
        declaredStates.emplace_back(state.Name);
    }
    CompareStringSet("processStates", processStates, std::move(declaredStates), diffs);
    if (diffs.empty())
    {
        return Result<void>::Ok();
    }
    // 首条分歧即拼消息（全收为后续任务留扩展位，消息面 §6 只钉一条）。
    return Result<void>::Err(Error{"manifest/binary mismatch for \"" + id + "\": " + diffs.front()});
}

} // namespace vase::detail
