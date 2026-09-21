# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 配套文件

- [`.claude/CLAUDE.md`](.claude/CLAUDE.md) —— CodeGraph 代码检索工具的用法约定。本仓库已在根目录建立 `.codegraph/` 索引，并在 `.mcp.json` 中注册了 codegraph MCP server。

  两者关系：**本文件**描述「这个项目是什么、怎么构建」；**`.claude/CLAUDE.md`** 描述「用什么工具去定位和理解代码」。当 `.codegraph/` 存在时，查找或理解代码应优先调用 `codegraph_explore`，而非 grep/find 或逐个读文件。

  注意：该文件由 CodeGraph 自动维护（内容包裹在 `<!-- CODEGRAPH_START -->` / `<!-- CODEGRAPH_END -->` 之间），**不要手工编辑**；需要改动时通过 CodeGraph 自身重新生成。

- [`wiki/vase-architecture.md`](wiki/vase-architecture.md) —— **架构设计文档 v3**：Vase 的定位与边界、分层模型（`PluginCatalog` / `PluginHost` / `Pod`）、插件清单与 Preset 格式、生命周期与服务解析、**热插拔（档 ①：局内仅装卸无人依赖的叶插件，依赖账本执法）**、验证策略。v3 的逐条变更见其 1.5 节；`Source/Session` → `Source/Pod` 的实体更名**已于 M1-T1 落地**；仓库凡提到现有目录/探针以 `Source/Pod` / `VasePod` 为准。

  注意：文档中的**接口签名与文件格式**仍为提议；**目录布局已确认**（第 10 节）。M1 已按其中的**最小形**落地（描述符宏与基类 + `HeaderVersion`、效果与作用域、服务与事件、依赖账本、Pod / PluginHost、Adopt / Eject 与三档证据），但**落地的是最小形，不等于提案的全量兑现**——例如 `LoadPlan` 就是手写形（D12），不是 §5.1 的清单格式；Catalog 接入时回归提案形。凡未落成代码的，仍按本文件「尚未确定的事项」处理——**先询问，不要假设**。

## 项目状态：M0 与 M1 均已完成

**M0（构建地基）与 M1（Pod 闭环 + 热插拔骨架）均已完成**。当前仓库里有什么：

- **构建系统已建立**：`CMakeLists.txt`、`CMakePresets.json`、`Cmake/Toolchains/`（三个工具链文件）、
  `Cmake/Triplets/x64-linux-libcxx.cmake`，依赖经 vcpkg manifest 模式拉取。
- **两个动态库 target**：`VasePod` 与 `VaseHost`（后者链前者）——「插件不依赖 Host」（D11）
  是链接期事实，不是约定。插件 target 一律经 `Cmake/VasePluginHelpers.cmake` 立（见规矩 5）。
- **架构代码已在**（`Include/Vase/` + `Source/{Pod,Host}`）：描述符宏/基类与 `HeaderVersion`、
  `Result<T>` / `Error`、效果与作用域（`IEffect` / `EffectScope` / `ScopePool`）、
  服务注册表与事件总线、依赖账本（最小形）、`Pod` / `Context`、`PluginHost`、
  `AdoptPlugin` / `EjectPlugin` 与三档卸载证据、`Loader` 与自写的 PE/ELF 镜像解析。
- **测试已建立**：GoogleTest 1.18.0（vcpkg manifest）+ `ctest` + `gtest_discover_tests`，
  分层落成 `Tests/{Smoke,Unit,Lifecycle,Integration,HotSwap,Abi}`（共用 `Tests/TestingSupport`），
  各线基数见「构建与测试」。M0 的跨 DLL 冒烟用例保留。
- **示例**：`Samples/{HelloCommon,HelloPlugin,Embedding}`——`VaseEmbedding play | loop <N> | swapdemo`。
- **仍无 CI**（M5）。

因此：

- 构建 / 测试 / 检查命令，**以本文件「构建与测试」一节为准**——那里写的是跑过的，
  其余任何地方（含 `wiki/`）写的都只是打算怎么做。
