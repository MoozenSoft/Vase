#pragma once

// 库文件命名的唯一知识（D54；M5/D129 起为公开面）：正向由清单 binary 拼名、反向由磁盘名恢复 binary，
// 对合法 stem 两函数互为反函数，由 round-trip 用例钉住（LibraryFileNameTests）。header-only 内联、不挂
// 导出宏（纯字符串、无 ABI 面）；iOS 的 .a 分支归 VasePack 那一波。

#include <optional>
#include <string>
#include <string_view>

namespace vase::catalog_detail
{

#ifdef _WIN32
inline constexpr std::string_view kLibraryPrefix;
inline constexpr std::string_view kLibrarySuffix = ".dll";
#elif defined(__APPLE__)
inline constexpr std::string_view kLibraryPrefix = "lib";
inline constexpr std::string_view kLibrarySuffix = ".dylib";
#else
inline constexpr std::string_view kLibraryPrefix = "lib";
inline constexpr std::string_view kLibrarySuffix = ".so";
#endif

inline std::string LibraryFileName(std::string_view stem)
{
    return std::string(kLibraryPrefix) + std::string(stem) + std::string(kLibrarySuffix);
}

// 反函数：本平台的前缀与后缀两侧都要剥；不是本平台的形态 → nullopt（不猜）。
// 剥完为空串也算不合法——一个没有名字的库不是插件。
inline std::optional<std::string> LibraryStem(std::string_view fileName)
{
    if (fileName.size() <= kLibraryPrefix.size() + kLibrarySuffix.size())
    {
        return std::nullopt;
    }
    if (!fileName.starts_with(kLibraryPrefix))
    {
        return std::nullopt;
    }
    if (!fileName.ends_with(kLibrarySuffix))
    {
        return std::nullopt;
    }
    const std::string_view stem =
        fileName.substr(kLibraryPrefix.size(), fileName.size() - kLibraryPrefix.size() - kLibrarySuffix.size());
    if (stem.empty())
    {
        return std::nullopt;
    }
    return std::string(stem);
}

} // namespace vase::catalog_detail
