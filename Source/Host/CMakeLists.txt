# 平台文件按平台选：Windows 走 PE，macOS 走 Mach-O + dyld，其余走 ELF + dl_iterate_phdr。
# 三套平台实现同一组接口，由 Source/Host/ImageInspectPlatform.h 声明（Host 内部，不进公开头）。
# **注意 Mach-O 的纯字节解析器不在这里**——它无条件编译（见 add_library 那一段）。
if(WIN32)
    set(VASE_HOST_PLATFORM_SOURCES
        ImageInspectWindows.cpp
        LoaderWindows.cpp)
elseif(APPLE)
    set(VASE_HOST_PLATFORM_SOURCES
        ImageInspectDarwin.cpp
        LoaderPosix.cpp)
else()
    set(VASE_HOST_PLATFORM_SOURCES
        ImageInspectLinux.cpp
        LoaderPosix.cpp)
endif()

add_library(VaseHost SHARED
    SmokeProbe.cpp
    ImageInspectCommon.cpp
    # Mach-O 解析器**无条件**编译（D161）：它是纯字节解析，与 PE/ELF 两段同性质；
    # 八条线的合成字节用例都要链接它。放进 APPLE 分支会让那族用例在非 macOS 上链接失败。
    ImageInspectMachO.cpp
    Loader.cpp
    PluginHost.cpp
    ConfigBlob.cpp
    Detail/ManifestCompare.cpp
    Inspect.cpp
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
