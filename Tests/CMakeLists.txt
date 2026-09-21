find_package(GTest CONFIG REQUIRED)

add_executable(VaseTests
    Smoke/CrossDllSmoke.cpp
    Unit/DetailTests.cpp
    Unit/DescriptorTests.cpp
    Unit/EffectTests.cpp
    Unit/LoaderTests.cpp
    Unit/RegistryBusTests.cpp
    Unit/LedgerTests.cpp
    Lifecycle/LifecycleTests.cpp
    Lifecycle/DiagnosticAttributionTests.cpp
    Integration/FailureSemanticsTests.cpp
    Integration/LedgerSemanticsTests.cpp
    Integration/ThreadGuardTests.cpp
    HotSwap/EjectTests.cpp
    HotSwap/AdoptTests.cpp
    HotSwap/HotSwapLoopTests.cpp
    HotSwap/TimerNoReentryTests.cpp
    Abi/ImportEnforcementTests.cpp
    TestingSupport/PodTestPeer.cpp)

# Loader 端到端断言要真实插件二进制：两个 target 同源码不同输出名。
add_subdirectory(Unit/fixtures/LoadProbe)

# 失败语义 fixture（T8）：四个独立插件二进制。
add_subdirectory(Integration/fixtures)

# 热插拔主循环 fixture（T12）：同名双版本 A/A′ + 邻居 B。
add_subdirectory(HotSwap/fixtures)

# 导入表执法 fixture（T13 §8.7）：一对真互链的插件 A→B。
add_subdirectory(Abi/fixtures)

target_link_libraries(VaseTests
    PRIVATE VaseBuildOptions
            VaseHost
            GTest::gtest_main)

# PodTestPeer.h 住在这里，测试文件以 `#include "PodTestPeer.h"` 取它。
target_include_directories(VaseTests PRIVATE TestingSupport)

# CMAKE_PATH：把生成器表达式结果转成正斜杠绝对路径——直接塞进 C++ 字符串字面量
# 会留下反斜杠转义事故（Windows 上 \V、\b 都是合法转义，字符串静默变形）。
target_compile_definitions(VaseTests PRIVATE
    VASE_FIXTURE_LOADPROBE="$<PATH:CMAKE_PATH,$<TARGET_FILE:LoadProbe>>"
    VASE_FIXTURE_UNLOADPROBE="$<PATH:CMAKE_PATH,$<TARGET_FILE:UnloadProbe>>"
    VASE_FIXTURE_HELLO="$<PATH:CMAKE_PATH,$<TARGET_FILE:HelloPlugin>>"
    VASE_FIXTURE_STALEHEADER="$<PATH:CMAKE_PATH,$<TARGET_FILE:StaleHeaderPlugin>>"
    VASE_FIXTURE_FAILINGLOAD="$<PATH:CMAKE_PATH,$<TARGET_FILE:FailingLoadPlugin>>"
    VASE_FIXTURE_FAILINGSTART="$<PATH:CMAKE_PATH,$<TARGET_FILE:FailingStartPlugin>>"
    VASE_FIXTURE_MISSINGCONSUMER="$<PATH:CMAKE_PATH,$<TARGET_FILE:RequiresMissingConsumer>>"
    VASE_FIXTURE_SHAREDPROVIDER="$<PATH:CMAKE_PATH,$<TARGET_FILE:SharedProviderPlugin>>"
    VASE_FIXTURE_EDGECONSUMER="$<PATH:CMAKE_PATH,$<TARGET_FILE:EdgeConsumerPlugin>>"
    VASE_FIXTURE_UNDECLAREDGET="$<PATH:CMAKE_PATH,$<TARGET_FILE:UndeclaredGetConsumer>>"
    VASE_FIXTURE_VERSIONEDA="$<PATH:CMAKE_PATH,$<TARGET_FILE:VersionedA>>"
    VASE_FIXTURE_VERSIONEDAPRIME="$<PATH:CMAKE_PATH,$<TARGET_FILE:VersionedAPrime>>"
    VASE_FIXTURE_NEIGHBORB="$<PATH:CMAKE_PATH,$<TARGET_FILE:NeighborB>>"
    VASE_FIXTURE_TIMER="$<PATH:CMAKE_PATH,$<TARGET_FILE:TimerPlugin>>"
    VASE_FIXTURE_BADLINKA="$<PATH:CMAKE_PATH,$<TARGET_FILE:BadLinkSiblingA>>"
    VASE_FIXTURE_BADLINKB="$<PATH:CMAKE_PATH,$<TARGET_FILE:BadLinkSiblingB>>")

# 档三「特征缺失」fixture（T11）：**整个 if 块（target 与它的宏定义）必须在同一道
# `if(NOT WIN32)` 里**——宏的值是 `$<TARGET_FILE:NoBuildIdPlugin>`，而生成器表达式
# 提到一个不存在的 target 会在 **configure 期**炸掉整棵树（不是只掉两条 Linux 用例）。
#
# 链接顺序是**实测**钉住的，不是照抄文档结论：`target_link_options` 排在工具链
# `CMAKE_SHARED_LINKER_FLAGS_INIT` 的 `-Wl,--build-id=sha1` 之后，ld 后者胜，产物确实
# 没有 .note.gnu.build-id（实验记录见 task-11-report.md）。若某天顺序变了，退路是
# POST_BUILD `llvm-objcopy --remove-section .note.gnu.build-id`——两条路通向同一个断言。
if(NOT WIN32)
    vase_add_plugin_fixture(NoBuildIdPlugin
        SOURCES HotSwap/fixtures/NoBuildIdPlugin/NoBuildIdPlugin.cpp
        LINK_LIBRARIES VasePod)
    target_link_options(NoBuildIdPlugin PRIVATE "-Wl,--build-id=none")
    target_compile_definitions(VaseTests PRIVATE
        VASE_FIXTURE_NOBUILDID="$<PATH:CMAKE_PATH,$<TARGET_FILE:NoBuildIdPlugin>>")
endif()

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
# gtest / VasePod / VaseHost 这些 DLL 未必已在可执行文件旁边，
# 会让构建无缘无故地失败。
gtest_discover_tests(VaseTests
    DISCOVERY_MODE PRE_TEST)
