#pragma once

// stem → 平台库文件名（D54）：纯字符串拼接。§13.1「平台差异收口在 Loader」管的是
// 加载行为；命名不属之（spec 勘误第 4 条）。iOS 的 .a 形态是 VasePack/M5 的问题。

#include <string>
#include <string_view>

namespace vase::catalog_detail
{

inline std::string LibraryFileName(std::string_view stem)
{
#ifdef _WIN32
    return std::string(stem) + ".dll";
#else
    return "lib" + std::string(stem) + ".so";
#endif
}

} // namespace vase::catalog_detail
