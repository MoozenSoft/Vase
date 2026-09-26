# 与 HelloPlugin 同法：插件形态唯一出口 vase_add_plugin_fixture（规矩 5），只链 VasePod。
vase_add_plugin_fixture(FailingPlugin SOURCES FailingPlugin.cpp LINK_LIBRARIES VasePod)
