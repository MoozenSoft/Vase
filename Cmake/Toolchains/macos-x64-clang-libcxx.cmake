# 平台工具链：macOS x64 + clang + libc++。
#
# 接线方向与另两侧一致：平台工具链文件是 CMAKE_TOOLCHAIN_FILE，
# 文件内部 include vcpkg 的，使平台配置先于 vcpkg 生效。

if(NOT DEFINED ENV{VCPKG_ROOT} OR "$ENV{VCPKG_ROOT}" STREQUAL "")
    message(FATAL_ERROR
        "环境变量 VCPKG_ROOT 未设置。\n"
        "macOS 侧它由 ~/.zshenv 提供（本机环境类变量住那里），交互式与登录 shell 都可见。")
endif()

# D6：由 PATH 解析，不写死安装路径。find_program 的结果进缓存——换 LLVM 后须干净 configure，
# 光改 PATH 不够（README「环境前提」同一条结论）。
# clang++-mp-23 **排在 clang++ 之前**：MacPorts 的 clang++ 会随默认版本漂移，
# 钉住 23 才与根 CMakeLists 的版本闸一致；真漂到别处时由 doctor 响亮拒绝，不静默降级。
find_program(VASE_CLANGXX_EXECUTABLE NAMES clang++-mp-23 clang++
    DOC "macOS 平台 C++ 编译器（从 PATH 解析）")
if(NOT VASE_CLANGXX_EXECUTABLE)
    message(FATAL_ERROR
        "PATH 里找不到 clang++-mp-23 / clang++。\n"
        "本仓库要求 clang 23.x：MacPorts 的 clang-23 提供 clang++-mp-23，"
        "Apple 自带的 clang 不满足（实测为 16.0.0，且 CMAKE_CXX_COMPILER_ID 的版本闸会拒）。")
endif()

# 名字必须留 "++"（驱动按 argv[0] 判 C++ 模式），只把**目录**规范化；但**两支序与
# Linux【一】【二】相反**——/opt/local/bin 是 wrapper 目录、两名字可同存，先认裸名
# clang++ 会把 find_program 的钉（D158）静默降级成 select 当前指的那个，故先认 mp-23。
file(REAL_PATH "${VASE_CLANGXX_EXECUTABLE}" VASE_CLANGXX_REALPATH)
get_filename_component(VASE_CLANGXX_REALDIR "${VASE_CLANGXX_REALPATH}" DIRECTORY)
if(EXISTS "${VASE_CLANGXX_REALDIR}/clang++-mp-23")
    set(VASE_CLANGXX_EXECUTABLE "${VASE_CLANGXX_REALDIR}/clang++-mp-23")
elseif(EXISTS "${VASE_CLANGXX_REALDIR}/clang++")
    set(VASE_CLANGXX_EXECUTABLE "${VASE_CLANGXX_REALDIR}/clang++")
endif()

set(CMAKE_CXX_COMPILER "${VASE_CLANGXX_EXECUTABLE}" CACHE STRING "macOS 平台 C++ 编译器" FORCE)

# **clang-scan-deps 必须一起钉（MacPorts 独有坑）**：PATH 上没有裸名 `clang-scan-deps` 的对位，
# CMake 找不到即把 `-NOTFOUND` 塞进 C++20 依赖扫描、configure 响亮失败；Windows/Linux 无此坑。
# 推导与实测记录见 plan 2026-10-01-vase-macos-x64-platform-leg 偏离登记 5。
find_program(VASE_CLANG_SCAN_DEPS_EXECUTABLE NAMES clang-scan-deps-mp-23 clang-scan-deps
    DOC "macOS 模块依赖扫描器（从 PATH 解析）")
if(NOT VASE_CLANG_SCAN_DEPS_EXECUTABLE)
    message(FATAL_ERROR
        "PATH 里找不到 clang-scan-deps-mp-23 / clang-scan-deps。\n"
        "CMake 4.x 按 C++20 构建时用它做依赖扫描；缺位时 configure 期全部 try-compile 会死。")
endif()
set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS "${VASE_CLANG_SCAN_DEPS_EXECUTABLE}"
    CACHE FILEPATH "macOS clang-scan-deps" FORCE)

# 显式写架构：「凡影响产物形态的默认值一律显式写出」。本机恰是 x86_64，不能靠「反正默认就对」。
set(CMAKE_OSX_ARCHITECTURES x86_64 CACHE STRING "macOS 目标架构" FORCE)

# **本平台没有对位的承重 linker flag**——这不是漏写。实测链接器必写 LC_UUID，
# 身份特征天然在场，无需 /DEBUG:FULL（Windows）或 -Wl,--build-id=sha1（Linux）那样的注入。
# 承重性由**负例反向把守**：Tests/CMakeLists.txt 的 NoIdentityPlugin 在 macOS 侧用
# -Wl,-no_uuid 主动摘掉它，档三必须拒。所以「摘掉标志 ⇒ 运行期响亮失败」这条判据在这里
# 由负例承担，而不是由 flag 承担。
#
# 8.5：libc++ 是 macOS 的默认，不需要 -stdlib=libc++（与 Linux 侧不同）。
set(CMAKE_MODULE_LINKER_FLAGS_INIT "")

# 位置要求：这两句都必须在下面那句 include **之前**（vcpkg 的工具链在 include 那一刻
# 就把它们读走并写进 CACHE）。理由同 linux-x64-clang-libcxx.cmake。
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_LIST_DIR}/../Triplets")
set(VCPKG_TARGET_TRIPLET x64-osx-libcxx)

include("$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
