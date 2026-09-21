# 失败语义 fixture（T8）：与 Sample 插件同一形态定义（Cmake/VasePluginHelpers.cmake）。
# 每个 fixture 一个子目录（与 Unit/fixtures/LoadProbe 同）；差别是那处每目录各带一份
# CMakeLists.txt，这里四个共用本文件。
# 这四件都是**测试材料**，故住在 Integration 下而非 Samples（§10.1：Samples 只放演示）。
vase_add_plugin_fixture(StaleHeaderPlugin
    SOURCES StaleHeaderPlugin/StaleHeaderPlugin.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(FailingLoadPlugin
    SOURCES FailingLoadPlugin/FailingLoadPlugin.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(FailingStartPlugin
    SOURCES FailingStartPlugin/FailingStartPlugin.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(RequiresMissingConsumer
    SOURCES RequiresMissingConsumer/RequiresMissingConsumer.cpp
    LINK_LIBRARIES VasePod)

# 账本语义 fixture（T9）：提供方 / 消费者 / 未声明解析各一件。
vase_add_plugin_fixture(SharedProviderPlugin
    SOURCES SharedProviderPlugin/SharedProviderPlugin.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(EdgeConsumerPlugin
    SOURCES EdgeConsumerPlugin/EdgeConsumerPlugin.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(UndeclaredGetConsumer
    SOURCES UndeclaredGetConsumer/UndeclaredGetConsumer.cpp
    LINK_LIBRARIES VasePod)
