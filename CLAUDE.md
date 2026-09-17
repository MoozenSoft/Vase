# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 配套文件

- [`.claude/CLAUDE.md`](.claude/CLAUDE.md) —— CodeGraph 代码检索工具的用法约定。本仓库已在根目录建立 `.codegraph/` 索引，并在 `.mcp.json` 中注册了 codegraph MCP server。

  两者关系：**本文件**描述「这个项目是什么、怎么构建」；**`.claude/CLAUDE.md`** 描述「用什么工具去定位和理解代码」。当 `.codegraph/` 存在时，查找或理解代码应优先调用 `codegraph_explore`，而非 grep/find 或逐个读文件。

  注意：该文件由 CodeGraph 自动维护（内容包裹在 `<!-- CODEGRAPH_START -->` / `<!-- CODEGRAPH_END -->` 之间），**不要手工编辑**；需要改动时通过 CodeGraph 自身重新生成。

- [`wiki/vase-architecture.md`](wiki/vase-architecture.md) —— **架构设计文档 v3**：Vase 的定位与边界、分层模型（`PluginCatalog` / `PluginHost` / `Pod`）、插件清单与 Preset 格式、生命周期与服务解析、**热插拔（档 ①：局内仅装卸无人依赖的叶插件，依赖账本执法）**、验证策略。v3 的逐条变更见其 1.5 节；`Source/Session` → `Source/Pod` 的**实体**更名待 M1（磁盘现状仍是 Session 命名，仓库里凡提到现有目录/探针仍以现状为准）。

  注意：文档中的**接口签名与文件格式**仍为提议；**目录布局已确认**（第 10 节）。凡未落成代码的，仍按本文件「尚未确定的事项」处理——**先询问，不要假设**。

## 项目状态：M0 已完成，架构代码尚未开始

**M0（构建地基）已完成**，M1（架构代码）待做。当前仓库里能跑的东西：

- **构建系统已建立**：`CMakeLists.txt`、`CMakePresets.json`、`Cmake/Toolchains/`（三个工具链文件）、
  `Cmake/Triplets/x64-linux-libcxx.cmake`，依赖经 vcpkg manifest 模式拉取。
- **测试已建立**：GoogleTest 1.18.0 + `ctest`，`Tests/` 下有一个跨 DLL 冒烟用例（2 个 TEST）。
- **代码只有一个探针**：`Source/Session`、`Source/Host` 各一个 `SmokeProbe.cpp`，
  `Include/Vase/` 下只有 `Plugin.h`（骨架）与 `Detail/`。**这不是架构代码**——
  M0 的全部价值是「构建地基已被双平台证明」，`PluginCatalog` / `PluginHost` / `Session`
  的实现在 M1，尚未开工。
- **仍无 CI**。

因此：

- 构建 / 测试 / 检查命令，**以本文件「构建与测试」一节为准**——那里写的是跑过的，
  其余任何地方（含 `wiki/`）写的都只是打算怎么做。
- **不要凭想象描述架构。** 下面「设计意图」一节只记录仓库中已明确写下的内容；
  接口签名与文件格式仍是提案。
- 新增源码 / 测试 / 目录时，若引入了本文件没记的规矩或命令，**请同步更新本文件**。

## 构建与测试

**六个 preset，全部已实测可用**（最近一次全量重测：2026-09-16，`cmake_minimum 4.3` + preset v9 降级后六条线从零 configure/build/ctest 全绿，`-N` 基数 2；本轮未重跑 tidy 与 format）。M0 验收时按
[`docs/superpowers/specs/2026-09-14-vase-m0-m1-design.md`](docs/superpowers/specs/2026-09-14-vase-m0-m1-design.md)
2.4 的判据跑过，但**覆盖面按判据不同**：

- 判据 **1 / 2 / 4 / 6**（构建零警告、`ctest`、跨 DLL 冒烟、gtest 同版本）在**六个 preset**
  上逐条跑过；
- 判据 **3**（`run-clang-tidy`）**只覆盖三条 debug 构建树**（clang-cl / cl.exe / Linux 各一，
  各 3 个 TU），**release 树从未跑过 tidy**；
- 判据 **5**（`clang-format`）与构建树无关，全仓一条命令。

另加「异常确实被挡住」一条（D17 落地验证）：证据是**三条 debug 线各一个独立探针 TU**，
flag 集抄自真实 target 的命令行。它的边界有两条——**release 线未单独验**；且它**不覆盖
「某个 target 忘了链 `VaseBuildOptions`」这一失效模式**（那恰是本项目真实炸过一次的模式，
见下方规矩 1，而探针 TU 手里本来就有那套 flag）：

