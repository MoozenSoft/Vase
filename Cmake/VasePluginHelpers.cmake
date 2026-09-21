# 插件形态的模板：SHARED + 只链 VasePod（D11 的链接期事实）+ 收紧可见性。
# M1 后续所有插件 target（Sample 与 fixture）都走这一个函数——插件形态定义只此一处，
# 防「某个 fixture 忘了 hidden visibility / 忘了链 VaseBuildOptions」（CLAUDE.md 规矩 1）。
function(vase_add_plugin_fixture name)
    cmake_parse_arguments(FIX "" "" "SOURCES;LINK_LIBRARIES" ${ARGN})
    add_library(${name} SHARED ${FIX_SOURCES})
    target_link_libraries(${name}
        PRIVATE VaseBuildOptions ${FIX_LINK_LIBRARIES})
    set_target_properties(${name} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
endfunction()
