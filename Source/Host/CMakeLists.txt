add_library(VaseHost SHARED
    SmokeProbe.cpp)

target_include_directories(VaseHost
    PUBLIC "${PROJECT_SOURCE_DIR}/Include")

target_compile_definitions(VaseHost
    PRIVATE VASE_HOST_BUILD)

set_target_properties(VaseHost PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

# PUBLIC：Host 依赖 Session，而 Session 不依赖 Host——
# 「插件不依赖 Host」由约定变成链接期事实（D11）。
target_link_libraries(VaseHost
    PUBLIC VaseSession
    PRIVATE VaseBuildOptions)