- **不要凭想象描述架构。** 已落地的机制以磁盘上的头文件与实现为准；wiki 里的接口签名与
  文件格式仍是提案。新增或变更机制前先与需求方确认。
- 新增源码 / 测试 / 目录时，若引入了本文件没记的规矩或命令，**请同步更新本文件**。

## 构建与测试

**六个 preset，全部已实测可用**。最近一次全量验收（M1-T14，2026-09-17）：**四棵 Windows 树与两棵
Linux 树全部删树重配**（`rm -rf` 后从零 configure），逐线 configure → build → ctest → `ctest -N`，
**六线全绿、构建零警告**；tidy 三条 debug 线各自跑（见「静态检查与格式」），format 一条命令。

| 平台 | preset |
|---|---|
| Windows / clang-cl | `win-x64-clang-debug`、`win-x64-clang-release` |
| Windows / cl.exe | `win-x64-msvc-debug`、`win-x64-msvc-release` |
| Linux / clang + libc++ | `linux-x64-clang-debug`、`linux-x64-clang-release` |

**各线 `ctest -N` 基数**（2026-09-17 实测；`ctest` 在没发现测试时同样返回 0，故基数要按线单独记，
见下方「核这些门禁时，退出码单独用是不够的」）：

| preset | `Total Tests` | 与 Win debug 的差 |
|---|---|---|
| `win-x64-{clang,msvc}-debug` | 75 | —（基线） |
| `win-x64-{clang,msvc}-release` | 74 | −1：T3 的 death test 受 `#ifndef NDEBUG` 门 |
| `linux-x64-clang-debug` | 77 | +2：T11 的两条 Linux-only（`Adopt.MissingIdentityFeatureRejectedWithPointer`、`Adopt.RenameReplacementCaughtByTierThree`——`NoBuildIdPlugin` 这个 fixture 在 `if(NOT WIN32)` 里） |
| `linux-x64-clang-release` | 76 | 同上两点相抵：+2 −1 |

**基数差是设计，不是漏注册**：debug 与 release 差的 1 条是 T3 的 death test（`#ifndef NDEBUG`），
Linux 与 Windows 差的 2 条是 T11 的 Linux-only 用例（`-Wl,--build-id=none` 的 fixture 只在 Linux 存在）。
两侧都与预期值逐位对上，说明 `gtest_discover_tests` 没有静默漏掉任何一条。

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

**四条 Windows 线的「删树重配全量」有脚本形态**：`Scripts\win-verify.cmd`（对应 Linux 侧的
`Scripts/linux-verify.sh`）。四棵 preset 树各自 `rmdir /s /q` 后跑 configure → build →
ctest → `ctest -N`，逐步打印退出码，末尾汇总并以失败步数作退出码；基数表它**不复制**，
仍以上面那张为准（写进脚本就是第二处真值）。前提是 `VCPKG_ROOT` 已设。

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

### 工具链 flag 是承重的（§8.2 档三的构建要求）

三个工具链文件里有两条 flag **不是可选的优化，而是身份特征的构建要求**（§8.2 档三 / 13.2 末行）：

- **Windows 两条线**（`Cmake/Toolchains/windows-x64-clangcl.cmake`、`windows-x64-msvc.cmake`）：
  `CMAKE_SHARED_LINKER_FLAGS_INIT` 追加 **`/DEBUG:FULL`**——没有它就没有 PE 调试目录里的
  CodeView(RSDS)，`AdoptPlugin` 会直接拒绝该二进制（响亮的失败，不做静默降级）；
- **Linux**（`linux-x64-clang-libcxx.cmake`）：`CMAKE_SHARED_LINKER_FLAGS_INIT` 追加
  **`-Wl,--build-id=sha1`**——`.note.gnu.build-id` 是与 RSDS 对位的身份特征，档三在内存镜像
  与磁盘文件上比对的就是这段 desc。

**摘掉它们的症状是运行期的响亮失败，不是编译失败**：构建照常全绿，直到 Adopt 全线拒绝才暴露。
所以摘除或改动这两条，**必须重跑 T12 主循环**（`-R HotSwap`，双平台各自留证据）。

