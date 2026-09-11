#include "Vase/Detail/SmokeProbe.h"

#include <gtest/gtest.h>

namespace
{

// 本用例证明：VaseSession 导出的符号，经 VaseHost 的导入表被真正调用到了。
// 它是 M0 的价值所在——导出宏、动态库链接、运行时库查找路径三件事同时被验证。
TEST(CrossDll, HostImportsSessionSymbol) { EXPECT_EQ(vase::HostSmokeProbe(), vase::SessionSmokeProbe()); }

// 反向对照：探针返回值非零，因此上面那条断言不会因为「两边都返回 0」而假绿。
TEST(CrossDll, ProbeValueIsNotTriviallyZero) { EXPECT_NE(vase::SessionSmokeProbe(), 0U); }

} // namespace
