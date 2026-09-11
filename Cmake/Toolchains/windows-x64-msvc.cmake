# 平台工具链：Windows x64 + cl.exe。
#
# 与 windows-x64-clangcl.cmake 是**同一 ABI 的两个前端**（8.5）：都目标 MSVC ABI、
# 都用 MSVC STL。Vase 本体两套都支持，让「宿主与插件可以各用一个」不只停在纸面。
#
# 与 clang-cl 的关键差别：**cl.exe 需要 VS Developer 环境**（PATH / INCLUDE / LIB），
# 它不像 clang-cl 那样能自行经注册表发现 MSVC。因此这套 preset 必须在
# Developer Command Prompt 里跑，或先执行 vcvars64.bat。
# 仓库内的可复现入口是 Scripts/msvc-env.cmd，它用 vswhere 定位 VS 并 call vcvars64。

if(NOT DEFINED ENV{VCPKG_ROOT} OR "$ENV{VCPKG_ROOT}" STREQUAL "")
    message(FATAL_ERROR
        "环境变量 VCPKG_ROOT 未设置。\n"
        "Windows 侧请设置 VCPKG_ROOT=D:/Developer/vcpkg（钉定的 commit 见 vcpkg.json 的 builtin-baseline）。\n"
        "注意：vcvars64.bat 会把 VCPKG_ROOT **覆盖**成 VS 自带的那份 vcpkg（不是清空），"
        "所以走 Developer 环境时不要在会话里手工 set 了事——用 Scripts/msvc-env.cmd，"
        "它会在 call vcvars 之后还原调用者的 VCPKG_ROOT。")
endif()

if(NOT DEFINED ENV{VSCMD_VER} AND NOT DEFINED ENV{VCINSTALLDIR})
    message(FATAL_ERROR
        "cl.exe 需要 VS Developer 环境。\n"
        "这一条与 clang-cl 不同：clang-cl 能自行发现 MSVC 头/库（实测无需 Developer 环境），"
        "而 cl.exe 依赖 PATH / INCLUDE / LIB 三件套。\n"
        "处置：从 Visual Studio 的 Developer Command Prompt 进入，或先 call vcvars64.bat。")
endif()

# 不写死安装路径：cl.exe 由 Developer 环境放进 PATH。
set(CMAKE_CXX_COMPILER cl CACHE STRING "Windows 平台 C++ 编译器（MSVC）" FORCE)

# 8.3：跨模块 new/delete 必须落在同一个堆——动态 CRT + 动态库。
#
# **位置要求**：必须在下面那句 `include` **之前**——vcpkg 的工具链在 include 那一刻就把
# `VCPKG_TARGET_TRIPLET` 读走并写进 CACHE（`vcpkg.cmake:242`，FORCE），据此定下
# `CMAKE_PREFIX_PATH` 等指向 `vcpkg_installed/<triplet>`（同文件 `:439`）。
# （与 `windows-x64-clangcl.cmake` 同一处的理由，那边写得长一些。）
set(VCPKG_TARGET_TRIPLET x64-windows)

include("$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
