# 自定义 triplet：Linux x64 + libc++。
#
# 存在理由：vcpkg 内置的 x64-linux 走 libstdc++，而 8.5 要求 libc++。
# 两侧 STL 一致是 8.3「跨 DLL 的 C++ 约束」的前提。

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# 必须连**编译器**一起钉，光钉 flags 不够（实测，非推测）。
#
# vcpkg 在 Linux 原生构建时不选编译器：scripts/toolchains/linux.cmake 只在
# **交叉编译**分支里设 CMAKE_CXX_COMPILER，原生分支把它留给 CMake 的平台默认值，
# 实测落到 /usr/bin/c++（GCC 13.3.0）。
#
# 于是 vcpkg 的编译器探测（scripts/detect_compiler，要编一个 C 语言的 try-compile）
# 会拿 GCC 去编，而 GCC 不认 VCPKG_LINKER_FLAGS 里的 -stdlib=libc++：
#   cc: error: unrecognized command-line option '-stdlib=libc++'
#   -> "vcpkg was unable to detect the active compiler's information"
# 整个 configure 在这里就失败，根本走不到 gtest。
#
# VCPKG_CMAKE_CONFIGURE_OPTIONS 由 vcpkg 的 vcpkg_configure_cmake 追加到每个 port 的
# configure 命令行（含 detect_compiler 自己在内），是 triplet 里钉编译器的正规入口。
# clang 在 C 模式下接受 -stdlib=libc++（只报一条 unused-argument warning，exit 0），
# 所以 C 与 CXX 都交给 clang 即可，不需要把 VCPKG_LINKER_FLAGS 按语言拆开。
# **与平台工具链的一处不对称，已知且暂容。** 工具链文件把编译器路径做了
# 「目录去引用化」（理由见 linux-x64-clang-libcxx.cmake 的【一】），这里却只能给裸名：
# triplet 是独立脚本，拿不到工具链解析出的真实目录，而重复一遍 find_program + REAL_PATH
# 会是同一条规则的第二次声明。
#
# 暂容的条件（**观测为实、通则未孤立验证**）：
#   观测——**我们自己的 target 就被扫描了**，尽管仓库里零模块源码、零
#   FILE_SET CXX_MODULES：它们按 C++20 构建，构建时真的调了 clang-scan-deps，
#   并且就挂在这里（报 'cstdint' file not found）。而 gtest 没被扫描——它按 C++17 构建。
#   推测——触发条件是**目标按 C++20（或更高）构建**，而不是「有模块文件」。
#   但这是推测，别当依据用；上面那条观测才是硬的。
#
# **将来若有 port 报 'cstdint' file not found，第一个该看的就是这里。**
set(VCPKG_CMAKE_CONFIGURE_OPTIONS
    "-DCMAKE_C_COMPILER=clang"
    "-DCMAKE_CXX_COMPILER=clang++")

# 只影响用 CMake 构建的依赖——当前就是 GoogleTest。
# libc++ 装在 /usr/local/lib，链接器自己找得到，不需要额外的 -L。
set(VCPKG_CXX_FLAGS "-stdlib=libc++")
set(VCPKG_C_FLAGS "")
set(VCPKG_LINKER_FLAGS "-stdlib=libc++")
