# No Android/iPhoneOS SDK required: exercise the actual packaging scripts.
foreach(required IN ITEMS VASE_SOURCE_DIR VASE_BINARY_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "MobileResourceStaging: missing ${required}")
    endif()
endforeach()
set(root "${VASE_BINARY_DIR}/mobile-resource-staging-check")
# The macOS build tree may live on an SMB mount. Exercise directory-rename
# rollback on the host's native filesystem to avoid SMB metadata-cache races.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND DEFINED ENV{TMPDIR} AND
   NOT "$ENV{TMPDIR}" STREQUAL "")
    string(SHA256 fixtureId "${VASE_BINARY_DIR}")
    string(SUBSTRING "${fixtureId}" 0 12 fixtureId)
    set(root "$ENV{TMPDIR}/vase-mobile-staging-${fixtureId}")
endif()
file(REMOVE_RECURSE "${root}")
set(host "${root}/host")
set(apk "${root}/apk")
set(app "${root}/Probe.app")
file(MAKE_DIRECTORY "${host}/mobile-resources/vase/Provider"
    "${host}/mobile-resources/vase/Consumer" "${apk}/assets" "${app}")
file(WRITE "${root}/jni.so" "jni")
file(WRITE "${root}/stl.so" "stl")
file(WRITE "${host}/mobile-resources/vase/Provider/pack.json" "{\"id\":\"Provider\"}")
file(WRITE "${host}/mobile-resources/vase/Consumer/pack.json" "{\"id\":\"Consumer\"}")
file(WRITE "${apk}/assets/unrelated.txt" "do not remove")
file(WRITE "${app}/unrelated.txt" "do not remove")

set(apkArgs "-DVASE_MOBILE_JNI=${root}/jni.so"
    "-DVASE_MOBILE_STL=${root}/stl.so"
    "-DVASE_MOBILE_HOST=${host}" "-DVASE_MOBILE_OUTPUT=${apk}")
set(iosArgs "-DVASE_IOS_HOST_BUILD=${host}" "-DVASE_IOS_APP_BUNDLE=${app}")

function(run_stage label expectedRc)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE rc
        OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
    if(expectedRc STREQUAL "success" AND NOT rc EQUAL 0)
        message(FATAL_ERROR "${label}: unexpectedly failed (${rc})\n${stdout}\n${stderr}")
    elseif(expectedRc STREQUAL "failure" AND rc EQUAL 0)
        message(FATAL_ERROR "${label}: unexpectedly passed\n${stdout}\n${stderr}")
    endif()
endfunction()

run_stage(apk-initial success "${CMAKE_COMMAND}" ${apkArgs}
    "-DVASE_MOBILE_SOURCES=Provider|Consumer"
    -P "${VASE_SOURCE_DIR}/Cmake/StageMobileApkInputs.cmake")
run_stage(ios-initial success "${CMAKE_COMMAND}" ${iosArgs}
    "-DVASE_IOS_SOURCE_IDS=Provider|Consumer"
    -P "${VASE_SOURCE_DIR}/Cmake/StageIosBundleResources.cmake")
foreach(path IN ITEMS
    "${apk}/assets/vase/Consumer/pack.json" "${app}/vase/Consumer/pack.json")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "initial selected Consumer missing: ${path}")
    endif()
endforeach()

run_stage(apk-reselect success "${CMAKE_COMMAND}" ${apkArgs}
    "-DVASE_MOBILE_SOURCES=Provider"
    -P "${VASE_SOURCE_DIR}/Cmake/StageMobileApkInputs.cmake")
run_stage(ios-reselect success "${CMAKE_COMMAND}" ${iosArgs}
    "-DVASE_IOS_SOURCE_IDS=Provider"
    -P "${VASE_SOURCE_DIR}/Cmake/StageIosBundleResources.cmake")
foreach(path IN ITEMS
    "${apk}/assets/vase/Consumer/pack.json" "${app}/vase/Consumer/pack.json"
    "${apk}/assets/vase/Consumer" "${app}/vase/Consumer")
    if(EXISTS "${path}")
        message(FATAL_ERROR "unselected resource remains after reselect: ${path}")
    endif()
endforeach()
foreach(path IN ITEMS
    "${apk}/assets/vase/Provider/pack.json" "${app}/vase/Provider/pack.json"
    "${apk}/assets/unrelated.txt" "${app}/unrelated.txt")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "selected/unrelated resource was removed: ${path}")
    endif()
endforeach()
file(SHA256 "${apk}/assets/vase/Provider/pack.json" oldApkHash)
file(SHA256 "${app}/vase/Provider/pack.json" oldIosHash)

# Both scripts must fail on missing candidate and preserve the prior release.
file(REMOVE "${host}/mobile-resources/vase/Provider/pack.json")
run_stage(apk-missing failure "${CMAKE_COMMAND}" ${apkArgs}
    "-DVASE_MOBILE_SOURCES=Provider"
    -P "${VASE_SOURCE_DIR}/Cmake/StageMobileApkInputs.cmake")
