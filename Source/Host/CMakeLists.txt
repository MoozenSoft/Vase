# 平台文件按 WIN32 选：Windows 走 PE（HMODULE / LoadLibraryExW），
# 其余平台走 ELF（dl_iterate_phdr / dlopen）。两套实现同一组接口，
# 由 Source/Host/ImageInspectPlatform.h 声明（Host 内部，不进公开头）。
if(WIN32)
    set(VASE_HOST_PLATFORM_SOURCES
        ImageInspectWindows.cpp
        LoaderWindows.cpp)
else()
    set(VASE_HOST_PLATFORM_SOURCES
        ImageInspectPosix.cpp
        LoaderPosix.cpp)
endif()

add_library(VaseHost SHARED
    SmokeProbe.cpp
    ImageInspectCommon.cpp
    Loader.cpp
    PluginHost.cpp
    ConfigBlob.cpp
    Detail/ManifestCompare.cpp
    ${VASE_HOST_PLATFORM_SOURCES})

target_include_directories(VaseHost
    PUBLIC "${PROJECT_SOURCE_DIR}/Include")

target_compile_definitions(VaseHost
    PRIVATE VASE_HOST_BUILD)

set_target_properties(VaseHost PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

# PUBLIC：Host 依赖 Pod——分层方向（§1：二进制层在实例层之上）与
# 「插件不依赖 Host」（D11）由链接图承载。
target_link_libraries(VaseHost
    PUBLIC VasePod
    PRIVATE VaseBuildOptions)
