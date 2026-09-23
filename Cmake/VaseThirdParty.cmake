# 第三方「只有头文件」依赖的编译面：include 路径与语言标准要求只在这里进构建。
#
# 为什么不是 add_subdirectory(ThirdParty/cli)：那会把上游 fork 自己的构建决策变成我们的
# 构建风险，而它恰好是上游改动最勤的地方（v2.2.0 之后那一串 "Fix policies for old versions
# of cmake" / "cmake minimum set to interval 3.8 ... 3.27" / boost 组件增减，全在此文件）。
# 具体三件：`find_package(Threads REQUIRED)` 是硬 configure 依赖（Linux 上还往每个消费者挂
# -lpthread）、`install()` 会把第三方头装进我们的分发树、嵌套 `project()` 带进一层政策作用域。
# 自立 target 之后这些对我们完全惰性——我们只消费 include/。
#
# 为什么不走 vcpkg / imported target：**imported target 的 include 会被加成 system 头**
# （`-isystem`）——那会把「第三方头带回 throw」这条本应响亮的失败吞成静默：实测同一份上游头
# 在 Linux 线 `-I` 下 20 error、`-isystem` 下 **0**。这个失效没有任何东西会报警，所以它必须先被
# 量出来再被写下来。
#
# 但 **`-I` 单独不足以把住这道关**（实测，三条驱动线各自的判据力不同）：
#
#   | 线 | 上游头（含 throw）诊断数 |
#   |---|---|
#   | Linux clang++ `-fno-exceptions` + `-I` | 20 error ✅ |
#   | Windows clang-cl `/EHs-c-`（拿不到 /external:*，那是 cl.exe 专属块） | 20 error ✅ |
#   | Windows **cl.exe** + `<cli/cli.h>` 尖括号 | **0** ❌ |
#   | Windows **cl.exe** + `"cli/cli.h"` 引号 | 5 × error C4530 ✅ |
#
# 根因是 VaseBuildOptions 里已有的 `/external:anglebrackets`：它把**所有尖括号包含的头**标为
# 外部头，于是 `/we4530` 升级出的 C4530 一并静音——与规矩 3 说的是同一个机制，只是方向相反：
# 规矩 3 要引号是为了"别放过我们自己头的警告"，这里要引号是为了"别放过第三方头的异常"。
#
# **因此规则：包含 cli 头一律用引号** —— `#include "cli/cli.h"`，不是尖括号。写 Shell.cpp 的人
# 第一眼会觉得这违反了"第三方用尖括号"的直觉，那不是笔误，是这条注释的全部内容。

add_library(VaseThirdPartyCli INTERFACE)

# 不得改成 SYSTEM / 不得加 /external:I，理由见上。实测：当前落地形式是 `-I`。
target_include_directories(VaseThirdPartyCli INTERFACE
    "${PROJECT_SOURCE_DIR}/ThirdParty/cli/include")

# fork 用 std::optional 替掉了原先「越界即 throw std::out_of_range」的失败通道，因此下限
# 是 C++17（上游原文写的是 C++14）。这条要求由**本文件**声明而不是指望 fork 的 CMakeLists：
# clang-cl 的默认标准是 C++14，漏掉它的症状是一条指向第三方头的
# `no template named 'optional' in namespace 'std'`，看不出与语言标准有关。
# 与 VaseBuildOptions 的 cxx_std_20 取大者，故对现有 target 无影响。
target_compile_features(VaseThirdPartyCli INTERFACE cxx_std_17)

# 已知留给 v1 的一条：CliLocalSession（行编辑 / Tab 补全）的键盘层用 std::thread，那时本
# target 需要补 `find_package(Threads)` + 链 Threads::Threads。现在不预加：v0 只用文件会话，
# 它的循环同步跑在调用线程上（见设计文档 §2.8），多链一个 pthread 只会白扩依赖面。