run_stage(ios-missing failure "${CMAKE_COMMAND}" ${iosArgs}
    "-DVASE_IOS_SOURCE_IDS=Provider"
    -P "${VASE_SOURCE_DIR}/Cmake/StageIosBundleResources.cmake")
file(SHA256 "${apk}/assets/vase/Provider/pack.json" retainedApkHash)
file(SHA256 "${app}/vase/Provider/pack.json" retainedIosHash)
if(NOT oldApkHash STREQUAL retainedApkHash OR NOT oldIosHash STREQUAL retainedIosHash)
    message(FATAL_ERROR "rejected staging replaced the last valid resource set")
endif()

# Simulate a process killed between moving the last valid resources into the
# backup and installing the new set. A missing candidate on the NEXT invocation
# must still restore the old bundle/APK inputs *before* rejecting that build.
file(RENAME "${apk}/assets/vase" "${apk}/assets/vase.vase-previous")
file(RENAME "${apk}/jniLibs/arm64-v8a" "${apk}/jniLibs/arm64-v8a.vase-previous")
file(RENAME "${app}/vase" "${app}/.vase-resources-previous")
run_stage(apk-interrupted-missing failure "${CMAKE_COMMAND}" ${apkArgs}
    "-DVASE_MOBILE_SOURCES=Provider"
    -P "${VASE_SOURCE_DIR}/Cmake/StageMobileApkInputs.cmake")
run_stage(ios-interrupted-missing failure "${CMAKE_COMMAND}" ${iosArgs}
    "-DVASE_IOS_SOURCE_IDS=Provider"
    -P "${VASE_SOURCE_DIR}/Cmake/StageIosBundleResources.cmake")
foreach(path IN ITEMS
    "${apk}/assets/vase/Provider/pack.json"
    "${apk}/jniLibs/arm64-v8a/libVaseMobileJni.so"
    "${apk}/jniLibs/arm64-v8a/libc++_shared.so"
    "${app}/vase/Provider/pack.json")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "interrupted release was not recovered before rejecting bad inputs: ${path}")
    endif()
endforeach()
foreach(path IN ITEMS
    "${apk}/assets/vase.vase-previous"
    "${apk}/jniLibs/arm64-v8a.vase-previous"
    "${app}/.vase-resources-previous")
    if(EXISTS "${path}")
        message(FATAL_ERROR "interrupted release backup remains after recovery: ${path}")
    endif()
endforeach()
file(SHA256 "${apk}/assets/vase/Provider/pack.json" interruptedApkHash)
file(SHA256 "${app}/vase/Provider/pack.json" interruptedIosHash)
if(NOT oldApkHash STREQUAL interruptedApkHash OR NOT oldIosHash STREQUAL interruptedIosHash)
    message(FATAL_ERROR "recovered interrupted release did not preserve the last valid resource bytes")
endif()

# A duplicate SourceId must never be published.
file(WRITE "${host}/mobile-resources/vase/Provider/pack.json" "{\"id\":\"Provider\"}")
run_stage(apk-duplicate failure "${CMAKE_COMMAND}" ${apkArgs}
    "-DVASE_MOBILE_SOURCES=Provider|Provider"
    -P "${VASE_SOURCE_DIR}/Cmake/StageMobileApkInputs.cmake")
run_stage(ios-duplicate failure "${CMAKE_COMMAND}" ${iosArgs}
    "-DVASE_IOS_SOURCE_IDS=Provider|Provider"
    -P "${VASE_SOURCE_DIR}/Cmake/StageIosBundleResources.cmake")
# A publish that was committed before backup cleanup must never roll back
# only the JNI half when interrupted cleanup left a partial single backup.
file(WRITE "${apk}/assets/vase/Provider/pack.json" "committed-assets")
file(WRITE "${apk}/jniLibs/arm64-v8a/libVaseMobileJni.so" "committed-jni")
file(WRITE "${apk}/jniLibs/arm64-v8a/libc++_shared.so" "committed-stl")
file(MAKE_DIRECTORY "${apk}/jniLibs/arm64-v8a.vase-previous")
file(WRITE "${apk}/jniLibs/arm64-v8a.vase-previous/libVaseMobileJni.so" "partial-old-jni")
file(WRITE "${apk}/.vase-publish-phase" "committed|1|1\n")
file(REMOVE "${host}/mobile-resources/vase/Provider/pack.json")
run_stage(apk-partial-cleanup-missing failure "${CMAKE_COMMAND}" ${apkArgs}
    "-DVASE_MOBILE_SOURCES=Provider"
    -P "${VASE_SOURCE_DIR}/Cmake/StageMobileApkInputs.cmake")
file(READ "${apk}/assets/vase/Provider/pack.json" committedAssets)
file(READ "${apk}/jniLibs/arm64-v8a/libVaseMobileJni.so" committedJni)
file(READ "${apk}/jniLibs/arm64-v8a/libc++_shared.so" committedStl)
if(NOT committedAssets STREQUAL "committed-assets" OR
   NOT committedJni STREQUAL "committed-jni" OR
   NOT committedStl STREQUAL "committed-stl" OR
   EXISTS "${apk}/jniLibs/arm64-v8a.vase-previous" OR
   EXISTS "${apk}/.vase-publish-phase")
    message(FATAL_ERROR "committed cleanup recovery mixed old and new generations")
