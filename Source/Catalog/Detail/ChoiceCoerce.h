#pragma once

// enum label → value 的唯一转换位（D80）：Solve 与 T9 的 BuildExpectation 都调这里，仓库内不另起查表。
// 线性扫（choices 十量级，表序=清单文件序）。反向的 value → label 曾在此预留，终态无消费者已删（YAGNI）。

#include "Vase/Catalog/ManifestView.h"

#include <cstdint>
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

} // namespace vase::catalog_detail