**另有一条 configure 期的坑（T9 实测）**：`CMAKE_*_FLAGS_INIT` **只在工具链首次 configure 时
进入缓存**。工具链后来才加上 `/DEBUG:FULL`、而某棵树在那之前就配过，那棵树的
`CMAKE_SHARED_LINKER_FLAGS` 就是空的、`LoadProbe.dll` 没有 `.pdb`，
`Loader.MemoryIdentityMatchesFileIdentity` 当场失败。**修法是删掉那棵树重新 configure**，
不是用 `-D` 钉一个永久的手工 override——后者会把后续所有工具链改动一起遮住。

### 静态检查与格式

```bash
run-clang-tidy -p build-win/win-x64-clang-debug          # clang-cl 线（48 个 TU）
run-clang-tidy -p build-win/win-x64-msvc-debug -extra-arg=-Wno-unused-command-line-argument
run-clang-tidy -p build-linux/linux-x64-clang-debug      # Linux 线**必须单独跑**
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' \
  | xargs -0 clang-format --dry-run --Werror
```

`clang-tidy` 与 `clang-format` 都是全平台同一份 LLVM 23.1.0（由 PATH 解析；两侧同源 commit
`ea7d852a`）。`git ls-files` 那串 `--cached --others --exclude-standard` 不能省——
**尚未 `git add` 的新文件会被静默跳过**，门禁照样绿（本仓库实测过）。

**三条 tidy 线都有脚本形态**：`Scripts/linux-clang-tidy.sh`（Linux，经登录 shell）与
`Scripts\win-clang-tidy.cmd`（Windows 两条 debug 线，参数 `clangcl` / `msvc` 可单跑一条）。
两者同契约：日志落在脚本旁边（`*.log`，已被 gitignore），stdout 打全「退出码 + 正文
`error:` 条数 + 正文 `warning:` 条数」三判据与摘要计数，**退出码非 0 即门禁未过**，
不必再手工 grep 日志。基数以下一节「核这些门禁时」的实测表为准，脚本不复制阈值。
实测两侧输出与该表逐位对上：Linux 49 TU / 133833 / NOLINT 18，Windows 两线各 48 TU /
309464 / NOLINT 30。

但**别指望 configure 帮你守住版本**：`CMakeLists.txt` 的版本校验对象是
**CXX 编译器**，只查 clang 的 `23.x` 主版本；cl.exe 分支更是只查平台、不查版本。
**它管不到 `clang-format` / `clang-tidy` 这两个独立可执行文件。**
「CXX 编译器的 clang 是 23.x」不等于「tidy / format 也是 23.x」——
后者由环境（PATH 最前）保证，不是 configure 保证的。

#### 「两侧 LLVM 同版本」是必要条件，不是充分条件

**「Windows 两条线绿」不能推「Linux 绿」。** 同一版 clang-tidy（两侧均 23.1.0）跑两棵树，
T11 实测（修复前）Linux 线报 **9 条**正文 warning，Windows 线 **0 条**。这 9 条要分两类看：

- **两条落在两侧都编译的共享文件上**：`Source/Pod/Pod.cpp:88`、`Source/Host/PluginHost.cpp:196`
  的反向 `for` 循环（`modernize-loop-convert`），Windows 线**就是不报**——成因是 **STL 不同**
  （MSVC STL 的 `rbegin()` 走另一条路径），**不是版本差**。这一对才是「同版本 + 同一份源码、
  结果仍不同」的实证；
- 其余 7 条落在 `Source/Host/LoaderPosix.cpp`、`ImageInspectPosix.cpp` 里，那两个 TU
  **在 Windows 上根本不编译**——它们的告警天然只有 Linux 线看得见。

那 9 条已在 T11 修复轮逐处处置（代码级修法优先，确无出路的就地 NOLINT 并写明理由）。

推论两半都要照做：

1. **三条 debug 线（clang-cl / cl.exe / Linux）各自跑、各自读正文**，别用一条推另一条；
2. **平台专属 TU（`*Posix.cpp` / `*Windows.cpp`）天然只在一条线上被检查**——
   它们的告警只有 Linux 线看得见，抑制数目也因此不对称。

