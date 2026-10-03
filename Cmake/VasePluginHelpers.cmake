# 插件形态的模板：SHARED + 只链 VasePod（D11 的链接期事实）+ 收紧可见性 + 产物落自己的目录。
# M1 后续所有插件 target（Sample 与 fixture）都走这一个函数——插件形态定义只此一处，
# 防「某个 fixture 忘了 hidden visibility / 忘了链 VaseBuildOptions」（CLAUDE.md 规矩 1）。
#
# 一插件一目录（spec D180/D183）：产物落 <产物根>/<target 名>/，宿主可执行与框架库仍平铺。
# Tests/HotSwap/fixtures 的 Versioned 四兄弟是记名的例外——它们**在调用本函数之后**
# 显式设三项输出目录到各自的 stage* 目录（D194），靠的是「后设者胜」，顺序别调换。
function(vase_add_plugin_fixture name)
    cmake_parse_arguments(FIX "" "MANIFEST" "SOURCES;LINK_LIBRARIES" ${ARGN})
    add_library(${name} SHARED ${FIX_SOURCES})
    target_link_libraries(${name}
        PRIVATE VaseBuildOptions ${FIX_LINK_LIBRARIES})
    set_target_properties(${name} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${name}")

    if(FIX_MANIFEST)
        # 构建后把清单铺到插件产物目录旁（D186 的「拷贝」形态）。本命令无 DEPENDS，
        # 只改源清单而不重链目标时不会重跑——不保证副本随源清单单独改动自动同步。
        add_custom_command(TARGET ${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${FIX_MANIFEST}" "$<TARGET_FILE_DIR:${name}>/plugin.json"
            COMMENT "staging plugin.json for ${name}")
    endif()
endfunction()
