#include "Vase/Host/ConfigBlob.h"

#include "Vase/Config/ConfigInfo.h"
#include "Vase/Config/FieldInfo.h"
#include "Vase/Config/Value.h"
#include "Vase/Detail/Fail.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace vase
{

namespace
{

// variant → 借用视图。get_if 一族：std::get 抛 bad_variant_access，关异常后没东西接得住（规矩 4）。
Value ToView(const ConfigBlob::Storage& stored)
{
    if (const auto* text = std::get_if<std::string>(&stored); text != nullptr)
    {
        return Value::From<const char*>(text->c_str());
    }
    if (const auto* flag = std::get_if<bool>(&stored); flag != nullptr)
    {
        return Value::From<bool>(*flag);
    }
    if (const auto* small = std::get_if<std::int32_t>(&stored); small != nullptr)
    {
        return Value::From<std::int32_t>(*small);
    }
    if (const auto* wide = std::get_if<std::int64_t>(&stored); wide != nullptr)
    {
        return Value::From<std::int64_t>(*wide);
    }
    if (const auto* single = std::get_if<float>(&stored); single != nullptr)
    {
        return Value::From<float>(*single);
    }
    if (const auto* doublePtr = std::get_if<double>(&stored); doublePtr != nullptr)
    {
        return Value::From<double>(*doublePtr);
    }
    detail::ProgrammerError("ConfigBlob::ToView: unreachable variant alternative");
}

ConfigBlob::Entry* FindMutable(std::vector<ConfigBlob::Entry>& fields, std::string_view key)
{
    for (ConfigBlob::Entry& entry : fields)
    {
        if (entry.Key == key)
        {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace

void ConfigBlob::Set(std::string_view key, const Value& view)
{
    if (view.Kind == ValueKind::kNone)
    {
        detail::ProgrammerError("ConfigBlob::Set with kNone value");
    }
    if (view.Kind == ValueKind::kString && view.GetAs<const char*>() == nullptr)
    {
        detail::ProgrammerError("ConfigBlob::Set with null kString view"); // std::string(nullptr) 是 UB，没东西接得住
    }
    // 先折算进本地 Storage 再动 Fields：kString 借用的深拷入必须先于 push_back 的潜在重分配，
    // 否则 Set(newkey, *Find(oldkey)) 这种自借用会在重分配挪走短串后读已失效指针。
    Storage converted;
    switch (view.Kind)
    {
    case ValueKind::kBool:
        converted = view.GetAs<bool>();
        break;
    case ValueKind::kInt32:
        converted = view.GetAs<std::int32_t>();
        break;
    case ValueKind::kInt64:
        converted = view.GetAs<std::int64_t>();
        break;
    case ValueKind::kFloat:
        converted = view.GetAs<float>();
        break;
    case ValueKind::kDouble:
        converted = view.GetAs<double>();
        break;
    case ValueKind::kString:
        converted = std::string(view.GetAs<const char*>()); // 深拷入：此后拥有存储就是唯一所有者
        break;
    default:
        detail::ProgrammerError("ConfigBlob::Set unknown kind");
    }
    Entry* existing = FindMutable(Fields, key);
    if (existing == nullptr)
    {
        Fields.push_back(Entry{.Key = std::string(key), .Stored = {}});
        existing = &Fields.back();
    }
    existing->Stored = std::move(converted);
}

std::optional<Value> ConfigBlob::Find(std::string_view key) const
{
    for (const Entry& entry : Fields)
    {
        if (entry.Key == key)
        {
            return ToView(entry.Stored);
        }
    }
    return std::nullopt;
}

void ConfigBlob::MergeShallow(const ConfigBlob& over)
{
    for (const Entry& entry : over.Fields)
    {
        Set(entry.Key, ToView(entry.Stored));
    }
}

ConfigBlob ConfigBlob::FromDefaults(const ConfigInfo& info)
{
    ConfigBlob blob;
    for (std::uint32_t index = 0; index < info.Count; ++index)
    {
        const FieldInfo& field = *std::next(info.Fields, static_cast<std::ptrdiff_t>(index));
        blob.Set(field.Name, field.Default);
    }
    return blob;
}

} // namespace vase
