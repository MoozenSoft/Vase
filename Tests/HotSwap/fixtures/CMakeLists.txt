# 热插拔主循环 fixture（T12）：三个 target，与 Integration/fixtures 同——共用本目录一份
# CMakeLists，只有 Unit/fixtures/LoadProbe 那种「每目录各带一份」的形态才不这样。
# 插件形态定义只走 vase_add_plugin_fixture（Cmake/VasePluginHelpers.cmake），不手写
# add_library(... SHARED)。
vase_add_plugin_fixture(VersionedA
    SOURCES VersionedA/VersionedA.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(VersionedAPrime
    SOURCES VersionedAPrime/VersionedAPrime.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(NeighborB
    SOURCES NeighborB/NeighborB.cpp
    LINK_LIBRARIES VasePod)
# 无再入探针（T13 §7.6）：与 B 同局，Eject 它之后由噪声拍证明没有回调落进已卸代码。
vase_add_plugin_fixture(TimerPlugin
    SOURCES TimerPlugin/TimerPlugin.cpp
    LINK_LIBRARIES VasePod)

# §12.1「同名、不同目录、运行时覆盖」的 CMake 形态：
# A′ 的输出名改成 VersionedA —— $<TARGET_FILE:VersionedAPrime> 因此指到 stageAPrime 下
# 那个**叫 VersionedA 名**的文件，测试才能用它的字节覆盖「A 的位置」。
set_target_properties(VersionedAPrime PROPERTIES OUTPUT_NAME VersionedA)

# 三个输出目录全分开（RUNTIME=.dll / .so 与可执行，LIBRARY=Linux .so，ARCHIVE=Windows
# 导入库 .lib）。ARCHIVE 这一项不是装饰：两个 target 同名时它默认都落 build/<preset>/lib/，
# Ninja 会在生成期报 "multiple rules generate .../VersionedA.lib" 而整棵树构建失败。
set_target_properties(VersionedA PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stageA"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stageA"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stageA")
set_target_properties(VersionedAPrime PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stageAPrime"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stageAPrime"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stageAPrime")
