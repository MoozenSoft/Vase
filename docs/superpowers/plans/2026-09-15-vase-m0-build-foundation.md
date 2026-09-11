# Vase M0（构建地基）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Win x64 与 Linux x64 两侧建立起可构建、可测试、可静态检查的地基，并用一个跨 DLL 冒烟测试证明「导出宏 / 动态库链接 / 运行时库查找路径」这条链路成立。

**Architecture:** 顶层 CMake 只放工程级约定（C++20、警告策略、输出目录、工具链 doctor），平台差异全部收在两个工具链文件里；每个工具链文件内部 `include` vcpkg 的工具链，从而绕开「CMake 只允许一个 `CMAKE_TOOLCHAIN_FILE`」的限制。工程被切成两个动态库 target（`VaseSession` / `VaseHost`），使「插件不依赖 Host」从约定变成链接期事实。

**Tech Stack:** CMake 4.4 + Ninja + clang/clang-cl 23.1.0 + vcpkg（manifest 模式，`builtin-baseline` 钉定）+ GoogleTest。

**Spec:** [`docs/superpowers/specs/2026-09-14-vase-m0-m1-design.md`](../specs/2026-09-14-vase-m0-m1-design.md)（上游架构：[`wiki/vase-architecture.md`](../../../wiki/vase-architecture.md)）

---

## 本计划的范围

**只覆盖 M0。** M1 另起一份计划，理由是 spec 第 4 节已给出的：M0 不产生任何架构结论，它的产出（真实可用的 CMake 变量、真实的工具链行为）会成为 M1 计划的输入；M0 本身也是一个独立可验收的交付物（spec 2.4 的六条判据不依赖任何架构代码）。

本计划实施完毕后应停下、验收、再开 M1 的计划——不要顺手往下写 M1 的头文件。

---

## Global Constraints

以下约束对**每一个** task 都成立，不再逐条重复。

- **语言标准**：C++20，`CMAKE_CXX_EXTENSIONS OFF`。
- **CMake 下限**：`cmake_minimum_required(VERSION 4.4)`；`CMakePresets.json` 的 `"version"` 取 **12**。不写 `<min>...<max>`。
- **vcpkg**：manifest 模式，`builtin-baseline` = `114d9fe62faf35856b45cf55cb93b57028a45d63`（两侧已实测一致）。**不使用 submodule。**
- **编译器**：clang / clang-cl **23.1.0**（同源 commit `ea7d852a`），**由 PATH 解析，工具链文件里不写死任何安装路径**。换 LLVM 只改 PATH，不动仓库。
- **Windows 上支持两个编译器**：clang-cl **与** cl.exe（8.5：同一 ABI 的两个前端，都用 MSVC STL）。各自一套 preset。支持两者的意义是让「宿主与插件可以各用一个」不只是纸面约定。**一条操作差异**：clang-cl 能自行发现 MSVC（实测无需 Developer 环境），**cl.exe 需要 VS Developer 环境**。
- **Triplet**：Windows 用内置 `x64-windows`（**禁止 `x64-windows-static`**）；Linux 用自定义 `x64-linux-libcxx`。
- **动态 CRT（8.3）**：跨模块 `new`/`delete` 必须落在同一个堆，因此**必须** `/MD`。注意这条同时是**驱动的默认值的反面**——实测 clang 与 clang-cl 在 Windows 上的默认都是静态 CRT（`libcmt`）。triplet 只管 vcpkg 构建的依赖，**我们自己的 target 由 `CMAKE_MSVC_RUNTIME_LIBRARY` 决定**，计划里显式设置并用产物验证。
- **不使用 C++ 异常**（架构文档 0.3 原则 7）：全项目 `-fno-exceptions`（Windows 侧 `/EHs-c-`，已实测合法且确实关异常）。错误一律经 `Result<T>` / `Error` 显式返回。
- **总原则**：凡影响 ABI 的默认值（CRT、异常模型、STL），**一律显式写出并验证**，不靠驱动或 CMake 的默认值兜着——插件与宿主可能由不同工具链构建，默认值不一致的代价在运行期才付。
- **警告**：警告即错误从 M0 就开，做成 `VASE_WERROR` 选项。但**三个编译器不能用同一组 flag 写法**：实测 clang-cl 的 `-Wall` 等价于 `/Wall` 即 `-Weverything`（GNU 驱动的 `-Wall` 才是精选集），所以警告等级是 Windows `/W4` / Linux `-Wall -Wextra`；而**警告即错误**又是另一回事——`cl.exe` 把 `-Werror` 当 `/Werror` 解析并报**硬错误 D8021**，它要用 `/WX`。两条都别想「一个列表伺候所有编译器」。
- **命名**：由 `.clang-tidy` 的 `readability-identifier-naming.*` 强制——命名空间 `lower_case`、类型/函数/成员 `CamelCase`、参数与局部变量 `camelBack`、全局变量 `gCamelCase`、各类常量 `kCamelCase`。
- **格式**：以 `.clang-format` 为准（Allman、4 空格、`PointerAlignment: Left`、`ColumnLimit: 120`）。**它是权威**——本计划里的代码片段若与 `clang-format` 的输出不一致，改代码片段，不改配置。
- **注释语言**：中文为主，专业术语可保留英文。**例外：`.cmd` / `.bat` 必须纯 ASCII**——cmd.exe 按 OEM 代码页解码批处理，中文会吞掉行尾 CRLF 并把下一行并进来，直接把脚本解析坏掉（实测，含 BOM 与 `chcp 65001` 两个无效对策）。理由见 Task 1 Step 6b。
- **只让有内容的 target 存在**：不建空壳 target。
- **构建树位置**：Windows `build-win/<presetName>/`，Linux `build-linux/<presetName>/`（`.gitignore` 的 `build-*/` 已覆盖）。

### 相对 spec 2.3 的增补

spec 2.3 的 M0 文件清单里没有下面这个文件。它的存在理由是 spec 自己给的：验收 #4 要求「VaseSession 导出符号 → VaseHost 导入并调用 → GoogleTest 断言」，而**导入方需要一个可见的声明**。

| 文件 | 性质 |
|---|---|
| `Include/Vase/Detail/SmokeProbe.h` | **M0 专用**，落在 `Detail/` 这个既定的内部目录里。M1 的公开面落地后**删除本文件**。它存在的唯一目的是让冒烟测试不必重复声明（声明只写一处）。 |

另有一处**与 spec 2.3 的字面描述有出入**，在此说明而不是静默处理：spec 把 `Include/Vase/Plugin.h` 描述为「最小公开面 + VASE_EXPORT」。M0 阶段尚无任何插件接口可言，本计划让该文件只承担两件事——**占住架构文档第 10 节已确认的目录位置**，以及**接上导出宏**。`VASE_PLUGIN` 宏、`Plugin` 基类、`Context`、`Result`、`Error` 全部留到 M1。

理由即 spec 自己写的那条原则：「只让有内容的 target 存在」。若 reviewer 认为 M0 连这个锚点都不该建，删掉该文件不影响本计划的任何其他步骤。

---

## 文件结构

M0 结束时仓库里应当恰好有这些新文件：

```text
CMakeLists.txt                                  顶层：工程约定 + 编译器 doctor + 子目录
CMakePresets.json                               6 个 configure preset（3 条构建线 × debug/release）
vcpkg.json                                      manifest；gtest；builtin-baseline
README.md                                       （修改）环境前提 + 构建与验收命令
CLAUDE.md                                       （修改）补上真实的 build/test 命令
Scripts/
└── msvc-env.cmd                                用 vswhere + vcvars64 包装 MSVC 那条线
Cmake/
├── Toolchains/
│   ├── windows-x64-clangcl.cmake               Windows 工具链（clang-cl，主构建）
│   ├── windows-x64-msvc.cmake                  Windows 工具链（cl.exe，需 Developer 环境）
│   └── linux-x64-clang-libcxx.cmake            Linux 平台工具链
└── Triplets/
    └── x64-linux-libcxx.cmake                  Linux 自定义 triplet（libc++）
Include/Vase/
├── Plugin.h                                    插件作者入口的位置锚点（内容在 M1）
└── Detail/
    ├── Export.h                                可见性宏 + 各库的 *_API 宏
    └── SmokeProbe.h                            M0 专用，M1 删除
Source/
├── Session/CMakeLists.txt + SmokeProbe.cpp     → VaseSession.dll / libVaseSession.so
└── Host/CMakeLists.txt    + SmokeProbe.cpp     → VaseHost.dll    / libVaseHost.so
Tests/
├── CMakeLists.txt                              接 ctest
└── Smoke/CrossDllSmoke.cpp                     验收 #4 的载体
```

---

## Task 1: Windows 构建地基（vcpkg + 工具链 + presets + 编译器 doctor）

这是 spec 风险表里**唯一标注「完全未实测」**的一环（「vcpkg 与 CMake 4.4.x 的接线」），也是后面所有测试任务的硬前提——vcpkg 解不出 gtest，Task 3 就无从谈起。所以它排第一。

