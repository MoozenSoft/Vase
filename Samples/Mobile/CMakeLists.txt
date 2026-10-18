# 移动发布共用静态包定义；宿主生成候选包清单，目标侧只编静态包与资源名表。
if(VASE_BUILD_PROFILE STREQUAL "mobile")
    if(NOT DEFINED VASE_MANIFEST_HOST_BUILD_DIR OR VASE_MANIFEST_HOST_BUILD_DIR STREQUAL "")
        message(FATAL_ERROR "mobile: VASE_MANIFEST_HOST_BUILD_DIR must point to a native manifest-host tree")
    endif()
    get_filename_component(hostBuildDir "${VASE_MANIFEST_HOST_BUILD_DIR}" ABSOLUTE
        BASE_DIR "${PROJECT_SOURCE_DIR}")
    if(NOT EXISTS "${hostBuildDir}/CMakeCache.txt" OR
       NOT EXISTS "${hostBuildDir}/mobile-resources/host-profile.txt")
        message(FATAL_ERROR "mobile: host build ${hostBuildDir} has no manifest-host identity")
    endif()
    file(STRINGS "${hostBuildDir}/CMakeCache.txt" hostProfile
        REGEX "^VASE_BUILD_PROFILE:STRING=")
    file(STRINGS "${hostBuildDir}/mobile-resources/host-profile.txt" hostIdentityLines)
    if(NOT hostProfile STREQUAL "VASE_BUILD_PROFILE:STRING=manifest-host" OR
       NOT "profile=manifest-host" IN_LIST hostIdentityLines OR
       NOT "host-system=${CMAKE_HOST_SYSTEM_NAME}" IN_LIST hostIdentityLines OR
       NOT "host-processor=${CMAKE_HOST_SYSTEM_PROCESSOR}" IN_LIST hostIdentityLines)
        message(FATAL_ERROR "mobile: VASE_MANIFEST_HOST_BUILD_DIR is not a matching native host/architecture")
    endif()
    # Identity validation and the later custom commands must use the SAME
    # absolute directory. Relative -D paths otherwise resolve from the
    # mobile build tree rather than from PROJECT_SOURCE_DIR.
    set(VASE_MANIFEST_HOST_BUILD_DIR "${hostBuildDir}")
endif()

include("${PROJECT_SOURCE_DIR}/Samples/Mobile/Packages.cmake")
vase_define_mobile_packages()

vase_define_mobile_scenario()