#### NOLINT 的口径是「有没有代码级出路」，不是数量

旧口径是数量门槛（「同类累计超过 4 处就改为在 `.clang-tidy` 里禁用该检查」）——**已作废**：
仓库现在的抑制数早已越过任何这样的门槛，而每一处都在**就地写明理由**并逐条过评审。现行规矩：

- **每一处抑制都要就地写清为什么**——具体到这个检查在这行上为什么没有代码级写法；
- 有价值的问题是**「有没有代码级出路」**，不是「有几处」。有出路的就改写法（改写法 ≠ 改语义，
  `Tests/Unit/LoaderTests.cpp` 用 `std::next + memcpy` 换掉一处抑制就是例子）；
- 「确无出路」要能说清是哪种：平台 API 的签名规定（`dl_iterate_phdr` 的回调参数类型、
  `open(2)` 本身是变参）、本仓库禁用的写法（`.at()` 被禁、`gsl::at` 不可用）——
  **不是「改起来麻烦」**。

### 核这些门禁时，退出码单独用是不够的

两个**静默**陷阱，都在本仓库实测过：

- **`run-clang-tidy` 退出 0 ≠ 没有 warning。** `.clang-tidy` 的 `WarningsAsErrors`
  为空，tidy 永远不会因 warning 失败。健康的输出长这样（每行都是摘要行，
  正文一条 `error:` / `warning:` 都没有）：

  ```
  Running clang-tidy in 12 threads for 48 files out of 48 in compilation database ...
  998 warnings generated.
  Suppressed 998 warnings (998 in non-user code).
  ```

  所以「无新 warning」的判据是三条一起：**退出 0 + 正文 `error:` 0 条 +
  正文 `warning:` 0 条**，再连摘要行一起读。只 grep `warning:` 会漏掉全部被抑制的量。
  实测基数（M1-T14 验收，2026-09-17，全部落在第三方头里，我们自己的代码零正文 warning）：

  | 线 | 文件数 | 摘要行 Suppressed 合计 | 单 TU 最小 / 最大 |
  |---|---|---|---|
  | clang-cl（`win-x64-clang-debug`） | 48 | 309464 | 998 / 37647 |
  | cl.exe（`win-x64-msvc-debug`） | 48 | 309464 | 998 / 37647 |
  | Linux（`linux-x64-clang-debug`） | 49 | 133833 | 359 / 11337 |

  数字变了不一定是错，但**要看它变在哪一类**——摘要行里还会出现 `N NOLINT`
  （本轮的抑制命中次数：Windows 30、Linux 18；**这不是仓库里的抑制处数**，
  同一个抑制会被每个包含它的 TU 各计一次），以及 gtest 模板实例化带来的巨量非用户代码告警。

- **`ctest` 在一个测试都没发现时同样返回 0。** 所以「测试全绿」不能只跑
  `ctest --preset <p>`，必须另跑一次 `ctest --preset <p> -N`，把 `Total Tests`
  与「构建与测试」一节那张**按线分账的基数表**逐位对上（Win debug 75 / Win release 74 /
  Linux debug 77 / Linux release 76）。`gtest_discover_tests` 用的是 `DISCOVERY_MODE PRE_TEST`，
  枚举发生在 ctest 运行时——测试被漏注册时，`ctest` 会一声不吭地报成功。

## 在这个仓库里干活要知道的规矩

以下六条：1 与 2 是**踩过的坑**（1 真实炸过一次构建，2 是实测出来的失败方式），
3 与 4 是**读这套 flag 时最容易想反的地方**，5 与 6 是**插件形态与测试主干的出口约定**。
改动相关代码前先读。

### 1. 新 target 必须链接 `VaseBuildOptions`

`VaseBuildOptions`（根 `CMakeLists.txt`）是编译选项的唯一出口：警告级别与
`/WX` / `-Werror`、`/utf-8`、`/EHs-c-` / `-fno-exceptions`、`_HAS_EXCEPTIONS=0`、
cl.exe 线的 `/we4530`、`/wd4251` 与 `/external:*` 全在里面。**新建 target 时忘了链它，
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

