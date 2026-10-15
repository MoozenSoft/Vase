# 显式发布目标（T11，spec §3.1）：把「生成包清单 + 装配发布目录」做成调用者**显式构建**的目标，
# 不经 POST_BUILD 藏进普通 build。调用点必须排在 VaseCliCore 之后（生成器要链它）。
#
#   形态：vase_stage_pack(<stageTarget>
#            PACK_TARGET <包 target> SOURCE_ID <标识>
#            OUTPUT_DIRECTORY <目录> [RUNTIME_FILES <路径...>])
#
# 编译期符号进生成器，全部路径走 argv；动态生成器只依赖包产物，静态生成器链包。
# 源清单、包与 runtime 都显式 DEPENDS，staging 清单不作输入（spec §3.1）。

function(vase_stage_pack stageTarget)
    cmake_parse_arguments(STAGE "" "PACK_TARGET;SOURCE_ID;OUTPUT_DIRECTORY" "RUNTIME_FILES" ${ARGN})
    if(NOT STAGE_PACK_TARGET OR NOT TARGET "${STAGE_PACK_TARGET}")
        message(FATAL_ERROR "vase_stage_pack(${stageTarget}): PACK_TARGET 必须是一个已存在的包目标")
    endif()
    if(NOT STAGE_SOURCE_ID)
        message(FATAL_ERROR "vase_stage_pack(${stageTarget}): 缺少 SOURCE_ID")
    endif()
    if(NOT STAGE_OUTPUT_DIRECTORY)
        message(FATAL_ERROR "vase_stage_pack(${stageTarget}): 缺少 OUTPUT_DIRECTORY")
    endif()
    get_target_property(packMembers ${STAGE_PACK_TARGET} VASE_PACK_MEMBERS)
    get_target_property(packKind ${STAGE_PACK_TARGET} VASE_PACK_KIND)
    if(packMembers-NOTFOUND OR packKind-NOTFOUND)
        message(FATAL_ERROR "vase_stage_pack(${stageTarget}): ${STAGE_PACK_TARGET} 不是 vase_pack_plugins 立的包")
    endif()
    set(stageStatic FALSE)
    if(packKind STREQUAL "STATIC")
        set(stageStatic TRUE)
    endif()

    # 输出目录重叠（相等或互为祖先）在配置期带名拒绝：两个 stage 目标互相踩踏是静默的坏产物。
    get_filename_component(stageOutput "${STAGE_OUTPUT_DIRECTORY}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    cmake_path(NORMAL_PATH stageOutput)
    set(stageOutputKey "${stageOutput}")
    if(WIN32 OR APPLE)
        string(TOLOWER "${stageOutputKey}" stageOutputKey)
    endif()
    get_property(stageOutputs GLOBAL PROPERTY VASE_STAGE_OUTPUTS)
    foreach(known IN LISTS stageOutputs)
        set(knownKey "${known}")
        if(WIN32 OR APPLE)
            string(TOLOWER "${knownKey}" knownKey)
        endif()
        string(FIND "${stageOutputKey}/" "${knownKey}/" knownAboveStage)
        string(FIND "${knownKey}/" "${stageOutputKey}/" stageAboveKnown)
        if(knownAboveStage EQUAL 0 OR stageAboveKnown EQUAL 0)
            message(FATAL_ERROR
                "vase_stage_pack(${stageTarget}): 输出目录 ${stageOutput} 与既有 stage 目标输出 ${known} 重叠")
        endif()
    endforeach()
    set_property(GLOBAL APPEND PROPERTY VASE_STAGE_OUTPUTS "${stageOutput}")

    # 生成器 TU 的编译期映射（configure_file 的变量名与模板占位逐字对应）。
    list(LENGTH packMembers memberCount)
    set(VASE_STAGE_MEMBER_COUNT "${memberCount}")
    set(VASE_STAGE_DECLS "")
    set(memberSpecs "")
    set(stageManifests "")
    if(stageStatic)
        string(APPEND VASE_STAGE_DECLS
            "// NOLINTBEGIN(readability-identifier-naming) 成员描述符符号由 VASE_PLUGIN 定名，无改名出路。\n")
    endif()
    foreach(member IN LISTS packMembers)
        get_target_property(pluginClass ${member} VASE_PLUGIN_CLASS)
        get_target_property(memberManifest ${member} VASE_PLUGIN_MANIFEST)
        if(memberManifest)
            list(APPEND stageManifests "${memberManifest}")
            set(hasManifest "true")
        else()
            set(hasManifest "false")
        endif()
        if(stageStatic)
            string(APPEND VASE_STAGE_DECLS
                "extern \"C\" const ::vase::PluginDescriptor* VasePluginDesc_${pluginClass}();\n")
            list(APPEND memberSpecs
                "tools::cli::package_detail::InProcessStageMember(&VasePluginDesc_${pluginClass}, ${hasManifest})")
        else()
            list(APPEND memberSpecs
                "tools::cli::package_detail::DynamicStageMember(\"VasePluginDesc_${pluginClass}\", ${hasManifest})")
        endif()
    endforeach()
    if(stageStatic)
        string(APPEND VASE_STAGE_DECLS "// NOLINTEND(readability-identifier-naming)\n")
    endif()

    # 单成员表压单行，避免 trailing-comma 与嵌套初始化格式冲突。
    if(memberCount EQUAL 1)
        set(VASE_STAGE_MEMBERS "${memberSpecs}")
    else()
        set(VASE_STAGE_MEMBERS "")
        foreach(spec IN LISTS memberSpecs)
            string(APPEND VASE_STAGE_MEMBERS "\n    ${spec},")
        endforeach()
        string(APPEND VASE_STAGE_MEMBERS "\n")
    endif()

    # 静态：入口与包含面都取自包目标的属性（T3 生成头 + 成员描述符函数）；动态：只留类型。
    if(stageStatic)
        get_target_property(entriesHeader ${STAGE_PACK_TARGET} VASE_PACK_HEADER)
        get_target_property(entriesNamespace ${STAGE_PACK_TARGET} VASE_PACK_ENTRY_NAMESPACE)
        get_filename_component(entriesHeaderName "${entriesHeader}" NAME)
        set(VASE_STAGE_EXTRA_INCLUDES "#include \"${entriesHeaderName}\"")
        set(VASE_STAGE_ENTRIES "${entriesNamespace}::kEntryPoints")
        set(VASE_STAGE_KIND "vase::PackageSourceKind::kInProcess")
    else()
        set(VASE_STAGE_EXTRA_INCLUDES "")
        set(VASE_STAGE_ENTRIES "tools::cli::package_detail::kNoStageEntries")
        set(VASE_STAGE_KIND "vase::PackageSourceKind::kDynamic")
    endif()

    set(generatorTarget "${stageTarget}.Generator")
    configure_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/VasePackageGenerator.cpp.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${stageTarget}.Generator.cpp" @ONLY)
    add_executable(${generatorTarget} EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_BINARY_DIR}/${stageTarget}.Generator.cpp")
    target_link_libraries(${generatorTarget} PRIVATE
        VaseBuildOptions VaseCliCore VaseHost VaseCatalog)
    if(stageStatic)
        target_link_libraries(${generatorTarget} PRIVATE ${STAGE_PACK_TARGET})
    endif()

    set(stageArgs
        "--source-id" "${STAGE_SOURCE_ID}"
        "--output" "${stageOutput}"
        "--protected" "${PROJECT_SOURCE_DIR}"
        "--protected" "${CMAKE_BINARY_DIR}")
    if(NOT stageStatic)
        list(APPEND stageArgs "--binary" "$<TARGET_FILE:${STAGE_PACK_TARGET}>")
    endif()
    foreach(manifest IN LISTS stageManifests)
        list(APPEND stageArgs "--manifest" "${manifest}")
    endforeach()
    foreach(runtime IN LISTS STAGE_RUNTIME_FILES)
        list(APPEND stageArgs "--runtime" "${runtime}")
    endforeach()

    add_custom_target(${stageTarget}
        COMMAND "$<TARGET_FILE:${generatorTarget}>" ${stageArgs}
        VERBATIM
        DEPENDS ${generatorTarget} ${STAGE_PACK_TARGET} ${stageManifests} ${STAGE_RUNTIME_FILES}
        COMMENT "staging package ${STAGE_SOURCE_ID} into ${stageOutput}")
endfunction()
