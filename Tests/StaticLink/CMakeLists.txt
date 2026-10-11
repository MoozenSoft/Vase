# 第二个 gtest 可执行（spec D223）：静态链入的见证必须自己是一个二进制——
# 链进 VaseTests 会让三个 Sample 从启动起驻留，与别处动态装的同名 Id 相撞。
#
# 保活探针 fixture（spec §5.2）：先于 vase_pack_plugins 定义——它的 .Pack 要被链进可执行，
# 但**不列入** PLUGINS（这就是「无人引用」的形态）。
add_subdirectory(fixtures/KeepAliveProbe)

# 组合库的真静态形：与 Samples/VasePlugins 的 SHARED 支共用同一套分发表生成器（D222），
# 只是多给一个 STATIC。产物落 VASE_PACK_INTERMEDIATE_DIR（D231）。
vase_pack_plugins(VasePluginsStatic PLUGINS HelloPlugin DependentPlugin FailingPlugin STATIC)

add_executable(VaseStaticLinkTests
    StaticLinkTests.cpp)

target_link_libraries(VaseStaticLinkTests
    PRIVATE VaseBuildOptions
            VaseHost
            VasePluginsStatic
            KeepAliveProbe.Pack
            GTest::gtest_main)

# 保活探针的探测机制按平台分岔，代码在 StaticLinkTests.cpp（spec §5.2）：
# ELF / Mach-O 走 __attribute__((weak))，Windows 两线走「读产物符号表」——COFF 无可靠弱引用
# 且 /alternatename 实测不阻止归档提取，故**不接链接器指令、不链替身源**（与 brief 的初版不同）。

# 入口声明见 StaticLinkTests.cpp 顶部的 extern "C" 声明：静态链入时 VasePlugin_GetPlugin
# 就是本可执行里的一个普通符号（由分发表 TU 定义，Cmake/VasePackDispatch.cpp.in），
# 没有 dllexport/import 那一套可谈。

include(GoogleTest)
gtest_discover_tests(VaseStaticLinkTests
    DISCOVERY_MODE PRE_TEST)
