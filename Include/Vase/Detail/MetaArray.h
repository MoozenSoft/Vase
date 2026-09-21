#pragma once

// 固定容量内联数组：描述符的 Requires/Provides 用它。不用 std::span——
// 没有 initializer_list 构造函数（spec 3.3(2)：文档写法直接编不过）；
// 也不押注 initializer_list **成员**的存储（探针未复现悬垂，正确性依赖实现细节）。
// 自持存储、constexpr 可构造、POD 可平凡拷贝——跨 Win/Linux 三套 STL 不赌行为。
//
// 存储用 std::array、填充用 std::copy：裸 C 数组与非常量下标的 operator[] 都过不了门禁，
// 而 std::array 在自持存储 / constexpr / 平凡拷贝 / 无堆分配四条上与裸数组等价。

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <initializer_list>
#include <iterator>

namespace vase
{

// 溢出出口：定义成**非 constexpr 的终止函数**，而不是 spec 3.3(2) 原始的「只声明不定义」。
// 两者都能让溢出的 constexpr 求值失败（碰到非 constexpr 调用即不是常量表达式）；差别在
// 非溢出路径——只声明不定义时符号引用照样发射，一切**运行期**构造都链接失败
// （实测 lld-link: undefined symbol，引它的是 `MetaArray<int,4>::MetaArray(initializer_list<int>)`），
// 而 T4 的 VASE_PLUGIN 正让 PluginMeta 全局在插件 DLL 里运行期构造。溢出照旧响：编译期报错、运行期 abort。
[[noreturn]] inline void MetaArrayCapacityExceeded() { std::abort(); }

template <typename T, std::size_t Capacity>
class MetaArray
{
public:
    constexpr MetaArray() = default;

    constexpr MetaArray(std::initializer_list<T> items)
    {
        if (items.size() > Capacity)
        {
            MetaArrayCapacityExceeded();
        }
        Count = items.size();
        std::copy(items.begin(), items.end(), Items.begin());
    }

    [[nodiscard]] constexpr std::size_t Size() const { return Count; }
    [[nodiscard]] constexpr bool Empty() const { return Count == 0; }
    [[nodiscard]] constexpr const T* Begin() const { return Items.data(); }
    // 用 std::next 而非 Items.data() + Count：指针算式过不了 cppcoreguidelines-pro-bounds-*。
    // 注意 std::array::begin() 在 MSVC STL 返回迭代器类而非指针，故这里必须从 Begin() 起算。
    [[nodiscard]] constexpr const T* End() const { return std::next(Begin(), static_cast<std::ptrdiff_t>(Count)); }
    [[nodiscard]] constexpr const T& operator[](std::size_t index) const
    {
        return *std::next(Items.begin(), static_cast<std::ptrdiff_t>(index));
    }

private:
    std::array<T, Capacity> Items{};
    std::size_t Count = 0;
};

} // namespace vase
