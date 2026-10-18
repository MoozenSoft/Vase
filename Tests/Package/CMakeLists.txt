# 桌面与宿主原生候选生成器共同消费唯一的 Provider/Consumer 包定义。
include("${PROJECT_SOURCE_DIR}/Samples/Mobile/Packages.cmake")
vase_define_mobile_packages()
vase_define_mobile_scenario()

# VaseTests 只持有动态路径；STATIC 包仅由独立见证程序链入。
target_compile_definitions(VaseTests PRIVATE
    VASE_PACKAGE_DYNAMIC_STAGE="$<PATH:CMAKE_PATH,${CMAKE_CURRENT_BINARY_DIR}/release dynamic>"
    VASE_FIXTURE_PACKAGEPROVIDER="$<PATH:CMAKE_PATH,$<TARGET_FILE:PackageProviderPlugin>>"
    VASE_FIXTURE_PACKAGECONSUMER="$<PATH:CMAKE_PATH,$<TARGET_FILE:PackageConsumerPlugin>>")
target_link_libraries(VaseTests PRIVATE PackageServices)

# --- T11：显式发布目标（vase_stage_pack）与构建级证人（spec §3.1 / A17）---
# 策略输入副本落构建树，Provider fixture 的 MANIFEST 属性指过来：证人脚本只改这份副本，
# 每个失败分支从源文件恢复——Samples 源文件全程不被触碰（fixture 并发由 RUN_SERIAL 排除）。
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stage-policy")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/fixtures/Provider/plugin.json"
    "${CMAKE_CURRENT_BINARY_DIR}/stage-policy/plugin.json" COPYONLY)
set_target_properties(PackageProviderPlugin PROPERTIES
    VASE_PLUGIN_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/stage-policy/plugin.json")

# 运行依赖输入由测试自造（缺它时的负例只挪这个副本，绝不删真实框架库）。
# 两处路径都含空格：一并验 COMMAND/路径的 VERBATIM 引号。
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stage runtime")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/stage runtime/fixture-extra-runtime.dat"
    "vase stage runtime payload\n")

# 静态包：生成器链 VasePackageProvider，用 T3 生成头与成员描述符函数。
vase_stage_pack(VaseStagePackageProvider
    PACK_TARGET VasePackageProvider
    SOURCE_ID Provider
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stage output/provider pkg"
    RUNTIME_FILES "${CMAKE_CURRENT_BINARY_DIR}/stage runtime/fixture-extra-runtime.dat")

# 动态包：生成器不链接 VasePlugins，只依赖它的产物（按路径装载读描述符）。
vase_stage_pack(VaseStageVasePlugins
    PACK_TARGET VasePlugins
    SOURCE_ID VasePlugins
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stage output/plugins pkg")

# 构建脚本不另解析 JSON：校验器用 Catalog 的同一份解析件按 Id 查启用策略，靠退出码报告结论。
# 生成器与校验器都不进任何插件 target 的链接图。
add_executable(VasePackageStageVerifier VerifyStageManifest.cpp)
target_link_libraries(VasePackageStageVerifier PRIVATE VaseBuildOptions VaseCatalog)

add_test(NAME VasePackageStageDependencies
    COMMAND ${vaseTestBuildCommand}
        "-DVASE_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DVASE_STAGE_TARGET=VaseStagePackageProvider"
        "-DVASE_STAGE_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/stage output/provider pkg"
        "-DVASE_DYNAMIC_STAGE_TARGET=VaseStageVasePlugins"
        "-DVASE_DYNAMIC_STAGE_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/stage output/plugins pkg"
        "-DVASE_DYNAMIC_LIBRARY=$<TARGET_FILE_NAME:VasePlugins>"
        "-DVASE_DYNAMIC_PLUGIN_ID=Vase.Hello"
        "-DVASE_POLICY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/Provider/plugin.json"
        "-DVASE_POLICY_COPY=${CMAKE_CURRENT_BINARY_DIR}/stage-policy/plugin.json"
        "-DVASE_RUNTIME_INPUT=${CMAKE_CURRENT_BINARY_DIR}/stage runtime/fixture-extra-runtime.dat"
        "-DVASE_PLUGIN_ID=Vase.PackageProvider"
        "-DVASE_VERIFIER=$<TARGET_FILE:VasePackageStageVerifier>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/VerifyStageDependencies.cmake")
set_tests_properties(VasePackageStageDependencies PROPERTIES
    RUN_SERIAL TRUE RESOURCE_LOCK PackageReleaseOutputs)

# 发布闭环从生产生成目标取清单与镜像；Provider 的源码启用策略保持 false。
vase_pack_plugins(VasePackageAssembly PLUGINS PackageProviderPlugin PackageConsumerPlugin FailingPlugin)
vase_stage_pack(VaseReleaseDynamic
    PACK_TARGET VasePackageAssembly
    SOURCE_ID Assembly
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/release dynamic/assembly pkg")
vase_stage_pack(VaseReleaseProvider
    PACK_TARGET VasePackageProvider
    SOURCE_ID Provider
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/release static/provider pkg")
vase_stage_pack(VaseReleaseConsumer
    PACK_TARGET VasePackageConsumer
    SOURCE_ID Consumer
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/release static/consumer pkg")
