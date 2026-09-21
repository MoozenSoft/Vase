#pragma once

// 编程错误的一死到底出口（§6.2）：这不是可恢复失败——可恢复失败一律走 Result。
// Debug 下 assert 把线程钉在断言点（方便挂调试器），两个构建都 abort。
// 无异常编译（§0.3-7），没有「可接住的东西」，这是唯一正确形态。
//
// 形态：具名函数 + source_location 默认参数，而**不是**宏——C++20 的 source_location
// 补上了 __FILE__ / __LINE__ 这点宏唯一不可替代的能力。

#include <array>
#include <cassert>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <source_location>
#include <string_view>

namespace vase::detail
{

inline void WriteStderr(std::string_view text) { static_cast<void>(std::fwrite(text.data(), 1, text.size(), stderr)); }

// 把行号就地写成十进制实测长度（不分配、不抛，§9.x 清单）。
inline std::size_t FormatLineNumber(char* buffer, std::size_t capacity, int line)
{
    const std::to_chars_result result =
        std::to_chars(buffer, std::next(buffer, static_cast<std::ptrdiff_t>(capacity)), line);
    if (result.ec != std::errc{})
    {
        return 0;
    }
    return static_cast<std::size_t>(std::distance(buffer, result.ptr));
}

[[noreturn]] inline void ProgrammerError(std::string_view message,
                                         const std::source_location& location = std::source_location::current())
{
    std::array<char, 16> lineBuffer{};
    const std::size_t lineLength =
        FormatLineNumber(lineBuffer.data(), lineBuffer.size(), static_cast<int>(location.line()));

    WriteStderr("Vase programmer error: ");
    WriteStderr(message);
    WriteStderr(" (");
    WriteStderr(location.file_name());
    WriteStderr(":");
    WriteStderr(std::string_view{lineBuffer.data(), lineLength});
    WriteStderr(")\n");
    static_cast<void>(std::fflush(stderr));
    assert(false && "Vase programmer error");
    std::abort();
}

} // namespace vase::detail
