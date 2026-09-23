# 换件材料（spec §3.2 推论 1）：与 HelloPlugin 同 Id、同 Provides、同 Requires，
# 只有 Greet() 返回的串不同。形态定义走 vase_add_plugin_fixture（CLAUDE.md 规矩 5），
# 与 Samples/HelloPlugin 的同名文件逐行同形。
vase_add_plugin_fixture(HelloPluginPrime
    SOURCES HelloPlugin.cpp
    LINK_LIBRARIES VasePod)

target_include_directories(HelloPluginPrime PRIVATE
    "${PROJECT_SOURCE_DIR}/Samples/HelloCommon")
