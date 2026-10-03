# 自定义 triplet：macOS x64 + libc++。
#
# 为什么不能直接用内置/community 的那份：
#   ① **编译器**——`scripts/toolchains/osx.cmake`（73 行）通篇不提编译器，原生构建时它
#      留给 CMake 平台默认值，也就是 Apple clang（实测 16.0.0，用 SDK 的 libc++ 头树），
#      而我们的 TU 由 MacPorts clang 23.1.2 编、吃的是 /opt/local/libexec/llvm-23 下自带
#      的那份 libc++ 头。不钉就会让 gtest 与我们在两棵头树上生成代码。
#   ② **linkage**——community/x64-osx.cmake 是 static。那**不构成** 8.3 的堆问题
#      （VCPKG_LIBRARY_LINKAGE 只影响 gtest 一个测试库，它静态链进 VaseTests、不跨我们的
#      模块边界），这里取 dynamic 只是为了与另三条线一致。**别把它读成正确性理由。**
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES x86_64)

# 必须连**编译器**一起钉，光钉 flags 不够（与 x64-linux-libcxx.cmake 同一条理由）：
# vcpkg 原生构建时不选编译器，探测那一步会拿 Apple clang 去编。
# **与平台工具链的一处不对称，已知且暂容**：工具链文件把编译器路径做了目录去引用化，
# 这里却只能给裸名——triplet 是独立脚本，拿不到工具链解析出的真实目录，而重复一遍
# find_program + REAL_PATH 会是同一条规则的第二次声明。
set(VCPKG_CMAKE_CONFIGURE_OPTIONS
    "-DCMAKE_C_COMPILER=clang-mp-23"
    "-DCMAKE_CXX_COMPILER=clang++-mp-23")

# **留空是对的**：macOS 上 clang 默认即 libc++（实测产物 LC_LOAD_DYLIB 里是
# /usr/lib/libc++.1.dylib），不需要 Linux 那一份的 -stdlib=libc++。别照 Linux 补齐。
set(VCPKG_CXX_FLAGS "")
set(VCPKG_C_FLAGS "")
