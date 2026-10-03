#pragma once

// PE / ELF / Mach-O 三个镜像解析器共用的读字节助手（macOS 腿提取：第三个解析器进来时
// 把那 ~90 行边界检查收成一处，PE/ELF 一并改用；纯搬移，无语义变化）。
//
// 纪律（三个解析器共同遵守）：每个取字节前先 InBounds，查不过就返回 Err，绝不越界读；
// 整数一律 memcpy 进本地变量——非对齐安全（三种格式的字段本来都不保证对齐）。
// 目标矩阵（x64 / arm64）全是小端，故「按本机序读」就是 LE 读法，不需要逐字节拼。
//
// inline 而非另立 .cpp：每个函数 3–8 行，独立 TU 只会让三个解析器跨 TU 调用、挡住内联。

#include "Vase/Detail/Result.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <utility>

namespace vase::detail
{

[[nodiscard]] inline bool InBounds(std::size_t offset, std::size_t length, std::size_t total)
{
    return offset <= total && length <= total - offset;
}

[[nodiscard]] inline std::uint8_t ReadU8(std::span<const std::uint8_t> bytes, std::size_t at)
{
    std::uint8_t value = 0;
    std::memcpy(&value, bytes.subspan(at, sizeof(value)).data(), sizeof(value));
    return value;
}

[[nodiscard]] inline std::uint16_t ReadU16(std::span<const std::uint8_t> bytes, std::size_t at)
{
    std::uint16_t value = 0;
    std::memcpy(&value, bytes.subspan(at, sizeof(value)).data(), sizeof(value));
    return value;
}

[[nodiscard]] inline std::uint32_t ReadU32(std::span<const std::uint8_t> bytes, std::size_t at)
{
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.subspan(at, sizeof(value)).data(), sizeof(value));
    return value;
}

[[nodiscard]] inline std::uint64_t ReadU64(std::span<const std::uint8_t> bytes, std::size_t at)
{
    std::uint64_t value = 0;
    std::memcpy(&value, bytes.subspan(at, sizeof(value)).data(), sizeof(value));
    return value;
}

// 读一条 NUL 结尾的字符串，扫到 end 仍没有 NUL 就是 Err（不越界、不猜）。
// end 由调用方给：PE/ELF 传镜像末尾，Mach-O 传该 load command 的末尾（名字只许落在条目内）。
[[nodiscard]] inline Result<std::string> ReadNulTerminated(std::span<const std::uint8_t> bytes, std::size_t at,
                                                           std::size_t end)
{
    std::string text;
    for (std::size_t index = at; index < end && index < bytes.size(); ++index)
    {
        const std::uint8_t value = ReadU8(bytes, index);
        if (value == 0U)
        {
            return Result<std::string>::Ok(std::move(text));
        }
        text.push_back(static_cast<char>(value));
    }
    return Result<std::string>::Err(Error{"unterminated string in image"});
}

[[nodiscard]] inline Result<std::string> ReadNulTerminated(std::span<const std::uint8_t> bytes, std::size_t at)
{
    return ReadNulTerminated(bytes, at, bytes.size());
}

} // namespace vase::detail
