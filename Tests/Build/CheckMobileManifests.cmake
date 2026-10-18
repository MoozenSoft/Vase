# 原生 manifest-host 真实构建证人：同一 Catalog 读取候选 pack.json，
# 并证明新生成失败不会让旧清单蒙混过关。
foreach(arg VASE_SOURCE_DIR VASE_BINARY_DIR VASE_GENERATOR VASE_TOOLCHAIN_FILE VASE_BUILD_TYPE)
    if(NOT DEFINED ${arg} OR "${${arg}}" STREQUAL "")
        message(FATAL_ERROR "CheckMobileManifests: missing ${arg}")
    endif()
endforeach()

string(TIMESTAMP token "%Y%m%d%H%M%S")
set(hostTree "${VASE_BINARY_DIR}/mobile-manifests/${token}")
set(configureArgs
    "${CMAKE_COMMAND}" -S "${VASE_SOURCE_DIR}" -B "${hostTree}" -G "${VASE_GENERATOR}"
    "-DCMAKE_TOOLCHAIN_FILE=${VASE_TOOLCHAIN_FILE}"
    "-DCMAKE_BUILD_TYPE=${VASE_BUILD_TYPE}"
    "-DVASE_BUILD_PROFILE=manifest-host"
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

function(require_command_ok label)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
    file(WRITE "${hostTree}/${label}.log" "${stdout}\n${stderr}\nRC=${result}\n")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${label}: RC=${result}\n${stdout}\n${stderr}")
    endif()
endfunction()

require_command_ok(configure ${configureArgs})
require_command_ok(initial-stage "${CMAKE_COMMAND}" --build "${hostTree}" --target VaseCheckMobileManifests)

set(providerManifest "${hostTree}/mobile-resources/vase/Provider/pack.json")
set(consumerManifest "${hostTree}/mobile-resources/vase/Consumer/pack.json")
set(policyCopy "${hostTree}/Samples/Mobile/stage-policy-provider/plugin.json")
foreach(path IN ITEMS "${providerManifest}" "${consumerManifest}" "${policyCopy}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "initial candidate output missing: ${path}")
    endif()
endforeach()
set(policyBackup "${hostTree}/stage-policy-provider.backup.json")
file(COPY_FILE "${policyCopy}" "${policyBackup}")
file(SHA256 "${policyCopy}" originalPolicyHash)
file(SHA256 "${providerManifest}" previousHash)

# 只破坏构建树副本；即使已有上一轮输出，stage 构建也必须因本轮生成失败而非零。
file(WRITE "${policyCopy}" "{ invalid json for negative witness")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${hostTree}" --target VaseMobileCandidateStage
    RESULT_VARIABLE invalidResult OUTPUT_VARIABLE invalidOutput ERROR_VARIABLE invalidError)
file(WRITE "${hostTree}/invalid-policy.log"
    "${invalidOutput}\n${invalidError}\nRC=${invalidResult}\n")
# 先无条件还原，失败判据再作决定，绝不残留测试损坏的策略文件。
file(COPY_FILE "${policyBackup}" "${policyCopy}")
if(NOT EXISTS "${providerManifest}")
    message(FATAL_ERROR "invalid stage removed the preceding candidate manifest")
endif()
file(SHA256 "${providerManifest}" preservedHash)
file(SHA256 "${policyCopy}" restoredPolicyHash)
string(FIND "${invalidOutput}\n${invalidError}" "malformed JSON" malformedIndex)
file(WRITE "${hostTree}/policy-restoration.txt"
    "policy-before=${originalPolicyHash}\npolicy-after=${restoredPolicyHash}\n"
    "manifest-before=${previousHash}\nmanifest-after=${preservedHash}\n"
    "bad-policy-stage-RC=${invalidResult}\nmalformed-json-index=${malformedIndex}\n")
file(REMOVE "${policyBackup}")
if(invalidResult EQUAL 0 OR malformedIndex EQUAL -1 OR
   NOT "${preservedHash}" STREQUAL "${previousHash}" OR
   NOT "${restoredPolicyHash}" STREQUAL "${originalPolicyHash}")
    message(FATAL_ERROR
        "invalid-policy witness failed: RC=${invalidResult}, malformedIndex=${malformedIndex}, manifest ${previousHash}->${preservedHash}, policy ${originalPolicyHash}->${restoredPolicyHash}")
endif()

require_command_ok(restored-stage "${CMAKE_COMMAND}" --build "${hostTree}" --target VaseCheckMobileManifests)
message(STATUS "MobileManifests: candidate checked, bad-policy stage rejected despite old output, restored")
