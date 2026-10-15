#pragma once

#include <cstdint>
#include <string_view>

namespace package_fixture
{

class ICounter
{
public:
    static constexpr std::string_view kName = "Vase.PackageProvider.Counter";
    static constexpr std::uint32_t kVersion = 1;

    ICounter() = default;
    ICounter(const ICounter&) = delete;
    ICounter& operator=(const ICounter&) = delete;
    ICounter(ICounter&&) = delete;
    ICounter& operator=(ICounter&&) = delete;
    virtual ~ICounter() = default;

    [[nodiscard]] virtual std::int32_t Value() const = 0;
};

class IResult
{
public:
    static constexpr std::string_view kName = "Vase.PackageConsumer.Result";
    static constexpr std::uint32_t kVersion = 1;

    IResult() = default;
    IResult(const IResult&) = delete;
    IResult& operator=(const IResult&) = delete;
    IResult(IResult&&) = delete;
    IResult& operator=(IResult&&) = delete;
    virtual ~IResult() = default;

    [[nodiscard]] virtual std::int32_t Observed() const = 0;
};

} // namespace package_fixture
