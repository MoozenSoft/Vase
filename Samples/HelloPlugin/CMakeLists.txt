# 示例插件与 fixture 走同一个形态定义函数（Cmake/VasePluginHelpers.cmake）——
# 「插件 target 长什么样」全仓库只此一处，示例也不例外。
vase_add_plugin_fixture(HelloPlugin SOURCES HelloPlugin.cpp LINK_LIBRARIES VasePod)

# 提供方与消费方共用 Greeter.h，它住在 Samples/HelloCommon。
target_include_directories(HelloPlugin PRIVATE
    "${PROJECT_SOURCE_DIR}/Samples/HelloCommon")