| 平台 | preset |
|---|---|
| Windows / clang-cl | `win-x64-clang-debug`、`win-x64-clang-release` |
| Windows / cl.exe | `win-x64-msvc-debug`、`win-x64-msvc-release` |
| Linux / clang + libc++ | `linux-x64-clang-debug`、`linux-x64-clang-release` |

### Windows（在 Git Bash 里直接跑）

```bash
cmake --preset win-x64-clang-debug
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug
```

**cl.exe 那条线必须经 `Scripts/msvc-env.cmd`**（cl.exe 依赖 `vcvars64.bat` 设的
`PATH` / `INCLUDE` / `LIB`；clang-cl 不需要）。调用前 `VCPKG_ROOT` 要已设：

```bash
Scripts/msvc-env.cmd cmake --preset win-x64-msvc-debug
Scripts/msvc-env.cmd cmake --build --preset win-x64-msvc-debug
ctest --preset win-x64-msvc-debug      # ctest 不调编译器，不需要 msvc-env
```

构建产物落 `build-win/<presetName>/`，可执行与 DLL 同处 `bin/`——这是 Windows
能找到 DLL 的前提，不要改 `CMAKE_RUNTIME_OUTPUT_DIRECTORY`。

### Linux / WSL

源码的 WSL 路径是 **`/mnt/d/Git/Vase`**，构建树落 `build-linux/<presetName>/`。
**必须经登录 shell**——`VCPKG_ROOT` 只由 `/etc/profile.d/vcpkg.sh` 提供给登录 shell：

```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --preset linux-x64-clang-debug'
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug'
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && ctest --preset linux-x64-clang-debug'
```

> **在 Git Bash 里拼 WSL 命令时别把 `$` 写进 `bash -lc "..."` 的双引号里**——
> `$VAR` 会被**外层 Git Bash 先展开**，命令照跑、结果是假的。涉及 `$` 的命令
> 先落成脚本文件，再 `wsl -d Ubuntu -- bash -lc 'bash <脚本>'`。本项目已因此栽过不止一次
> （本任务执行期间又撞到一次：`$CXX` / `$FLAGS` 被外层吃空，命令照跑、结果假绿）。

### 静态检查与格式

```bash
run-clang-tidy -p build-win/win-x64-clang-debug        # clang-cl 线
run-clang-tidy -p build-win/win-x64-msvc-debug -extra-arg=-Wno-unused-command-line-argument
git ls-files -z '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
```

`clang-tidy` 与 `clang-format` 都是全平台同一份 LLVM 23.1.0（由 PATH 解析）。
两侧 LLVM 必须同版本——判据一致性依赖这一条。

但**别指望 configure 帮你守住它**：`CMakeLists.txt` 的版本校验对象是
**CXX 编译器**，只查 clang 的 `23.x` 主版本；cl.exe 分支更是只查平台、不查版本。
**它管不到 `clang-format` / `clang-tidy` 这两个独立可执行文件。**
「CXX 编译器的 clang 是 23.x」不等于「tidy / format 也是 23.x」——
后者由环境（PATH 最前）保证，不是 configure 保证的。

### 核这些门禁时，退出码单独用是不够的

两个**静默**陷阱，都在本仓库实测过：

- **`run-clang-tidy` 退出 0 ≠ 没有 warning。** `.clang-tidy` 的 `WarningsAsErrors`
  为空，tidy 永远不会因 warning 失败。健康的输出长这样（每行都是摘要行，
  正文一条 `error:` / `warning:` 都没有）：

  ```
  Running clang-tidy in 12 threads for 3 files out of 3 in compilation database ...
  998 warnings generated.
  Suppressed 998 warnings (998 in non-user code).
  ```

  所以「无新 warning」的判据是三条一起：**退出 0 + 正文 `error:` 0 条 +
  正文 `warning:` 0 条**，再连摘要行一起读。只 grep `warning:` 会漏掉全部被抑制的量。
  实测基数（M0 验收，全部落在第三方头里，我们自己的代码零 warning）：
  Linux 359 + 359 + 3293 = 4011，Windows 两条线同为 998 + 998 + 5375 = 7371。
  数字变了不一定是错，但**要看它变在哪一类**。