**Files:**
- Create: `vcpkg.json`
- Create: `CMakeLists.txt`
- Create: `Cmake/Toolchains/windows-x64-clangcl.cmake`
- Create: `CMakePresets.json`
- Modify: `README.md`（新增「环境前提」一节）

**Interfaces:**
- Produces: 供后续所有 task 使用的 configure/build 入口——`cmake --preset win-x64-clang-debug` / `win-x64-clang-release`；供所有 target 链接的接口 target `VaseBuildOptions`；输出目录约定 `${CMAKE_BINARY_DIR}/bin`（可执行与 DLL）与 `${CMAKE_BINARY_DIR}/lib`。

---

- [ ] **Step 1: 写 `vcpkg.json`**

```json
{
  "name": "vase",
  "version-string": "0.1.0",
  "description": "插件管理能力库",
  "dependencies": [
    "gtest"
  ],
  "builtin-baseline": "114d9fe62faf35856b45cf55cb93b57028a45d63"
}
```

`builtin-baseline` 已在本机两侧核对为同一 commit（`114d9fe62faf35856b45cf55cb93b57028a45d63`），它承担跨平台的版本钉定职责。

- [ ] **Step 2: 写顶层 `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 4.4)

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
# 测试可执行文件与 VaseSession/VaseHost 同处 bin/ 才能直接跑起来。
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

# 摘掉 CMake 默认为 MSVC 追加的 /EHsc。
#
# 不摘掉的话，它会和我们下面显式的 /EHs-c- 形成覆盖，在每个 TU 上产生
# `D9025 : overriding '/EHs' with '/EHs-'`——而 **D 类命令行警告不受 /WX 影响**，
# 于是 MSVC 线永远达不到「零警告」的验收口径（构建仍是绿的，所以只会在 Task 6
# 对验收判据时才发现）。与其覆盖，不如先摘掉：没有覆盖就没有警告，
# 也没有对命令行顺序的依赖。
#
# **位置要求（承重）**：这一块必须写在所有 `add_subdirectory` **之前**——
# 子目录在 `add_subdirectory` 那一刻对父作用域取快照。日后有人「整理 flag 块」
# 把它挪下去，会静默把 /EHsc 装回来并带回那四组警告。
#
# 注意 `if(MSVC)` 对 **clang-cl 也为真**（本仓库已知：clang-cl 下
# CMAKE_CXX_COMPILER_ID 是 Clang，而 MSVC 变量为 1）。实测两条线的
# CMAKE_CXX_FLAGS 里都带着 /EHsc，所以这一段对两条线都有实际作用。
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
    # 改动这一带时，仍请用「`try` 是否报错」实测复核，不要只看 flags 列表——
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
#   cl.exe   —— 报 C4530；而 `/external:W0` 管不到 C4530（实测：它能挡住外部头里的
#               C4189、连模板实例化出来的也挡，唯独 C4530 不受管辖），
#               `/we4530` 又是全局开关，无法只豁免 STL
#
# 显式置 0 之后两侧行为一致，并且顺带消掉「STL 内部仍会抛、穿过无异常栈帧」这条残余
# 风险（spec 9.4）。**注意 `if(MSVC)` 对 clang-cl 也为真，这正是我们要的**——两条
# Windows 线都置 0，不只在 cl.exe 上。
#
# 已知代价（需求方 2026-09-15 已知情并选定）：`_HAS_EXCEPTIONS=0` 是 MSVC STL 的
# **非官方支持**配置，属长期承诺；STL 升级时需要重新验证这一条。
#
# **另一条代价：它会改变 STL 异常类的 vtable。** `exception:66` 起是 `!_HAS_EXCEPTIONS`
# 分支，其中 `exception:131` 给 `std::exception` **加了一个 virtual 成员**
# `virtual void _Doraise() const {}`，各异常类的 override 也都在 `#if !_HAS_EXCEPTIONS`
# 之下。于是我们 TU 里的 `std::exception` / `runtime_error` / `system_error` /
# `filesystem_error` … 与 **vcpkg 预编译的库**（gtest 走 `x64-windows` triplet，
# 仍按 `_HAS_EXCEPTIONS=1` 构建）**vtable 不一致**。
#
# 为什么判定不需要动 triplet：`_Doraise` 的**全部**出现点（已核）都在异常类定义里
# （`exception` / `stdexcept` / `system_error` / `filesystem` / `future` / `typeinfo` /
# `xiosbase` / `optional` / `expected` / `execution`），而 D17 本来就禁止我们抛/接异常，
# 这些类型**本就不该跨模块传递**——分歧被限制在我们已被禁止使用的类型上。
#
# **未被穷尽审计的残余**（如实记下，别当成「已证明无问题」）：除 `_Doraise` 外还有若干
# `#if _HAS_EXCEPTIONS` 条件块（`chrono` / `xlocale` / `yvals.h` / `__msvc_ostream.hpp` 等），
# 其中已知的有 `_THROW` / `_RAISE` 的**行为**差异（inline，不影响布局）；至于有没有
# **非异常类型**的布局也随该宏变化，没查到穷尽。若要闭合，做法是编一个对常见 STL 类型
# 做 `sizeof` / `offsetof` 断言的探针，在两个宏取值下各编一次做对照——M0 未做。
if(MSVC)
    target_compile_definitions(VaseBuildOptions INTERFACE _HAS_EXCEPTIONS=0)
endif()

enable_testing()

# 子目录在这里逐个加入——Task 2 加 Source/Session 与 Source/Host，Task 3 加 Tests。
```

- [ ] **Step 3: 写两个 Windows 平台工具链文件**

**3a. `Cmake/Toolchains/windows-x64-clangcl.cmake`**

```cmake
# 平台工具链：Windows x64 + clang-cl（MSVC ABI + MSVC STL）。
#
# 接线方向（spec 2.2）：CMake 只允许一个 CMAKE_TOOLCHAIN_FILE，因此定为
# 「平台工具链文件是它，文件内部 include vcpkg 的」，使平台配置先于 vcpkg 生效。

if(NOT DEFINED ENV{VCPKG_ROOT} OR "$ENV{VCPKG_ROOT}" STREQUAL "")
    message(FATAL_ERROR
        "环境变量 VCPKG_ROOT 未设置。\n"
        "CMakePresets 用 \$env{VCPKG_ROOT} 之外的路径无法定位 vcpkg 工具链，空值会让 configure 在这里失败。\n"
        "Windows 侧请设置 VCPKG_ROOT=D:/Developer/vcpkg（钉定的 commit 见 vcpkg.json 的 builtin-baseline）。")
endif()

# D6：clang-cl 由 PATH 解析，不写死安装路径——换 LLVM 只改 PATH，不动仓库。
set(CMAKE_CXX_COMPILER clang-cl CACHE STRING "Windows 平台 C++ 编译器" FORCE)

# 8.3：跨模块 new/delete 必须落在同一个堆——动态 CRT + 动态库。
set(VCPKG_TARGET_TRIPLET x64-windows)

