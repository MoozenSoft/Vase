#pragma once

// T12 热插拔主循环的一份测试材料，三个 fixture 与 HotSwapLoopTests 共用：
// Counter（A 与 A′ 同名双版本，返回值就是版本身份）、Heart（邻居 B 的心跳）、
// Tick（B 订阅的事件）。解析靠 kName 字符串跨模块匹配（§6.1：不用 type_index）——
// C++ 类型是否同名不是判据，标识一致才是。

#include <cstdint>
#include <string_view>

namespace samples_fixture
{

// A 与 A′ 都提供它：Value() 返回 1 = A，返回 2 = A′。「行为翻面」的判据就落在这一个数上。
class ICounter
{
public:
    static constexpr std::string_view kName = "Vase.Test.Counter";
    static constexpr std::uint32_t kVersion = 1;

    // 接口有身份：五个特殊成员全处置、不拷贝不移动（与 IGreeter / ISharedService 同形）。
    ICounter() = default;
    ICounter(const ICounter&) = delete;
    ICounter& operator=(const ICounter&) = delete;
    ICounter(ICounter&&) = delete;
    ICounter& operator=(ICounter&&) = delete;
    virtual ~ICounter() = default;

    [[nodiscard]] virtual int Value() const = 0;
};

// 邻居 B 提供它：Beats() 的自增全由订阅 TickEvent 驱动——「心跳正常」的可观测形态。
class IHeart
{
public:
    static constexpr std::string_view kName = "Vase.Test.Heart";
    static constexpr std::uint32_t kVersion = 1;

    IHeart() = default;
    IHeart(const IHeart&) = delete;
    IHeart& operator=(const IHeart&) = delete;
    IHeart(IHeart&&) = delete;
    IHeart& operator=(IHeart&&) = delete;
    virtual ~IHeart() = default;

    [[nodiscard]] virtual int Beats() const = 0;
};

struct TickEvent
{
    static constexpr std::string_view kName = "Vase.Test.Tick";
    static constexpr std::uint32_t kVersion = 1;
    int Seq;
};

} // namespace samples_fixture
