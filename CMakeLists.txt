cmake_minimum_required(VERSION 4.3)

project(Vase
    VERSION 0.1.0
    DESCRIPTION "插件管理能力库"
    LANGUAGES CXX)

# --- 工具链 doctor ---------------------------------------------------------
#
# Windows 上 Vase 本体支持**两个编译器**：clang-cl 与 cl.exe。8.5 说两者是
# 同一 ABI 的两个前端（都目标 MSVC ABI、都用 MSVC STL）；本体两套都支持，
# 是为了让「宿主与插件可以各用一个」这条不只是纸面约定。其余平台只支持 clang。
#
# 两个分支的检查强度不同：
#   clang / clang-cl —— 必须 23.x。两侧 LLVM 同版本是 clang-format / clang-tidy
#                       判据双平台成立的前提（spec 2.1.1(c)）
#   cl.exe           —— 只查存在性与平台，版本交给 VS 安装自己管
#                       （cl 的版本与 tidy/format 判据无关）

set(VASE_REQUIRED_CLANG_MAJOR 23)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(NOT CMAKE_CXX_COMPILER_VERSION MATCHES "^${VASE_REQUIRED_CLANG_MAJOR}\\.")
        message(FATAL_ERROR
            "Vase 要求 clang ${VASE_REQUIRED_CLANG_MAJOR}.x，当前为 ${CMAKE_CXX_COMPILER_VERSION}（${CMAKE_CXX_COMPILER}）。\n"
            "两侧 LLVM 必须同版本，否则 clang-format / clang-tidy 的双平台判据不成立。\n"
            "处置：把 LLVM ${VASE_REQUIRED_CLANG_MAJOR}.1.x 的 bin 目录放到 PATH 最前（见 README「环境前提」）。")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if(NOT WIN32)
        message(FATAL_ERROR "cl.exe 只支持 Windows，当前平台是 ${CMAKE_SYSTEM_NAME}。")
    endif()
else()
    message(FATAL_ERROR
        "Vase 支持的 CXX 编译器：clang / clang-cl（全平台）、cl.exe（仅 Windows）。"
        "当前为 ${CMAKE_CXX_COMPILER_ID}（${CMAKE_CXX_COMPILER}）。")
endif()

# --- 工程约定 ---------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 可执行与动态库放同一目录：Windows 的 DLL 查找路径里包含可执行文件所在目录，
# 测试可执行文件与 VasePod/VaseHost 同处 bin/ 才能直接跑起来。
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# --- 跨模块的堆一致性（8.3 的硬约束）----------------------------------------
#
# clang-cl 与 clang 的**驱动默认**都是静态 CRT（实测：clang-cl 传
# --dependent-lib=libcmt，clang 传 -defaultlib:libcmt，即 /MT）。
# 静态 CRT 会让每个动态库各持一份堆，跨模块的 new/delete 落到不同的堆上——
# 这正是 spec 8.3 禁止 x64-windows-static 的理由，而它同样是**驱动默认值**的坑。
#
# CMake 的 CMAKE_MSVC_RUNTIME_LIBRARY 默认值本就是「动态 CRT、Debug 用调试版」
# （即 /MD 与 /MDd），恰好符合要求（实测：Debug 下传给 clang-cl 的是 -MDd）。
# 这里**显式写出来**而不依赖默认值：它是承重约束，不该靠一个可被他人改动的
# 默认值兜着。Task 2 结束时要用产物本身验证它确实生效。
if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

option(VASE_WERROR "把编译器警告当作错误" ON)

add_library(VaseBuildOptions INTERFACE)

# C++20 标准**也走这个 interface**，不能只靠上面的 CMAKE_CXX_STANDARD。
#
# 上方那个变量是**目录作用域变量**：它只在「在本仓库这棵目录树里创建」的 target 上
# 自动生效。任何在别的作用域里创建、却链接了 `VaseBuildOptions` 的 target（树外消费者、
# 第三方的 `add_subdirectory`，以及日后 M5 若把 VasePack 放到别的工程里）会拿到下面
# 那一整套警告 / `/utf-8` / 无异常开关，**却用编译器默认标准编译**——不报错、不警告。
# 这与计划自己的 Global Constraint「凡影响 ABI 的默认值一律显式写出」同类：
# 语言标准是影响 ABI 的默认值，而「编译选项的唯一出口」（CLAUDE.md 规矩 1）必须把它带上。
target_compile_features(VaseBuildOptions INTERFACE cxx_std_20)

