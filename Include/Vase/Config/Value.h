#pragma once

// 配置值的 POD 面（spec 3.2 / D24）：Kind 标签 + 64 位存储。标量走 uint64 位形 + bit_cast；
// kString 走同一 8 字节的 const char* 覆盖成员：字面量或宿主 blob 存储的**借用**（窗口见 ConfigBlob.h）。
// 为什么 string 不存位形：constexpr 里指针↔整数没有任何合法写法（P2736 属 C++26，C++20 的
// 常量评测器连 struct 包裹与 union 闲置读一并拒绝）——而描述符表是 static constexpr 的（D24）。
// enum 不做（D22）：它随清单 schema 在 M2b 定案，届时布局要变就再 bump。

#include <bit>
#include <cstdint>
#include <type_traits>

namespace vase
{

enum class ValueKind : std::uint8_t
{
    kNone,
    kBool,
    kInt32,
    kInt64,
    kFloat,
    kDouble,
    kString,
};

template <typename T>
struct AlwaysInvalidConfigType : std::false_type
{
};

template <typename T>
consteval ValueKind KindOf()
{
    if constexpr (std::is_same_v<T, bool>)
    {
        return ValueKind::kBool;
    }
    else if constexpr (std::is_same_v<T, std::int32_t>)
    {
        return ValueKind::kInt32;
    }
    else if constexpr (std::is_same_v<T, std::int64_t>)
    {
        return ValueKind::kInt64;
    }
    else if constexpr (std::is_same_v<T, float>)
    {
        return ValueKind::kFloat;
    }
    else if constexpr (std::is_same_v<T, double>)
    {
        return ValueKind::kDouble;
    }
    else if constexpr (std::is_same_v<T, const char*>)
    {
        return ValueKind::kString;
    }
    else
    {
        static_assert(AlwaysInvalidConfigType<T>::value,
                      "config type must be bool/int32_t/int64_t/float/double/const char* (enum deferred to M2b, D22)");
        return ValueKind::kNone;
    }
}

struct Value
{
    ValueKind Kind = ValueKind::kNone;
    union
    {
        std::uint64_t Bits; // 标量位形；4 字节类型住在低位
        const char* Str;    // kString 借用；与 Bits 同 8 字节，两侧互斥地按 Kind 取用
    };

    template <typename T>
    [[nodiscard]] static constexpr Value From(T value)
    {
        Value out{};
        out.Kind = KindOf<T>();
        // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access) D24 位形存储按 Kind 互斥读写，无代码级出路。
        if constexpr (std::is_same_v<T, bool>)
        {
            out.Bits = value ? 1U : 0U;
        }
        else if constexpr (std::is_same_v<T, std::int32_t>)
        {
            out.Bits = static_cast<std::uint64_t>(static_cast<std::uint32_t>(value));
        }
        else if constexpr (std::is_same_v<T, std::int64_t>)
        {
            out.Bits = static_cast<std::uint64_t>(value);
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            out.Bits = std::bit_cast<std::uint32_t>(value);
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            out.Bits = std::bit_cast<std::uint64_t>(value);
        }
        else
        {
            out.Str = value; // const char*：借用直接进覆盖成员（位形进不了 constexpr，见头注）
        }
        // NOLINTEND(cppcoreguidelines-pro-type-union-access)
        return out;
    }

    // 无 Kind 检查的解码：调用方持有类型（宏生成的 Apply / blob 装换都按 Kind 分派过），
    // 读错 Kind 的防线在 ApplyToImpl 那唯一一处运行时检查里。
    template <typename T>
    [[nodiscard]] constexpr T GetAs() const
    {
        // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access) 与 From 处同机制。
        if constexpr (std::is_same_v<T, bool>)
        {
            return Bits != 0U;
        }
        else if constexpr (std::is_same_v<T, std::int32_t>)
        {
            return static_cast<std::int32_t>(static_cast<std::uint32_t>(Bits));
        }
        else if constexpr (std::is_same_v<T, std::int64_t>)
        {
            return static_cast<std::int64_t>(Bits);
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            return std::bit_cast<float>(static_cast<std::uint32_t>(Bits));
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            return std::bit_cast<double>(Bits);
        }
        else
        {
            // 白名单收口：上面五个分支精确匹配其余五类，走到这里 T 必须是 const char*——
            // 不再静默返回 Str（KindOf 处有同款 static_assert）。
            static_assert(std::is_same_v<T, const char*>,
                          "GetAs<T>: readable types are bool/int32_t/int64_t/float/double/const char* (six ValueKind)");
            return Str;
        }
        // NOLINTEND(cppcoreguidelines-pro-type-union-access)
    }

    // C++20 下含匿名 union 的类 defaulted == 隐式删除（object-representation 比较属
    // C++23 P2195），手写 Kind 分派：string 比指针借用，其余比位形——与 D24 的
    // 「Kind 参与相等」语义逐点一致。
    [[nodiscard]] constexpr bool operator==(const Value& other) const
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) 与 From 处同机制。
        return Kind == other.Kind && (Kind == ValueKind::kString ? Str == other.Str : Bits == other.Bits);
    }
};

static_assert(std::is_trivially_copyable_v<Value>);
static_assert(std::is_trivially_destructible_v<Value>);

} // namespace vase