- **`ctest` 在一个测试都没发现时同样返回 0。** 所以「测试全绿」不能只跑
  `ctest --preset <p>`，必须另跑一次 `ctest --preset <p> -N` 核对
  `Total Tests: 2`（当前基数）。`gtest_discover_tests` 用的是 `DISCOVERY_MODE PRE_TEST`，
  枚举发生在 ctest 运行时——测试被漏注册时，`ctest` 会一声不吭地报成功。

## 在这个仓库里干活要知道的规矩

以下四条：1 与 2 是**踩过的坑**（1 真实炸过一次构建，2 是实测出来的失败方式），
3 与 4 是**读这套 flag 时最容易想反的地方**。改动相关代码前先读。

### 1. 新 target 必须链接 `VaseBuildOptions`

`VaseBuildOptions`（根 `CMakeLists.txt`）是编译选项的唯一出口：警告级别与
`/WX` / `-Werror`、`/utf-8`、`/EHs-c-` / `-fno-exceptions`、`_HAS_EXCEPTIONS=0`、
cl.exe 线的 `/we4530` 与 `/external:*` 全在里面。**新建 target 时忘了链它，
不是一个「少几个警告」的问题**：

- 少了 `/EHs-c-`，异常策略就没了编译期强制；
- **少了 `/utf-8` 会以完全看不出与编码有关的方式炸掉**：cl.exe 改按系统代码页
  （本机 CP936）解释 UTF-8 源码，中文注释的字节被解成别的字符，于是报出一个
  **与「编码」二字毫无字面关联**的错误。本项目见过两种形态：
  - **`fatal error C1019: unexpected #else`**（真实炸过一次构建：错误信息指向
    预处理指令，看着像 `#if` 写错了）；
  - `warning C4819` 之后紧跟 **`error C2447`**（另一处实测记录的形态，
    见 M0 设计文档 2.1.1(e) 的表；那是一次「故意摘掉 `/utf-8`」的对照实验）。

  两种都指向「去翻源码找语法错误」，而真正的原因在代码页。所以别去查语法。
  （`/utf-8` 只在 Windows 侧加；Linux 侧 clang 默认 UTF-8，不需要。）

### 2. 测试里不要用 `EXPECT_THROW` 一族

含 `ASSERT_THROW` / `EXPECT_ANY_THROW` / `EXPECT_NO_THROW`。

理由不是「静默退化」，而是**编译期硬失败**：`gtest.h` 的 `EXPECT_THROW` 无条件展开成
`gtest-internal.h` 的 `GTEST_TEST_THROW_`，而后者**不受 `GTEST_HAS_EXCEPTIONS` 包裹**，
宏体里就是一个裸 `try/catch`。我们全项目关异常，于是：

```
clang-cl  error: cannot use 'try' with exceptions disabled   （exit 1）
cl.exe    error C4530（被 /we4530 升为 error）                （exit 2）
```

根因是 gtest 头在本仓库看到的 `GTEST_HAS_EXCEPTIONS == 0`，而预编译的 gtest DLL 是 1，
两侧不一致（推导与实测写在 `Tests/CMakeLists.txt` 的注释里）。

**要测「某个操作必须失败」，只能用 `Result<T>` / `Error` 的显式返回值。**

### 3. Vase 自己的头文件一律用引号包含

写 `#include "Vase/Plugin.h"`，**不要写 `#include <Vase/Plugin.h>`**。

cl.exe 线的 `VaseBuildOptions` 带 `/external:anglebrackets`，它把**所有以尖括号包含的头**
标记为外部头，再由 `/external:W0` 豁免其警告。于是 Vase 自己的头一旦被尖括号包含，
它的警告就**静默**绕过了 `/W4 /WX`——门禁看起来还是绿的。

> `wiki/vase-architecture.md` 给**插件作者**的示例用的是尖括号，那是仓库外的写法，
> 与仓库内部不同。仓库内部一律引号。

### 4. `_HAS_EXCEPTIONS=0` 的语义代价

`VaseBuildOptions` 在 Windows 侧定义了 `_HAS_EXCEPTIONS=0`（该宏**不由 `/EH` 推导**，
MSVC STL 默认给它 1，必须显式置 0）。代价是 **MSVC STL 的前置条件失败从「抛异常」
变成「进程终止」**：`vector::at` 越界、`std::stoi` 解析失败、`<filesystem>` 的
error overload 等，全部直接 abort，没有可接住的东西。