# 警告级别：两侧**不能**用同一个 flag 写法。
#
# 实测：clang-cl 把 `-Wall` 解析成 MSVC 的 `/Wall`，等价于 `-Weverything`
# （连 `-Wc++98-compat` 都报）；而 GNU 驱动的 `-Wall` 是那套精选集。
# 同一个 `-Wall` 在两侧含义完全不同，配上 `-Werror` 会在 Windows 上炸一片。
#
# 所以各用本平台惯用的等级。**代价要如实记下**：两侧 warning 集合并不相同。
# spec 2.1.1(c) 把「诊断与 warning 集合一致」当作 D6 的理由，那句要收回——
# 同 clang 版本真正保证的是 **clang-tidy / clang-format 规则一致**（验收 #3 / #5），
# 而 warning 集合因驱动方言不同而不同（验收 #1 只要求各自零警告，不受影响）。
if(WIN32)
    set(VASE_WARN_FLAGS /W4)
else()
    set(VASE_WARN_FLAGS -Wall -Wextra)
endif()

if(VASE_WERROR)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        # cl.exe 不认 -Werror：它把 -Werror 当作 /Werror 解析（即 /W + 非数字参数），
        # 报**硬错误 D8021: invalid numeric argument '/Werror'**，不是可忽略的 D9002。
        # MSVC 的等价开关是 /WX。
        list(APPEND VASE_WARN_FLAGS /WX)
    else()
        # clang-cl 接受 -Werror（实测确实把 warning 提升成 error）。
        list(APPEND VASE_WARN_FLAGS -Werror)
    endif()
endif()

target_compile_options(VaseBuildOptions INTERFACE ${VASE_WARN_FLAGS})

if(WIN32)
    # 本仓库注释以中文为主，源码是 UTF-8。显式固定，免得受系统代码页影响。
    target_compile_options(VaseBuildOptions INTERFACE /utf-8)
endif()

# CMake 为 MSVC 默认追加的 CMAKE_CXX_FLAGS 里带 /EHsc。我们用自己的 /EHs-c- 去覆盖它，
# 会在每个 TU 上产生 D9025（"overriding '/EHs' with '/EHs-'"）——而 D 类命令行警告
# **不受 /WX 影响**，于是 MSVC 线永远达不到「零警告」的验收口径。
# 与其覆盖，不如先把它摘掉：没有覆盖就没有警告，也没有对命令行顺序的依赖。
#
# **位置要求（承重）**：这一块必须写在所有 `add_subdirectory` **之前**——
# 子目录在 `add_subdirectory` 那一刻对父作用域取快照。日后有人「整理 flag 块」
# 把它挪下去，会静默把 /EHsc 装回来，于是每个 TU 上重新出现 D9025
# （实测 cl.exe：`cl /c /EHsc /EHs-c- t.cpp` 输出两行 `Command line warning D9025`
# —— `overriding '/EHs' with '/EHs-'` 与 `overriding '/EHc' with '/EHc-'`，**exit 0**）。
# 而「静默」是三个条件叠出来的：构建**仍然绿**（D 类命令行警告不影响退出码）、
# /WX 管不到 D 类、于是验收判据「各自零警告」被违反而门禁不报。
#
# 顺带说明：`CMakeCache.txt` 里**仍然写着 /EHsc**，这是预期的——上面这一处摘除是
# 普通变量对缓存值的遮蔽，**没有改缓存**。去缓存里查证会看到它，别据此以为它没被摘掉。
#
# 注意 if(MSVC) 对 **clang-cl 也为真**（本仓库已知：clang-cl 下 CMAKE_CXX_COMPILER_ID
# 是 Clang，而 MSVC 变量为 1）。实测两条线的 CMAKE_CXX_FLAGS 里都带着 /EHsc，所以这一
# 段对两条线都有实际作用，不是只对 cl.exe 生效。
if(MSVC)
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

