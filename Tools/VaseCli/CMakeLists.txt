# 内部静态库 + 薄 main（M5/D124）：编排与检查住库里，gtest 可直接链它做单变量用例；
# main 只做 argv 解析与打印。CMake 顺序要求：本目录必须排在 add_subdirectory(Tests) 之前
# （Tests 的 VaseTests 要链 VaseCliCore），与 VaseHost / VaseCatalog / VaseConsole 同例。
add_library(VaseCliCore STATIC
    Cli.cpp
    Scan.cpp
    Validate.cpp
    ManifestMerge.cpp
    Plan.cpp
    Doctor.cpp)

target_link_libraries(VaseCliCore
    PRIVATE VaseBuildOptions)   # 只链 VaseBuildOptions：编排面自己用 VaseHost/VaseCatalog 的类型
                                # 由 VaseCli 侧链接（静态库不传播，避免 Tests 被迫全链）
target_include_directories(VaseCliCore
    PUBLIC "${PROJECT_SOURCE_DIR}/Include"
           "${CMAKE_CURRENT_SOURCE_DIR}")

# 不链 VaseThirdPartyCli：VaseCli 是批处理工具，argv 手写（D137）。
add_executable(VaseCli main.cpp)
target_link_libraries(VaseCli
    PRIVATE VaseBuildOptions
            VaseCliCore
            VaseHost
            VaseCatalog)

# 退出码是 CLI 层的机器判据（D135）：`main` / `Run` 的 argv 分发由这两条端到端守。
# 判据形状照 Tools/VaseConsole：WILL_FAIL 钉非零，兄弟用例只钉文本——
# 两者不可合并（ctest 4.4.3 实测：同设 WILL_FAIL + PASS_REGULAR_EXPRESSION 时命中反判 Failed）。
add_test(NAME VaseCliNoArgsIsUsageError COMMAND VaseCli)
set_tests_properties(VaseCliNoArgsIsUsageError PROPERTIES WILL_FAIL TRUE)
add_test(NAME VaseCliNoArgsReason COMMAND VaseCli)
set_tests_properties(VaseCliNoArgsReason PROPERTIES PASS_REGULAR_EXPRESSION "usage: VaseCli")

add_test(NAME VaseCliUnknownSubcommandIsUsageError COMMAND VaseCli frobnicate)
set_tests_properties(VaseCliUnknownSubcommandIsUsageError PROPERTIES WILL_FAIL TRUE)
add_test(NAME VaseCliUnknownSubcommandReason COMMAND VaseCli frobnicate)
set_tests_properties(VaseCliUnknownSubcommandReason PROPERTIES PASS_REGULAR_EXPRESSION "unknown command")

add_test(NAME VaseCliPlanNoArgsIsUsageError COMMAND VaseCli plan)
set_tests_properties(VaseCliPlanNoArgsIsUsageError PROPERTIES WILL_FAIL TRUE)
add_test(NAME VaseCliPlanNoArgsReason COMMAND VaseCli plan)
set_tests_properties(VaseCliPlanNoArgsReason PROPERTIES PASS_REGULAR_EXPRESSION "usage: VaseCli plan")

add_test(NAME VaseCliDoctorNoArgsIsUsageError COMMAND VaseCli doctor)
set_tests_properties(VaseCliDoctorNoArgsIsUsageError PROPERTIES WILL_FAIL TRUE)
add_test(NAME VaseCliDoctorNoArgsReason COMMAND VaseCli doctor)
set_tests_properties(VaseCliDoctorNoArgsReason PROPERTIES PASS_REGULAR_EXPRESSION "usage: VaseCli doctor")
add_test(NAME VaseCliDoctorTooManyArgsIsUsageError COMMAND VaseCli doctor a b)
set_tests_properties(VaseCliDoctorTooManyArgsIsUsageError PROPERTIES WILL_FAIL TRUE)
add_test(NAME VaseCliDoctorTooManyArgsReason COMMAND VaseCli doctor a b)
set_tests_properties(VaseCliDoctorTooManyArgsReason PROPERTIES PASS_REGULAR_EXPRESSION "usage: VaseCli doctor")