endif()

# A pre-commit interruption, by contrast, must undo newly installed assets
# while leaving the original JNI output untouched if JNI was never moved.
file(RENAME "${apk}/assets/vase" "${apk}/assets/vase.vase-previous")
file(MAKE_DIRECTORY "${apk}/assets/vase/Provider")
file(WRITE "${apk}/assets/vase/Provider/pack.json" "uncommitted-assets")
file(WRITE "${apk}/.vase-publish-phase" "rollback|1|1\n")
run_stage(apk-rollback-before-candidate-missing success "${CMAKE_COMMAND}"
    "-DVASE_MOBILE_OUTPUT=${apk}" -DVASE_MOBILE_RECOVER_ONLY=ON
    -P "${VASE_SOURCE_DIR}/Cmake/StageMobileApkInputs.cmake")
file(READ "${apk}/assets/vase/Provider/pack.json" recoveredAssets)
file(READ "${apk}/jniLibs/arm64-v8a/libVaseMobileJni.so" recoveredJni)
if(NOT recoveredAssets STREQUAL "committed-assets" OR
   NOT recoveredJni STREQUAL "committed-jni" OR
   EXISTS "${apk}/.vase-publish-phase")
    message(FATAL_ERROR "pre-candidate recovery failed or mixed generations")
endif()
# Candidate failure is still a hard error after restoration.
run_stage(apk-recovered-missing failure "${CMAKE_COMMAND}" ${apkArgs}
    "-DVASE_MOBILE_SOURCES=Provider"
    -P "${VASE_SOURCE_DIR}/Cmake/StageMobileApkInputs.cmake")
file(WRITE "${host}/mobile-resources/vase/Provider/pack.json" "{\"id\":\"Provider\"}")

# Legacy (markerless) interrupted cleanup is ambiguous when both active
# directories coexist with a sole partial JNI backup. Fail closed instead of
# restoring part of an old generation and reporting success.
file(MAKE_DIRECTORY "${apk}/jniLibs/arm64-v8a.vase-previous")
file(WRITE "${apk}/jniLibs/arm64-v8a.vase-previous/libVaseMobileJni.so" "truncated-old")
run_stage(apk-ambiguous-legacy failure "${CMAKE_COMMAND}"
    "-DVASE_MOBILE_OUTPUT=${apk}" -DVASE_MOBILE_RECOVER_ONLY=ON
    -P "${VASE_SOURCE_DIR}/Cmake/StageMobileApkInputs.cmake")
file(READ "${apk}/jniLibs/arm64-v8a/libVaseMobileJni.so" afterAmbiguousLegacy)
if(NOT afterAmbiguousLegacy STREQUAL "committed-jni" OR
   NOT EXISTS "${apk}/jniLibs/arm64-v8a.vase-previous")
    message(FATAL_ERROR "ambiguous markerless backup was unexpectedly installed or discarded")
endif()
file(REMOVE_RECURSE "${apk}/jniLibs/arm64-v8a.vase-previous")

# Linux fixture: second directory publication fails after assets have already
# been installed. The first publication must be rolled back to OLD, not left
# paired with old or missing JNI artifacts.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(failedApk "${root}/second-publish-failure")
    file(MAKE_DIRECTORY "${failedApk}/assets/vase/Provider")
    file(WRITE "${failedApk}/assets/vase/Provider/pack.json" "old-marker")
    file(CREATE_LINK "/proc" "${failedApk}/jniLibs" SYMBOLIC RESULT linkResult)
    if(NOT linkResult STREQUAL "0")
        message(FATAL_ERROR "Cannot prepare second-publish failure fixture: ${linkResult}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DVASE_MOBILE_JNI=${root}/jni.so"
        "-DVASE_MOBILE_STL=${root}/stl.so"
        "-DVASE_MOBILE_HOST=${host}"
        "-DVASE_MOBILE_OUTPUT=${failedApk}"
        "-DVASE_MOBILE_SOURCES=Provider"
        -P "${VASE_SOURCE_DIR}/Cmake/StageMobileApkInputs.cmake"
        RESULT_VARIABLE secondPublishRc
        OUTPUT_VARIABLE secondPublishOut ERROR_VARIABLE secondPublishErr)
    if(secondPublishRc EQUAL 0 OR
       NOT "${secondPublishOut}\n${secondPublishErr}" MATCHES "restored previous assets")
        message(FATAL_ERROR "Second-directory publish failure was not handled: ${secondPublishRc}\n${secondPublishOut}\n${secondPublishErr}")
    endif()
    file(READ "${failedApk}/assets/vase/Provider/pack.json" afterFailure)
    if(NOT afterFailure STREQUAL "old-marker" OR
       EXISTS "${failedApk}/assets/vase.vase-previous")
        message(FATAL_ERROR "Second-directory publish failure did not roll back existing assets")
    endif()
    file(REMOVE "${failedApk}/jniLibs")
endif()
message(STATUS "MobileResourceStaging: stale members removed, unowned resources retained, bad inputs and half-publish rejected")
file(REMOVE_RECURSE "${root}")