if(VASE_BUILD_PROFILE STREQUAL "mobile" AND CMAKE_SYSTEM_NAME STREQUAL "Android")
    # 未入双包分发表的 archive：只连接，不显式引用，守住链接器按需提取的反证。
    add_subdirectory("${PROJECT_SOURCE_DIR}/Tests/StaticLink/fixtures/KeepAliveProbe"
        "${CMAKE_CURRENT_BINARY_DIR}/fixtures/KeepAliveProbe")
    add_library(VaseMobileJni SHARED Android/JniBridge.cpp)
    target_include_directories(VaseMobileJni PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}" ${VASE_MOBILE_RESOURCE_HEADER_DIRS})
    target_link_libraries(VaseMobileJni PRIVATE
        VaseBuildOptions VaseMobileScenarioCore KeepAliveProbe.Pack android)
    set_target_properties(VaseMobileJni PROPERTIES
        CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)

    get_filename_component(ndkCompilerDirectory "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(androidSharedStl
        "${ndkCompilerDirectory}/../sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" ABSOLUTE)
    if(NOT EXISTS "${androidSharedStl}")
        message(FATAL_ERROR "mobile Android: libc++_shared.so not found in NDK: ${androidSharedStl}")
    endif()
    set(mobileSourceIds "")
    foreach(package IN LISTS VASE_MOBILE_PACKAGE_TARGETS)
        get_target_property(sourceId ${package} VASE_MOBILE_SOURCE_ID)
        list(APPEND mobileSourceIds "${sourceId}")
    endforeach()
    list(JOIN mobileSourceIds "|" mobileSourceNames)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    find_program(androidLlvmNm NAMES llvm-nm llvm-nm.exe
        PATHS "${ndkCompilerDirectory}" NO_DEFAULT_PATH REQUIRED)
    find_program(androidLlvmReadobj NAMES llvm-readobj llvm-readobj.exe
        PATHS "${ndkCompilerDirectory}" NO_DEFAULT_PATH REQUIRED)
    add_custom_target(VaseVerifyMobileSymbols
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Scripts/VerifyMobileSymbols.py"
            "--elf=$<TARGET_FILE:VaseMobileJni>"
            "--llvm-nm=${androidLlvmNm}"
            "--llvm-readobj=${androidLlvmReadobj}"
        DEPENDS VaseMobileJni VERBATIM)
    # Restore any interrupted prior publication BEFORE invoking the host
    # candidate generator. A failing candidate must not strand old APK inputs.
    add_custom_target(VaseMobileApkRecover
        COMMAND "${CMAKE_COMMAND}"
            "-DVASE_MOBILE_OUTPUT=${CMAKE_BINARY_DIR}/apk-inputs"
            -DVASE_MOBILE_RECOVER_ONLY=ON
            -P "${PROJECT_SOURCE_DIR}/Cmake/StageMobileApkInputs.cmake"
        VERBATIM)
    add_dependencies(VaseMobileJni VaseMobileApkRecover)
    add_custom_target(VaseMobileApkInputs
        COMMAND "${CMAKE_COMMAND}" --build "${VASE_MANIFEST_HOST_BUILD_DIR}"
            --target VaseMobileCandidateStage
        COMMAND "${CMAKE_COMMAND}"
            "-DVASE_MOBILE_JNI=$<TARGET_FILE:VaseMobileJni>"
            "-DVASE_MOBILE_STL=${androidSharedStl}"
            "-DVASE_MOBILE_HOST=${VASE_MANIFEST_HOST_BUILD_DIR}"
            "-DVASE_MOBILE_OUTPUT=${CMAKE_BINARY_DIR}/apk-inputs"
            "-DVASE_MOBILE_SOURCES=${mobileSourceNames}"
            -P "${PROJECT_SOURCE_DIR}/Cmake/StageMobileApkInputs.cmake"
        DEPENDS VaseMobileJni VaseVerifyMobileSymbols
        VERBATIM)
endif()

