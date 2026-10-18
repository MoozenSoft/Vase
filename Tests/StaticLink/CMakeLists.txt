# 第二个 gtest 可执行（spec D223）：静态链入的见证必须自己是一个二进制——
# 链进 VaseTests 会让三个 Sample 从启动起驻留，与别处动态装的同名 Id 相撞。
#
# 保活探针 fixture（spec §5.2）：先于 vase_pack_plugins 定义——它的 .Pack 要被链进可执行，
# 但**不列入** PLUGINS（这就是「无人引用」的形态）。
add_subdirectory(fixtures/KeepAliveProbe)

# 组合库的真静态形：与 Samples/VasePlugins 的 SHARED 支共用同一套分发表生成器（D222），
# 只是多给一个 STATIC。产物落 VASE_PACK_INTERMEDIATE_DIR（D231）。
vase_pack_plugins(VasePluginsStatic PLUGINS HelloPlugin DependentPlugin FailingPlugin STATIC)

vase_stage_pack(VaseReleaseStaticSamples
    PACK_TARGET VasePluginsStatic
    SOURCE_ID StaticSamples
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Tests/Package/release samples/samples")
add_custom_target(VasePackageReleaseStage DEPENDS
    VaseReleaseDynamic VaseReleaseProvider VaseReleaseConsumer VaseReleaseStaticSamples)
add_test(NAME PackageReleaseStage
    COMMAND ${vaseTestBuildCommand} --build "${CMAKE_BINARY_DIR}" --target VasePackageReleaseStage)
set_tests_properties(PackageReleaseStage PROPERTIES
    FIXTURES_SETUP PackageReleaseStage RESOURCE_LOCK PackageReleaseOutputs)

add_executable(VaseStaticLinkTests
    StaticLinkTests.cpp)

target_link_libraries(VaseStaticLinkTests
    PRIVATE VaseBuildOptions
            VaseHost
            VasePluginsStatic
            KeepAliveProbe.Pack
            GTest::gtest_main)

# 保活探针的探测机制按平台分岔，代码在 StaticLinkTests.cpp（spec §5.2）：
# ELF 走弱符号，Mach-O 走 dlsym，Windows 走产物导出表；各支的负例与对照用同一机制。

# STATIC 包通过生成的声明头取址，专属入口只拉取本包成员。

include(GoogleTest)
gtest_discover_tests(VaseStaticLinkTests
    DISCOVERY_MODE PRE_TEST)

add_executable(VaseMultiPackageLinkTests MultiPackageLinkTests.cpp PackageAssemblyTests.cpp)
target_link_libraries(VaseMultiPackageLinkTests
    PRIVATE VaseBuildOptions VaseHost VaseCatalog VaseCliCore PackageServices
            VasePackageProvider VasePackageConsumer VaseMobileScenarioCore VasePluginsStatic GTest::gtest_main)
target_include_directories(VaseMultiPackageLinkTests PRIVATE ../TestingSupport
    "${PROJECT_SOURCE_DIR}/Samples/Mobile")
target_compile_definitions(VaseMultiPackageLinkTests PRIVATE
    VASE_PACKAGE_STATIC_STAGE="$<PATH:CMAKE_PATH,${CMAKE_BINARY_DIR}/Tests/Package/release static>"
    VASE_PACKAGE_STATIC_SAMPLE_STAGE="$<PATH:CMAKE_PATH,${CMAKE_BINARY_DIR}/Tests/Package/release samples/samples>")
gtest_discover_tests(VaseMultiPackageLinkTests
    DISCOVERY_MODE PRE_TEST
    TEST_FILTER "-StaticPackageAssembly.*:StaticResourceAssembly.*")
gtest_discover_tests(VaseMultiPackageLinkTests
    DISCOVERY_MODE PRE_TEST
    TEST_FILTER "StaticPackageAssembly.*:StaticResourceAssembly.*"
    PROPERTIES FIXTURES_REQUIRED PackageReleaseStage)
