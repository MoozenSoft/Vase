find_package(GTest CONFIG REQUIRED)

add_executable(VaseTests
    Smoke/CrossDllSmoke.cpp)

target_link_libraries(VaseTests
    PRIVATE VaseBuildOptions
            VaseHost
            GTest::gtest_main)

# gtest 头文件在本仓库看到的异常配置，与预编译的 gtest DLL **不一致**（实测两边）：
#
#   我们的 TU      GTEST_HAS_EXCEPTIONS == 0
#                  （/EP 实测：clang-cl 与 cl.exe 都是 0。推导路径见 gtest-port.h：
#                    本项目 /EHs-c- 使 _CPPUNWIND / __EXCEPTIONS 都不定义）
#   gtest DLL      GTEST_HAS_EXCEPTIONS == 1
#                  （vcpkg 的构建日志 config-x64-windows-dbg-ninja.log 里带
#                    -D_HAS_EXCEPTIONS=1）
#
# **由此产生一条对写测试的人有约束力的规则：不要用 EXPECT_THROW / ASSERT_THROW /
# EXPECT_ANY_THROW / EXPECT_NO_THROW 这一族宏。**
#
# 注意它们的失败方式（实测，与「静默退化」相反，是编译期硬失败）：gtest.h 的
# EXPECT_THROW 无条件展开成 gtest-internal.h 的 GTEST_TEST_THROW_，而后者**不受
# GTEST_HAS_EXCEPTIONS 保护**，宏体里就是一个裸 try/catch。于是：
#   clang-cl  error: cannot use 'try' with exceptions disabled   （EXIT=1）
#   cl.exe    error C4530（被 /we4530 升为错误）                  （EXIT=2）
# 两条线都是构建失败，不是「断言恒假」或「空操作」——所以这条不会静默，但也因此
# 没有商量余地：想测「某个操作必须失败」，只能用 Result<T> / Error 的显式返回值，
# 不能用异常断言。
include(GoogleTest)

# DISCOVERY_MODE PRE_TEST：把「枚举测试」推迟到 ctest 运行时。
# 默认模式会在**构建期**执行测试可执行文件来枚举用例，而那时 Windows 上的
# gtest / VaseSession / VaseHost 这些 DLL 未必已在可执行文件旁边，
# 会让构建无缘无故地失败。
gtest_discover_tests(VaseTests
    DISCOVERY_MODE PRE_TEST)
