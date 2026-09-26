// Farewell.h —— 与 Greeter.h 同约定的提供方接口（§3.1）：标识住头，跨 ABI 各方同指一名。
#pragma once

#include <cstdint>
#include <string_view>

namespace samples
{

class IFarewell
{
public:
    static constexpr std::string_view kName = "Vase.Farewell";
    static constexpr std::uint32_t kVersion = 1;

    // 多态接口五特殊成员全处置（与 IGreeter 同因：接口有身份，不拷贝不移动）。
    IFarewell() = default;
    IFarewell(const IFarewell&) = delete;
    IFarewell& operator=(const IFarewell&) = delete;
    IFarewell(IFarewell&&) = delete;
    IFarewell& operator=(IFarewell&&) = delete;
    virtual ~IFarewell() = default;

    [[nodiscard]] virtual std::string_view Farewell() const = 0;
};

} // namespace samples
