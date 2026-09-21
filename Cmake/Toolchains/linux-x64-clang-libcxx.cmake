# 平台工具链：Linux x64 + clang + libc++（8.5 要求 libc++，非 libstdc++）。
#
# 接线方向与 Windows 侧一致：平台工具链文件是 CMAKE_TOOLCHAIN_FILE，
# 文件内部 include vcpkg 的，使平台配置先于 vcpkg 生效。

if(NOT DEFINED ENV{VCPKG_ROOT} OR "$ENV{VCPKG_ROOT}" STREQUAL "")
    message(FATAL_ERROR
        "环境变量 VCPKG_ROOT 未设置。\n"
        "Linux 侧它由 /etc/profile.d/vcpkg.sh 提供，需要登录 shell 才会加载：\n"
        "  bash -l -c '...'   ✓\n"
        "  bash -c '...'      ✗（shell 的加载规则，配置改不了）\n"
        "非登录非交互的调用方必须自己显式带上 VCPKG_ROOT。")
endif()

# 编译器路径要「目录去引用化、名字保留 clang++」。两件事各有实测理由，都不能省。
#
# 本机的符号链接链（Ubuntu 24.04 + clang 23.1.0）：
#     /usr/local/bin/clang++ -> /opt/llvm-23.1.0/bin/clang++ -> clang -> clang-23
#
# 【一】目录必须是真实的 LLVM bin 目录，否则模块扫描挂。
#
# 实测对照（硬事实）：编译器传 /usr/local/bin/clang++ → FAIL；
#                     传 /opt/llvm-23.1.0/bin/clang++ → OK。
# 症状原文：
#     fatal error: 'cstdint' file not found
#
# 已核实的环境事实：libc++ 头在 /opt/llvm-23.1.0/include/c++/v1；
# /usr/local/include/c++ **不存在**；符号链接链见上方两行。
#
# **为什么这样会挂，未孤立验证**——推测是 clang-scan-deps 拿命令行里那个字符串去推
# 驱动目录、于是去 /usr/local/include/c++/v1 找头。但这是推测，上面的诊断原文也
# 没打出它搜的路径，别当依据用；上面那组实测对照才是硬事实。
#
# 【二】名字必须留着 "++"，否则退化成 C 驱动、链接期不带 -lc++。
# clang 的驱动按 **argv[0] 里有没有 "++"** 判 C++ 模式。一路 REAL_PATH 解到底会得到
# .../clang-23，驱动退回 C 模式，链接 VaseTests 时报：
#     undefined reference to `std::__1::basic_string<...>::append(char const*)'
#     /usr/local/lib/libc++.so.1: error adding symbols: DSO missing from command line
# （实测。两个动态库没暴露这个错，因为 -shared 允许未定义符号。）
#
# 所以：只把**目录**规范化，名字仍取 clang++。
#
# **仍然不写死安装路径**——换 LLVM 只改 PATH，与 Windows 侧同一条约定（D6）；
# 这里只是把 PATH 解析出来的结果去引用化，不是把某个安装路径钉进仓库。
# 注意 find_program 的结果进缓存，且下面又 FORCE 写进 CMAKE_CXX_COMPILER 的缓存：
# **换 LLVM 后需要干净 configure（新构建树或删缓存），或 -U VASE_CLANGXX_EXECUTABLE**，
# 光改 PATH 是不够的——只要旧那份安装还在，解析到的就还是它。
# 同一个 build 目录里重跑 configure 不算数：它会复用已缓存的
# VASE_CLANGXX_EXECUTABLE，不会重新解析 PATH。
find_program(VASE_CLANGXX_EXECUTABLE NAMES clang++
    DOC "Linux 平台 C++ 编译器（从 PATH 解析）")
if(NOT VASE_CLANGXX_EXECUTABLE)
    message(FATAL_ERROR
        "PATH 里找不到 clang++。\n"
        "README「环境前提」要求 clang 23.x 的 bin 目录在 PATH 最前。")
endif()
file(REAL_PATH "${VASE_CLANGXX_EXECUTABLE}" VASE_CLANGXX_REALPATH)
get_filename_component(VASE_CLANGXX_REALDIR "${VASE_CLANGXX_REALPATH}" DIRECTORY)
if(EXISTS "${VASE_CLANGXX_REALDIR}/clang++")
    set(VASE_CLANGXX_EXECUTABLE "${VASE_CLANGXX_REALDIR}/clang++")
else()
    # 该目录下没有 clang++（罕见的安装布局）：退回 PATH 找到的那个。
    # 此时【一】不成立，模块扫描可能找不到 libc++ 头——真遇到再针对该布局处理。
    message(WARNING
        "${VASE_CLANGXX_REALDIR} 下没有 clang++，回退用 ${VASE_CLANGXX_EXECUTABLE}。\n"
        "若随后在 clang-scan-deps 阶段报 'cstdint' file not found，原因在这里。")
endif()

set(CMAKE_CXX_COMPILER "${VASE_CLANGXX_EXECUTABLE}" CACHE STRING "Linux 平台 C++ 编译器" FORCE)

# 8.5：libstdc++ → libc++。编译与链接都要给，否则链接期找不到 libc++ 的符号。
#
# 追加的 --build-id=sha1 是**身份特征**（§8.2 档三 / 13.2 末行），不是可选优化：
# .note.gnu.build-id 是 Linux 侧的身份特征，与 PE 的 RSDS 对位；档三在内存镜像
# 与磁盘文件上比对的就是这段 desc。
#
# **计划要求同时追加 `-fno-gnu-unique`，本文件没有加——实测 clang 不接受该标志。**
# 证据（clang 23.1.0，本机）：
#   clang++: error: unknown argument: '-fno-gnu-unique'   （exit 1）
# 它是 GCC 独有的开关（`clang++ --help | grep gnu-unique` 零命中）。但**设计意图
# 不受影响**：该开关的作用是阻止编译器把符号标成 STB_GNU_UNIQUE，而 **clang 从不
# 生成这种绑定**。同一份「inline 函数 + 静态局部变量」源码实测对比：
#   g++    -fPIC -shared → 符号表里 4 个 UNIQUE
#   clang++ -fPIC -shared → 0 个
# 并且 libc++.so.1 与 Vase 自己的 .so 里也各是 0 个。即：Linux 线本来就没有
# STB_GNU_UNIQUE 的成因可封，该标志对这条工具链是空操作、且名字不存在。
# 仍记在此处而非删掉：后来人换 GCC 工具链时，这一条要重新评估。
set(CMAKE_CXX_FLAGS_INIT "-stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-stdlib=libc++")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-stdlib=libc++ -Wl,--build-id=sha1")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-stdlib=libc++")

# 内置 x64-linux 走 libstdc++，这里换成同目录下的自定义 triplet。
#
# **位置要求：这两句都必须在下面那句 `include` 之前。** vcpkg 的工具链在 include 那一刻
# 就把它们读走并写进 CACHE（`VCPKG_TARGET_TRIPLET` 见 `vcpkg.cmake:242`、
# `VCPKG_OVERLAY_TRIPLETS` 见同文件 `:114`，两处都是 FORCE），并据此定下
# `CMAKE_PREFIX_PATH` / install-root 等指向 `vcpkg_installed/<triplet>`（同文件 `:439`）与
# manifest 安装要用的 `--overlay-triplets`（同文件 `:516`）。晚设的话，状态已经按
# 默认的 `x64-linux` 算过一遍了。
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_LIST_DIR}/../Triplets")
set(VCPKG_TARGET_TRIPLET x64-linux-libcxx)

include("$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