**因此错误处理必须走 `Result<T>` / `Error`，不能指望 STL 的前置条件检查给出
可恢复的失败。** 更完整的说明（含 `/external:W0` 管不着 C4530 这类边界）在
根 `CMakeLists.txt` 的注释里——`/external:*` 那一段与 `_HAS_EXCEPTIONS` 那一段。
（`Cmake/` 下的三个工具链文件与一个 triplet 只讲编译器定位、vcvars 与 STL 选型，
**不涉及异常设置**，别去那里找。）

## 语言约定

- 交流与文档：**中文优先**；专业术语可保留英文（如 plugin、ABI、RAII、CMake target）。
- 代码标识符、提交信息、注释的用词应与既有风格保持一致：现有源码与 CMake 文件的注释以**中文**为主（`Scripts/msvc-env.cmd` 是唯一的例外，那里**必须纯 ASCII**——cmd.exe 用 OEM 代码页解码批处理，理由写在该文件头部）。
- **代码注释简明扼要，不写废话**。判据是「删掉它，读者会不会踩坑」——会，才留：

  | 该写 | 不该写 |
  |---|---|
  | 代码看不出的**原因**、约束、代价（`// 删拷贝就别留着隐式移动`） | 把代码翻译一遍（`// 遍历数组`、`// 返回结果`） |
  | 出处指针（`// §5.6 规则 ②`、`// spec 3.3(2) 探针结论`） | 与相邻注释重复的话 |
  | 反直觉处的一句警告（`// 多继承下两者可能不同址`） | 调试/评审过程中的推演、试验流水账 |
  | 门禁/workaround 的**为什么**（`// 无异常编译下没有可接住的东西`） | 罗列「本仓库开了哪些检查」（那属于 `.clang-tidy` 与该处一句结论） |

  单条以一到三行为宜；超过五行先问自己是不是把设计文档抄了一份。
  计划/文档里说清的事，代码里只留结论与指针，不搬全本。

## `.claude` 目录存放约定

