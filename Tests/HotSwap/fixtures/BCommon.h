#pragma once

// T12 热插拔主循环的一份测试材料，HotSwap 的 fixture 与测试 TU 共用：本文件声明测试所需的
// 全部接口与事件（**不逐项列举**——它还在长，列举出去必腐）。
// 解析靠 kName 字符串跨模块匹配——C++ 类型同名不是判据，标识一致才是（§6.1）。

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

// rung3 的漂移维（M4/D106）：与 ICounter **同名、主版本 2**。存在的理由：`Context::Provide<T>()`
// 的注册键来自接口常量（T::kVersion）而非描述符，所以「声明提供 v2」的 fixture 必须真有
// 一个 v2 接口可注册，否则描述符就是在撒谎。
class ICounterV2
{
public:
    static constexpr std::string_view kName = "Vase.Test.Counter";
    static constexpr std::uint32_t kVersion = 2;

    ICounterV2() = default;
    ICounterV2(const ICounterV2&) = delete;
    ICounterV2& operator=(const ICounterV2&) = delete;
    ICounterV2(ICounterV2&&) = delete;
    ICounterV2& operator=(ICounterV2&&) = delete;
    virtual ~ICounterV2() = default;

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

// M3/D91 的观测量：进程级状态是**跨 Pod 存活**的静态计数，`Loads()` 报它。
// 证人形 = 「Eject 后再 Adopt，读数回到 1」——没归零的话它会继续累加到 2。
class IStateProbe
{
public:
    static constexpr std::string_view kName = "Vase.Test.StateProbe";
    static constexpr std::uint32_t kVersion = 1;

    IStateProbe() = default;
    IStateProbe(const IStateProbe&) = delete;
    IStateProbe& operator=(const IStateProbe&) = delete;
    IStateProbe(IStateProbe&&) = delete;
    IStateProbe& operator=(IStateProbe&&) = delete;
    virtual ~IStateProbe() = default;

    [[nodiscard]] virtual int Loads() const = 0;
};

// 第二只邻居的服务标识（M3/D90）：与 IHeart **必须不同名**——一个 Pod 里两个同键提供方
// 会被 ServiceRegistry 当场终止（「一服务一实现」）。心跳的驱动事件仍是 TickEvent（订阅无唯一性）。
class IPulse
{
public:
    static constexpr std::string_view kName = "Vase.Test.Pulse";
    static constexpr std::uint32_t kVersion = 1;

    IPulse() = default;
    IPulse(const IPulse&) = delete;
    IPulse& operator=(const IPulse&) = delete;
    IPulse(IPulse&&) = delete;
    IPulse& operator=(IPulse&&) = delete;
    virtual ~IPulse() = default;

    [[nodiscard]] virtual int Pulses() const = 0;
};

} // namespace samples_fixture
