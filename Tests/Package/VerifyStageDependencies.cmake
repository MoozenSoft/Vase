# T11 构建级证人（spec §3.1 / A17）：真的跑 stage 目标，依次证明
#   ① 普通 build 不执行 staging（目标不在 ALL 里）→ 显式构建才有产物；
#   ② 只改成员源码清单的启用策略 → 只重跑目标即重新生成，输出随策略变；
#   ③ 手改 staging 里的 pack.json 不成为下一次生成的输入 → 重跑后源策略覆盖它；
#   ④ 缺一个 runtime 输入时 staging 失败且原包目录一个字节不动；
#   ⑤ 动态包的生成器不链接包也能生成并装配（清单与库文件同处包目录）。
# 脚本只改自己的策略副本（构建树），每个失败分支先把它从源文件恢复再报错。
#
# cmake -P 调用；入参由 Tests/Package/CMakeLists.txt 经 -D 传入。

function(vase_restore_policy)
    file(READ "${VASE_POLICY_SOURCE}" vaseOriginalPolicy)
    file(WRITE "${VASE_POLICY_COPY}" "${vaseOriginalPolicy}")
endfunction()

# 只允许动构建树里的路径：staging 与 runtime 副本都不该指到源树去。
function(vase_assert_under_build target_path)
    string(FIND "${target_path}" "${VASE_BUILD_DIR}" vaseIndex)
    if(NOT vaseIndex EQUAL 0)
        vase_restore_policy()
        message(FATAL_ERROR "${target_path} 不在构建树 ${VASE_BUILD_DIR} 内，拒绝删除或改名")
    endif()
endfunction()

function(vase_run_stage target out_var)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${VASE_BUILD_DIR}" --target "${target}"
        RESULT_VARIABLE vaseBuildRc OUTPUT_VARIABLE vaseBuildOut ERROR_VARIABLE vaseBuildErr)
    set(${out_var} "${vaseBuildRc}" PARENT_SCOPE)
    if(NOT vaseBuildRc EQUAL 0)
        message(STATUS "  [${target}] build rc=${vaseBuildRc}")
        message(STATUS "  ${vaseBuildErr}")
    endif()
endfunction()

function(vase_expect_enabled manifest_path plugin_id expected label)
    execute_process(
        COMMAND "${VASE_VERIFIER}" "${manifest_path}" "${plugin_id}" "${expected}"
        RESULT_VARIABLE vaseVerifyRc OUTPUT_VARIABLE vaseVerifyOut ERROR_VARIABLE vaseVerifyErr)
    if(NOT vaseVerifyRc EQUAL 0)
        vase_restore_policy()
        message(FATAL_ERROR "${label}: 校验失败 rc=${vaseVerifyRc} ${vaseVerifyOut} ${vaseVerifyErr}")
    endif()
    message(STATUS "  ${label}: ${plugin_id} enabledByDefault=${expected} OK")
endfunction()

macro(vase_fail text)
    vase_restore_policy()
    message(FATAL_ERROR "${text}")
endmacro()

set(vaseStagedManifest "${VASE_STAGE_OUTPUT}/pack.json")

# ① 普通 build 不执行 staging：清掉上一轮残留后跑默认目标，包目录必须仍不存在。
vase_assert_under_build("${VASE_STAGE_OUTPUT}")
file(REMOVE_RECURSE "${VASE_STAGE_OUTPUT}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${VASE_BUILD_DIR}"
    RESULT_VARIABLE vaseDefaultRc OUTPUT_VARIABLE vaseDefaultOut ERROR_VARIABLE vaseDefaultErr)
if(NOT vaseDefaultRc EQUAL 0)
    vase_fail("默认 build 失败 rc=${vaseDefaultRc} ${vaseDefaultErr}")
endif()
if(EXISTS "${VASE_STAGE_OUTPUT}")
    vase_fail("普通 build 不该执行 staging，但 ${VASE_STAGE_OUTPUT} 已存在")
endif()
message(STATUS "普通 build 未生成 staging 目录 OK")

# ② 首次显式构建 → 源策略 enabledByDefault=false 落进包清单。
vase_run_stage("${VASE_STAGE_TARGET}" vaseFirstRc)
if(NOT vaseFirstRc EQUAL 0)
    vase_fail("首次显式构建 ${VASE_STAGE_TARGET} 失败 rc=${vaseFirstRc}")
endif()
vase_expect_enabled("${vaseStagedManifest}" "${VASE_PLUGIN_ID}" "false" "首次 staging")