# 不使用 C++ 异常（架构文档 0.3 原则 7）：错误一律经 Result<T> / Error 显式返回。
#
# 理由不只是 ABI（异常不该穿过插件边界），也在于关掉之后「没有控制流能从
# 插件镜像里跳回宿主」由**编译期**强制，而不是靠约定。
#
# 两侧默认**不一致**，必须显式写（实测）：
#   clang-cl  默认就是关闭异常（不传 /EH 时 `try` / `throw` 报
#             "cannot use 'throw' with exceptions disabled"）——正合要求
#   clang++   默认开启，必须显式关闭
# 只写一侧，就会让同一份源码在两个平台行为不同。
if(WIN32)
    # /EHs-c-：关闭异常。已实测合法，且确实关闭（`try` 报错）。
    # 显式写出来而非吃默认值，是为了让意图可见、不被将来某个 /EH 开关改回去。
    #
    # **这里不存在 /EH 覆盖**：CMake 默认塞进 CMAKE_CXX_FLAGS 的 /EHsc 已在上方
    # 被摘掉，命令行上只剩这一个 /EH 开关，没有「后者胜」可依赖。
    # （先前此处写的是一条「顺序依赖（承重）」注释，说明靠 /EHs-c- 排在 /EHsc 之后
    # 才关得掉异常。那条前提已随 /EHsc 的摘除而消失，故改写——留着会把后来的人
    # 引向一个已经不存在的风险。）
    # 改动这一带时，仍请用「`try` 是否报错」实测复核，不要只看 flags 列表：
    # 这条不变式是编译期强制的核心，读 flags 不足以证明它成立。
    target_compile_options(VaseBuildOptions INTERFACE /EHs-c-)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        # cl.exe 与 clang-cl 在「关掉异常」这件事上**不等价**（实测）：
        #   clang-cl                 关闭异常时 try / throw / catch 全是硬错误 → 完整强制
        #   cl.exe（默认或 /EHs-c-）  只报 warning C4530，编译照过（exit=0）→ 强制是**软的**
        # /we4530 把 C4530 升为错误，补上 try/catch 这一半。
        #
        # **残余缺口**：裸 `throw`（同 TU 内没有 try/catch）cl.exe 完全静默接受
        # （实测 exit=0，加 /we4530 也一样）。这一形态靠 clang-cl 构建兜住——
        # clang-cl 连裸 throw 都拒。只跑 cl.exe 构建时它是漏网的，记在 spec 第 9 节。
        target_compile_options(VaseBuildOptions INTERFACE /we4530)

        # 导出类带 STL 成员（池、槽表、账本）时 cl.exe 报 C4251，/WX 下即 error；本项目有意
        # 这么写，前提由 §8.5 的工具链与 STL 矩阵钉死、D13 的 HeaderVersion 兜着。用全局豁免
        # 而不是头文件里的逐类 #pragma 区域，代价两条，都要如实接受：
        #   ① 整 TU 免检——新增「导出类带 STL 成员」不再有逐处把关；
        #   ② 出不了这棵树——树外的插件/嵌入方会在**我们的头**那一行撞上 C4251，得自己加。
        target_compile_options(VaseBuildOptions INTERFACE /wd4251)

        # 第三方头文件（目前只有 gtest）不进我们的警告口径：/external:anglebrackets 把
        # 「以尖括号包含的头文件」整体标记为外部，/external:W0 让它们的警告不参与 /WX。
        #
        # **已实测它挡得住什么、挡不住什么**（Task 3 修复轮 1）：
        #   挡得住：外部头里的普通警告。对照：同一个头里的 C4189，不加本开关会报，
        #           加了不报（模板实例化出来的 C4189 同样被挡）。
        #   挡不住：C4530。MSVC STL 的 `__msvc_ostream.hpp` 自带 `try/catch`
        #           （`_HAS_EXCEPTIONS` 实测在 /EHs-c- 下仍为 1，不随 /EH 关闭），
        #           gtest 的 printer 一旦实例化 `std::operator<<` 就把它拉进 TU；
        #           实测「加上 /external:anglebrackets /external:W0、且不加 /WX 也不加
        #           /we4530」时，C4530 仍照样从 `__msvc_ostream.hpp` 打印出来。
        #
        #   **但上面那句结论原先写得太宽，已按新实测收窄**（见 Cmake/VaseThirdParty.cmake）：
        #   对**我们自己在 -I 目录里、又以尖括号包含的第三方头**，本开关**摘得动**。实测同一份
        #   含 throw 的头在 cl.exe 线上：`<cli/cli.h>` → **0 条诊断**，`"cli/cli.h"` →
        #   **5 条 error C4530**。STL 那一支之所以不受影响，是因为它本来就走编译器自己的系统头
        #   处理，不在 anglebrackets 这条规则的射程里——所以原结论只对 STL 成立，不是一条普遍事实。
        #   实际后果：那条"try/catch 一定会被 /we4530 抓到"的保证，**对尖括号包含的第三方头不成立**。
        #
        # 不变式（实测复核，改动这一带时必须重跑）：**我们自己的代码不受影响**。
        # 仓库源码一律用引号包含（`#include "Vase/..."`），且编译的是自己的 .cpp；
        # 往 `Source/Pod/SmokeProbe.cpp` 里插一个 try/catch，cl.exe 线仍报
        # `error C4530`（exit 2），即 D17 的强制力没有被本开关削弱。
        #
        # **前向陷阱（写新代码时必须守）：Vase 自己的头文件一律用引号包含，
        # 不要写成 `#include <Vase/Plugin.h>`。** 本开关把**所有**尖括号包含的头
        # 标为外部，Vase 的公开头一旦被这么包含，它的警告就会被 /external:W0
        # **静默豁免**、绕过 /W4 /WX——而 wiki/vase-architecture.md 251 行给插件作者
        # 的示例恰恰是尖括号形式。今天仓库里还没有这种写法（评审已 grep，零命中），
        # 但第一个照文档写 sample 的人就会踩上：那时要么改 sample，要么给
        # Vase 的 Include 目录加 /external:I 之外的处理，不能就这么放着。
        # （这也是本开关唯一的已知前向风险，写在这里免得后来人重新发现一遍。）
        target_compile_options(VaseBuildOptions INTERFACE
            /external:anglebrackets
            /external:W0)
    endif()