**C4251 走全局 `/wd4251`，不走头文件里的逐类 `#pragma` 区域**（导出类带 STL 成员：池、
槽表、账本）。前提由 §8.5 的工具链与 STL 矩阵钉死、D13 的 `HeaderVersion` 兜着。
两条代价已知并接受：该 flag 是整 TU 免检，所以新增「导出类带 STL 成员」**不再有逐处把关**；
且它**出不了这棵树**——仓库外的插件作者与嵌入方 include Vase 头时，C4251 报在我们的头文件
那一行、由他们自己的编译选项管，需要他们自行加 `/wd4251`。**若 §8.5 那条前提破掉**（允许
插件用不同版本的 MSVC STL 构建），全局豁免即失去依据，届时改 pimpl 或把 STL 成员移出导出面。
站点数、M5 分发时的两条还债路径，记在 [`wiki/vase-architecture.md`](wiki/vase-architecture.md) 的 13.3。

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

### 5. 插件 target 一律经 `vase_add_plugin_fixture`

`Cmake/VasePluginHelpers.cmake` 的 `vase_add_plugin_fixture(name SOURCES … LINK_LIBRARIES …)`
是**插件形态的唯一出口**——示例与测试 fixture 都走它（`Samples/HelloPlugin/CMakeLists.txt` 亦然）。
它承载两件手搭 `add_library(SHARED)` 会漏掉的事：**只链 VasePod**（D11：「插件不依赖 Host」
是链接期事实而非约定）与**可见性收紧**（`CXX_VISIBILITY_PRESET hidden` + `VISIBILITY_INLINES_HIDDEN`）。
手搭的插件 target 视为违规——那也正是「某个 target 忘了链 `VaseBuildOptions`」（规矩 1）的复发点。

### 6. `Tests/HotSwap` 按主干对待（它的判据力按平台不对称）

v3 §12.2：v2 里 `Tests/Reload` 是「不必每次提交都跑」的尾部测试；热插拔升为主干承诺后，
**任何改动 Loader、依赖账本、Eject / Adopt 路径、描述符布局或 `HeaderVersion` 的提交，
必须跑 `Tests/HotSwap/` 的全部用例，且 Windows 与 Linux 各自留证据**。
这几类改动正是「六线全绿」最靠不住的地方。

> **选择子是 `-R 'HotSwap|Eject|Adopt'`，不是 `-R HotSwap`。** `Tests/HotSwap/` 的用例名按套件
> 分三类：`HotSwap.*`（主循环）、`Eject.*`（拆除路径）、`Adopt.*`（领回路径）。**`-R HotSwap`
> 只选到第一类**，会把守 Eject / Adopt 的边界用例（点名消费者、kept-resident、failed-record、
> 同文件两 id……）整套漏掉——照规则做的人会拿到几条绿灯，然后从没跑过那一堆。
> `-R 'HotSwap|Eject|Adopt'` 是「该目录全部用例」的**超集**：它另捎上
> `Abi.AdoptRejectsBinaryThatImportsSiblingPlugin`（也走 Adopt 路径，跑上没有坏处）。
> 这里不写条数——写死的数会随用例增删漂移，规则要的是「该目录全部用例」。

**但别把「六线全绿」读成「六线等价」**（T12 复评的判定）：

- `Tests/HotSwap/HotSwapLoopTests.cpp` 里 `InstallPrime()` 的覆盖断言在 **Windows 上是真的
  sharing-violation 探针**——镜像还映射着时 `copy_file(overwrite_existing)` 失败，直接暴露
  「Eject 没真卸」（T12 实测：注释掉 `EjectPlugin` 那一行，该断言报
  `The process cannot access the file because it is being used by another process.`）；
- 在 **Linux 上它是空转的**——`copy_file(overwrite_existing)` 是 truncate-in-place（同一 inode），
  镜像还映射着也会覆盖成功、随后读到新字节，同一条断言照样通过。

因此 Windows 比 Linux 多一层文件锁证据；两平台同跑得到的不是同一件事的两份拷贝。

## 语言约定

