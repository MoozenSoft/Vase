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

# 第二消费者探针（M2a-T9）：与 EdgeConsumer 抢同一提供方——点名两条的拒绝才是清单。
vase_add_plugin_fixture(SharedConsumer2Plugin
    SOURCES SharedConsumer2Plugin/SharedConsumer2Plugin.cpp
    LINK_LIBRARIES VasePod)

# M2a 装配语义探针（T5）：归因链与环退化。
vase_add_plugin_fixture(DeadProviderPlugin
    SOURCES DeadProviderPlugin/DeadProviderPlugin.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(DeadConsumerPlugin
    SOURCES DeadConsumerPlugin/DeadConsumerPlugin.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(CycleAPlugin
    SOURCES CycleAPlugin/CycleAPlugin.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(CycleBPlugin
    SOURCES CycleBPlugin/CycleBPlugin.cpp
    LINK_LIBRARIES VasePod)

# 落边即败探针（T5 R5-2）：Failed 即时拆账的承重现场材料。
vase_add_plugin_fixture(FailingEdgeConsumerPlugin
    SOURCES FailingEdgeConsumerPlugin/FailingEdgeConsumerPlugin.cpp
    LINK_LIBRARIES VasePod)

# 配置应用探针（T6）：正常消费者 + 作者契约反例（读布局不符的结构体）。
vase_add_plugin_fixture(ConfigConsumerPlugin
    SOURCES ConfigConsumerPlugin/ConfigConsumerPlugin.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(ConfigLayoutMisusePlugin
    SOURCES ConfigLayoutMisusePlugin/ConfigLayoutMisusePlugin.cpp
    LINK_LIBRARIES VasePod)

# OnStart 递归拆除探针（T7）：失败提供者 + 四形态下游 + 传递链第二跳。
vase_add_plugin_fixture(StartFailProviderPlugin
    SOURCES StartFailProviderPlugin/StartFailProviderPlugin.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(BehindStrictUsedPlugin
    SOURCES BehindStrictUsedPlugin/BehindStrictUsedPlugin.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(BehindStrictUnusedPlugin
    SOURCES BehindStrictUnusedPlugin/BehindStrictUnusedPlugin.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(BehindOptUsedPlugin
    SOURCES BehindOptUsedPlugin/BehindOptUsedPlugin.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(BehindOptUnusedPlugin
    SOURCES BehindOptUnusedPlugin/BehindOptUnusedPlugin.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(BehindFarPlugin
    SOURCES BehindFarPlugin/BehindFarPlugin.cpp
    LINK_LIBRARIES VasePod)

# Provides 碰撞探针（T8）：与 SharedProvider 抢同一个 Vase.Test.Shared 的重复者。
vase_add_plugin_fixture(CollisionProviderPlugin
    SOURCES CollisionProviderPlugin/CollisionProviderPlugin.cpp
    LINK_LIBRARIES VasePod)

# 跳过者碰撞证人（T8 补充 / R8-1）：声明入账后被 (b) 永缺依赖跳过的第二个 Dead 提供者。
vase_add_plugin_fixture(DeadProvider2Plugin
    SOURCES DeadProvider2Plugin/DeadProvider2Plugin.cpp
    LINK_LIBRARIES VasePod)

# 加载期比对材料（M2b 波 2-T7）：双服务逆序声明 + 无 .Config（序不敏感与 D89 两判据共用）。
vase_add_plugin_fixture(TwoProvidersPlugin
    SOURCES TwoProvidersPlugin/TwoProvidersPlugin.cpp
    LINK_LIBRARIES VasePod)
