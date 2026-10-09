# 示例插件与 fixture 走同一个形态定义函数（Cmake/VasePluginHelpers.cmake）——
# 「插件 target 长什么样」全仓库只此一处，示例也不例外。
vase_add_plugin_fixture(HelloPlugin
    SOURCES HelloPlugin.cpp
    LINK_LIBRARIES VasePod HelloCommon
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    PACK)

# 注：提供的头（Greeter.h）住在 Samples/HelloCommon，经 LINK_LIBRARIES 取——
# 调用点之后不再追加编译期输入（VasePack spec D210）。
