find_package(nlohmann_json CONFIG REQUIRED)

add_library(VaseCatalog SHARED
    PluginCatalog.cpp
    ManifestJson.cpp
    PresetJson.cpp
    Solve.cpp)

target_include_directories(VaseCatalog
    PUBLIC "${PROJECT_SOURCE_DIR}/Include")

target_compile_definitions(VaseCatalog
    PRIVATE VASE_CATALOG_BUILD
            JSON_NOEXCEPTION)

set_target_properties(VaseCatalog PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

# D45：Catalog 站 Host 之上产定形 LoadPlan。nlohmann 只在本 target 的 PRIVATE
# 包含面出现（D55）；它的 imported target 走 system include，tidy 按非用户代码抑制。
target_link_libraries(VaseCatalog
    PUBLIC VaseHost
    PRIVATE VaseBuildOptions
            nlohmann_json::nlohmann_json)
