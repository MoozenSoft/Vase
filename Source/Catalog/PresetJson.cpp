#include "Vase/Catalog/Preset.h"

#include "Vase/Config/Value.h"
#include "Vase/Detail/Result.h"

// nlohmann::json 别名声明在 json_fwd.hpp（json.hpp 只带 forward 声明）——两条都得在。
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <ios>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace vase
{

namespace
{

Error PresetError(const std::filesystem::path& file, std::string_view detail)
{
    return Error("preset file \"" + file.string() + "\": " + std::string(detail));
}

// unknown 键闸（D83），与 ManifestJson 同族形；首个 offender 键经出参带回，供调用方点名进诊断。
bool OnlyKeys(const nlohmann::json& object, const std::initializer_list<std::string_view>& allowed,
              std::string& offender)
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

} // namespace

Result<Preset> LoadPreset(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    if (!in)
    {
        return Result<Preset>::Err(PresetError(file, "cannot open file"));
    }
    const std::string text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    nlohmann::json root = nlohmann::json::parse(text, nullptr, false); // D55：非抛形
    if (root.is_discarded())
    {
        return Result<Preset>::Err(PresetError(file, "malformed JSON"));
    }
    if (!root.is_object())
    {
        return Result<Preset>::Err(PresetError(file, "top level must be a JSON object"));
    }
    if (std::string unknownField; !OnlyKeys(root, {"schemaVersion", "displayName", "overrides"}, unknownField)) // D83
    {
        return Result<Preset>::Err(PresetError(file, "unknown top-level field \"" + unknownField + "\" (D83)"));
    }

    const auto schemaVersion = root.find("schemaVersion");
    if (schemaVersion == root.end() || !schemaVersion->is_number_integer() || schemaVersion->get<std::int64_t>() != 1)
    {
        return Result<Preset>::Err(PresetError(file, "schemaVersion required, integer major 1 (D59)"));
    }
    if (const auto name = root.find("displayName"); name != root.end() && !name->is_string())
    {
        return Result<Preset>::Err(PresetError(file, "displayName must be a string"));
    }
    const auto overrides = root.find("overrides");
    if (overrides == root.end() || !overrides->is_object())
    {
        return Result<Preset>::Err(PresetError(file, "field \"overrides\" must be an object"));
    }

    Preset preset;
    preset.Version = 1;
    if (const auto name = root.find("displayName"); name != root.end())
    {
        preset.Name = name->get_ref<const std::string&>();
    }
    for (auto it = overrides->begin(); it != overrides->end(); ++it)
    {
        const nlohmann::json& entry = it.value();
        if (!entry.is_object())
        {
            return Result<Preset>::Err(PresetError(file, "override for \"" + it.key() + "\" must be an object"));
        }
        PluginOverride item;
        item.Id = it.key();
        for (auto field = entry.begin(); field != entry.end(); ++field)
        {
            if (field.key() == "enabled")
            {
                if (!field.value().is_boolean())
                {
                    return Result<Preset>::Err(
                        PresetError(file, "override for \"" + item.Id + "\": enabled must be a bool"));
                }
                item.Enabled = field.value().get<bool>();
                continue;
            }
            if (field.key() != "config")
            {
                return Result<Preset>::Err(
                    PresetError(file, "override for \"" + item.Id + "\": unknown override field"));
            }
            const nlohmann::json& config = field.value();
            if (!config.is_object())
            {
                return Result<Preset>::Err(
                    PresetError(file, "override for \"" + item.Id + "\": config must be an object"));
            }
            for (auto kv = config.begin(); kv != config.end(); ++kv)
            {
                const nlohmann::json& raw = kv.value();
                if (raw.is_boolean())
                {
                    item.Config.Set(kv.key(), Value::From(raw.get<bool>()));
                }
                else if (raw.is_number_integer())
                {
                    // D59 原样形：整数一律 int64；越 int64 的 unsigned 没有合法窄化，直接拒
                    if (raw.is_number_unsigned() &&
                        raw.get<std::uint64_t>() > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
                    {
                        return Result<Preset>::Err(PresetError(file, "override \"" + item.Id + "\" key \"" + kv.key() +
                                                                         "\": integer too large"));
                    }
                    item.Config.Set(kv.key(), Value::From(raw.get<std::int64_t>()));
                }
                else if (raw.is_number_float())
                {
                    item.Config.Set(kv.key(), Value::From(raw.get<double>()));
                }
                else if (raw.is_string())
                {
                    item.Config.Set(kv.key(), Value::From(raw.get_ref<const std::string&>().c_str())); // Set 深拷入
                }
                else
                {
                    // 嵌套对象/数组与 null：§4.2 禁嵌套在结构段的兑现。
                    return Result<Preset>::Err(PresetError(file, "override \"" + item.Id + "\" key \"" + kv.key() +
                                                                     "\": config values must be scalars"));
                }
            }
        }
        preset.Items.push_back(std::move(item));
    }
    return Result<Preset>::Ok(std::move(preset));
}

} // namespace vase