include("$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
```

> 注意：`VCPKG_TARGET_TRIPLET` 必须在 `include` 之前设置，否则 vcpkg 的工具链会先用默认值（`x64-windows`）把状态算一遍。
>
> **`$env{...}` 在 `message()` 的引号参数里必须转义成 `\$env{...}`。** 不转义时 CMake 会在解析期展开它，而 `$env{}` 不是合法语法，于是报 `Syntax $env{{} is not supported`——**报错指向工具链文件的语法，而不是「VCPKG_ROOT 未设置」**，恰好把这条守卫的全部价值抵消掉。只 `\$` 一处，其余一字不改；转义后产生字面 `$`。

**3b. `Cmake/Toolchains/windows-x64-msvc.cmake`**

```cmake
# 平台工具链：Windows x64 + cl.exe。
#
# 与 windows-x64-clangcl.cmake 是**同一 ABI 的两个前端**（8.5）：都目标 MSVC ABI、
# 都用 MSVC STL。Vase 本体两套都支持，让「宿主与插件可以各用一个」不只停在纸面。
#
# 与 clang-cl 的关键差别：**cl.exe 需要 VS Developer 环境**（PATH / INCLUDE / LIB），
# 它不像 clang-cl 那样能自行经注册表发现 MSVC。因此这套 preset 要用
# Scripts/msvc-env.cmd 启动（它负责 call vcvars64.bat）。

if(NOT DEFINED ENV{VCPKG_ROOT} OR "$ENV{VCPKG_ROOT}" STREQUAL "")
    message(FATAL_ERROR
        "环境变量 VCPKG_ROOT 未设置。\n"
        "Windows 侧请设置 VCPKG_ROOT=D:/Developer/vcpkg（钉定的 commit 见 vcpkg.json 的 builtin-baseline）。\n"
        "注意：vcvars64.bat 会把 VCPKG_ROOT **覆盖**成 VS 自带的那份 vcpkg（不是清空）。"
        "Scripts/msvc-env.cmd 会还原调用者的值；调用者本就没有时它会清掉，好让这里响亮报错。")
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
set(VCPKG_TARGET_TRIPLET x64-windows)

include("$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
```

`VSCMD_VER` / `VCINSTALLDIR` 由 vcvars 设置，用它们做「是否在 Developer 环境里」的哨兵，比事后解析 `cl` 找不到的报错要清楚得多。

- [ ] **Step 4: 写 `CMakePresets.json`**

```json
{
  "version": 12,
  "cmakeMinimumRequired": { "major": 4, "minor": 4, "patch": 0 },
  "configurePresets": [
    {
      "name": "win-x64-clang",
      "hidden": true,
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build-win/${presetName}",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/Cmake/Toolchains/windows-x64-clangcl.cmake",
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
      }
    },
    {
      "name": "win-x64-clang-debug",
      "inherits": "win-x64-clang",
      "displayName": "Windows x64 / clang-cl / Debug",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" }
    },
    {
      "name": "win-x64-clang-release",
      "inherits": "win-x64-clang",
      "displayName": "Windows x64 / clang-cl / Release",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" }
    },
    {
      "name": "win-x64-msvc",
      "hidden": true,
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build-win/${presetName}",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/Cmake/Toolchains/windows-x64-msvc.cmake",
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
      }
    },
    {
      "name": "win-x64-msvc-debug",
      "inherits": "win-x64-msvc",
      "displayName": "Windows x64 / cl.exe / Debug",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" }
    },
    {
      "name": "win-x64-msvc-release",
      "inherits": "win-x64-msvc",
      "displayName": "Windows x64 / cl.exe / Release",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" }
    }
  ],
  "buildPresets": [
    { "name": "win-x64-clang-debug", "configurePreset": "win-x64-clang-debug" },
    { "name": "win-x64-clang-release", "configurePreset": "win-x64-clang-release" },
    { "name": "win-x64-msvc-debug", "configurePreset": "win-x64-msvc-debug" },
    { "name": "win-x64-msvc-release", "configurePreset": "win-x64-msvc-release" }
  ],
  "testPresets": [
    {
      "name": "win-x64-clang-debug",
      "configurePreset": "win-x64-clang-debug",
      "output": { "outputOnFailure": true }
    },
    {
      "name": "win-x64-clang-release",
      "configurePreset": "win-x64-clang-release",
      "output": { "outputOnFailure": true }
    },
    {
      "name": "win-x64-msvc-debug",
      "configurePreset": "win-x64-msvc-debug",
      "output": { "outputOnFailure": true }
    },
    {
      "name": "win-x64-msvc-release",
      "configurePreset": "win-x64-msvc-release",
      "output": { "outputOnFailure": true }
    }
  ]
}
```

- [ ] **Step 5: 验证 doctor 真的会响（红）**

```bash
cd /d/Git/Vase
VCPKG_ROOT= cmake --preset win-x64-clang-debug
```

期望：**失败**，且错误信息是我们自己那句 `环境变量 VCPKG_ROOT 未设置。`，不是 CMake 或 vcpkg 的原始报错。

这一步是在验证 spec 2.1.1(b) 担心的场景：`VCPKG_ROOT` 为空时，报错若不指向环境变量，而指向工具链文件，排查成本会高得多。

- [ ] **Step 6: 正常 configure（绿）**

```bash
cd /d/Git/Vase
cmake --preset win-x64-clang-debug
```

期望：输出以 `-- Configuring done` / `-- Generating done` 结束，无 error。

> **此处会有一条 `unused-cli` warning**（`CMAKE_EXPORT_COMPILE_COMMANDS` 未被使用），**属预期，不要为它改 preset**。已用对照实验定位：临时加一个真实 target → warning 消失、`compile_commands.json` 正常产出；撤掉 → 复现。成因是「当前没有任何产出编译命令的 target」。Task 2/3 加入真实 target 后应自行消失——**若那时仍在，说明这个推断是错的，回来重新定位。**

> **可能撞上的坑（spec 2.1.1(a)）**：若报错来自**第三方依赖**的 `CMakeLists.txt` 声明了 `cmake_minimum_required(VERSION <3.5)`，在 preset 的 `cacheVariables` 里加 `"CMAKE_POLICY_VERSION_MINIMUM": "3.5"` 后重试。这是 CMake 4.x 时代依赖构建失败最常见的原因，且报错指向依赖库而非我们的代码。
>
> **另一处需要留意**：若报 clang-cl 找不到 MSVC 头/库，先确认 `clang-cl --version` 能从 PATH 解析到 23.1.0。本机已实测裸 clang-cl 无需 Developer 环境即可编译、链接、运行（MSVC 头/库经注册表自动发现），因此这一步**不应**需要 VS Developer 环境。

- [ ] **Step 6b: 写 `Scripts/msvc-env.cmd`，并用它跑通 MSVC 的 configure**

cl.exe 依赖 vcvars 设置的 `PATH` / `INCLUDE` / `LIB`，而每次构建都手工 `call vcvars64.bat` 不可复现。用一个包装脚本把这件事收口——它也是**人**跑 MSVC 那条线的入口。

`Scripts/msvc-env.cmd`：

```bat
@echo off
rem Usage: Scripts\msvc-env.cmd <command...>
rem
rem Why this script exists: cl.exe depends on PATH / INCLUDE / LIB being set by
rem vcvars64.bat, while clang-cl does not (it discovers MSVC through the
rem registry). This script funnels vcvars into one entry point, so the MSVC
rem build line is as simple to use as the clang-cl one.
rem
rem vswhere is used to locate VS instead of hardcoding a versioned path --
rem the same principle as the toolchain file not hardcoding an LLVM path.
rem
rem Two cmd.exe quirks are worked around below.
rem
rem 1. ENCODING -- KEEP THIS FILE PURE ASCII.
rem    cmd.exe decodes batch files with the OEM code page, not UTF-8. A
rem    non-ASCII line whose last byte happens to look like a lead byte in that
rem    code page swallows the following CRLF, merging it with the next line;
rem    the parser then runs the merged text as a command. A UTF-8 BOM is worse
rem    (even "@echo off" fails), and `chcp 65001` does not help because cmd has
rem    already buffered the file.
rem
rem 2. DELAYED EXPANSION -- parentheses inside expanded paths.
rem    %ProgramFiles(x86)% itself is fine, but once VSWHERE holds a path
rem    containing "(x86)", writing `if not exist "%VSWHERE%" (` breaks: the
rem    parser scans for the block-opening "(" before expanding the variable and
rem    treats the ")" in "(x86)" as closing it, yielding
rem    "\Microsoft was unexpected at this time." Quotes do not protect it.
rem    Expanding with !VSWHERE! defers substitution to execution time, so the
rem    parser only ever sees the paren-free literal.

setlocal enabledelayedexpansion

rem vcvars64.bat overrides VCPKG_ROOT with the vcpkg bundled inside Visual
rem Studio (observed: VS 18 sets it to ...\VC\vcpkg). Vase pins one specific
rem vcpkg -- see the builtin-baseline in vcpkg.json -- so remember the caller's
rem value and restore it after vcvars. Without this, the MSVC preset would
rem silently configure against a different vcpkg than the clang-cl preset, and
rem a non-empty-but-wrong VCPKG_ROOT slips past the toolchain file's guard.
set "VASE_VCPKG_ROOT=%VCPKG_ROOT%"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" (
    echo [msvc-env] vswhere.exe not found: !VSWHERE!
    echo [msvc-env] Install the Visual Studio C++ build tools.
    exit /b 1
)

set "VSINSTALL="
for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if "!VSINSTALL!"=="" (
    echo [msvc-env] vswhere found no VS install with the C++ tools.
    exit /b 1
)

call "!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo [msvc-env] vcvars64.bat failed.
    exit /b 1
)

rem Restore the caller's VCPKG_ROOT. When the caller had none, CLEAR the value
rem vcvars just installed instead of keeping it: a non-empty-but-wrong
rem VCPKG_ROOT passes the toolchain file's "is it set?" guard, so leaving VS's
rem bundled vcpkg in place would silently configure the MSVC preset against a
rem non-pinned vcpkg (observed: it then tries to fetch the registry over the
rem network and hangs). Clearing makes that case fail loudly at the guard,
rem which is what we want on a machine or CI job that never set VCPKG_ROOT.
if "!VASE_VCPKG_ROOT!"=="" (set "VCPKG_ROOT=") else (set "VCPKG_ROOT=!VASE_VCPKG_ROOT!")

