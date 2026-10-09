# HelloCommon——Samples 共用的服务契约头（Greeter.h / Mood.h）。
#
# 立成 INTERFACE 库而非裸头目录（VasePack spec D209）：插件形态的唯一出口
# （vase_add_plugin_fixture，CLAUDE.md 规矩 5）要求插件的**编译期输入全部经参数表达**，
# 因为组合库的 pack 形目标拿不到「函数调用之后」加的 include。形状照 Cmake/VaseThirdParty.cmake
# 的 VaseThirdPartyCli（INTERFACE + 裸路径，本仓库无 install()）。
add_library(HelloCommon INTERFACE)
target_include_directories(HelloCommon INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