需要放进 `.claude` 目录的文件（配置、规则、skill、hook 等），**优先选用本项目的 `.claude/`**（`D:\Git\Vase\.claude\`），而不是用户空间的 `C:\Users\xingxing\.claude\`。

理由：项目级配置随仓库一起版本管理，团队成员与后续的 Claude 实例都能直接获得，不依赖某个人的本机环境。

例外：由 Claude Code 自身管理、路径固定的文件（例如会话的 memory 目录），位置不由本约定决定。

## 技术前提（由需求方指定）

- 语言标准：**C++20**。
- 项目性质：**plugin 插件管理能力库**——只负责发现插件、解析依赖、装配、运行、干净关停；不提供业务逻辑，不提供编辑器，不提供引擎适配。
- 目标平台：**Win x64 / Linux x64 / macOS arm64**（开发 + 发布）、**Android arm64 / iOS arm64**（仅发布）。编译器与 STL 矩阵见架构文档 8.5——注意 Windows 上 `cl` 与 `clang-cl` 都可，Linux/macOS/Android/iOS 用 `clang` + `libc++`。
- 许可证：MIT，版权归 MoozenSoft。
- **不使用 C++ 异常**：全项目以关闭异常的方式编译（Windows `/EHs-c-`、Linux/macOS `-fno-exceptions`）。错误一律经 `Result<T>` / `Error` 显式返回，**不写 `throw` / `try` / `catch`**。连带约束（标准库与第三方库的抛错 API 改用不抛形式、`nlohmann/json` 开 `JSON_NOEXCEPTION`、`EXPECT_THROW` 在本项目 TU 不可用）见架构文档 0.3 原则 7 与 13.2，以及 M0/M1 设计文档第 9 节。

## 设计意图（摘自 README）

> Vase is a plugin framework that treats every module like a branch in flower arranging — carefully selected, gracefully placed, and cleanly removed.

即核心关注点是**模块的选取、挂载与干净卸载**。实现该目标的具体机制（动态库加载方式、ABI 稳定层、插件生命周期接口等）**已有设计提案，见 [`wiki/vase-architecture.md`](wiki/vase-architecture.md)**，但尚未落成代码。不要在文档或代码注释中把提案写成既定事实——新增或变更机制前先与需求方确认。

## 格式化与静态检查

配置：`.clang-format`、`.clang-tidy`。

**`.clang-format`** —— 基线为 LLVM style、标准 C++20，但有多处**显著偏离**（逐项以 `.clang-format` 文件为准），不要按「LLVM 风格」的直觉去写代码。其中 `Allman` 与 `PointerAlignment: Left` 是最容易写错的两项。

**一个例外：宏参数内部的花括号对不受 `BreakBeforeBraces` 管辖。** `Tests/Smoke/CrossDllSmoke.cpp`
的两个 `TEST(...)` 体在本配置下**只能是单行**（实测：把它们折成 Allman，`clang-format --dry-run --Werror`
报 3 处 violation 并 exit 1；`clang-format -i` 会把它们折回单行）——那是格式门要求的输出，不要「修」它。
同一文件里 `namespace` 的花括号仍是 Allman，两者不矛盾。

### 命名规范

由 `.clang-tidy` 的 `readability-identifier-naming.*` 强制，clang-tidy 会直接给出建议名，`--fix` 可自动改写（**包括自动补上前后缀**）。

| 类别 | 规范 | 示例 |
|---|---|---|
| 命名空间 | `lower_case` | `plugin_detail` |
| 类 / 结构 / 联合 / 枚举 / typedef | `CamelCase` | `PluginRegistry` |
| 模板参数（类型 / 模板模板 / 非类型） | `CamelCase` | `TypeParam` |
| 函数（含方法） | `CamelCase` | `LoadPlugin()` |
| 成员变量 | `CamelCase` | `PluginCount` |
| 参数 / 局部变量 | `camelBack` | `pluginPath` |
| **全局变量** | `g` + `CamelCase` | `gPluginCount` |
| **常量**（全局 / 类 / 静态） | `k` + `CamelCase` | `kMaxSize` |
| **constexpr 变量** | `k` + `CamelCase` | `kLimit` |
| **枚举常量** | `k` + `CamelCase` | `kPluginLoaded` |

三条机制要点：

- **前缀之后才判 Case**：`kmaxSize` 违规，`kMaxSize` 合规。
- **成员名不带任何前后缀**：`Message_` / `mCount` / `_private` 全部违规，写 `Message`。
  成员与同名访问器撞车时**让成员改名**，别去改公开的访问器名——`Message()` 与成员撞车，
  成员叫 `Text`；`Root()` / `Failures()` / `OwnerLabel()` 同理。
- `VariableCase: camelBack` 是**伞形兜底**；全局变量、各类常量、constexpr 都已单独覆盖，不再走它。同理 `StructCase` 回退到 `ClassCase`。

两个**空转选项**（留着无害，但当前不产生任何效果）：

- `StaticConstantCase` / `StaticConstantPrefix` —— 实测五种 static 常量写法全部归入其他类目（文件作用域的归 global constant、类内的归 class constant、带 constexpr 的归 constexpr variable），对照实验删除后输出完全一致。
- `TemplateParameterCase` —— 真实的泛型兜底，但 `TypeTemplateParameterCase` 与 `TemplateTemplateParameterCase` 均已设置，故不触发。

## 尚未确定的事项

下表逐项对照「架构文档怎么说的」与「仓库里实际有什么」。**凡「仓库里的状态」仍写着「无」的，都属未定，请先询问而不是假设**——架构文档里「有决定或提案」不等于「有实现」：

| 项 | 架构文档里的状态 | 仓库里的状态 |
|---|---|---|
| 构建系统 | 已决定：CMake + vcpkg（13.2） | **已建立**：`CMakeLists.txt`、`CMakePresets.json`、`Cmake/Toolchains/`（3 个）、`Cmake/Triplets/x64-linux-libcxx.cmake`，六个 preset 全绿（见「构建与测试」） |
| 目录布局与模块划分 | **已确认**（第 10 节） | 目录已按第 10 节起了骨架，但**只有 M0 的探针**：`Source/Session`、`Source/Host`、`Include/Vase/`、`Tests/`。其余模块等真有内容再立 |
| 测试框架与运行方式 | 有分层与验证策略（12 节） | **已建立**：GoogleTest 1.18.0（vcpkg manifest）+ `ctest` + `gtest_discover_tests`；唯一用例是跨 DLL 冒烟（2 个 TEST）。12 节的分层策略尚未落成 |
| 插件接口 / ABI 约定 | 有完整设计（第 3、8 节） | **无头文件**（`Include/Vase/Plugin.h` 只是 M0 的骨架，不是第 3 节的接口） |

**不要从文档推断出可用的命令、路径或接口签名**——文档写的是「打算怎么做」，本文件负责说明「现在有什么」。
