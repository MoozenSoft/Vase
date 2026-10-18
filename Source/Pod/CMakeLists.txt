add_library(VasePod ${VASE_FRAMEWORK_LINKAGE}
    SmokeProbe.cpp
    ScopePool.cpp
    EffectScope.cpp
    ServiceRegistry.cpp
    EventBus.cpp
    Context.cpp
    Pod.cpp
    DependencyLedger.cpp)

target_include_directories(VasePod
    PUBLIC "${PROJECT_SOURCE_DIR}/Include")

target_compile_definitions(VasePod
    PRIVATE VASE_POD_BUILD)

if(NOT VASE_BUILD_PROFILE STREQUAL "desktop")
    target_compile_definitions(VasePod PUBLIC VASE_STATIC)
    set_target_properties(VasePod PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# Linux/macOS 上默认可见性是全开，必须收紧到「只露 VASE_EXPORT 的」。
# 这也是插件宿主能干净卸载的前提之一。
set_target_properties(VasePod PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

target_link_libraries(VasePod
    PRIVATE VaseBuildOptions)
