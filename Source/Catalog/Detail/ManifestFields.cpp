// 单插件与包成员共用规则；JSON_NOEXCEPTION 下任何 get<T>() 之前先检查类型。
#include "ManifestFields.h"

#include "Vase/Catalog/ManifestView.h"
#include "Vase/Config/Value.h"
#include "Vase/Detail/Result.h"
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <ios>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace vase::catalog_detail
{
using nlohmann::json;

Error ManifestError(const std::filesystem::path& file, std::string_view detail)
{
    return ManifestError(std::string_view(file.string()), detail);
}

Error ManifestError(std::string_view location, std::string_view detail)
{
    return Error("plugin manifest \"" + std::string(location) + "\": " + std::string(detail));
}

Result<json> ReadJsonContents(std::string_view contents, std::string_view location)
{
    // JSON 的原始 NUL 非法；lexer 将其视作 EOF，须阻止有效前缀掩盖后续字节。
    if (contents.find('\0') != std::string_view::npos)
    {
        return Result<json>::Err(ManifestError(location, "malformed JSON"));
    }
    json parsed = json::parse(contents.begin(), contents.end(), nullptr, false);
    if (parsed.is_discarded())
    {
        return Result<json>::Err(ManifestError(location, "malformed JSON"));
    }
    return Result<json>::Ok(std::move(parsed));
}

Result<json> ReadJsonFile(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    if (!in)
    {
        return Result<json>::Err(ManifestError(file, "cannot open file"));
    }
    const std::string text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    return ReadJsonContents(text, file.string());
}

bool OnlyKeys(const json& object, const std::initializer_list<std::string_view>& allowed, std::string& offender)
{
    for (auto it = object.begin(); it != object.end(); ++it)
    {
        if (std::ranges::find(allowed, std::string_view(it.key())) == allowed.end())
        {
            offender = it.key();
            return false;
        }
    }
    return true;
}

namespace
{

bool AsInt64(const json& v, std::int64_t& out)
{
    if (!v.is_number_integer())
    {
        return false;
    }
    if (v.is_number_unsigned())
    {
        const std::uint64_t u = v.get<std::uint64_t>();
        if (u > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        {
            return false;
        }
        out = static_cast<std::int64_t>(u);
        return true;
    }
    out = v.get<std::int64_t>();
    return true;
}

bool TypeName(std::string_view name, ValueKind& out)
{
    if (name == "bool")
    {
        out = ValueKind::kBool;
    }
    else if (name == "int32")
    {
        out = ValueKind::kInt32;
    }
    else if (name == "int64")
    {
        out = ValueKind::kInt64;
    }
    else if (name == "float")
    {
        out = ValueKind::kFloat;
    }
    else if (name == "double")
    {
        out = ValueKind::kDouble;
    }
    else if (name == "string")
    {
        out = ValueKind::kString;
    }
    else if (name == "enum")
    {
        out = ValueKind::kEnum;
    }
    else
    {
        return false;
    }
    return true;
}

// 按声明类型装值（spec §4：同型；数值加宽仅 int→float/double）。false = 不匹配。
bool Encode(const json& v, ValueKind kind, std::uint64_t& bits, std::string& str)
{
    std::int64_t i = 0;
    switch (kind)
    {
    case ValueKind::kBool:
        if (!v.is_boolean())
        {
            return false;
        }
        bits = v.get<bool>() ? 1U : 0U;
        return true;
    case ValueKind::kInt32:
        if (!AsInt64(v, i) || i < std::numeric_limits<std::int32_t>::min() ||
            i > std::numeric_limits<std::int32_t>::max())
        {
            return false;
        }
        bits = static_cast<std::uint64_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(i)));
        return true;
    case ValueKind::kInt64:
        if (!AsInt64(v, i))
        {
            return false;
        }
        bits = static_cast<std::uint64_t>(i);
        return true;
    case ValueKind::kFloat:
    {
        double d = 0.0;
        if (v.is_number_integer())
        {
            if (!AsInt64(v, i))
            {
                return false;
            }
            d = static_cast<double>(i);
        }
        else if (v.is_number_float())
        {
            d = v.get<double>();
        }
        else
        {
            return false;
        }
        bits = std::bit_cast<std::uint32_t>(static_cast<float>(d));
        return true;
    }
    case ValueKind::kDouble:
        if (v.is_number_integer())
        {
            if (!AsInt64(v, i))
            {
                return false;
            }
            bits = std::bit_cast<std::uint64_t>(static_cast<double>(i));
            return true;
        }
        if (!v.is_number_float())
        {
            return false;
        }
        bits = std::bit_cast<std::uint64_t>(v.get<double>());
        return true;
    case ValueKind::kString:
        if (!v.is_string())
        {
            return false;
        }
        str = v.get<std::string>();
        return true;
    default:
        return false;
    }
}

// min/max 仅数值四型可出现（ManifestView.h 的解析期保证）；bool/string 在闸外。
bool IsNumericKind(ValueKind kind)
{
    return kind == ValueKind::kInt32 || kind == ValueKind::kInt64 || kind == ValueKind::kFloat ||
           kind == ValueKind::kDouble;
}

// min≤max 用；两侧同 Kind（Encode 保证），数值四型才有意义（bool/string 在闸外）。
bool ValueLess(ValueKind kind, std::uint64_t a, std::uint64_t b)
{
    switch (kind)
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
}

// unknown 键闸（D49/D64）；首个 offender 键经出参带回，供调用方点名进诊断。
// ===== 序列化侧（M5/T5，D122/D134）：写出的键与上面白名单同源，改一处必查另一处 =====

// ValueKind → 清单类型名（与上面解析侧的七支逐字对应：改一处必改另一处，round-trip 用例守）。
std::string_view KindName(ValueKind kind)
{
    if (kind == ValueKind::kBool)
    {
        return "bool";
    }
    if (kind == ValueKind::kInt32)
    {
        return "int32";
    }
    if (kind == ValueKind::kInt64)
    {
        return "int64";
    }
    if (kind == ValueKind::kFloat)
    {
        return "float";
    }
    if (kind == ValueKind::kDouble)
    {
        return "double";
    }
    if (kind == ValueKind::kString)
    {
        return "string";
    }
    return "enum";
}

json ValueToJson(const Value& value)
{
    switch (value.Kind)
    {
    case ValueKind::kBool:
        return value.GetAs<bool>();
    case ValueKind::kInt32:
    case ValueKind::kEnum: // enum 的 min/max 是数值；default 另走 label 支（D80）
        return value.GetAs<std::int32_t>();
    case ValueKind::kInt64:
        return value.GetAs<std::int64_t>();
    case ValueKind::kFloat:
        return value.GetAs<float>();
    case ValueKind::kDouble:
        return value.GetAs<double>();
    case ValueKind::kString:
    {
        const char* text = value.GetAs<const char*>();
        return text == nullptr ? json("") : json(std::string(text));
    }
    case ValueKind::kNone:
        break;
    }
    return {}; // kNone 不进清单：调用方只在有值时调本函数
}

// 键一律 emplace 进新建 object()（非 operator[] 赋值）：pro-bounds 一族按本仓库 tidy 口径改写法，
// 键皆新且唯一 ⇒ 与 upsert 语义逐位相同；object_t 是 std::map，dump 键序两条路都是字典序。
json FieldToJson(const ManifestConfigField& field)
{
    json item = json::object();
    item.emplace("key", field.Key);
    item.emplace("type", std::string(KindName(field.Kind)));
    if (!field.DisplayName.empty())
    {
        item.emplace("displayName", field.DisplayName);
    }
    if (field.Kind == ValueKind::kString)
    {
        item.emplace("default", field.DefaultStr); // 拥有字符串可能含 NUL，不能经 c_str() 恢复长度。
    }
    else if (field.Kind == ValueKind::kEnum)
    {
        item.emplace("default", field.DefaultStr); // D80：enum 中间形存 label，不是 value
        json choices = json::array();
        for (const ManifestChoice& choice : field.Choices)
        {
            choices.push_back(json{{"value", choice.Value}, {"label", choice.Label}});
        }
        item.emplace("choices", std::move(choices));
    }
    else
    {
        const Value def = field.DefaultValue();
        if (def.Kind != ValueKind::kNone)
        {
            item.emplace("default", ValueToJson(def));
        }
    }
    if (field.HasMin)
    {
        item.emplace("min", ValueToJson(field.MinValue()));
    }
    if (field.HasMax)
    {
        item.emplace("max", ValueToJson(field.MaxValue()));
    }
    return item;
}

json DepsToJson(const std::vector<ManifestDependency>& deps)
{
    json array = json::array();
    for (const ManifestDependency& dep : deps)
    {
        array.push_back(json{{"service", dep.Service}, {"version", dep.Version}});
    }
    return array;
}

Result<std::vector<ManifestDependency>> ParseDeps(const json& array, std::string_view fieldName, std::string_view file)
{
    std::vector<ManifestDependency> out;
    if (!array.is_array())
    {
        return Result<std::vector<ManifestDependency>>::Err(
            ManifestError(file, "field \"" + std::string(fieldName) + "\" must be an array"));
    }
    for (const json& item : array)
    {
        if (!item.is_object())
        {
            return Result<std::vector<ManifestDependency>>::Err(
                ManifestError(file, std::string(fieldName) + " entries must be {service, version} objects"));
        }
        std::string offender;
        if (!OnlyKeys(item, {"service", "version"}, offender))
        {
            return Result<std::vector<ManifestDependency>>::Err(
                ManifestError(file, std::string(fieldName) + " entries must be {service, version} objects; got \"" +
                                        offender + "\""));
        }
        const auto service = item.find("service");
        const auto version = item.find("version");
        if (service == item.end() || !service->is_string() || service->get_ref<const std::string&>().empty())
        {
            return Result<std::vector<ManifestDependency>>::Err(
                ManifestError(file, std::string(fieldName) + ": \"service\" must be a non-empty string"));
        }
        std::int64_t v = 0;
        if (version == item.end() || !AsInt64(*version, v) || v < 1 ||
            !std::cmp_less_equal(v, std::numeric_limits<std::uint32_t>::max()))
        {
            return Result<std::vector<ManifestDependency>>::Err(
                ManifestError(file, std::string(fieldName) + ": \"version\" must be an integer >= 1 (D64)"));
        }
        const auto& serviceName = service->get_ref<const std::string&>();
        for (const ManifestDependency& held : out)
        {
            if (held.Service == serviceName && std::cmp_equal(held.Version, v))
            {
                return Result<std::vector<ManifestDependency>>::Err(
                    ManifestError(file, std::string(fieldName) + ": duplicate (service,version) entry (D64)"));
            }
        }
        out.push_back(ManifestDependency{.Service = serviceName, .Version = static_cast<std::uint32_t>(v)});
    }
    return Result<std::vector<ManifestDependency>>::Ok(std::move(out));
}

// M3/D93：进程级状态的名字数组。重名即刻拒——与 choices 的重值/重 label 同例（D80），
// 而不是留给比对器报「多重集不等」（那是作者拿不到的归因）。
Result<std::vector<std::string>> ParseProcessStates(const json& array, std::string_view file)
{
    std::vector<std::string> out;
    if (!array.is_array())
    {
        return Result<std::vector<std::string>>::Err(ManifestError(file, "field \"processStates\" must be an array"));
    }
    for (const json& item : array)
    {
        if (!item.is_string() || item.get_ref<const std::string&>().empty())
        {
            return Result<std::vector<std::string>>::Err(
                ManifestError(file, "processStates entries must be non-empty strings"));
        }
        const auto& name = item.get_ref<const std::string&>();
        for (const std::string& held : out)
        {
            if (held == name)
            {
                return Result<std::vector<std::string>>::Err(
                    ManifestError(file, "processStates: duplicate \"" + name + "\" (D93)"));
            }
        }
        out.push_back(name);
    }
    return Result<std::vector<std::string>>::Ok(std::move(out));
}

Result<std::vector<ManifestConfigField>> ParseConfig(const json& array, std::string_view file)
{
    std::vector<ManifestConfigField> out;
    if (!array.is_array())
    {
        return Result<std::vector<ManifestConfigField>>::Err(ManifestError(file, "field \"config\" must be an array"));
    }
    for (const json& item : array)
    {
        if (!item.is_object())
        {
            return Result<std::vector<ManifestConfigField>>::Err(
                ManifestError(file, "config entries accept only {key,type,default,min,max,displayName,choices}"));
        }
        std::string offender;
        if (!OnlyKeys(item, {"key", "type", "default", "min", "max", "displayName", "choices"}, offender))
        {
            return Result<std::vector<ManifestConfigField>>::Err(ManifestError(
                file,
                "config entries accept only {key,type,default,min,max,displayName,choices}; got \"" + offender + "\""));
        }
        const auto key = item.find("key");
        const auto type = item.find("type");
        const auto def = item.find("default");
        if (key == item.end() || !key->is_string() || key->get_ref<const std::string&>().empty())
        {
            return Result<std::vector<ManifestConfigField>>::Err(
                ManifestError(file, "config: \"key\" must be a non-empty string"));
        }
        ManifestConfigField field;
        field.Key = key->get_ref<const std::string&>();
        if (type == item.end() || !type->is_string() || !TypeName(type->get_ref<const std::string&>(), field.Kind))
        {
            return Result<std::vector<ManifestConfigField>>::Err(ManifestError(
                file, "config \"" + field.Key + "\": unknown type (seven kinds; enum legal since wave 2, D49)"));
        }
        const auto choices = item.find("choices");
        if (field.Kind != ValueKind::kEnum)
        {
            if (choices != item.end())
            {
                return Result<std::vector<ManifestConfigField>>::Err(
                    ManifestError(file, "config \"" + field.Key + R"(": "choices" only with enum type (D80))"));
            }
        }
        else
        {
            if (choices == item.end())
            {
                return Result<std::vector<ManifestConfigField>>::Err(
                    ManifestError(file, "config \"" + field.Key + R"(": enum requires "choices" (D80))"));
            }
            if (!choices->is_array() || choices->empty())
            {
                return Result<std::vector<ManifestConfigField>>::Err(ManifestError(
                    file, "config \"" + field.Key + R"(": enum "choices" must be a non-empty array (D80))"));
            }
            for (const json& choice : *choices)
            {
                if (!choice.is_object())
                {
                    return Result<std::vector<ManifestConfigField>>::Err(ManifestError(
                        file, "config \"" + field.Key + "\": enum choices entries must be {value,label} objects"));
                }
                std::string choiceOffender;
                if (!OnlyKeys(choice, {"value", "label"}, choiceOffender))
                {
                    return Result<std::vector<ManifestConfigField>>::Err(
                        ManifestError(file, "config \"" + field.Key +
                                                "\": enum choices entries must be {value,label} objects; got \"" +
                                                choiceOffender + "\""));
                }
                // 越界与 default 的 int32 闸同通道（Encode 的 kInt32 段）；非整数一并落此消息。
                std::uint64_t valueBits = 0;
                std::string valueIgnored;
                const auto value = choice.find("value");
                if (value == choice.end() || !Encode(*value, ValueKind::kInt32, valueBits, valueIgnored))
                {
                    return Result<std::vector<ManifestConfigField>>::Err(
                        ManifestError(file, "config \"" + field.Key +
                                                R"(": enum choice "value" must be an int32-range integer (D80))"));
                }
                const auto label = choice.find("label");
                if (label == choice.end() || !label->is_string() || label->get_ref<const std::string&>().empty())
                {
                    return Result<std::vector<ManifestConfigField>>::Err(ManifestError(
                        file, "config \"" + field.Key + R"(": enum choice "label" must be a non-empty string (D80))"));
                }
                ManifestChoice candidate{
                    .Value = static_cast<std::int32_t>(static_cast<std::uint32_t>(valueBits)),
                    .Label = label->get_ref<const std::string&>(),
                };
                for (const ManifestChoice& held : field.Choices)
                {
                    if (held.Value == candidate.Value)
                    {
                        return Result<std::vector<ManifestConfigField>>::Err(
                            ManifestError(file, "config \"" + field.Key + "\": enum choices: duplicate value (D80)"));
                    }
                    if (held.Label == candidate.Label)
                    {
                        return Result<std::vector<ManifestConfigField>>::Err(
                            ManifestError(file, "config \"" + field.Key + "\": enum choices: duplicate label (D80)"));
                    }
                }
                field.Choices.push_back(std::move(candidate));
            }
        }
        if (def == item.end())
        {
            return Result<std::vector<ManifestConfigField>>::Err(
                ManifestError(file, "config \"" + field.Key + R"(": "default" is required)"));
        }
        if (field.Kind == ValueKind::kEnum)
        {
            if (!def->is_string())
            {
                return Result<std::vector<ManifestConfigField>>::Err(
                    ManifestError(file, "config \"" + field.Key + "\": enum default must be a choice label (D80)"));
            }
            const auto& defLabel = def->get_ref<const std::string&>();
            if (std::ranges::find(field.Choices, defLabel, &ManifestChoice::Label) == field.Choices.end())
            {
                return Result<std::vector<ManifestConfigField>>::Err(ManifestError(
                    file, "config \"" + field.Key + "\": enum default \"" + defLabel + "\" not in choices (D80)"));
            }
            field.DefaultStr = defLabel; // 中间形存 label 的 kString；换 value 归 Solve（D80）
        }
        else if (!Encode(*def, field.Kind, field.DefaultBits, field.DefaultStr))
        {
            return Result<std::vector<ManifestConfigField>>::Err(
                ManifestError(file, "config \"" + field.Key + "\": default does not match declared type"));
        }
        std::uint64_t boundBits = 0;
        std::string ignored;
        if (const auto min = item.find("min"); min != item.end())
        {
            if (!IsNumericKind(field.Kind))
            {
                return Result<std::vector<ManifestConfigField>>::Err(
                    ManifestError(file, "config \"" + field.Key + "\": min/max only for numeric types"));
            }
            if (!Encode(*min, field.Kind, boundBits, ignored))
            {
                return Result<std::vector<ManifestConfigField>>::Err(
                    ManifestError(file, "config \"" + field.Key + "\": min does not match declared type"));
            }
            field.MinBits = boundBits;
            field.HasMin = true;
        }
        if (const auto max = item.find("max"); max != item.end())
        {
            if (!IsNumericKind(field.Kind))
            {
                return Result<std::vector<ManifestConfigField>>::Err(
                    ManifestError(file, "config \"" + field.Key + "\": min/max only for numeric types"));
            }
            if (!Encode(*max, field.Kind, boundBits, ignored))
            {
                return Result<std::vector<ManifestConfigField>>::Err(
                    ManifestError(file, "config \"" + field.Key + "\": max does not match declared type"));
            }
            field.MaxBits = boundBits;
            field.HasMax = true;
        }
        if (field.HasMin && field.HasMax && ValueLess(field.Kind, field.MaxBits, field.MinBits))
        {
            return Result<std::vector<ManifestConfigField>>::Err(
                ManifestError(file, "config \"" + field.Key + "\": min > max (D64)"));
        }
        if (const auto display = item.find("displayName"); display != item.end())
        {
            if (!display->is_string())
            {
                return Result<std::vector<ManifestConfigField>>::Err(
                    ManifestError(file, "config \"" + field.Key + "\": displayName must be a string"));
            }
            field.DisplayName = display->get_ref<const std::string&>();
        }
        for (const ManifestConfigField& held : out)
        {
            if (held.Key == field.Key)
            {
                return Result<std::vector<ManifestConfigField>>::Err(
                    ManifestError(file, "config: duplicate key \"" + field.Key + "\" (D64)"));
            }
        }
        out.push_back(std::move(field));
    }
    return Result<std::vector<ManifestConfigField>>::Ok(std::move(out));
}

} // namespace

Result<ManifestEntry> ParsePluginFields(const json& root, std::string_view file, std::string_view subdirectoryName,
                                        ManifestFieldMode mode)
{
    if (!root.is_object())
    {
        return Result<ManifestEntry>::Err(ManifestError(file, "top level must be a JSON object"));
    }
    std::string unknownField;
    const bool single = mode == ManifestFieldMode::kSinglePlugin;
    const bool keysOk = single ? OnlyKeys(root,
                                          {
                                              "schemaVersion",
                                              "id",
                                              "displayName",
                                              "version",
                                              "binary",
                                              "enabledByDefault",
                                              "requires",
                                              "optionalRequires",
                                              "provides",
                                              "config",
                                              "processStates",
                                          },
                                          unknownField)
                               : OnlyKeys(root,
                                          {
                                              "id",
                                              "displayName",
                                              "version",
                                              "enabledByDefault",
                                              "requires",
                                              "optionalRequires",
                                              "provides",
                                              "config",
                                              "processStates",
                                          },
                                          unknownField);
    if (!keysOk)
    {
        return Result<ManifestEntry>::Err(
            ManifestError(file, "unknown top-level field \"" + unknownField + "\" (D49)"));
    }

    ManifestEntry entry;
    entry.Subdirectory = std::string(subdirectoryName);

    if (single)
    {
        const auto schemaVersion = root.find("schemaVersion");
        if (schemaVersion == root.end() || !schemaVersion->is_number_integer())
        {
            return Result<ManifestEntry>::Err(
                ManifestError(file, "schemaVersion required, integer form only (1.0 rejected, D64)"));
        }
        if (schemaVersion->get<std::int64_t>() != 1)
        {
            return Result<ManifestEntry>::Err(ManifestError(file, "unsupported schemaVersion major (expected 1)"));
        }
    }
    const auto id = root.find("id");
    if (id == root.end() || !id->is_string() || id->get_ref<const std::string&>().empty())
    {
        return Result<ManifestEntry>::Err(ManifestError(file, "\"id\" required and non-empty"));
    }
    entry.Id = id->get_ref<const std::string&>();

    if (const auto dn = root.find("displayName"); dn != root.end())
    {
        if (!dn->is_string())
        {
            return Result<ManifestEntry>::Err(ManifestError(file, "displayName must be a string"));
        }
        entry.DisplayName = dn->get_ref<const std::string&>();
    }
    else
    {
        entry.DisplayName = entry.Id; // spec §4 缺省
    }
    if (const auto ver = root.find("version"); ver != root.end())
    {
        if (!ver->is_string())
        {
            return Result<ManifestEntry>::Err(ManifestError(file, "version must be a string"));
        }
        entry.Version = ver->get_ref<const std::string&>();
    }
    if (single)
    {
        if (const auto bin = root.find("binary"); bin != root.end())
        {
            if (!bin->is_string())
            {
                return Result<ManifestEntry>::Err(ManifestError(file, "binary must be a string"));
            }
            const auto& stem = bin->get_ref<const std::string&>();
            if (stem.empty() || stem == "." || stem == ".." || stem.find_first_of("/\\") != std::string::npos)
            {
                return Result<ManifestEntry>::Err(
                    ManifestError(file, "binary must be a plain file name (no path characters, D64)"));
            }
            entry.Binary = stem;
        }
        else
        {
            entry.Binary = entry.Subdirectory; // 缺省 = 子目录名（D54）
        }
    }
    if (const auto enabled = root.find("enabledByDefault"); enabled != root.end())
    {
        if (!enabled->is_boolean())
        {
            return Result<ManifestEntry>::Err(ManifestError(file, "enabledByDefault must be a bool"));
        }
        entry.EnabledByDefault = enabled->get<bool>();
    }

    if (const auto array = root.find("requires"); array != root.end())
    {
        auto parsed = ParseDeps(*array, "requires", file);
        if (!parsed.IsOk())
        {
            return Result<ManifestEntry>::Err(parsed.GetError());
        }
        entry.Requires = std::move(parsed.Value());
    }
    if (const auto array = root.find("optionalRequires"); array != root.end())
    {
        auto parsed = ParseDeps(*array, "optionalRequires", file);
        if (!parsed.IsOk())
        {
            return Result<ManifestEntry>::Err(parsed.GetError());
        }
        entry.OptionalRequires = std::move(parsed.Value());
    }
    if (const auto array = root.find("provides"); array != root.end())
    {
        auto parsed = ParseDeps(*array, "provides", file);
        if (!parsed.IsOk())
        {
            return Result<ManifestEntry>::Err(parsed.GetError());
        }
        entry.Provides = std::move(parsed.Value());
    }
    if (const auto array = root.find("config"); array != root.end())
    {
        auto parsed = ParseConfig(*array, file);
        if (!parsed.IsOk())
        {
            return Result<ManifestEntry>::Err(parsed.GetError());
        }
        entry.Config = std::move(parsed.Value());
    }
    if (const auto array = root.find("processStates"); array != root.end())
    {
        auto parsed = ParseProcessStates(*array, file);
        if (!parsed.IsOk())
        {
            return Result<ManifestEntry>::Err(parsed.GetError());
        }
        entry.ProcessStates = std::move(parsed.Value());
    }

    return Result<ManifestEntry>::Ok(std::move(entry));
}

Result<ManifestEntry> ParsePluginFields(const json& root, const std::filesystem::path& file,
                                        std::string_view subdirectoryName, ManifestFieldMode mode)
{
    return ParsePluginFields(root, std::string_view(file.string()), subdirectoryName, mode);
}

json SerializePluginFields(const ManifestEntry& entry, ManifestFieldMode mode)
{
    // 键写一律 emplace 进新建 object()，与 FieldToJson 处同机制。
    json root = json::object();
    if (mode == ManifestFieldMode::kSinglePlugin)
    {
        root.emplace("schemaVersion", 1); // 格式常量，恒 1（D122）：它的语义是「这份清单的格式版本」，由写者决定
    }
    root.emplace("id", entry.Id);
    root.emplace("displayName", entry.DisplayName);
    if (!entry.Version.empty())
    {
        root.emplace("version", entry.Version);
    }
    if (mode == ManifestFieldMode::kSinglePlugin)
    {
        root.emplace("binary", entry.Binary); // 观察事实（D122）：缺失会让加载期按子目录名猜，故总是显式写
    }
    if (!entry.EnabledByDefault)
    {
        root.emplace("enabledByDefault", false); // 只在 false 时写：缺键 ≡ true，输出因此稳定可重复跑
    }
    if (!entry.Requires.empty())
    {
        root.emplace("requires", DepsToJson(entry.Requires));
    }
    if (!entry.OptionalRequires.empty())
    {
        root.emplace("optionalRequires", DepsToJson(entry.OptionalRequires));
    }
    if (!entry.Provides.empty())
    {
        root.emplace("provides", DepsToJson(entry.Provides));
    }
    if (!entry.Config.empty())
    {
        json fields = json::array();
        for (const ManifestConfigField& field : entry.Config)
        {
            fields.push_back(FieldToJson(field));
        }
        root.emplace("config", std::move(fields));
    }
    if (!entry.ProcessStates.empty())
    {
        root.emplace("processStates", entry.ProcessStates);
    }

    return root;
}

Result<void> ValidatePluginValues(const ManifestEntry& entry, const std::filesystem::path& file)
{
    for (const ManifestConfigField& field : entry.Config)
    {
        const bool numeric = IsNumericKind(field.Kind);
        if (field.Kind == ValueKind::kNone || (field.Kind != ValueKind::kEnum && !field.Choices.empty()) ||
            (!numeric && (field.HasMin || field.HasMax)))
        {
            return Result<void>::Err(ManifestError(file, "config \"" + field.Key + "\": invalid value shape"));
        }
        const auto validBits = [&field](std::uint64_t bits)
        {
            switch (field.Kind)
            {
            case ValueKind::kBool:
                return bits <= 1;
            case ValueKind::kInt32:
                return bits <= std::numeric_limits<std::uint32_t>::max();
            case ValueKind::kFloat:
                return bits <= std::numeric_limits<std::uint32_t>::max() &&
                       std::isfinite(std::bit_cast<float>(static_cast<std::uint32_t>(bits)));
            case ValueKind::kDouble:
                return std::isfinite(std::bit_cast<double>(bits));
            case ValueKind::kInt64:
            case ValueKind::kString:
            case ValueKind::kEnum:
                return true;
            case ValueKind::kNone:
                return false;
            }
            return false;
        };
        if (!validBits(field.DefaultBits) || (field.HasMin && !validBits(field.MinBits)) ||
            (field.HasMax && !validBits(field.MaxBits)))
        {
            return Result<void>::Err(ManifestError(file, "config \"" + field.Key + "\": invalid value storage"));
        }
    }
    return Result<void>::Ok();
}

Result<void> WriteJsonAtomically(const std::filesystem::path& file, const json& root)
{
    // replace 模式避免无异常环境下非法 UTF-8 使 dump 终止；回读等值闸拒绝任何替换或非有限数。
    const std::string text = root.dump(2, ' ', false, json::error_handler_t::replace);
    const json checked = json::parse(text, nullptr, false);
    if (checked.is_discarded() || checked != root)
    {
        return Result<void>::Err(ManifestError(file, "values cannot be represented losslessly as JSON"));
    }
    // 原子写（D134）：先落临时文件、再 rename 覆盖。两侧 rename 都是覆盖语义。
    const std::filesystem::path temp = std::filesystem::path(file.string() + ".tmp");
    {
        std::ofstream out(temp, std::ios::binary);
        if (!out)
        {
            return Result<void>::Err(ManifestError(temp, "cannot open for writing"));
        }
        out << text << '\n';
        out.flush(); // 失败检查前置到 flush：小清单时真正的 fd 写发生在析构 flush，不先 flush 则下面的检查失明
        out.close();
        if (!out)
        {
            std::error_code ignored;
            std::filesystem::remove(temp, ignored); // flush 失败与写失败同路：清临时文件，绝不 rename 半份
            return Result<void>::Err(ManifestError(temp, "write failed"));
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp, file, ec);
    if (ec)
    {
        std::error_code ignored;
        std::filesystem::remove(temp, ignored); // 尽力清理，不留半个临时文件
        return Result<void>::Err(ManifestError(file, "rename failed: " + ec.message()));
    }
    return Result<void>::Ok();
}

} // namespace vase::catalog_detail
