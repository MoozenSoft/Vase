#pragma once

// 事件标识约定（§2.4）：与 ServiceKey 同构（名字 + 整数主版本）。
// 生命周期契约同文件注释执行：**派发是同步的；handler 不得保存事件对象
// 或其任何视图**（§0.3-6 在本层的实例化）。

#include <concepts>
#include <cstdint>
#include <string_view>

namespace vase
{

template <typename E>
concept HasEventIdentity = requires {
    { E::kName } -> std::convertible_to<std::string_view>;
    { E::kVersion } -> std::convertible_to<std::uint32_t>;
};

struct EventKey
{
    std::string_view Name;
    std::uint32_t Version = 0; // 与 ServiceKey 同：0 = 未设，避免聚合体默认构造留下未初始化字节

    bool operator==(const EventKey&) const = default;
};

} // namespace vase
