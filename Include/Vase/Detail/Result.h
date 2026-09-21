#pragma once

// Result<T> / Error——全项目的错误通道（§0.3-7）。C++20 没有 std::expected，
// 手写最小形（spec 第 7 节：不引第三方，以免把 ABI 面交给别人）。
//
// Error 全拥有（std::string）：它是**返回值**，必须活得比调用久、且可能比插件
// 镜像活得久（spec 3.3(3)——DestroyPod 后宿主才读报告的路径）。
//
// 成员与访问器撞名时成员让位（Message() 与成员撞名 → 成员叫 Text）。

#include "Vase/Detail/Fail.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace vase
{

// 失败发生的阶段。kAdopt / kEject 为 v3 新增（§5.6 进出判定流）。
enum class Phase : std::uint8_t
{
    kLoad,
    kStart,
    kAdopt,
    kEject,
    kUnload,
};

struct ErrorContext
{
    std::string PluginId;
    std::string ServiceName;
    std::uint32_t ServiceVersion = 0;
    Phase Stage = Phase::kLoad;
};

class Error
{
public:
    Error() = default;
    explicit Error(std::string message, ErrorContext context = {})
        : Text(std::move(message))
        , Info(std::move(context))
    {
    }

    [[nodiscard]] bool IsSet() const { return !Text.empty(); }
    [[nodiscard]] const std::string& Message() const { return Text; }
    [[nodiscard]] const ErrorContext& Context() const { return Info; }

private:
    std::string Text;
    ErrorContext Info;
};

template <typename T>
class [[nodiscard]] Result
{
public:
    static Result Ok(T value)
    {
        Result r;
        r.Stored = std::move(value);
        return r;
    }

    static Result Err(Error error)
    {
        Result r;
        r.Failure = std::move(error);
        return r;
    }

    [[nodiscard]] bool IsOk() const { return Stored.has_value(); }

    [[nodiscard]] T& Value()
    {
        if (!Stored.has_value())
        {
            detail::ProgrammerError("Result::Value() called on error result");
        }
        return *Stored;
    }

    [[nodiscard]] const T& Value() const
    {
        if (!Stored.has_value())
        {
            detail::ProgrammerError("Result::Value() called on error result");
        }
        return *Stored;
    }

    [[nodiscard]] const Error& GetError() const
    {
        if (!Failure.has_value())
        {
            detail::ProgrammerError("Result::GetError() called on ok result");
        }
        return *Failure;
    }

private:
    Result() = default;

    std::optional<T> Stored;
    std::optional<Error> Failure;
};

template <>
class [[nodiscard]] Result<void>
{
public:
    static Result Ok()
    {
        Result r;
        r.Succeeded = true;
        return r;
    }

    static Result Err(Error error)
    {
        Result r;
        r.Failure = std::move(error);
        return r;
    }

    [[nodiscard]] bool IsOk() const { return Succeeded; }

    [[nodiscard]] const Error& GetError() const
    {
        if (!Failure.has_value())
        {
            detail::ProgrammerError("Result::GetError() called on ok result");
        }
        return *Failure;
    }

private:
    Result() = default;

    bool Succeeded = false;
    std::optional<Error> Failure;
};

} // namespace vase