%*
exit /b %ERRORLEVEL%
```

> **这个文件是「中文注释为主」的一处有意例外，必须保持纯 ASCII。** 上面第 1 条注释写的成因是实测的：cmd.exe 按 **OEM 代码页**（本机 cp936）解码批处理，中文注释行末尾的 UTF-8 字节落入前导字节区间时会**吞掉 CRLF 并把下一行并进来**，合并后的文本不再以 `rem` 开头，于是被当命令执行。后果不止噪音——中文 `echo` 行末尾的 `。` 同样吞 CRLF，会把紧随的 `exit /b 1` 并进 `echo` 里，**守卫静默失效**。
>
> 两个常见对策都已实测排除：**UTF-8 BOM 更糟**（连 `@echo off` 都认不出），**`chcp 65001` 无效**（cmd 已缓冲文件）。纯 ASCII 下 **LF 与 CRLF 都能跑**，承重性质是「ASCII」而非「CRLF」。

用法与验证：

```bash
cd /d/Git/Vase
Scripts/msvc-env.cmd cmake --preset win-x64-msvc-debug
```

期望：以 `-- Configuring done` / `-- Generating done` 结束。

> **这一步在 Task 1 只验 configure。** 此时仓库里还没有任何 target，build 与 ctest 都是空转——构建、测试、以及 `/we4530` 的强制力验证都在 **Task 2**（那里才有 `Source/Session/SmokeProbe.cpp`）。
>
> **vcvars64.bat 会覆盖（不是清空）`VCPKG_ROOT`**，指向 VS 自带的那份 vcpkg（实测 VS 18 → `...\VC\vcpkg`）。`Scripts/msvc-env.cmd` 会还原调用者的值；**调用者本就没有时它会清掉**，好让工具链守卫响亮报错，而不是静默用上非钉定的 vcpkg（那条路实测会联网取 registry 并挂住）。所以：想让 MSVC 那条线跑起来，`VCPKG_ROOT` 必须由你自己设好——它不会替你猜。
>
> 若报 `cl.exe 需要 VS Developer 环境`，说明哨兵变量（`VSCMD_VER` / `VCINSTALLDIR`）没看到——检查 `msvc-env.cmd` 是否真的 call 成功了。

- [ ] **Step 7: 确认 vcpkg 解出了 gtest，并记下版本**

```bash
cd /d/Git/Vase
ls build-win/win-x64-clang-debug/vcpkg_installed/x64-windows/share/gtest/
cat build-win/win-x64-clang-debug/vcpkg_installed/x64-windows/share/gtest/vcpkg.spdx.json 2>/dev/null | grep -m1 version
```

期望：目录存在，且能记下 gtest 的具体版本号——Task 5 要在 Linux 侧核对**同一版本**（spec 2.4 验收 #6）。

- [ ] **Step 8: 在 `README.md` 补「环境前提」一节**

`README.md` 目前只有两行。追加：

```markdown
## 环境前提

- **CMake ≥ 4.4**，**Ninja**（任一近期版本）。
- **clang / clang-cl 23.1.x**，且**由 PATH 解析**。Windows 侧把 LLVM 的 `bin`
  目录加入 PATH 最前（本机为 `D:\Developer\LLVM\bin`）。仓库不写死任何 LLVM
  安装路径，换版本只改 PATH。
  - 两侧 LLVM 必须同版本：`clang-format` / `clang-tidy` 的判据一致性依赖此条。
  - 版本不符时 `cmake` 会在 configure 阶段直接报错，不会静默降级。
- **vcpkg**，钉定 commit `114d9fe62faf35856b45cf55cb93b57028a45d63`，
  并由环境变量 `VCPKG_ROOT` 指向其根目录（Windows：`D:\Developer\vcpkg`）。
  - Linux/WSL：`VCPKG_ROOT` 由 `/etc/profile.d/vcpkg.sh` 提供，登录 shell 可见；
    **非登录非交互的 `bash -c` 拿不到它**，这类调用方（CI 步骤、构建钩子）必须
    自己显式带上 `VCPKG_ROOT`。这是 shell 的加载规则，配置改不了。
- **Git Bash 下手动调用 clang-cl 时注意**：MSYS 会把 `/MD`、`/nologo` 这类
  `/` 开头的参数当成路径转换掉（实际报错会指向 `D:/Program Files/Git/MD`
  之类的路径）。手动调用前先 `export MSYS2_ARG_CONV_EXCL='*'`，或改用等价的
  `-` 前缀写法。CMake 自己调编译器不受影响，只有命令行手敲时才会撞上。
```

- [ ] **Step 9: Commit**

```bash
cd /d/Git/Vase
git add vcpkg.json CMakeLists.txt CMakePresets.json Cmake/ README.md
git commit -m "构建地基：CMake 4.4 + vcpkg manifest + clang-cl 工具链与 Windows presets"
```

---

## Task 2: 两个动态库 target 与跨模块导出

**Files:**
- Create: `Include/Vase/Detail/Export.h`
- Create: `Include/Vase/Detail/SmokeProbe.h`
- Create: `Include/Vase/Plugin.h`
- Create: `Source/Session/CMakeLists.txt`、`Source/Session/SmokeProbe.cpp`
- Create: `Source/Host/CMakeLists.txt`、`Source/Host/SmokeProbe.cpp`
- Modify: `CMakeLists.txt`（加两个 `add_subdirectory`）

**Interfaces:**
- Consumes: Task 1 的 `VaseBuildOptions`、输出目录约定。
- Produces: 供 Task 3 使用——
  - `std::uint32_t vase::SessionSmokeProbe();`（由 `VaseSession` 导出）
  - `std::uint32_t vase::HostSmokeProbe();`（由 `VaseHost` 导出，内部调用前者）
  - CMake target `VaseSession`、`VaseHost`（`VaseHost` 以 `PUBLIC` 链接 `VaseSession`）
  - 宏 `VASE_EXPORT` / `VASE_IMPORT` / `VASE_HIDDEN` / `VASE_SESSION_API` / `VASE_HOST_API`

---

- [ ] **Step 1: 写 `Include/Vase/Detail/Export.h`**

```cpp
#pragma once

// 跨动态库的符号可见性。
//
// Windows：符号默认不导出，必须显式 dllexport。
// Linux/macOS：符号默认全部可见，因此要靠 -fvisibility=hidden 收紧（见 CMake 侧
//   的 CXX_VISIBILITY_PRESET），再由 VASE_EXPORT 把该露的放出来。

#if defined(_WIN32)
#    define VASE_EXPORT __declspec(dllexport)
#    define VASE_IMPORT __declspec(dllimport)
#    define VASE_HIDDEN
#else
#    define VASE_EXPORT __attribute__((visibility("default")))
#    define VASE_IMPORT __attribute__((visibility("default")))
#    define VASE_HIDDEN __attribute__((visibility("hidden")))
#endif

// 各动态库自己的公开 API：构建该库时导出，其它地方导入。
// 构建宏由 CMake 侧 target_compile_definitions(<target> PRIVATE VASE_<LIB>_BUILD) 提供。
#if defined(VASE_SESSION_BUILD)
#    define VASE_SESSION_API VASE_EXPORT
#else
#    define VASE_SESSION_API VASE_IMPORT
#endif

#if defined(VASE_HOST_BUILD)
#    define VASE_HOST_API VASE_EXPORT
#else
#    define VASE_HOST_API VASE_IMPORT
#endif
```

- [ ] **Step 2: 写 `Include/Vase/Detail/SmokeProbe.h`**

```cpp
#pragma once

// M0 冒烟专用。M1 的公开面落地后删除本文件。
//
// 存在理由（spec 2.4 验收 #4）：把「导出宏 / 动态库链接 / 运行时库查找路径」
// 这三件最容易在 M1 才炸的事，提前到还没有任何架构代码时撞上。

#include "Vase/Detail/Export.h"

#include <cstdint>

namespace vase
{

// 定义在 VaseSession。
VASE_SESSION_API std::uint32_t SessionSmokeProbe();

// 定义在 VaseHost，内部调用 SessionSmokeProbe()——让这次调用真的跨过动态库边界。
VASE_HOST_API std::uint32_t HostSmokeProbe();

}  // namespace vase
```

- [ ] **Step 3: 写 `Include/Vase/Plugin.h`**

M0 只建立这个文件的位置与导出宏的接线；宏、`Plugin` 基类、`Context` 在 M1 落地。

```cpp
#pragma once

// 插件作者的唯一入口。
//
// M0 只在这里建立目录位置与导出宏的接线（架构文档第 10 节的目录布局已确认）；
// VASE_PLUGIN 宏、Plugin 基类、Context、Result、Error 在 M1 落地。

#include "Vase/Detail/Export.h"
```

- [ ] **Step 4: 写 `Source/Session/SmokeProbe.cpp`**

```cpp
#include "Vase/Detail/SmokeProbe.h"

namespace vase
{

std::uint32_t SessionSmokeProbe()
{
    // 取值本身无意义；用一个非平凡常量，避免被优化成一眼可判定的常数。
    return 0x56415345u;  // "VASE"
}

}  // namespace vase
```

- [ ] **Step 5: 写 `Source/Session/CMakeLists.txt`**

```cmake
add_library(VaseSession SHARED
    SmokeProbe.cpp)