else()
    target_compile_options(VaseBuildOptions INTERFACE -fno-exceptions)
endif()

# 让 MSVC STL 也走无异常路径。
#
# STL 用 `_HAS_EXCEPTIONS` 决定内部走不走异常路径，而这个宏**不由 /EHs-c- 推导**——
# MSVC STL 的 <yvals.h> 在我们没显式定义时一律默认它为 1。实测两条线的取值：
#
#   /EHs-c- 下    _CPPUNWIND 未定义     _HAS_EXCEPTIONS = 1
#   cl.exe        ✓                     ✓
#   clang-cl      ✓                     ✓
#
# 于是 STL 内部照常编译它的 try/catch。两侧的**表现**不同：
#   clang-cl —— 那处 try 不报错（**已实测**：同一 TU、同一组 flag，clang-cl 绿）。
#               至于**为什么**不报，未孤立验证，推测是 clang 把 MSVC STL 当系统头
#               抑制了诊断——但这是推测，不是结论，别当依据用。
#   cl.exe   —— 报 C4530；而 `/external:W0` 压不住这条 C4530（实测：它能挡住外部头里的
#               C4189、连模板实例化出来的也挡，唯独 MSVC STL 自己走编译器系统头处理、
#               不在 anglebrackets 射程里，故它那条 C4530 不受管辖。**注意范围**：对 `-I` 的
#               尖括号第三方头，本开关**压得住** C4530——实测与机制见 Cmake/VaseThirdParty.cmake），
#               `/we4530` 又是全局开关，无法只豁免 STL
#
# 显式置 0 之后两侧行为一致，并且顺带消掉「STL 内部仍会抛、穿过无异常栈帧」这条残余
# 风险（spec 9.4）。**注意 `if(MSVC)` 对 clang-cl 也为真，这正是我们要的**——两条
# Windows 线都置 0，不只在 cl.exe 上。
#
# **语义代价（M1 的设计输入，不只是工具链注记）**：置 0 之后 STL 的**前置条件失败**
# 不再抛，而是**直接终止进程**——`vector::at` 越界、`std::stoi` 解析失败、
# `<filesystem>` 的 error overload 等，全部变成 abort 而不是可以接住的异常。
# 在我们这套无异常策略下这是**预期**的（那些异常本来也无处可接、穿过无异常栈帧即 UB），
# 但它意味着：**错误处理必须走 Result<T> / Error 显式返回，不能指望 STL 的前置条件检查
# 给出可恢复的失败**。M1 设计公开面时请把这一条算进去。
#
# 已知代价（需求方 2026-09-15 已知情并选定）：`_HAS_EXCEPTIONS=0` 是 MSVC STL 的
# **非官方支持**配置，属长期承诺；STL 升级时需要重新验证这一条。
if(MSVC)
    target_compile_definitions(VaseBuildOptions INTERFACE _HAS_EXCEPTIONS=0)
endif()

# 插件 target 的形态定义（vase_add_plugin_fixture）：Samples 与 Tests 都要用，
# 故在 add_subdirectory 之前就位。
include(Cmake/VasePluginHelpers.cmake)

# 第三方「只有头文件」依赖（VaseThirdPartyCli → ThirdParty/cli）的编译面。
# 走自立 INTERFACE target 而非 add_subdirectory / vcpkg，理由与「不得标成 system 头」那条
# 禁令都在文件里——它关乎异常不变式能否被编译期把守，不是风格选择。
include(Cmake/VaseThirdParty.cmake)

enable_testing()

add_subdirectory(Source/Pod)
add_subdirectory(Source/Host)

# Samples 在 Tests 之前：Tests 与 VaseEmbedding 都要用 $<TARGET_FILE:HelloPlugin>，
# 生成器表达式求值时该 target 必须已存在。
add_subdirectory(Samples/HelloPlugin)
# 换件材料要在 Tools/VaseConsole **之前**：VaseConsole 的 file(GENERATE) 里有
# $<TARGET_FILE:HelloPluginPrime>，求值时该 target 必须已存在。
add_subdirectory(Samples/HelloPluginPrime)
add_subdirectory(Samples/Embedding)
add_subdirectory(Tools/VaseConsole)

add_subdirectory(Tests)
