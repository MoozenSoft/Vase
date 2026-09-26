#pragma once

// M2a 装配语义 fixture 群的测试服务标识（T5–T9 共用；与 SharedCommon.h 同款定位：测试材料）。
// 解析靠 kName 字符串跨模块匹配（§6.1），与类型是否同名无关。

#include <cstdint>
#include <string_view>

namespace m2_fixture
{

// 声明了 Provides 但 OnLoad 返回 Err 的提供者：级联归因（D41「提供者已死」支）的主角。
class IDeadService
{
public:
    static constexpr std::string_view kName = "Vase.Test.Dead";
    static constexpr std::uint32_t kVersion = 1;
    IDeadService() = default;
    IDeadService(const IDeadService&) = delete;
    IDeadService& operator=(const IDeadService&) = delete;
    IDeadService(IDeadService&&) = delete;
    IDeadService& operator=(IDeadService&&) = delete;
    virtual ~IDeadService() = default;
    [[nodiscard]] virtual int Value() const = 0;
};

// 配置应用判据：把 ctx.Config<T>().Echo 原样吐回来（值 = 供给 blob 的，或被默认）；
// MoodLabel 是 enum 应用现场读回（D79 正反两半：域内值落进成员、读得到 label）。
class IConfigEcho
{
public:
    static constexpr std::string_view kName = "Vase.Test.ConfigEcho";
    static constexpr std::uint32_t kVersion = 1;
    IConfigEcho() = default;
    IConfigEcho(const IConfigEcho&) = delete;
    IConfigEcho& operator=(const IConfigEcho&) = delete;
    IConfigEcho(IConfigEcho&&) = delete;
    IConfigEcho& operator=(IConfigEcho&&) = delete;
    virtual ~IConfigEcho() = default;
    [[nodiscard]] virtual int Value() const = 0;
    // R-F1 证人：调用时才从类型化配置结构体读 kString 借用指针（非 OnLoad 期的深拷）。
    [[nodiscard]] virtual const char* Banner() const = 0;
    // enum 读回证人：调用时按**已应用的** Mood 成员现选 label（域内值 → 表里文本）。
    [[nodiscard]] virtual const char* MoodLabel() const = 0;
};

// 环退化互缺（D40）：A strict 需 B、B strict 需 A。
class ICycleA
{
public:
    static constexpr std::string_view kName = "Vase.Test.CycleA";
    static constexpr std::uint32_t kVersion = 1;
    ICycleA() = default;
    ICycleA(const ICycleA&) = delete;
    ICycleA& operator=(const ICycleA&) = delete;
    ICycleA(ICycleA&&) = delete;
    ICycleA& operator=(ICycleA&&) = delete;
    virtual ~ICycleA() = default;
};

class ICycleB
{
public:
    static constexpr std::string_view kName = "Vase.Test.CycleB";
    static constexpr std::uint32_t kVersion = 1;
    ICycleB() = default;
    ICycleB(const ICycleB&) = delete;
    ICycleB& operator=(const ICycleB&) = delete;
    ICycleB(ICycleB&&) = delete;
    ICycleB& operator=(ICycleB&&) = delete;
    virtual ~ICycleB() = default;
};

// §5.2 判据 5（T7）：OnStart 必败的提供者，拆它的下游。
class IBehind
{
public:
    static constexpr std::string_view kName = "Vase.Test.Behind";
    static constexpr std::uint32_t kVersion = 1;
    IBehind() = default;
    IBehind(const IBehind&) = delete;
    IBehind& operator=(const IBehind&) = delete;
    IBehind(IBehind&&) = delete;
    IBehind& operator=(IBehind&&) = delete;
    virtual ~IBehind() = default;
};

// 传递链第二跳：由 BehindStrictUsed 提供。
class IBehind2
{
public:
    static constexpr std::string_view kName = "Vase.Test.Behind2";
    static constexpr std::uint32_t kVersion = 1;
    IBehind2() = default;
    IBehind2(const IBehind2&) = delete;
    IBehind2& operator=(const IBehind2&) = delete;
    IBehind2(IBehind2&&) = delete;
    IBehind2& operator=(IBehind2&&) = delete;
    virtual ~IBehind2() = default;
};

} // namespace m2_fixture