# ③ 只改源码策略副本（已知字节替换）→ 只重跑该目标即重生成。
file(READ "${VASE_POLICY_COPY}" vasePolicyText)
string(REPLACE "\"enabledByDefault\": false" "\"enabledByDefault\": true" vasePolicyText "${vasePolicyText}")
file(WRITE "${VASE_POLICY_COPY}" "${vasePolicyText}")
file(READ "${VASE_POLICY_COPY}" vasePolicyCheck)
string(FIND "${vasePolicyCheck}" "\"enabledByDefault\": true" vasePolicyIndex)
if(vasePolicyIndex EQUAL -1)
    vase_fail("已知字节替换没改到策略副本：${VASE_POLICY_COPY}")
endif()
vase_run_stage("${VASE_STAGE_TARGET}" vasePolicyRc)
if(NOT vasePolicyRc EQUAL 0)
    vase_fail("改策略后重跑 ${VASE_STAGE_TARGET} 失败 rc=${vasePolicyRc}")
endif()
vase_expect_enabled("${vaseStagedManifest}" "${VASE_PLUGIN_ID}" "true" "改策略后重跑")

# ④ 手改 staging 输出：先证明手改真的落到了盘上，再重跑证明源策略覆盖它（staging 不是输入）。
# 往成员条目里插一个 enabledByDefault:false（写出侧只在 false 时写该键，故这里是插入不是替换）。
file(READ "${vaseStagedManifest}" vaseStagedText)
string(REPLACE "\"id\": \"${VASE_PLUGIN_ID}\"" "\"id\": \"${VASE_PLUGIN_ID}\", \"enabledByDefault\": false"
    vaseStagedText "${vaseStagedText}")
file(WRITE "${vaseStagedManifest}" "${vaseStagedText}")
vase_expect_enabled("${vaseStagedManifest}" "${VASE_PLUGIN_ID}" "false" "手改 staging")
vase_run_stage("${VASE_STAGE_TARGET}" vaseHandEditRc)
if(NOT vaseHandEditRc EQUAL 0)
    vase_fail("手改 staging 后重跑 ${VASE_STAGE_TARGET} 失败 rc=${vaseHandEditRc}")
endif()
vase_expect_enabled("${vaseStagedManifest}" "${VASE_PLUGIN_ID}" "true" "手改后重跑（源策略恢复）")

# ⑤ 缺 runtime 输入：目标失败，且原包目录内容不动（用测试自有的副本，不碰框架库）。
vase_assert_under_build("${VASE_RUNTIME_INPUT}")
file(RENAME "${VASE_RUNTIME_INPUT}" "${VASE_RUNTIME_INPUT}.away")
vase_run_stage("${VASE_STAGE_TARGET}" vaseMissingRc)
file(RENAME "${VASE_RUNTIME_INPUT}.away" "${VASE_RUNTIME_INPUT}")
if(vaseMissingRc EQUAL 0)
    vase_fail("缺 runtime 输入时 staging 必须失败，实际 rc=0")
endif()
vase_expect_enabled("${vaseStagedManifest}" "${VASE_PLUGIN_ID}" "true" "缺 runtime 输入后原目录保持")
message(STATUS "缺 runtime 输入时目标失败且原包目录未动 OK")

# ⑥ 动态包：生成器不链接包，按路径装载读描述符，清单与库文件同处包目录。
vase_assert_under_build("${VASE_DYNAMIC_STAGE_OUTPUT}")
file(REMOVE_RECURSE "${VASE_DYNAMIC_STAGE_OUTPUT}")
vase_run_stage("${VASE_DYNAMIC_STAGE_TARGET}" vaseDynamicRc)
if(NOT vaseDynamicRc EQUAL 0)
    vase_fail("动态 stage 目标 ${VASE_DYNAMIC_STAGE_TARGET} 失败 rc=${vaseDynamicRc}")
endif()
if(NOT EXISTS "${VASE_DYNAMIC_STAGE_OUTPUT}/${VASE_DYNAMIC_LIBRARY}")
    vase_fail("动态包目录缺少库文件 ${VASE_DYNAMIC_LIBRARY}")
endif()
vase_expect_enabled("${VASE_DYNAMIC_STAGE_OUTPUT}/pack.json" "${VASE_DYNAMIC_PLUGIN_ID}" "true" "动态 staging")

vase_restore_policy()
message(STATUS "VasePackageStageDependencies OK")
