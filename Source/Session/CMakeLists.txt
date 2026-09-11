add_library(VaseSession SHARED
    SmokeProbe.cpp)

target_include_directories(VaseSession
    PUBLIC "${PROJECT_SOURCE_DIR}/Include")

target_compile_definitions(VaseSession
    PRIVATE VASE_SESSION_BUILD)

# Linux/macOS 上默认可见性是全开，必须收紧到「只露 VASE_EXPORT 的」。
# 这也是插件宿主能干净卸载的前提之一。
set_target_properties(VaseSession PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

target_link_libraries(VaseSession
    PRIVATE VaseBuildOptions)