if(VASE_BUILD_PROFILE STREQUAL "mobile" AND CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # 真 iPhoneOS app 链接证人：实际调用 bundle 读取与 Scenario，不在桌面运行。
    add_subdirectory("${PROJECT_SOURCE_DIR}/Tests/StaticLink/fixtures/KeepAliveProbe"
        "${CMAKE_CURRENT_BINARY_DIR}/fixtures/KeepAliveProbe")
    add_executable(VaseMobileIosLinkProbe MACOSX_BUNDLE Ios/main.cpp Ios/BundleResources.cpp)
    target_include_directories(VaseMobileIosLinkProbe PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}" ${VASE_MOBILE_RESOURCE_HEADER_DIRS})
    target_link_libraries(VaseMobileIosLinkProbe PRIVATE
        VaseBuildOptions VaseMobileScenarioCore KeepAliveProbe.Pack "-framework CoreFoundation")
    set(iosSourceIds "")
    foreach(package IN LISTS VASE_MOBILE_PACKAGE_TARGETS)
        get_target_property(sourceId ${package} VASE_MOBILE_SOURCE_ID)
        list(APPEND iosSourceIds "${sourceId}")
    endforeach()
    list(JOIN iosSourceIds "|" iosSourceNames)
    # Bundle rollback recovery precedes all host-generation dependencies.
    add_custom_target(VaseMobileIosRecover
        COMMAND "${CMAKE_COMMAND}"
            "-DVASE_IOS_APP_BUNDLE=$<TARGET_BUNDLE_DIR:VaseMobileIosLinkProbe>"
            -DVASE_IOS_RECOVER_ONLY=ON
            -P "${PROJECT_SOURCE_DIR}/Cmake/StageIosBundleResources.cmake"
        VERBATIM)
    add_dependencies(VaseMobileIosLinkProbe VaseMobileIosRecover)
    add_custom_target(VaseMobileIosBundleAssets
        COMMAND "${CMAKE_COMMAND}" --build "${VASE_MANIFEST_HOST_BUILD_DIR}"
            --target VaseMobileCandidateStage
        COMMAND "${CMAKE_COMMAND}"
            "-DVASE_IOS_HOST_BUILD=${VASE_MANIFEST_HOST_BUILD_DIR}"
            "-DVASE_IOS_APP_BUNDLE=$<TARGET_BUNDLE_DIR:VaseMobileIosLinkProbe>"
            "-DVASE_IOS_SOURCE_IDS=${iosSourceNames}"
            -P "${PROJECT_SOURCE_DIR}/Cmake/StageIosBundleResources.cmake"
        DEPENDS VaseMobileIosLinkProbe
        VERBATIM)
endif()

if(VASE_BUILD_PROFILE STREQUAL "manifest-host")
    # 只在本机 profile 编译描述符生成器；移动目标不可执行该生成器。
    add_subdirectory("${PROJECT_SOURCE_DIR}/Tools/VaseCli" "${CMAKE_CURRENT_BINARY_DIR}/VaseCli")

    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/mobile-resources")
    file(WRITE "${CMAKE_BINARY_DIR}/mobile-resources/host-profile.txt"
        "profile=manifest-host\nhost-system=${CMAKE_HOST_SYSTEM_NAME}\nhost-processor=${CMAKE_HOST_SYSTEM_PROCESSOR}\n")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stage-policy-provider")
    configure_file("${PROJECT_SOURCE_DIR}/Tests/Package/fixtures/Provider/plugin.json"
        "${CMAKE_CURRENT_BINARY_DIR}/stage-policy-provider/plugin.json" COPYONLY)
    set_target_properties(PackageProviderPlugin PROPERTIES
        VASE_PLUGIN_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/stage-policy-provider/plugin.json")

    foreach(package IN LISTS VASE_MOBILE_PACKAGE_TARGETS)
        get_target_property(sourceId ${package} VASE_MOBILE_SOURCE_ID)
        if(NOT sourceId)
            message(FATAL_ERROR "mobile manifest-host: missing source id on ${package}")
        endif()
        vase_stage_pack(VaseMobileStage${sourceId}
            PACK_TARGET ${package} SOURCE_ID ${sourceId}
            OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/mobile-resources/vase/${sourceId}")
        list(APPEND stageTargets VaseMobileStage${sourceId})
    endforeach()
    add_custom_target(VaseMobileCandidateStage DEPENDS ${stageTargets})

    add_executable(VaseMobileManifestVerifier
        "${PROJECT_SOURCE_DIR}/Tests/Build/VerifyMobileManifests.cpp")
    target_include_directories(VaseMobileManifestVerifier PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}" ${VASE_MOBILE_RESOURCE_HEADER_DIRS})
    target_link_libraries(VaseMobileManifestVerifier PRIVATE
        VaseBuildOptions VaseCatalog VaseHost ${VASE_MOBILE_PACKAGE_TARGETS})
    add_custom_target(VaseCheckMobileManifests
        COMMAND "$<TARGET_FILE:VaseMobileManifestVerifier>" "${CMAKE_BINARY_DIR}/mobile-resources/vase"
        DEPENDS VaseMobileCandidateStage VaseMobileManifestVerifier
        VERBATIM)
endif()

# Task 3 留下的真实 STATIC 框架烟测，宿主 profile 仍可独立构建。
if(VASE_BUILD_PROFILE STREQUAL "manifest-host")
    add_executable(VaseStaticFrameworkSmoke
        "${PROJECT_SOURCE_DIR}/Tests/Build/StaticFrameworkSmoke.cpp")
    target_link_libraries(VaseStaticFrameworkSmoke PRIVATE VaseBuildOptions VaseCatalog)
    add_custom_target(VaseCheckStaticFramework
        COMMAND $<TARGET_FILE:VaseStaticFrameworkSmoke>
        DEPENDS VaseStaticFrameworkSmoke
        VERBATIM)
endif()
