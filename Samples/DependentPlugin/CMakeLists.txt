# 与 HelloPlugin 同法：插件形态唯一出口 vase_add_plugin_fixture（规矩 5），只链 VasePod。
vase_add_plugin_fixture(DependentPlugin
    PACK
    SOURCES DependentPlugin.cpp
    LINK_LIBRARIES VasePod HelloCommon
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json")