target_include_directories(VaseSession
    PUBLIC "${PROJECT_SOURCE_DIR}/Include")

target_compile_definitions(VaseSession
    PRIVATE VASE_SESSION_BUILD)

# Linux/macOS 上默认可见性是全开，必须收紧到「只露 VASE_EXPORT 的」。
# 这也是插件宿主能干净卸载的前提之一。
set_target_properties(VaseSession PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

target_link_libraries(VaseSession
    PRIVATE VaseBuildOptions)
```

- [ ] **Step 6: 写 `Source/Host/SmokeProbe.cpp`**

```cpp
#include "Vase/Detail/SmokeProbe.h"

namespace vase
{

std::uint32_t HostSmokeProbe()
{
    // 这次调用发生在 VaseHost 内部：
    //   导入库没接上 → 链接错误；运行时找不到 VaseSession 动态库 → 加载失败。
    return SessionSmokeProbe();
}

}  // namespace vase
```

- [ ] **Step 7: 写 `Source/Host/CMakeLists.txt`**

```cmake
add_library(VaseHost SHARED
    SmokeProbe.cpp)

target_include_directories(VaseHost
    PUBLIC "${PROJECT_SOURCE_DIR}/Include")

target_compile_definitions(VaseHost
    PRIVATE VASE_HOST_BUILD)

set_target_properties(VaseHost PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

# PUBLIC：Host 依赖 Session，而 Session 不依赖 Host——
# 「插件不依赖 Host」由约定变成链接期事实（D11）。
target_link_libraries(VaseHost
    PUBLIC VaseSession
    PRIVATE VaseBuildOptions)
```

- [ ] **Step 8: 在顶层 `CMakeLists.txt` 加入两个子目录**

把文件末尾的注释行替换为：

```cmake
add_subdirectory(Source/Session)
add_subdirectory(Source/Host)
```

- [ ] **Step 9: 构建**

```bash
cd /d/Git/Vase
cmake --build --preset win-x64-clang-debug
```

期望：成功，无 warning。产出：

```bash
ls build-win/win-x64-clang-debug/bin/
```

期望看到 `VaseSession.dll` 与 `VaseHost.dll`。

- [ ] **Step 10: 检查导出表与导入表（这一步才是本 task 的真正判据）**

Windows 上「编过了」不等于「导出了」。分别检查两件事：

```bash
cd /d/Git/Vase/build-win/win-x64-clang-debug/bin
llvm-readobj --coff-exports VaseSession.dll | grep -i SessionSmokeProbe
llvm-readobj --coff-imports  VaseHost.dll    | grep -i SessionSmokeProbe
```

期望：
- 第一条能打印出 `SessionSmokeProbe`（**导出**表里有它）；
- 第二条能打印出 `SessionSmokeProbe`（**导入**表里有它，即 VaseHost 真的在向 VaseSession 要这个符号）。

若第二条为空，说明编译器把这次调用内联或优化掉了——检查 `CMAKE_BUILD_TYPE` 是否为 Debug，并确认 `HostSmokeProbe()` 的返回值确实来自 `SessionSmokeProbe()`。

- [ ] **Step 11: 确认用的是动态 CRT（8.3 的硬约束）**

这是最容易静默出错、且后果最晚才暴露的一条：静态 CRT 下跨模块 `new`/`delete` 落在不同的堆上，编译链接全绿，只在运行期以诡异的内存问题出现。而两个 clang 驱动的**默认就是静态 CRT**。

从产物本身验证，不看 CMake 变量：

```bash
cd /d/Git/Vase/build-win/win-x64-clang-debug/bin
llvm-readobj --coff-imports VaseSession.dll | grep -iE "MSVCP|VCRUNTIME|ucrtbase"
```

期望：能看到 `MSVCP140D.dll` / `VCRUNTIME140D.dll`（Debug 下的动态 CRT）。**若输出为空，说明 CRT 被静态链了进去**——回头检查顶层 `CMakeLists.txt` 的 `CMAKE_MSVC_RUNTIME_LIBRARY` 是否在两个库里都生效了。

- [ ] **Step 12: 跑通 MSVC 那条线，并验证 `/we4530` 真的挡住了异常**

Task 1 只验了 MSVC 的 configure（那时还没有任何 target）；现在有了真实的库和源文件，才能验构建、测试与异常的强制力。

```bash
cd /d/Git/Vase
Scripts/msvc-env.cmd cmake --build --preset win-x64-msvc-debug
Scripts/msvc-env.cmd ctest --preset win-x64-msvc-debug
```

期望：构建成功、测试绿。

再做导出/导入表与 CRT 两项检查（命令同 Step 10 / Step 11，路径换成 `build-win/win-x64-msvc-debug/bin/`）——cl.exe 是**另一个前端**，导出宏与 CRT 选择要独立验证，不能从 clang-cl 的结果外推。

**然后是 `/we4530` 的强制力验证**（D17 在 MSVC 侧的唯一抓手）：往 `Source/Session/SmokeProbe.cpp` 的 `SessionSmokeProbe()` 开头临时插入

```cpp
    try
    {
    }
    catch (...)
    {
    }
```

重新构建，**期望编译失败并报 `error C4530`**。确认后删掉，重跑一次构建确认恢复绿色。**不要留在仓库里。**

> 只验 `try` / `catch` 这一形态。**裸 `throw` 在 cl.exe 下本来就是漏网的**（实测静默通过），不要在这一步期待它失败——这条缺口记在 spec 第 9 节，靠 clang-cl 那条线兜底。

- [ ] **Step 13: Commit**

```bash
cd /d/Git/Vase
git add Include/ Source/ CMakeLists.txt
git commit -m "建立 VaseSession/VaseHost 两个动态库与跨模块导出宏"
```

---

## Task 3: 接入 GoogleTest 与跨 DLL 冒烟测试

**Files:**
- Create: `Tests/CMakeLists.txt`
- Create: `Tests/Smoke/CrossDllSmoke.cpp`
- Modify: `CMakeLists.txt`（加 `add_subdirectory(Tests)`）

**Interfaces:**
- Consumes: `vase::SessionSmokeProbe()`、`vase::HostSmokeProbe()`（Task 2）、target `VaseHost`（`PUBLIC` 地带着 `VaseSession`）、vcpkg 的 `GTest::gtest_main`（Task 1 的 manifest）。
- Produces: ctest 用例——`CrossDll.HostImportsSessionSymbol`、`CrossDll.ProbeValueIsNotTriviallyZero`。

---

- [ ] **Step 1: 写 `Tests/CMakeLists.txt`（先不写 find_package，制造红）**

```cmake
add_executable(VaseTests
    Smoke/CrossDllSmoke.cpp)

target_link_libraries(VaseTests
    PRIVATE VaseBuildOptions
            VaseHost
            GTest::gtest_main)
```

> **`VaseBuildOptions` 不能省。** 它不是「顺手带上警告级别」，而是**承重的**：它带来的 `/utf-8` 是这份源码能被 cl.exe 正确解码的前提。缺了它，cl.exe 按系统代码页（CP936）读 UTF-8 源文件，而源码里的中文注释一旦某个汉字的 UTF-8 末字节落在 CP936 的前导字节区间，就会**吃掉行尾 CR**、把下一行并进来，最终以一种与编码毫无字面关联的方式炸掉——实测报的是 `<文件>(行号): fatal error C1019: unexpected #else`。这类错误不会让人想到「少了个编码开关」，所以这条必须写死。
>
> 另一半理由是口径：**测试代码也是我们的代码**，没有理由不按仓库同一套（`/W4`、警告即错误）来要求。第三方头文件（gtest）的警告由 MSVC 的 `/external` 机制挡在外面——见 Task 1 的 `VaseBuildOptions`。

- [ ] **Step 2: 跑 configure，确认它因缺 find_package 而失败**

```bash
cd /d/Git/Vase
# 先把 Tests 接进构建树
cmake --build --preset win-x64-clang-debug 2>&1 | head -20
```

（若 `add_subdirectory(Tests)` 还没加，先在顶层 `CMakeLists.txt` 末尾加上，再重跑。）

期望：**失败**，报 `Target "VaseTests" links to target "GTest::gtest_main" but the target was not found`（或等价信息）。这条报错证明 `GTest::gtest_main` 不是 vcpkg 自动提供的，必须显式 `find_package`。

- [ ] **Step 3: 补上 `find_package`，让它变绿**

在 `Tests/CMakeLists.txt` 顶部插入：

```cmake
find_package(GTest CONFIG REQUIRED)
```

- [ ] **Step 4: 写测试 `Tests/Smoke/CrossDllSmoke.cpp`**

```cpp
#include "Vase/Detail/SmokeProbe.h"

#include <gtest/gtest.h>

namespace
{

// 本用例证明：VaseSession 导出的符号，经 VaseHost 的导入表被真正调用到了。
// 它是 M0 的价值所在——导出宏、动态库链接、运行时库查找路径三件事同时被验证。
TEST(CrossDll, HostImportsSessionSymbol)
{
    EXPECT_EQ(vase::HostSmokeProbe(), vase::SessionSmokeProbe());
}

// 反向对照：探针返回值非零，因此上面那条断言不会因为「两边都返回 0」而假绿。
TEST(CrossDll, ProbeValueIsNotTriviallyZero)
{
    EXPECT_NE(vase::SessionSmokeProbe(), 0u);
}

}  // namespace
```

- [ ] **Step 5: 注册到 ctest，并解释为什么用 PRE_TEST 模式**

在 `Tests/CMakeLists.txt` 末尾追加：

```cmake
include(GoogleTest)

# DISCOVERY_MODE PRE_TEST：把「枚举测试」推迟到 ctest 运行时。
# 默认模式会在**构建期**执行测试可执行文件来枚举用例，而那时 Windows 上的
# gtest / VaseSession / VaseHost 这些 DLL 未必已在可执行文件旁边，
# 会让构建无缘无故地失败。
gtest_discover_tests(VaseTests
    DISCOVERY_MODE PRE_TEST)
```

- [ ] **Step 6: 构建并跑测试（绿）**

```bash
cd /d/Git/Vase
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug
```

期望：`100% tests passed out of 2`。

> **实测的措辞差异，别拿子串判绿**：CMake 4.4.3 在**无失败**时**不打印** `N tests failed out of M` 那个子句——上面写的 `0 tests failed` 压根不会出现。有失败时才补上（已用 1/3 失败的对照确认：`67% tests passed, 1 tests failed out of 3`）。
>
> **真正的判据是「发现了几个用例」，不是退出码。** `ctest` 在**一个测试都没发现**时同样返回 0，输出只有一句 `No tests were found!!!`。必须另跑一次 `ctest --preset <p> -N`，核对 `Total Tests: 2`。
>
> 附一条实测：cl.exe 线**构建失败**时（测试可执行文件根本没产出），`DISCOVERY_MODE PRE_TEST` 会合成一个 `VaseTests_NOT_BUILT` 占位用例并让它失败，`ctest` 退出 **8**。所以本仓库不存在「构建红但 ctest 绿」这条静默路径。

> **若报找不到 `gtest.dll` / `VaseSession.dll`**：说明运行时库查找路径没接上。
> 先确认两个我们的 DLL 确实与 `VaseTests.exe` 同在 `bin/`（`CMAKE_RUNTIME_OUTPUT_DIRECTORY` 只管我们自己的 target）；gtest 的 DLL 由 vcpkg 的 applocal 机制拷贝。若 applocal 没有生效，退路是在 `Tests/CMakeLists.txt` 里显式给测试加 `ENVIRONMENT`：
>
> ```cmake
> if(WIN32)
>     set_tests_properties(VaseTests PROPERTIES
>         ENVIRONMENT "PATH=${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin;$ENV{PATH}")
> endif()
> ```
>
> 把实际结论记进 README——这类环境差异正是 M0 要提前撞出来的。

- [ ] **Step 7: 两个 preset 都跑一遍**

```bash
cd /d/Git/Vase
cmake --preset win-x64-clang-release
cmake --build --preset win-x64-clang-release
ctest --preset win-x64-clang-release
```

期望：同样绿。Release 下优化更强，能顺带验证 Task 2 Step 10 担心的内联问题不会让冒烟测试失去意义。

- [ ] **Step 8: Commit**

```bash
cd /d/Git/Vase
git add Tests/ CMakeLists.txt
git commit -m "接入 GoogleTest，加入跨 DLL 冒烟测试并接入 ctest"
```

---

## Task 4: 静态检查与格式门（Windows 侧）

spec 2.4 的验收 #3 与 #5 要求两侧都成立，本 task 先在 Windows 侧把门立起来。

**Files:**
- Modify: `README.md`（新增「构建与验收」一节）
- 视检查结果可能修改：`Include/`、`Source/`、`Tests/` 下的源文件

**Interfaces:**
- Consumes: Task 1 的 `CMAKE_EXPORT_COMPILE_COMMANDS: ON`、仓库既有的 `.clang-tidy` 与 `.clang-format`。
- Produces: 可复制的两条门禁命令，Task 5 与 Task 6 直接复用。

---

- [ ] **Step 1: 确认编译数据库已生成**

```bash
cd /d/Git/Vase
ls -la build-win/win-x64-clang-debug/compile_commands.json
head -c 200 build-win/win-x64-clang-debug/compile_commands.json
```

期望：文件存在且是合法 JSON 数组。

- [ ] **Step 2: 跑 clang-tidy（红：接受它大概率会报东西）**

```bash
cd /d/Git/Vase
run-clang-tidy -p build-win/win-x64-clang-debug
```

期望：能跑完（本机 `C:\...\LLVM\bin\run-clang-tidy` 是 Python 脚本，Python 3.14.3 已就位）。**大概率会报出若干条**——这正是这一步的目的。

> 本仓库 `.clang-tidy` 的 `WarningsAsErrors` 为空，因此 clang-tidy 的产出是 `warning:` 而非 `error:`。验收 #3 的口径是「无错」，即输出里**不得出现 `error:`**；warning 逐条判断：能改的改，属于误报的在 `.clang-tidy` 的检查集里按既有分组规则添加禁用项（注意注释里写明的顺序规则：禁用项必须紧跟同组的启用通配之后）。

> **新加的一步（MSVC）**：对 cl.exe 产出的编译数据库再跑一次——
>
> ```bash
> run-clang-tidy -p build-win/win-x64-msvc-debug \
>     -extra-arg=-Wno-unused-command-line-argument
> ```
>
> **实测结果（既不是我预设的「能跑通」也不是「解析错误」，是第三种）**：clang-tidy **能完整跑完**，分析结果与 clang-cl 线**逐条一致**；但**不加那个 extra-arg 时字面命令 exit 1 并打出 3 条 `error:`**：
>
> ```
> argument unused during compilation: '/external:anglebrackets'
>   [clang-diagnostic-unused-command-line-argument]
> ```
>
> 根因不在 clang-tidy 也不在 `.clang-tidy`：`/external:anglebrackets` 只在 **cl.exe** 那条线上加（clang-cl 不需要它），而 clang-tidy 的前端是 clang、不认这个 MSVC 专用开关；该诊断又被 MSVC 线的 `/WX` 提升成了 error。不经 clang-tidy、直接把同一命令行喂 clang-cl 可复现：无 `/WX` 是 warning，带 `/WX` 是 error。
>
> **代价要记住**：`-Wno-unused-command-line-argument` 是**整类**压制，会一并压掉其它「参数未被使用」的提示。替代方案要动 `VaseBuildOptions`（属禁区），故本 task 未采用。
>
> **不要为了让它在 MSVC 线上跑通而去改 `.clang-tidy` 的检查集。**

- [ ] **Step 3: 修掉 clang-tidy 报出的问题**

按 Step 2 的实际输出执行。若某条是误报，改 `.clang-tidy` 并在该文件顶部的「检查集取舍说明」里补一行理由——不要静默禁用。

- [ ] **Step 4: 跑格式门（红 → 绿）**

```bash
cd /d/Git/Vase
git ls-files -z '*.h' '*.cpp' | xargs -0 clang-format --dry-run --Werror
```

期望：先看它报哪些文件（新写的文件很可能有 1–2 处与 `clang-format` 的出入，例如 `TEST(...)` 宏后的花括号位置）。直接修：

```bash
cd /d/Git/Vase
git ls-files -z '*.h' '*.cpp' | xargs -0 clang-format -i
git ls-files -z '*.h' '*.cpp' | xargs -0 clang-format --dry-run --Werror   # 复跑应为空
```

**改源码去迁就 clang-format，不改 `.clang-format`。**

> **已知会失败的文件（Task 2 评审实测，不是猜测）**：
>
> - `Include/Vase/Detail/Export.h` —— `#if` 里用了缩进的 `#    define`，而 LLVM 默认 `IndentPPDirectives: None` 要求 `#define` 顶格。**两处分支都失败。** 那一版是计划原文照抄进实现的（计划里的片段是错的），所以这是**改代码、不是改配置**的典型例子。
> - `Include/Vase/Detail/SmokeProbe.h` / `Source/Session/SmokeProbe.cpp` / `Source/Host/SmokeProbe.cpp` —— `}  // namespace vase` 一类尾注释用了两个空格。**根因是版本漂移**：`SpacesBeforeTrailingComments` 在 clang-format ≥21 从 2 改成了 1，而 `.clang-format` 没有钉这个键。这一条是**仓库级隐患**（以后任何按 LLVM 经典风格新写的文件都会在新工具链上失败），值得单独记为「要么钉住这个键，要么明确要求 clang-format 版本」——但钉键属于改配置，需要与需求方确认，**不在本 task 擅自处理**。
>
> 这两条都是 Task 2 评审报出、由控制器裁定**归本 task 处置**的（Task 2 的修复循环没有为它们开轮）。修完请在报告里写明 `clang-format -i` 实际改动了哪几个文件、哪些行。

---

> **实现后补进代码、而本计划原片段没有的四条事实**（都在 `CMakeLists.txt` 与 `Tests/CMakeLists.txt` 的注释里；此处记一遍，免得后来人只读计划时漏掉）：
>
> 1. **本仓库不能用 `EXPECT_THROW` 一族。** 理由不是「会静默退化」，而是**编译期硬失败**——`EXPECT_THROW` 的宏体是裸 `try/catch`，没有 `#if GTEST_HAS_EXCEPTIONS` 包裹。实测：clang-cl 报 `cannot use 'try' with exceptions disabled`（exit 1）、cl.exe 报 `error C4530`（exit 2）。规则成立，但**炸得很响，不会静默**。
> 2. **gtest 头与 gtest DLL 的 `GTEST_HAS_EXCEPTIONS` 不一致**：我们的 TU 是 0（`/EP` 实测，两个前端都是），vcpkg 预编译的 DLL 是 1。
> 3. **`/external:anglebrackets` 对公开头是个前向陷阱**：架构文档要求插件作者写 `#include <Vase/Plugin.h>`，而尖括号会被标为外部头、静默逃出 `/W4 /WX`。**Vase 自己的头一律用引号包含。**
> 4. **`_HAS_EXCEPTIONS=0` 的语义代价**：不只是「非官方支持配置」，而是 MSVC STL 的**前置条件失败**（`vector::at`、`stoi`、filesystem 的 error overload 等）从「抛异常」变成**进程终止**。这是 M1 的设计输入。

- [ ] **Step 5: 确认 `-Werror` 真的在生效**

故意注入一个警告再撤回，验证门是活的。直接改 `Source/Session/SmokeProbe.cpp`，在 `return` 前插入一行 `int unusedProbe = 1;`，然后：

```bash
cd /d/Git/Vase
cmake --build --preset win-x64-clang-debug 2>&1 | grep -E "error|warning" | head -5
```

期望：编译**失败**，报 `unused variable 'unusedProbe'` 且因为是 `-Werror` 而成为 `error:`。若它只是 warning 就过了，说明 `VASE_WERROR` 没接上——回头检查 `VaseBuildOptions` 是否被两个库 target 链接。

确认后把那行删掉，重跑一次构建确认恢复绿色。**不要把这个变量留在仓库里。**

- [ ] **Step 6: 在 `README.md` 补「构建与验收」一节**

````markdown
## 构建与验收

```bash
cmake --preset win-x64-clang-debug          # 或 win-x64-clang-release
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug
```

静态检查与格式。**注意两部分并不对称**：

```bash
# clang-cl 线：直接跑
run-clang-tidy -p build-win/win-x64-clang-debug

# cl.exe 线：必须多带一个 extra-arg
run-clang-tidy -p build-win/win-x64-msvc-debug \
    -extra-arg=-Wno-unused-command-line-argument

# 格式：两侧完全相同
git ls-files -z '*.h' '*.cpp' | xargs -0 clang-format --dry-run --Werror
```

> **为什么 MSVC 线要那个 extra-arg**：`/external:anglebrackets` 只加在 cl.exe 线上，
> 而 clang-tidy 的前端是 clang、不认这个 MSVC 专用开关，于是报
> `argument unused during compilation`；该诊断在 MSVC 线上又被 `/WX` 提升成 error。
> 代价是它**整类**压掉「参数未被使用」提示，会一并压掉其它同类告警。
````

- [ ] **Step 7: Commit**

```bash
cd /d/Git/Vase
git add -A
git commit -m "立起 clang-tidy 与 clang-format 门禁（clang-cl 线全绿，MSVC 线需一个 extra-arg）"
```

---

## Task 5: Linux 侧（工具链 + 自定义 triplet + WSL 全流程）

**Files:**
- Create: `Cmake/Toolchains/linux-x64-clang-libcxx.cmake`
- Create: `Cmake/Triplets/x64-linux-libcxx.cmake`
- Modify: `CMakePresets.json`（加 linux 的 configure/build/test preset 各两个）
- Modify: `README.md`（补 WSL 侧命令与 `VCPKG_ROOT` 的注意事项）

**Interfaces:**
- Consumes: Task 1–4 的全部产出；`VCPKG_ROOT=/mnt/d/Developer/vcpkg-linux`（由登录 shell 的 `/etc/profile.d/vcpkg.sh` 提供）。
- Produces: `linux-x64-clang-{debug,release}` 的 configure/build/test preset 名，命名与 Windows 侧同构。

---

- [ ] **Step 1: 在 `CMakePresets.json` 里加 Linux preset**

在既有数组里追加下列各项（`configurePresets` 加 3 项、`buildPresets` 加 2 项、`testPresets` 加 2 项）：

```json
{
  "name": "linux-x64-clang",
  "hidden": true,
  "generator": "Ninja",
  "binaryDir": "${sourceDir}/build-linux/${presetName}",
  "cacheVariables": {
    "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/Cmake/Toolchains/linux-x64-clang-libcxx.cmake",
    "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
  }
},
{
  "name": "linux-x64-clang-debug",
  "inherits": "linux-x64-clang",
  "displayName": "Linux x64 / clang / libc++ / Debug",
  "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" }
},
{
  "name": "linux-x64-clang-release",
  "inherits": "linux-x64-clang",
  "displayName": "Linux x64 / clang / libc++ / Release",
  "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" }
}
```

```json
{ "name": "linux-x64-clang-debug", "configurePreset": "linux-x64-clang-debug" },
{ "name": "linux-x64-clang-release", "configurePreset": "linux-x64-clang-release" }
```

```json
{
  "name": "linux-x64-clang-debug",
  "configurePreset": "linux-x64-clang-debug",
  "output": { "outputOnFailure": true }
},
{
  "name": "linux-x64-clang-release",
  "configurePreset": "linux-x64-clang-release",
  "output": { "outputOnFailure": true }
}
```

- [ ] **Step 2: 跑 configure，确认它因工具链文件不存在而失败（红）**

```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --preset linux-x64-clang-debug'
```

期望：**失败**，报找不到 `Cmake/Toolchains/linux-x64-clang-libcxx.cmake`。

- [ ] **Step 3: 写 `Cmake/Triplets/x64-linux-libcxx.cmake`**

```cmake
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
# vcpkg 在 Linux 原生构建时**不选编译器**：scripts/toolchains/linux.cmake 只在
# **交叉编译**分支里设 CMAKE_CXX_COMPILER，原生分支把它留给 CMake 的平台默认值，
# 实测落到 /usr/bin/c++（GCC 13.3.0）。
#
# 于是 vcpkg 的编译器探测（scripts/detect_compiler，要编一个 C 语言的 try-compile）
# 会拿 GCC 去编，而 GCC 不认 VCPKG_LINKER_FLAGS 里的 -stdlib=libc++：
#   cc: error: unrecognized command-line option '-stdlib=libc++'
#   -> "vcpkg was unable to detect the active compiler's information"
# **整个 configure 在这里就失败，根本走不到 gtest。**
#
# VCPKG_CMAKE_CONFIGURE_OPTIONS 由 vcpkg 的 vcpkg_configure_cmake 追加到每个 port 的
# configure 命令行（含 detect_compiler 自己在内），是 triplet 里钉编译器的正规入口。
# clang 在 C 模式下接受 -stdlib=libc++（只报一条 unused-argument warning，exit 0），
# 所以 C 与 CXX 都交给 clang 即可，不需要把 VCPKG_LINKER_FLAGS 按语言拆开。
#
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
```

- [ ] **Step 4: 写 `Cmake/Toolchains/linux-x64-clang-libcxx.cmake`**

```cmake
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

# 编译器路径要「**目录去引用化、名字保留 clang++**」。两件事各有实测理由，都不能省。
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
# **注意这个错误极具误导性**：编译本身是好的，只有模块扫描挂。
#
# 【二】名字必须留着 "++"，否则退化成 C 驱动、链接期不带 -lc++。
# clang 的驱动按 **argv[0] 里有没有 "++"** 判 C++ 模式。一路 REAL_PATH 解到底会得到
# .../clang-23，驱动退回 C 模式，链接 VaseTests 时报：
#     undefined reference to `std::__1::basic_string<...>::append(char const*)'
#     /usr/local/lib/libc++.so.1: error adding symbols: DSO missing from command line
# **这个错误只在可执行文件上暴露**：两个 .so 因 `-shared` 允许未定义符号而"看似成功"。
#
# 所以：只把**目录**规范化，名字仍取 clang++。
#
# **仍然不写死安装路径**——换 LLVM 只改 PATH，与 Windows 侧同一条约定（D6）；
# 这里只是把 PATH 解析出来的结果去引用化，不是把某个安装路径钉进仓库。
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
set(CMAKE_CXX_FLAGS_INIT "-stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-stdlib=libc++")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-stdlib=libc++")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-stdlib=libc++")

# 内置 x64-linux 走 libstdc++，这里换成同目录下的自定义 triplet。
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_LIST_DIR}/../Triplets")
set(VCPKG_TARGET_TRIPLET x64-linux-libcxx)

