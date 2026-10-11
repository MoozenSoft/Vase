# 插件形态的模板：SHARED + 只链 VasePod（D11 的链接期事实）+ 收紧可见性 + 产物落自己的目录。
# M1 后续所有插件 target（Sample 与 fixture）都走这一个函数——插件形态定义只此一处，
# 防「某个 fixture 忘了 hidden visibility / 忘了链 VaseBuildOptions」（CLAUDE.md 规矩 1）。
#
# 一插件一目录（spec D180/D183）：产物落 <产物根>/<target 名>/，宿主可执行与框架库仍平铺。
# Tests/HotSwap/fixtures 的 Versioned 四兄弟是记名的例外——它们**在调用本函数之后**
# 显式设三项输出目录到各自的 stage* 目录（D194），靠的是「后设者胜」，顺序别调换。
#
# PACK（VasePack spec D199）：额外建一个**静态库** ${name}.Pack——同一个源、同一组链接库、
# 同样的可见性收紧，外加 VASE_PLUGIN_PACK_OBJECT（pack 形不发两枚每库一份的入口）与
# POSITION_INDEPENDENT_CODE（它要被链进 SHARED 库的 vase_pack_plugins 产物）。
# 必须显式设 PIC：全仓今天零处 POSITION_INDEPENDENT_CODE，本目标是第一个被链进 SHARED 的静态库。
# 归档落 VASE_PACK_INTERMEDIATE_DIR：它是构建中间物，不进产物根（D212）。
# VASE_PLUGIN_CLASS（D198）：分发表拼 VasePluginDesc_<类名> 时读它，缺省 = target 名。
#
# 统一的中间产物目录（本波 D231）：打包链路的**链接输入**都落这里——`.Pack` 归档与静态组合库。
# 判据是「打包链路的中转」，不是「任何静态库」：`VaseCliCore` 有自己的消费者、不算；
# 生成的 dispatch TU 是**源**（进 tidy 面，D205）、也不算。
# 位置取产物根（bin/ / lib/）之外的兄弟目录——「会不会被发布」因此是结构事实，不靠约定。
set(VASE_PACK_INTERMEDIATE_DIR "${CMAKE_BINARY_DIR}/pack-objects")
function(vase_add_plugin_fixture name)
    cmake_parse_arguments(FIX "PACK" "MANIFEST" "SOURCES;LINK_LIBRARIES" ${ARGN})
    add_library(${name} SHARED ${FIX_SOURCES})
    target_link_libraries(${name}
        PRIVATE VaseBuildOptions ${FIX_LINK_LIBRARIES})
    set_target_properties(${name} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        VASE_PLUGIN_CLASS "${name}"
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

    if(FIX_PACK)
        add_library(${name}.Pack STATIC ${FIX_SOURCES})
        target_link_libraries(${name}.Pack
            PRIVATE VaseBuildOptions ${FIX_LINK_LIBRARIES})
        target_compile_definitions(${name}.Pack PRIVATE VASE_PLUGIN_PACK_OBJECT)
        set_target_properties(${name}.Pack PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON
            POSITION_INDEPENDENT_CODE ON
            ARCHIVE_OUTPUT_DIRECTORY "${VASE_PACK_INTERMEDIATE_DIR}")
    endif()
endfunction()

# 发布期组合库（VasePack spec D196）：把若干 pack 形静态库链成一个 SHARED 库，
# 并生成一张统一入口的分发表 TU。表单 §8.6 的「硬引用链」保活——分发表显式调用每个
# 插件的描述符符号，而静态库按需提取，那条引用链才把成员拉进库（D199/D213）。
# 主动闸三项（D211）：未 PACK / 重复 / 空——后两项都会**静默**造出坏产物（重复 Id 的
# 枚举面、装零个插件的库），故一律在配置期带名拒绝。
# 三条地雷（D214）：① 一个描述符符号不得由两个 .Pack 定义——LoadProbe 与 UnloadProbe 编**同一份**
# LoadProbe.cpp（类 LoadProbePlugin）⇒ 同符号 VasePluginDesc_LoadProbePlugin；同列 PLUGINS ⇒
# 重定义、链接失败（响亮）。（HelloPlugin 与 HelloPluginPrime 编**两个不同文件**、符号不同，不属 ①。）
# ② 两个 target 共享同一 Id 者不得同列 PLUGINS——HelloPlugin 与 HelloPluginPrime 都是 Vase.Hello，
# 枚举面会带重复 Id；上面的重复闸只挡 target 名、挡不住这形，**静默**。
# ③ VasePlugin_Descriptors 返回顺序无契约（= PLUGINS 书写顺序，不对外承诺）。
# 裸关键字 STATIC（本波 D222）：出静态组合库而非 SHARED——它是**链接输入**（宿主链接期链入），
# 故落 VASE_PACK_INTERMEDIATE_DIR 而非产物根；SHARED 支的平铺落位（D201）逐字不变。
function(vase_pack_plugins target)
    cmake_parse_arguments(PACK "STATIC" "" "PLUGINS" ${ARGN})

    if(NOT PACK_PLUGINS)
        message(FATAL_ERROR
            "vase_pack_plugins(${target}): PLUGINS 为空——装零个插件的组合库没有意义（同 VaseCli 的 D135 同一律）")
    endif()
    set(declared ${PACK_PLUGINS})
    list(REMOVE_DUPLICATES declared)
    list(LENGTH PACK_PLUGINS declaredCount)
    list(LENGTH declared uniqueCount)
    if(NOT declaredCount EQUAL uniqueCount)
        message(FATAL_ERROR
            "vase_pack_plugins(${target}): PLUGINS 有重复成员——会生成含重复 Id 的枚举面")
    endif()

    set(VASE_PACK_DECLS "")
    set(VASE_PACK_ENTRIES "")
    foreach(plugin IN LISTS PACK_PLUGINS)
        if(NOT TARGET ${plugin}.Pack)
            message(FATAL_ERROR
                "vase_pack_plugins(${target}): ${plugin} 未声明 PACK——请在它的 vase_add_plugin_fixture 调用点加 PACK")
        endif()
        # VASE_PLUGIN_CLASS 由 vase_add_plugin_fixture 无条件设（见上），故此处不会拿到 NOTFOUND；
        # 刻意不设兜底——真绕过该函数立库时，拼出的 VasePluginDesc_pluginClass-NOTFOUND() 是非法
        # 符号名、编生成的 TU 即响亮失败，胜过静默。
        get_target_property(pluginClass ${plugin} VASE_PLUGIN_CLASS)
        string(APPEND VASE_PACK_DECLS
            "extern \"C\" const ::vase::PluginDescriptor* VasePluginDesc_${pluginClass}();\n")
        string(APPEND VASE_PACK_ENTRIES "    VasePluginDesc_${pluginClass}(),\n")
    endforeach()

    configure_file(
        "${CMAKE_SOURCE_DIR}/Cmake/VasePackDispatch.cpp.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${target}.dispatch.cpp"
        @ONLY)

    # 缺省 SHARED（发布形，D201）；STATIC 是链接输入（本波 D222/D231），落中间目录。
    set(linkage SHARED)
    if(PACK_STATIC)
        set(linkage STATIC)
    endif()
    add_library(${target} ${linkage} "${CMAKE_CURRENT_BINARY_DIR}/${target}.dispatch.cpp")
    target_link_libraries(${target} PRIVATE VaseBuildOptions VasePod)
    set_target_properties(${target} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
    if(PACK_STATIC)
        set_target_properties(${target} PROPERTIES
            POSITION_INDEPENDENT_CODE ON
            ARCHIVE_OUTPUT_DIRECTORY "${VASE_PACK_INTERMEDIATE_DIR}")
    endif()
    foreach(plugin IN LISTS PACK_PLUGINS)
        target_link_libraries(${target} PRIVATE ${plugin}.Pack)
    endforeach()
endfunction()
