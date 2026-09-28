#pragma once

// enum label → value 的唯一转换位（D80）：Solve 与 T9 的 BuildExpectation 都调这里，仓库内不另起查表。
// 线性扫（choices 十量级，表序=清单文件序）。反向的 value → label 由 M5 的 `scan` 投影
// （enum 的 default 在清单侧存 label，D80）重新引入。

#include "Vase/Catalog/ManifestView.h"
#include "Vase/Config/FieldInfo.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string_view>
#include <vector>

namespace vase::catalog_detail
{

inline std::optional<std::int32_t> LabelToValue(std::string_view label, const std::vector<ManifestChoice>& choices)
{
    for (const ManifestChoice& choice : choices)
    {
        if (choice.Label == label)
        {
            return choice.Value;
        }
    }
    return std::nullopt;
}

inline std::optional<std::string_view> ValueToLabel(std::int32_t value, const ChoiceInfo* items, std::uint32_t count)
{
    for (std::uint32_t index = 0; index < count; ++index)
    {
        if (std::next(items, static_cast<std::ptrdiff_t>(index))->Value == value)
        {
            return std::next(items, static_cast<std::ptrdiff_t>(index))->Label;
        }
    }
    return std::nullopt;
}

} // namespace vase::catalog_detail
