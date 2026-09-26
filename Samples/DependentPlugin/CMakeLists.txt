# 与 HelloPlugin 同法：插件形态唯一出口 vase_add_plugin_fixture（规矩 5），只链 VasePod。
vase_add_plugin_fixture(DependentPlugin SOURCES DependentPlugin.cpp LINK_LIBRARIES VasePod)

# 消费端与 Hello 提供方共用 Greeter.h（标识 kName/kVersion 从它来）。
target_include_directories(DependentPlugin PRIVATE
    "${PROJECT_SOURCE_DIR}/Samples/HelloCommon")
