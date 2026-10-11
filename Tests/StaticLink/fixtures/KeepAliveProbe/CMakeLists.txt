# PACK 造出 KeepAliveProbe.Pack（静态库）——它会被链进见证可执行，但**不列入** PLUGINS。
vase_add_plugin_fixture(KeepAliveProbe
    SOURCES KeepAliveProbe.cpp
    LINK_LIBRARIES VasePod
    PACK)

# 分发表按 VASE_PLUGIN_CLASS 拼 VasePluginDesc_<类名>，该属性缺省即 target 名；本 fixture
# 的类名 KeepAliveProbePlugin ≠ target 名 KeepAliveProbe，故显式钉住。否则 Step 4 把探针
# 临时列入 PLUGINS 时，分发表会去找不存在的 VasePluginDesc_KeepAliveProbe、链接失败。
set_target_properties(KeepAliveProbe PROPERTIES VASE_PLUGIN_CLASS KeepAliveProbePlugin)
