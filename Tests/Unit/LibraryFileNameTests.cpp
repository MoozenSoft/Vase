// 库文件命名的正反两函数（M5/D129）：唯一能从磁盘文件名恢复 binary 值的实现。
// 判据是 round-trip——两侧平台各自的合法文件名必须能原样还原。
#include "Vase/Catalog/LibraryFileName.h"

#include <gtest/gtest.h>
#include <optional>
#include <string>

namespace
{

std::string RoundTrip(const std::string& stem)
{
    const std::optional<std::string> back =
        vase::catalog_detail::LibraryStem(vase::catalog_detail::LibraryFileName(stem));
    return back.has_value() ? *back : std::string{"<none>"};
}

TEST(LibraryFileName, RoundTripsOnThisPlatformsOwnNaming)
{
    EXPECT_EQ(RoundTrip("HelloPlugin"), "HelloPlugin");
    EXPECT_EQ(RoundTrip("VersionedA"), "VersionedA");
}

TEST(LibraryFileName, RejectsForeignAndMalformedNames)
{
    // 别的平台的形态本平台不认（这是刻意的：识别面只认自己的命名，不猜）。
    EXPECT_FALSE(vase::catalog_detail::LibraryStem("HelloPlugin.dll").has_value() ==
                 vase::catalog_detail::LibraryStem("libHelloPlugin.so").has_value());
    EXPECT_FALSE(vase::catalog_detail::LibraryStem("readme.txt").has_value());
    EXPECT_FALSE(vase::catalog_detail::LibraryStem("").has_value());
    // 前缀剥完是空串 ⇒ 不是合法的 stem。
#ifdef _WIN32
    EXPECT_FALSE(vase::catalog_detail::LibraryStem(".dll").has_value());
#else
    EXPECT_FALSE(vase::catalog_detail::LibraryStem("lib.so").has_value());
#endif
}

} // namespace
