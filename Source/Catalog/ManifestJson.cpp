#include "Vase/Catalog/PluginCatalog.h"

#include "Detail/ManifestFields.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Config/Value.h"
#include "Vase/Detail/Result.h"

// Result<json> 的 optional 与序列化返回值需要完整类型，include-cleaner 只把别名归到 json_fwd.hpp。
#include <nlohmann/json.hpp> // NOLINT(misc-include-cleaner)
#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <string_view>

namespace vase
{

Value ManifestConfigField::DefaultValue() const
{
    Value out{};
    // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access) D24 位形按 Kind 互斥读写（与 Value::From 同机制）
    if (Kind == ValueKind::kString || Kind == ValueKind::kEnum)
    {
        out.Kind = ValueKind::kString; // enum 中间形：default 即 label 字串，换 value 归 Solve（D80）
        out.Str = DefaultStr.c_str();
    }
    else
    {
        out.Kind = Kind;
        out.Bits = DefaultBits;
    }
    // NOLINTEND(cppcoreguidelines-pro-type-union-access)
    return out;
}

Value ManifestConfigField::MinValue() const
{
    Value out{};
    if (!HasMin)
    {
        return out; // kNone
    }
    out.Kind = Kind;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) 同上。
    out.Bits = MinBits;
    return out;
}

Value ManifestConfigField::MaxValue() const
{
    Value out{};
    if (!HasMax)
    {
        return out;
    }
    out.Kind = Kind;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) 同上。
    out.Bits = MaxBits;
    return out;
}

Result<ManifestEntry> ParseManifestFile(const std::filesystem::path& file, std::string_view subdirectoryName)
{
    Result<nlohmann::json> read = catalog_detail::ReadJsonFile(file);
    if (!read.IsOk())
    {
        return Result<ManifestEntry>::Err(read.GetError());
    }
    return catalog_detail::ParsePluginFields(read.Value(), file, subdirectoryName,
                                             catalog_detail::ManifestFieldMode::kSinglePlugin);
}

Result<void> WriteManifestFile(const std::filesystem::path& file, const ManifestEntry& entry)
{
    auto values = catalog_detail::ValidatePluginValues(entry, file);
    if (!values.IsOk())
    {
        return values;
    }
    const nlohmann::json root =
        catalog_detail::SerializePluginFields(entry, catalog_detail::ManifestFieldMode::kSinglePlugin);
    const auto checked = catalog_detail::ParsePluginFields(root, file, entry.Subdirectory,
                                                           catalog_detail::ManifestFieldMode::kSinglePlugin);
    if (!checked.IsOk())
    {
        return Result<void>::Err(checked.GetError());
    }
    return catalog_detail::WriteJsonAtomically(file, root);
}

} // namespace vase