- 交流与文档：**中文优先**；专业术语可保留英文（如 plugin、ABI、RAII、CMake target）。
- 代码标识符、提交信息、注释的用词应与既有风格保持一致：现有源码与 CMake 文件的注释以**中文**为主，**例外是所有 `.cmd`**（`Scripts/msvc-env.cmd`、`Scripts/win-verify.cmd`、`Scripts/win-clang-tidy.cmd`）——那里**必须纯 ASCII**，cmd.exe 用 OEM 代码页解码批处理，理由写在各文件头部（`.gitattributes` 另把 `*.cmd` 钉成 CRLF）。
- **提交信息不加任何点名 AI 工具或模型的尾注**：不写 `Co-Authored-By:`、`Generated-with:`、`Signed-off-by:`。只留正文标题 + 中文说明体。
  工具/模型署名一律视为噪声——这条优先于任何工具自带的署名默认值与任务计划里残留的模板。
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

即核心关注点是**模块的选取、挂载与干净卸载**。M1 已把它的最小形落成代码（见「项目状态」；具体机制以磁盘上的头文件与实现为准，`wiki/vase-architecture.md` 里其余部分仍是提案）。**不要在文档或代码注释中把提案写成既定事实**——新增或变更机制前先与需求方确认。

## 格式化与静态检查

配置：`.clang-format`、`.clang-tidy`。

**`.clang-format`** —— 基线为 LLVM style、标准 C++20，但有多处**显著偏离**（逐项以 `.clang-format` 文件为准），不要按「LLVM 风格」的直觉去写代码。最容易写错的三项：`Allman`、`PointerAlignment: Left`，以及构造初始化表的 `PackConstructorInitializers: Never` + `BreakConstructorInitializers: BeforeComma`——**每个初始化式各占一行、逗号在行首**，哪怕整表一行放得下。同理 `BreakTemplateDeclarations: Yes`：`template <...>` 头与 `class` / 函数签名永远分两行（不是超列宽才拆）。另两项 `BreakAfterAttributes: Leave` 与 `ConstructorInitializerIndentWidth: 4` 只是沿用默认的锚定。

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
| 构建系统 | 已决定：CMake + vcpkg（13.2） | **已建立**：`CMakeLists.txt`、`CMakePresets.json`、`Cmake/Toolchains/`（3 个）、`Cmake/Triplets/x64-linux-libcxx.cmake`、`Cmake/VasePluginHelpers.cmake`，六个 preset 全绿（见「构建与测试」） |
| 目录布局与模块划分 | **已确认**（第 10 节） | **已按第 10 节落成**：`Include/Vase/`（`Plugin.h` / `PluginDescriptor.h` / `Effect` / `Service` / `Event` / `Pod` / `Host` / `Detail`）、`Source/{Pod,Host}` 双 target、`Samples/{HelloCommon,HelloPlugin,Embedding}`、`Tests/{Smoke,Unit,Lifecycle,Integration,HotSwap,Abi,TestingSupport}`。第 10 节里尚未出现的实体（`Catalog/`、`Host/` 的清单解析面）等真有内容再立 |
| 测试框架与运行方式 | 有分层与验证策略（12 节） | **已落成**：GoogleTest 1.18.0（vcpkg manifest）+ `ctest` + `gtest_discover_tests`；分层即 12.2 那五类（Unit / Integration / Lifecycle / HotSwap / Abi，外加 M0 的 Smoke），各线基数见「构建与测试」的按线分账表。12 节里依赖 M2 起的部分（Catalog 求解链、账本 3b 完整执法）尚未落成 |
| 插件接口 / ABI 约定 | 有完整设计（第 3、8 节） | **已落地最小形**：§3.1 的宏（`VASE_PLUGIN`）、插件基类、描述符结构体与 `kHeaderVersion` 都在 M1；**清单/`Preset` 格式与 `Catalog` 解析在 M2**。接口以磁盘上的 `Include/Vase/Plugin.h` 与 `PluginDescriptor.h` 为准，第 3、8 节其余部分仍是提案 |

**不要从文档推断出可用的命令、路径或接口签名**——文档写的是「打算怎么做」，本文件负责说明「现在有什么」。
