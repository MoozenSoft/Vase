# 示范组合库（VasePack spec D202）：把 Samples 三个插件链成一个发布形 SHARED 库。
# 三者的类名恰好等于 target 名，故只写 PACK、无需另指类名——分发表按 target 属性 VASE_PLUGIN_CLASS
# 拼 VasePluginDesc_<类名>，该属性缺省即 target 名（D198）。vase_add_plugin_fixture 认的关键字是
# PACK / MANIFEST / SOURCES / LINK_LIBRARIES，**没有 CLASS 关键字**。默认路径自证。
# DependentPlugin 与 FailingPlugin 的 PACK 在各自的 CMakeLists 里。
vase_pack_plugins(VasePlugins PLUGINS HelloPlugin DependentPlugin FailingPlugin)
