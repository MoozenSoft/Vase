#pragma once

// 服务标识约定（§6.1）：编译期常量名字 + 整数主版本。不用 type_index——
// 它跨 DLL 的比较依赖编译器实现细节，而字符串常量是稳定的（附录 B）。
// 事件与服务面对完全相同的跨 DLL 标识问题，所以用同一套解法（§2.4）。

#include <concepts>
#include <cstdint>
#include <string_view>

namespace vase
{

template <typename T>
concept HasServiceIdentity = requires {
    { T::kName } -> std::convertible_to<std::string_view>;
    { T::kVersion } -> std::convertible_to<std::uint32_t>;
};

// 来源是**注册位置**的属性，不是接口的属性（§6.1）：插件子 Context 里 Provide
// 即 kPlugin，Pod 根 Context 里 Provide 即 kHost。接口上不声明它。
enum class ServiceOrigin : std::uint8_t
{
    kPlugin,
    kHost,
};

struct ServiceKey
{
    std::string_view Name;
    std::uint32_t Version = 0; // 与 ServiceRef 同：0 = 未设，避免聚合体默认构造留下未初始化字节

    bool operator==(const ServiceKey&) const = default;
};

} // namespace vase
