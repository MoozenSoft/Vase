# 平台工具链：Windows x64 + clang-cl（MSVC ABI + MSVC STL）。
#
# 接线方向（spec 2.2）：CMake 只允许一个 CMAKE_TOOLCHAIN_FILE，因此定为
# 「平台工具链文件是它，文件内部 include vcpkg 的」，使平台配置先于 vcpkg 生效。

# 下面那条 message 里的 `\$env{...}` 中，`\$` 是**承重的**，不是排版。
# `message()` 的引号参数在执行时展开：不转义的话，CMake 会在解析这个字符串时报
# `Syntax error in cmake code ... when parsing string` +
# `Syntax $env{{} is not supported.  Only ${}, $ENV{}, and $CACHE{} are allowed.`
# （实测，CMake 4.4）——**报错指向工具链文件的语法，而不是「VCPKG_ROOT 未设置」**，
# 恰好把这条守卫的全部价值抵消掉。转义后产生字面 `$`。
# 注意它只在守卫真的触发（VCPKG_ROOT 未设）时才会被展开到，所以「平时没事」不代表写对了。
if(NOT DEFINED ENV{VCPKG_ROOT} OR "$ENV{VCPKG_ROOT}" STREQUAL "")
    message(FATAL_ERROR
        "环境变量 VCPKG_ROOT 未设置。\n"
        "CMakePresets 用 \$env{VCPKG_ROOT} 之外的路径无法定位 vcpkg 工具链，空值会让 configure 在这里失败。\n"
        "Windows 侧请设置 VCPKG_ROOT=D:/Developer/vcpkg（钉定的 commit 见 vcpkg.json 的 builtin-baseline）。")
endif()

# D6：clang-cl 由 PATH 解析，不写死安装路径——换 LLVM 只改 PATH，不动仓库。
# 但**「只改 PATH 就够」这个直觉不准确**：解析结果会进构建树缓存（这里是名字
# `clang-cl`，真实路径另记在 CMakeFiles/<CMake 版本>/CMakeCXXCompiler.cmake 里），
# 换 LLVM 后须**删构建树或换新树**——在同一个 build 目录里重跑 `cmake --preset`
# 不算数，它复用缓存、不会重新解析 PATH。详见 README「环境前提」。
set(CMAKE_CXX_COMPILER clang-cl CACHE STRING "Windows 平台 C++ 编译器" FORCE)

# 8.3：跨模块 new/delete 必须落在同一个堆——动态 CRT + 动态库。
#
# **位置要求**：必须在下面那句 `include` **之前**。vcpkg 的工具链在 include 那一刻
# 就把 `VCPKG_TARGET_TRIPLET` 读走、写进 CACHE（`vcpkg.cmake:242`，FORCE），并据此定下
# `CMAKE_PREFIX_PATH` / install-root 等指向 `vcpkg_installed/<triplet>`（同文件 `:439`）。
# 晚设的话，状态已经按平台推出来的默认 triplet 算过一遍了。
set(VCPKG_TARGET_TRIPLET x64-windows)

# §8.2 档三 · 身份特征是插件的**构建要求**（13.2 末行）：MSVC 线没有 /DEBUG
# 就没有 PE 调试目录里的 CodeView(RSDS)，Adopt 会直接拒绝该二进制（响亮的失败，
# 不做静默降级）。/DEBUG:FULL 而非 FASTLINK：FASTLINK 的调试目录形态与增量
# 链接器绑定，档三只认 RSDS 的 GUID+Age 稳定存在。
# 用 *_FLAGS_INIT 而非 VaseBuildOptions：这是**链接器**标志，且必须早于
# project() 的编译器检测进入缓存（CLAUDE.md 规矩 1 管编译选项的唯一出口，
# 工具链文件管「平台与产物的构建形态」——两者不重叠，此处注释互相指认）。
# 代价如实记下：vcpkg 子构建也走这份工具链，gtest 的 DLL 会多生成 PDB。
string(APPEND CMAKE_SHARED_LINKER_FLAGS_INIT " /DEBUG:FULL")

include("$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
