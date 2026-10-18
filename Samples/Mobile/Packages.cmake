# Provider/Consumer 定义的唯一入口：桌面测试与宿主生成器共用这份成员/包形状。
# 调用目录控制构建输出：桌面保持原有 Tests/Package/fixtures 与归档落位。
function(vase_define_mobile_packages)
    if(TARGET PackageServices)
        message(FATAL_ERROR "vase_define_mobile_packages: already defined")
    endif()

    add_library(PackageServices INTERFACE)
    target_link_libraries(PackageServices INTERFACE VaseBuildOptions)
    target_include_directories(PackageServices INTERFACE
        "${PROJECT_SOURCE_DIR}/Tests/Package/fixtures")

    add_subdirectory("${PROJECT_SOURCE_DIR}/Tests/Package/fixtures/Provider"
        "${CMAKE_CURRENT_BINARY_DIR}/fixtures/Provider")
    add_subdirectory("${PROJECT_SOURCE_DIR}/Tests/Package/fixtures/Consumer"
        "${CMAKE_CURRENT_BINARY_DIR}/fixtures/Consumer")

    if(VASE_BUILD_PROFILE STREQUAL "desktop")
        foreach(packageFixture IN ITEMS PackageProviderPlugin PackageConsumerPlugin)
            set_target_properties(${packageFixture} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixture plugins/${packageFixture}"
                LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixture plugins/${packageFixture}")
        endforeach()
    endif()

    vase_pack_plugins(VasePackageProvider PLUGINS PackageProviderPlugin STATIC)
    vase_pack_plugins(VasePackageConsumer PLUGINS PackageConsumerPlugin STATIC)
    set_target_properties(VasePackageProvider PROPERTIES VASE_MOBILE_SOURCE_ID Provider)
    set_target_properties(VasePackageConsumer PROPERTIES VASE_MOBILE_SOURCE_ID Consumer)
    set(VASE_MOBILE_PACKAGE_TARGETS VasePackageProvider VasePackageConsumer PARENT_SCOPE)
endfunction()

# 每个 profile 只用自己的构建目录生成资源表，保持静态包定义唯一。
function(vase_define_mobile_scenario)
    if(TARGET VaseMobileScenarioCore)
        message(FATAL_ERROR "vase_define_mobile_scenario: already defined")
    endif()
    include("${PROJECT_SOURCE_DIR}/Cmake/VaseMobileResources.cmake")
    vase_generate_mobile_resources("${CMAKE_CURRENT_BINARY_DIR}/VaseMobileResources.h"
        PACKAGES ${VASE_MOBILE_PACKAGE_TARGETS})

    add_library(VaseMobileScenarioCore STATIC
        "${PROJECT_SOURCE_DIR}/Samples/Mobile/MobileScenario.cpp")
    set_target_properties(VaseMobileScenarioCore PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_include_directories(VaseMobileScenarioCore
        PUBLIC "${PROJECT_SOURCE_DIR}/Samples/Mobile"
        PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" ${VASE_MOBILE_RESOURCE_HEADER_DIRS})
    target_link_libraries(VaseMobileScenarioCore
        PUBLIC VaseBuildOptions VaseHost VaseCatalog PackageServices ${VASE_MOBILE_PACKAGE_TARGETS})
    set(VASE_MOBILE_RESOURCE_HEADER_DIRS "${VASE_MOBILE_RESOURCE_HEADER_DIRS}" PARENT_SCOPE)
endfunction()