include("$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
```

> `VCPKG_OVERLAY_TRIPLETS` 与 `VCPKG_TARGET_TRIPLET` 都必须在 `include` **之前**设置。

- [ ] **Step 5: configure + build（绿）**

```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --preset linux-x64-clang-debug && cmake --build --preset linux-x64-clang-debug'
```

期望：成功，无 warning。构建树落在 `/mnt/d/Git/Vase/build-linux/linux-x64-clang-debug/`。

> **可能撞上的坑**：vcpkg 构建 GoogleTest 时的 `-stdlib=libc++` 若没有正确传到 CMake 依赖内部（vcpkg 的 `VCPKG_CXX_FLAGS` 只作用于它自己的构建流程，某些 port 的 CMakeLists 会覆盖 `CMAKE_CXX_FLAGS`），表现为 gtest 链接到 `libstdc++`。Step 7 就是专门查这个的。

- [ ] **Step 6: 跑测试（绿）**

```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && ctest --preset linux-x64-clang-debug'
```

期望：`100% tests passed, 0 tests failed out of 2`。

Linux 侧不需要 Windows 那样的 DLL 路径处理：CMake 会自动把链接到的共享库目录写进构建树的 RPATH，我们的库在 `lib/`、vcpkg 的 gtest 在 `vcpkg_installed/.../lib`，都在其中。

- [ ] **Step 7: 验证 gtest 真的链的是 libc++（而不是 libstdc++）**

```bash
wsl -d Ubuntu -- bash -lc 'ldd /mnt/d/Git/Vase/build-linux/linux-x64-clang-debug/bin/VaseTests | grep -E "libc\+\+|libstdc\+\+"'
```

期望：出现 `libc++.so.1`，**不**出现 `libstdc++.so.6`。

若出现了 libstdc++，说明 triplet 的 flag 没穿透到 gtest 的构建里——此时需要改 triplet（常见做法是同时给 `VCPKG_CXX_FLAGS` 与 port 的前置覆盖），把实际结论写进本 triplet 的注释。

- [ ] **Step 8: 验证导出符号在 Linux 侧也被正确收紧**

```bash
wsl -d Ubuntu -- bash -lc 'nm -D --defined-only /mnt/d/Git/Vase/build-linux/linux-x64-clang-debug/lib/libVaseSession.so | grep -i smokeprobe'
```

期望：能看到 `SessionSmokeProbe`（说明 `VASE_EXPORT` 生效）。可选地再确认 C++ 运行时内部符号没有大面积外露——这是 `CXX_VISIBILITY_PRESET hidden` 的效果。

- [ ] **Step 9: 跑 Linux 侧的两道门禁**

```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && run-clang-tidy -p build-linux/linux-x64-clang-debug'
wsl -d Ubuntu -- bash -lc "cd /mnt/d/Git/Vase && git ls-files -z '*.h' '*.cpp' | xargs -0 clang-format --dry-run --Werror"
```

期望：tidy 输出无 `error:`；格式检查无输出。**两侧同为 clang 23.1.0**，因此这里的结论应与 Windows 侧完全一致——不一致就说明版本没对齐，回头查 PATH。

- [ ] **Step 10: 核对两侧的 gtest 是同一版本（spec 2.4 验收 #6）**

把 Windows 侧 Task 1 Step 7 记下的版本与 Linux 侧对比：

```bash
wsl -d Ubuntu -- bash -lc 'ls /mnt/d/Git/Vase/build-linux/linux-x64-clang-debug/vcpkg_installed/x64-linux-libcxx/share/gtest/'
```

期望：与 Windows 侧**同版本**（同一个 `builtin-baseline` 保证这一点）。

- [ ] **Step 11: 在 `README.md` 补 WSL 侧说明**

写明：源码在 Windows 侧（`D:\Git\Vase`），WSL 里的路径是 `/mnt/d/Git/Vase`，构建树落 `build-linux/`；以及「`cmake` 必须经由登录 shell 调用，否则 `VCPKG_ROOT` 是空的」这条。

- [ ] **Step 12: Commit**

```bash
cd /d/Git/Vase
git add Cmake/ CMakePresets.json README.md
git commit -m "加入 Linux x64 + libc++ 工具链与自定义 triplet，WSL 侧全流程跑通"
```

---

## Task 6: M0 验收与文档收尾

**Files:**
- Modify: `CLAUDE.md`（**项目指令要求**：首次建立构建系统时同步补入真实可用的命令）
- Modify: `README.md`（如验收中发现需补记的内容）

**Interfaces:**
- Consumes: 前五个 task 的全部产出。
- Produces: M0 的验收证据，以及 M1 计划可以引用的、真实存在的构建/测试/检查命令。

---

- [ ] **Step 1: 逐条核对 spec 2.4 的六条判据**

| # | 判据 | 命令 | Win(clang-cl) | Win(cl.exe) | Linux |
|---|---|---|---|---|---|
| 1 | 各 preset 都能 configure + build，警告即错误下零警告 | `cmake --preset <p> && cmake --build --preset <p>` | | | |
| 2 | `ctest` 各条线全绿 | `ctest --preset <p>` | | | |
| 3 | 生成 `compile_commands.json`，`run-clang-tidy -p <build>/` 无错 | `run-clang-tidy -p <build-dir>` | | | |
| 4 | 跨 DLL 冒烟测试通过 | `ctest --preset <p> -R CrossDll` | | | |
| 5 | `clang-format --dry-run --Werror` 全绿 | `git ls-files -z '*.h' '*.cpp' \| xargs -0 clang-format --dry-run --Werror` | | | |
| 6 | `vcpkg.json` 各条线解出同一版本 gtest | 对比 `vcpkg_installed/.../share/gtest/` | | | |
| 7 | 异常确实被挡住（新增） | clang-cl：临时写 `throw` 应编不过；cl.exe：临时写 `try/catch` 应报 C4530 | | | — |

**这份表格要把实际输出填进去**，不是打勾了事——M1 的计划会引用这里的结论。

> 第 3 行对 cl.exe 那条可能填「不适用」——clang-tidy 能否吃 MSVC 风格的编译数据库尚未实测（见 Task 4）。填「不适用」是**可接受的结论**，只要写清原因。
>
> 第 7 行是 D17 的落地验证。注意 cl.exe 侧只验 `try/catch`：裸 `throw` 在 cl.exe 下**本来就是漏网的**（实测静默通过），这条缺口已在 spec 第 9 节记录，不要在这一步期待它失败。

- [ ] **Step 2: 更新 `CLAUDE.md`**

`CLAUDE.md` 明确写着「当你首次建立构建系统 / 测试 / 目录结构时，请同步更新本文件，把真实可用的命令补进来」。需要改三处：

1. **「项目状态：脚手架阶段（尚无代码）」一节**——不再是「没有任何源代码」了。改写为当前真实状态：构建系统与测试已建立，架构代码尚未开始（M0 完成、M1 待做）。
2. **新增「构建与测试」一节**——把 Task 4 与 Task 5 验证过的**真实命令**写进去（Windows 与 Linux 两套）。不要写没跑过的命令。
3. **「尚未确定的事项」表格**——`构建系统` 的「仓库里的状态」从 `**无任何 CMake 文件**` 改为指向真实存在的文件；`测试框架与运行方式` 同理改为 GoogleTest + ctest。`插件接口 / ABI 约定` 仍为「无头文件」，保持不动。

- [ ] **Step 3: 确认没有把空壳留在仓库里**

```bash
cd /d/Git/Vase
git status --short
git ls-files | grep -E "^(Source|Include|Tests|Cmake)/"
```

期望：没有 `VaseCatalog` / `VaseCli` / `VasePack` 之类的空壳 target，没有 `Include/Vase/` 下 spec 10 列出的其余六个子目录。它们等真的有内容再立。

- [ ] **Step 4: Commit**

```bash
cd /d/Git/Vase
git add CLAUDE.md README.md
git commit -m "M0 完成：补入真实构建/测试命令并更新项目状态"
```

- [ ] **Step 5: 停下，回到 spec 第 4 节决定 M1**

M0 到此为止。下一步是**另起一份 M1 的计划**（`writing-plans`），而不是接着写头文件——spec 第 4 节明确规定了两者的边界，M0 的实测结论（尤其是 triplet 与 applocal 的真实行为）要先进入 M1 计划的约束里。

---

## 完成后

M0 结束时应当能回答「这台机器上要怎么构建、怎么测、怎么检查」——而且答案是跑过的，不是推断的。这正是 spec 说的：M0 的价值不在「空壳可编译」，而在构建地基已被双平台证明。
