# CLAUDE.md


This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 目录

四组按「先弄清要做什么 → 再动手 → 最后宣布完成」排。

**节名是稳定接口**：`.claude/skills/vase-cpp-engineering/` 里有几十处按名字指向本文件各节（`构建与测试`、`静态检查与格式`、`工具链 flag 是承重的`、`规矩 N`…）。**改这些标题要同步那些链路**，否则技能里的指针当场变孤儿（见规矩 7）。

- **一、这个项目是什么** —— 定位、现状，以及本文件与技能 / wiki 的分工。第一次接触仓库先读这一组。
- **二、怎么跑** —— 六个 preset 的命令、按线基数、产物落位、第三方依赖的两条入口，与两条承重的工具链 flag。
- **三、怎么写** —— 语言与提交约定、七条规矩、格式与命名的偏离项。改代码前读。
- **四、怎么验** —— tidy 与 format 怎么跑、退出码为什么单独不够、基数怎么读。宣布完成前读。

找命令与基数去 `构建与测试`；找七条规矩去 `在这个仓库里干活要知道的规矩`；跑门禁与读基数表去 `静态检查与格式` 与 `核这些门禁时，退出码单独用是不够的`（这两节原先嵌在 `构建与测试` 里，现独立成组）。

## 一、这个项目是什么

定位、现状，以及本文件与技能 / wiki 的分工。第一次接触仓库先读这一组。

### 技术前提（由需求方指定）


- 语言标准：**C++20**。
- 项目性质：**plugin 插件管理能力库**——只负责发现插件、解析依赖、装配、运行、干净关停；不提供业务逻辑，不提供编辑器，不提供引擎适配。
- 目标平台：**Win x64 / Linux x64 / macOS arm64**（开发 + 发布）、**Android arm64 / iOS arm64**（仅发布）。编译器与 STL 矩阵见架构文档 8.5——注意 Windows 上 `cl` 与 `clang-cl` 都可，Linux/macOS/Android/iOS 用 `clang` + `libc++`。
- 许可证：MIT，版权归 MoozenSoft。
- **不使用 C++ 异常**：全项目以关闭异常的方式编译（Windows `/EHs-c-`、Linux/macOS `-fno-exceptions`）。错误一律经 `Result<T>` / `Error` 显式返回，**不写 `throw` / `try` / `catch`**。连带约束（标准库与第三方库的抛错 API 改用不抛形式、`nlohmann/json` 开 `JSON_NOEXCEPTION`、`EXPECT_THROW` 在本项目 TU 不可用）见架构文档 0.3 原则 7 与 13.2，以及 M0/M1 设计文档第 9 节。

### 设计意图（摘自 README）


> Vase is a plugin framework that treats every module like a branch in flower arranging — carefully selected, gracefully placed, and cleanly removed.

即核心关注点是**模块的选取、挂载与干净卸载**。M1 已把它的最小形落成代码，M2a 补了装配地基那一层（见「项目状态」；具体机制以磁盘上的头文件与实现为准，`wiki/vase-architecture.md` 里其余部分仍是提案）。**不要在文档或代码注释中把提案写成既定事实**——新增或变更机制前先与需求方确认。

### 项目状态：M0、M1 完成；M2a 完成（M2 第一波），M2b 未起


**M0（构建地基）、M1（Pod 闭环 + 热插拔骨架）与 M2a（M2 第一波：装配地基——配置面、`LoadPlan` 定形、
多插件装配与跳过/碰撞执法、递归拆除、报告结构化）均已完成**。
**M2b（第二波：`Catalog` / 清单解析 / `Solve` / JSON / Preset）未起**——求解链、清单格式、
`enum` 配置型与 choices schema 都在那一波。当前仓库里有什么：

- **构建系统已建立**：`CMakeLists.txt`、`CMakePresets.json`、`Cmake/Toolchains/`（三个工具链文件）、
  `Cmake/Triplets/x64-linux-libcxx.cmake`，依赖经 vcpkg manifest 模式拉取。
- **两个动态库 target**：`VasePod` 与 `VaseHost`（后者链前者）——「插件不依赖 Host」（D11）
  是链接期事实，不是约定。插件 target 一律经 `Cmake/VasePluginHelpers.cmake` 立（见规矩 5）。
- **架构代码已在**（`Include/Vase/` + `Source/{Pod,Host}`）：描述符宏/基类与 `HeaderVersion`、
  `Result<T>` / `Error`、效果与作用域（`IEffect` / `EffectScope` / `ScopePool`）、
  服务注册表与事件总线、依赖账本（最小形）、`Pod` / `Context`、`PluginHost`、
  `AdoptPlugin` / `EjectPlugin` 与三档卸载证据、`Loader` 与自写的 PE/ELF 镜像解析；
  M2a 增**配置面**——`Include/Vase/Config/` 四头（`Value` / `FieldInfo` / `ConfigInfo` /
  `ConfigMacros`，header-only，无 Source 对称实体）+ 宿主拥有层 `ConfigBlob`、`LoadPlan` 定形
  （含 `BinaryPath` 与缺字段回退，D23）、装配预检与 Provides 碰撞执法（两态形，D27/D33）、
  `OnStart` 失败的递归拆除（闭包 + 逆数组序，D28）、`EjectReport` / `AdoptReport` 结构化（D20/D21）。
- **测试已建立**：GoogleTest 1.18.0（vcpkg manifest）+ `ctest` + `gtest_discover_tests`，
  分层落成 `Tests/{Smoke,Unit,Lifecycle,Integration,HotSwap,Abi}`（共用 `Tests/TestingSupport`），
  各线基数见「构建与测试」。M0 的跨 DLL 冒烟用例保留。M2a 新增测试文件 7 个
  （`Unit/{ConfigValueTests,ConfigMacroTests,ConfigBlobTests}`、
  `Integration/{MultiPluginAssemblyTests,ConfigApplyTests,RecursiveTeardownTests}`、
  `Lifecycle/MultiPluginCycleTests`），`Tests/Integration/fixtures/` 现共 23 个 fixture 插件，
  其中 M2a 新增 16 个（Behind\* 五个、Cycle 一对、Dead 三个、StartFail / FailingEdge /
  SharedConsumer2 / Collision / ConfigConsumer / ConfigLayoutMisuse 各一）。
- **示例**：`Samples/{HelloCommon,HelloPlugin,HelloPluginPrime,Embedding}`——
  `VaseEmbedding play | loop <N> | swapdemo`，`HelloPluginPrime` 是 `file install` 的换件材料。
  Hello 两个插件自 M2a 起带**真实配置**（`VASE_CONFIG` 的 `Repeats` 字段），`play` 打出的
  `… x1` 就是配置默认值走通的肉眼证人（换件后 console 回放链打 `prime v2 x1`）。
- **工具**：`Tools/VaseConsole/`（target 与二进制同名）是热插拔验证台——
  `VaseConsole [--script <file>]`，不给 `--script` 即交互，退出码 = 全程是否 Clean。
  **别与架构 §11.1 的 `Tools/VaseCli` 混为一谈**：那是尚不存在的清单扫描 / 校验工具（M5），
  与本仓库实有的这个交互式验证台是两回事（更名与移位的经过见「尚未确定的事项」表）。
- **第三方源码依赖一个**：`ThirdParty/cli`（`MoozenSoft/cli` 的 **git submodule**，`daniele77/cli`
  的无异常无 asio fork，BSL-1.0）。它**不走 vcpkg**，接入方式见「构建与测试」的
  「第三方依赖有两条入口」那条。
- **仍无 CI**（M5）。

因此：

- 构建 / 测试 / 检查命令，**以本文件「构建与测试」一节为准**——那里写的是跑过的，
  其余任何地方（含 `wiki/`）写的都只是打算怎么做。
- **不要凭想象描述架构。** 已落地的机制以磁盘上的头文件与实现为准；wiki 里的接口签名与
  文件格式仍是提案。新增或变更机制前先与需求方确认。
- 新增源码 / 测试 / 目录时，若引入了本文件没记的规矩或命令，**请同步更新本文件**。

### 配套文件


- [`.claude/CLAUDE.md`](.claude/CLAUDE.md) —— CodeGraph 代码检索工具的用法约定。本仓库已在根目录建立 `.codegraph/` 索引，并在 `.mcp.json` 中注册了 codegraph MCP server。

  两者关系：**本文件**描述「这个项目是什么、怎么构建」；**`.claude/CLAUDE.md`** 描述「用什么工具去定位和理解代码」。当 `.codegraph/` 存在时，查找或理解代码应优先调用 `codegraph_explore`，而非 grep/find 或逐个读文件。

  注意：该文件由 CodeGraph 自动维护（内容包裹在 `<!-- CODEGRAPH_START -->` / `<!-- CODEGRAPH_END -->` 之间），**不要手工编辑**；需要改动时通过 CodeGraph 自身重新生成。

- [`wiki/vase-architecture.md`](wiki/vase-architecture.md) —— **架构设计文档 v3**：Vase 的定位与边界、分层模型（`PluginCatalog` / `PluginHost` / `Pod`）、插件清单与 Preset 格式、生命周期与服务解析、**热插拔（档 ①：局内仅装卸无人依赖的叶插件，依赖账本执法）**、验证策略。v3 的逐条变更见其 1.5 节；`Source/Session` → `Source/Pod` 的实体更名**已于 M1-T1 落地**；仓库凡提到现有目录/探针以 `Source/Pod` / `VasePod` 为准。

  注意：文档中的**接口签名与文件格式**仍为提议；**目录布局已确认**（第 10 节）。M1 已按其中的**最小形**落地（描述符宏与基类 + `HeaderVersion`、效果与作用域、服务与事件、依赖账本、Pod / PluginHost、Adopt / Eject 与三档证据），M2a 又补上配置面与装配执法。**落地的是最小形，不等于提案的全量兑现**——`LoadPlan` 到 M2a 已**定形**（D12 手写形的过渡结束：`Entry` 含 `BinaryPath`、缺字段回退默认，装配序由预检兜住；M2b 只是换 `Solve` 来生产同形计划，零返工，见 §5.1 落地勘误）。凡未落成代码的，仍按本文件「尚未确定的事项」处理——**先询问，不要假设**。

- [`wiki/vase-console-use.md`](wiki/vase-console-use.md) —— **`Tools/VaseConsole` 的使用文档**：两种运行方式、命令一览、plan 格式、报告字段读法、退出码规则与回放资产清单。与上面那份不同，它描述的是**实有工具**，输出示例均为实测截取；命令与退出码行为以磁盘上的 `Tools/VaseConsole/` 为准，构建命令与测试基数仍以本文件为准（它不复制）。

- [`.claude/skills/vase-cpp-engineering/`](.claude/skills/vase-cpp-engineering/) —— **本仓库的 C++ 工程约束技能**：`SKILL.md` 给工程权重、五条不可违反、干活流程、提交前自查与「改什么必须验什么」矩阵；`references/` 按 architecture / ownership-lifetime / error-handling / abi-boundary / plugin-lifecycle / concurrency / performance / portability / verification 分面，另附一份通用 C++20 写法参考。设计、实现或评审任何 C++ 之前先读它。

  **与本文件的关系**：本文件是命令、构建规矩与测试基数的**唯一真值来源**。技能该给的是「什么边界不能跨、该读哪一份、某类改动必须验到哪一档」这一层判断，而不是把本文件的话在远处再说一遍——门禁阈值抄进第二处必腐：M2 每加一条用例，本文件改、技能不改，拿技能当准的人会算出「少了几条」，把健康的构建判成漏注册。

  **债已收（2026-09-23 复测）**：基数与差值算式、preset 名、命令块、工具链文件清单、HotSwap 选择子字符串都已撤出技能（首轮评测实测到的「把数字递到手上，回答只会复述结论」那个后果，其教训现记在 `references/verification.md` §2）。技能里残留的字面值命中规矩 7 的**允许两类**——嵌在论述里的承重 flag 与配置值（各处已带「字面值以本文件为准」的就地指针），以及允许两边各写一份的判据与理由。跑「在这个仓库里干活要知道的规矩」末尾那条自检 grep，预期命中的就是这一合规形态；此后它每命中一处，仍要逐条判类别。冲突时一律**以本文件为准，并回头修正技能**。

  接入方式：Claude Code 从项目级 `.claude/skills/` **自动发现**它，靠 frontmatter 的 `description` 触发，不需要在 `settings.json` 里开任何东西；主会话与 `Agent` 工具起的子代理都会拿到（已实测）。同一个约束在本仓库可能有**三种载体**：源文件自己的头部注释、本文件、技能。**越靠近改动现场越不易腐**——`PluginDescriptor.h` 的 POD 纪律、`Context.cpp` 落账本边那一段，都属于第一类，它们不需要、也不应该在别处再写一遍。

### 尚未确定的事项


下表逐项对照「架构文档怎么说的」与「仓库里实际有什么」。**凡「仓库里的状态」仍写着「无」的，都属未定，请先询问而不是假设**——架构文档里「有决定或提案」不等于「有实现」：

| 项 | 架构文档里的状态 | 仓库里的状态 |
|---|---|---|
| 构建系统 | 已决定：CMake + vcpkg（13.2） | **已建立**：`CMakeLists.txt`、`CMakePresets.json`、`Cmake/Toolchains/`（3 个）、`Cmake/Triplets/x64-linux-libcxx.cmake`、`Cmake/VasePluginHelpers.cmake`，六个 preset 全绿（见「构建与测试」） |
| 目录布局与模块划分 | **已确认**（第 10 节） | **磁盘上是这些**：`Include/Vase/`（`Plugin.h` / `PluginDescriptor.h` / `Config` / `Effect` / `Service` / `Event` / `Pod` / `Host` / `Detail`）、`Source/{Pod,Host}` 双 target、`Samples/{HelloCommon,HelloPlugin,HelloPluginPrime,Embedding}`、`Tools/VaseConsole/`、`Tests/{Smoke,Unit,Lifecycle,Integration,HotSwap,Abi,TestingSupport}`、`ThirdParty/cli`；第 10 节里尚未出现的实体（`Catalog/`、`Host/` 的清单解析面）等真有内容再立。**但别把这一格读成"§10 是当前状态的描述"**：§10 的 `Samples/` 子树列着两个从未存在的目录、漏掉两个实有的；它提议的 `Tools/VaseCli/`（清单扫描、校验、索引生成，M5）与实有的 `Tools/VaseConsole/`（交互式验证台）**不是一回事**——两者同在 `Tools/` 下、名字只差一个词，别混；后者原名 `Samples/VaseCli`，2026-09-23 因「CLI 这个名字不体现它是交互式验证台」而更名并移出 `Samples/`，同一轮的 spec / plan 在 `docs/superpowers/` 下同步改名为 `vase-console-*`。**改 §10 本身要需求方过目**（它标着"已确认"），不要顺手修 |
| 测试框架与运行方式 | 有分层与验证策略（12 节） | **已落成**：GoogleTest 1.18.0（vcpkg manifest）+ `ctest` + `gtest_discover_tests`；分层即 12.2 那五类（Unit / Integration / Lifecycle / HotSwap / Abi，外加 M0 的 Smoke），各线基数见「构建与测试」的按线分账表。12 节里依赖 M2 起的部分（Catalog 求解链、账本 3b 完整执法）尚未落成 |
| 插件接口 / ABI 约定 | 有完整设计（第 3、8 节） | **已落地最小形**：§3.1 的宏（`VASE_PLUGIN`）、插件基类、描述符结构体与 `kHeaderVersion` 都在 M1；M2a 补配置面（`VASE_CONFIG` 六型、`ConfigInfo`/`ConfigBlob`，`enum` 与描述符布局的再变更留 M2b 随 bump）；**清单/`Preset` 格式与 `Catalog` 解析在 M2b**。接口以磁盘上的 `Include/Vase/Plugin.h` 与 `PluginDescriptor.h` 为准，第 3、8 节其余部分仍是提案 |

**不要从文档推断出可用的命令、路径或接口签名**——文档写的是「打算怎么做」，本文件负责说明「现在有什么」。
## 二、怎么跑

六个 preset 的命令、按线基数、产物落位、第三方依赖的两条入口，与两条承重的工具链 flag。

### 构建与测试


**六个 preset，全部已实测可用**。基数的最近一次复核（终审修复波复测，2026-09-24）：**六条 preset 线
各自 configure → build → ctest → `ctest -N`，六线全绿、构建零警告**——走的是下面那两个
脚本的**删树重配全量**（四棵 Windows 树与两棵 Linux 树，`rm -rf` 后从零 configure）。
本轮的全量跑了两次（tidy 代码级出路前后各一次）：两次都六线一次过、无失败步，
未撞到下面记的那类 `z-applocal` 文件锁假红。
tidy 三条 debug 线各自跑（见「静态检查与格式」），format 一条命令。

| 平台 | preset |
|---|---|
| Windows / clang-cl | `win-x64-clang-debug`、`win-x64-clang-release` |
| Windows / cl.exe | `win-x64-msvc-debug`、`win-x64-msvc-release` |
| Linux / clang + libc++ | `linux-x64-clang-debug`、`linux-x64-clang-release` |

**各线 `ctest -N` 基数**（六线全部为终审修复波复测 2026-09-24 实测：终审修复波加两条用例
（`ConfigApply.HostSuppliedStringConfigOutlivesPlanDonor` 钉用例 + `ConfigBlob.FromDefaultsLaysFieldsInDeclarationOrder`），
两例均无 death 门与平台门，六线相对 M2a 收口波同幅 +2；
`ctest` 在没发现测试时同样返回 0，故基数要按线单独记，见「四、怎么验」下的「核这些门禁时，退出码单独用是不够的」）：

| preset | `Total Tests` | 与 Win debug 的差 |
|---|---|---|
| `win-x64-{clang,msvc}-debug` | 141 | —（基线） |
| `win-x64-{clang,msvc}-release` | 140 | −1：T3 的 death test 受 `#ifndef NDEBUG` 门 |
| `linux-x64-clang-debug` | 143 | +2：T11 的两条 Linux-only（`Adopt.MissingIdentityFeatureRejectedWithPointer`、`Adopt.RenameReplacementCaughtByTierThree`——`NoBuildIdPlugin` 这个 fixture 在 `if(NOT WIN32)` 里） |
| `linux-x64-clang-release` | 142 | 同上两点相抵：+2 −1 |

**基数差是设计，不是漏注册**：debug 与 release 差的 1 条仍是 T3 的 death test（`#ifndef NDEBUG`）——
M2a 新增的 `ConfigApply.LayoutMismatchTerminates` 同为 death test 但**不带**这道门，
debug/release 同计（两条 release 线实测全绿），故 −1 差值不因它而变；
Linux 与 Windows 差的 2 条仍是 T11 的 Linux-only 用例（`-Wl,--build-id=none` 的 fixture 只在 Linux 存在），
M2a 新 fixture 群无一 Linux-only，+2 不变；终审修复波新增的两条用例同无门，差值亦不因它们而变。
两侧都与预期值逐位对上（用例名集合的 win/linux 差集实测恰为这两条，终审修复波复测再确认，
规矩 6 的选择子 `-R 'HotSwap|Eject|Adopt'` 亦双侧重跑全绿、差集恰为这两条），
说明 `gtest_discover_tests` 没有静默漏掉任何一条。
console 套件（`ctest -R VaseConsole` 实测 25 条）与 Embedding 回放**不含** `#ifndef NDEBUG` 门与平台门，
所以上面这两处差值不因它们而变。

**这一档有环境性假红**（2026-09-22 实测）：`win-x64-clang-release` 的首次删树重配在一个
`vcpkg z-applocal` 步上撞到 `The process cannot access the file ... being used by another process`
（目标 `FailingLoadPlugin.dll`），构建中止、连带 4 条 HotSwap / Abi 用例因缺 DLL 而红；同一条线
立即重跑即 94/94 全绿。是 Windows 文件锁层面的抖动，不是仓库缺陷——**但它是重跑才显形的那一类**，
所以删树全量的退出码非 0 时，先重跑那一条线再判。
M2a 收口波（2026-09-24）同一位置再撞过一次（目标 `VersionedA.dll`），单线删树重跑即 138/138 全绿；
终审修复波复测两轮全量均未再撞。

#### Windows（在 Git Bash 里直接跑）


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

#### Linux / WSL


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

#### 第三方依赖有两条入口，只有 gtest 走 vcpkg

- **要编译的依赖 → vcpkg manifest**（现在只有 `gtest`）。`vcpkg.json` 的 `dependencies` 就是它的清单。
- **只有头文件的依赖 → git submodule + `Cmake/VaseThirdParty.cmake`**。现在是 `ThirdParty/cli`
  （`MoozenSoft/cli`，BSL-1.0 的 fork，随仓库版本管理，不进 vcpkg）。在该文件里为它立一个
  `VaseThirdPartyCli` INTERFACE target，消费者 `target_link_libraries(... VaseBuildOptions VaseThirdPartyCli)`。

**为什么不 `add_subdirectory(ThirdParty/cli)`、也不给它做 vcpkg 端口**：实测两条路线都编得过，但
`add_subdirectory` 会把上游的构建决策变成我们的风险（`find_package(Threads REQUIRED)` 是硬
configure 依赖、Linux 上往每个消费者挂 `-lpthread`、`install()` 会把第三方头装进我们的分发树、
嵌套 `project()` 带进一层政策作用域），而上游那个文件恰好是它改动最勤的地方。vcpkg 路线还会把
include 降级成 `-isystem`——那正是下面这条禁令要防的。

**这个文件里两条规则都是承重的，改之前先读它的注释**：① include 不得标 `SYSTEM` / 不得
`/external:I`（否则"第三方头带回 `throw`"从编译期硬错误退化成运行期 terminate，且**没有任何东西
会报警**）；② 它替第三方声明 `cxx_std_17`——这类"库需要什么"的知识搬到了我们手里，漏掉的症状是一条
指向第三方头文件的 `no template named 'optional' in namespace 'std'`。

**submodule 的钉（pin）要跟着 fork 的提交走**：父仓库记的是 gitlink commit。若 fork 里的改动还没
提交推送、而父仓库已经依赖它，别人 clone 到的是没有那些改动的 commit——症状是响亮的（编译失败），
但只有你自己这台机器是绿的。加/更新 submodule 依赖时的顺序是：**fork 内提交并推送 → 父仓库
`git add ThirdParty/<name>` 推进 gitlink → 再提交父仓库**。

#### 工具链 flag 是承重的（§8.2 档三的构建要求）


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

## 三、怎么写

语言与提交约定、七条规矩、格式与命名的偏离项。改代码前读。

### 语言约定


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

  **留什么与留多少（2026-09-23 收紧）**：上表回答「该不该留」，下面三条回答「该留成什么样」——

  - **单条 ≤2 行为宜，硬上限 3 行**。写到第 4 行还在展开，说明手里拿的是论证：论证属于计划 / spec /
    commit message，代码里换成一句结论 + 指针（`// 三条 tidy 检查的出路推导见 M1 spec 3.3(1)`）。
  - **实测报错原文、修复前后对照、探针与试验记录不进代码注释**——它们是当时的评审材料，
    日后的读者只需要结论那一句。
  - **同一个理由只在一处写全（canonical），别处一句带过**（`// 与 XX 处同机制`）；NOLINT 的
    就地理由压成一句「为什么没有代码级出路」，不复述 fix-it 为什么错。

  **存量注释不专项回扫，改到哪个文件顺手收哪个**（2026-09-23 决定）。已知债的所在地：`Include/`
  约 28 处 ≥4 行长块（最长 18 行，`PluginDescriptor.h` 的宏体注释）、`PluginHost.cpp:204` 与 `:228`
  的同文件逐字重复、`Console.cpp` 的 mtime 修复流水账。

### `.claude` 目录存放约定


需要放进 `.claude` 目录的文件（配置、规则、skill、hook 等），**优先选用本项目的 `.claude/`**（`D:\Git\Vase\.claude\`），而不是用户空间的 `C:\Users\xingxing\.claude\`。

理由：项目级配置随仓库一起版本管理，团队成员与后续的 Claude 实例都能直接获得，不依赖某个人的本机环境。

例外：由 Claude Code 自身管理、路径固定的文件（例如会话的 memory 目录），位置不由本约定决定。

### 在这个仓库里干活要知道的规矩


以下七条：1 与 2 是**踩过的坑**（1 真实炸过一次构建，2 是实测出来的失败方式），
3 与 4 是**读这套 flag 时最容易想反的地方**，5 与 6 是**插件形态与测试主干的出口约定**，
7 管**文档自己的叠层**（1–6 讲代码）。改动相关代码前先读。

#### 1. 新 target 必须链接 `VaseBuildOptions`


`VaseBuildOptions`（根 `CMakeLists.txt`）是编译选项的唯一出口：警告级别与
`/WX` / `-Werror`、`/utf-8`、`/EHs-c-` / `-fno-exceptions`、`_HAS_EXCEPTIONS=0`、
cl.exe 线的 `/we4530`、`/wd4251`、`/Zc:preprocessor`（variadic 宏前提，M2a-T1 裁定 R1-2——
旧预处理器不转发 `__VA_ARGS__`）与 `/external:*` 全在里面。**新建 target 时忘了链它，
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

#### 2. 测试里不要用 `EXPECT_THROW` 一族


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

#### 3. Vase 自己的头文件一律用引号包含


写 `#include "Vase/Plugin.h"`，**不要写 `#include <Vase/Plugin.h>`**。

cl.exe 线的 `VaseBuildOptions` 带 `/external:anglebrackets`，它把**所有以尖括号包含的头**
标记为外部头，再由 `/external:W0` 豁免其警告。于是 Vase 自己的头一旦被尖括号包含，
它的警告就**静默**绕过了 `/W4 /WX`——门禁看起来还是绿的。

> `wiki/vase-architecture.md` 给**插件作者**的示例用的是尖括号，那是仓库外的写法，
> 与仓库内部不同。仓库内部一律引号。

> **同一条机制的反方向应用：`ThirdParty/cli` 的头必须用引号包含。** 规矩 3 要引号是"别放过
> 我们自己头的警告"，这里要引号是"**别放过第三方头的异常**"：cl.exe 线的 `/external:anglebrackets`
> 会把尖括号包含的整棵头树标为外部头，连 `/we4530` 升出的 C4530 一并静音——实测同一份含 `throw`
> 的上游头，尖括号包含 **0 条诊断**、引号包含 **5 条 `error C4530`**。这条看起来像笔误（第三方库
> 用尖括号才是直觉），而它坏了没有任何东西会报警，机制与被否的写法都记在
> `Cmake/VaseThirdParty.cmake` 与根 `CMakeLists.txt` 的注释里。

#### 4. `_HAS_EXCEPTIONS=0` 的语义代价


`VaseBuildOptions` 在 Windows 侧定义了 `_HAS_EXCEPTIONS=0`（该宏**不由 `/EH` 推导**，
MSVC STL 默认给它 1，必须显式置 0）。代价是 **MSVC STL 的前置条件失败从「抛异常」
变成「进程终止」**：`vector::at` 越界、`std::stoi` 解析失败、`<filesystem>` 的
error overload 等，全部直接 abort，没有可接住的东西。

**因此错误处理必须走 `Result<T>` / `Error`，不能指望 STL 的前置条件检查给出
可恢复的失败。** 更完整的说明（含 `/external:W0` 的边界：它对 `-I` 的尖括号第三方头
**压得住**普通警告与 C4530，压不住的只是 **MSVC STL 自己** try/catch 出的那条 C4530——
机制与实测见 `Cmake/VaseThirdParty.cmake`）在根 `CMakeLists.txt` 的注释里——
`/external:*` 那一段与 `_HAS_EXCEPTIONS` 那一段。（`Cmake/` 下的三个工具链文件与一个 triplet
只讲编译器定位、vcvars 与 STL 选型，**不涉及异常设置**，别去那里找。）

#### 5. 插件 target 一律经 `vase_add_plugin_fixture`


`Cmake/VasePluginHelpers.cmake` 的 `vase_add_plugin_fixture(name SOURCES … LINK_LIBRARIES …)`
是**插件形态的唯一出口**——示例与测试 fixture 都走它（`Samples/HelloPlugin/CMakeLists.txt` 亦然）。
它承载两件手搭 `add_library(SHARED)` 会漏掉的事：**只链 VasePod**（D11：「插件不依赖 Host」
是链接期事实而非约定）与**可见性收紧**（`CXX_VISIBILITY_PRESET hidden` + `VISIBILITY_INLINES_HIDDEN`）。
手搭的插件 target 视为违规——那也正是「某个 target 忘了链 `VaseBuildOptions`」（规矩 1）的复发点。

#### 6. `Tests/HotSwap` 按主干对待（它的判据力按平台不对称）


v3 §12.2：v2 里 `Tests/Reload` 是「不必每次提交都跑」的尾部测试；热插拔升为主干承诺后，
**任何改动 Loader、依赖账本、Eject / Adopt 路径、描述符布局或 `HeaderVersion` 的提交，
必须跑 `Tests/HotSwap/` 的全部用例，且 Windows 与 Linux 各自留证据**。
这几类改动正是「六线全绿」最靠不住的地方。

> **选择子是 `-R 'HotSwap|Eject|Adopt'`，不是 `-R HotSwap`。** `Tests/HotSwap/` 的用例名按套件
> 分三类：`HotSwap.*`（主循环）、`Eject.*`（拆除路径）、`Adopt.*`（领回路径）。**`-R HotSwap`
> 只选到第一类**，会把守 Eject / Adopt 的边界用例（点名消费者、kept-resident、failed-record、
> 同文件两 id……）整套漏掉——照规则做的人会拿到几条绿灯，然后从没跑过那一堆。
> `-R 'HotSwap|Eject|Adopt'` 是「该目录全部用例」的**超集**：它另捎上
> `Abi.AdoptRejectsBinaryThatImportsSiblingPlugin`（也走 Adopt 路径，跑上没有坏处），
> 以及 `Tools/VaseConsole` 的换件链路 `VaseConsoleHotSwap*` 一族（下方「第二实例」那条；含
> `…Reason` / `…ShowReason` / `…AdoptReason` 三条只钉文本的兄弟用例）与执法拒绝回放证人
> `VaseConsoleEjectBlocked*` / `VaseConsoleAdoptBlocked*` 两族（各 `…IsFailure` 钉退出码、
> `…Reason` 点名文本——用例名恰含 Eject/Adopt 子串，正是被本选择子拉进来的），并经其 fixture
> 拉进 `VaseConsoleSwapStage`。这里不写条数——写死的数会随用例增删漂移，规则要的是「该目录全部用例」。

**判据子串的收窄（M2a 收口，2026-09-24）**：这族用例匹配的消息子串里，`unknown plugin id` /
`already in pod` / `not in pod` 与身份类子串仍是契约；**`provided by [ … ]` 与
`unresolved declarations` 两族自 M2a 起改按 `EjectReport` / `AdoptReport` 字段断言**
（迁移完成于 T9/T10，通道边界见 M2a spec §5.3）——别再为这两族新增消息子串匹配。

**但别把「六线全绿」读成「六线等价」**（T12 复评的判定）：

- `Tests/HotSwap/HotSwapLoopTests.cpp` 里 `InstallPrime()` 的覆盖断言在 **Windows 上是真的
  sharing-violation 探针**——镜像还映射着时 `copy_file(overwrite_existing)` 失败，直接暴露
  「Eject 没真卸」（T12 实测：注释掉 `EjectPlugin` 那一行，该断言报
  `The process cannot access the file because it is being used by another process.`）；
- 在 **Linux 上它是空转的**——`copy_file(overwrite_existing)` 是 truncate-in-place（同一 inode），
  镜像还映射着也会覆盖成功、随后读到新字节，同一条断言照样通过。
- **第二实例：同一个动作交给用户之后**（`Tools/VaseConsole` 的 `VaseConsoleHotSwap` 走 `file install`）
  判据力同样按平台不对称——Windows 上靠"写不开"证明「Eject 没真卸」，Linux 上覆盖是空转、
  它绿的原因是 `adopt` 的档三身份比对（`file install` 的输出本身就把这条声明打出来，见 spec §4）。

因此 Windows 比 Linux 多一层文件锁证据；两平台同跑得到的不是同一件事的两份拷贝。

#### 7. 同一件事只准有一处字面真值（本文件与技能的叠层口径）


本文件、`.claude/skills/vase-cpp-engineering/`、以及源文件自己的头部注释，是同一个约束的**三种载体**。判据不是字数重不重，是**仓库变一次，这个副本要不要跟着变**：

| 类别 | 例 | 处置 |
|---|---|---|
| **阈值与基数** | `ctest -N` 的各线 `Total Tests`、tidy 的 TU 数与抑制合计 | **只住本文件**，技能里出现即违规。除了腐烂，实测还有一个更实的害处：把数字直接递到手上的那份回答，只复述了「差值是设计不是漏注册」，没读技能的那份反而给出了成因 |
| **可整段引用的块** | 六个 preset 的命令块、WSL 登录 shell 那三条、`git ls-files` 那串 | **只住本文件**，技能写指针。整段抄过去换不到任何可读性 |
| **嵌在论述里的单个字面值** | 一句论证里提到 `/DEBUG:FULL`、卸载选择子的字符串、`WarningsAsErrors` 为空 | **可以留在技能里**，但该处必须带「字面值以本文件 X 节为准」的指针——危险的不是副本，是没有链路的副本：带指针的能 grep 到、跟着一起改，没指针的是孤儿 |
| **判据与理由** | 为什么关了异常就不能写 `throw`、为什么摘掉承重 flag 的症状是运行期而非编译期 | **允许两边各写一份，不需要指针**。它随代码一起变，而真要变时是一次实质复审，不是同步动作 |

自检（技能目录里扫字面真值，命中就逐条判它属上表哪一类）：

```bash
grep -rn "Total Tests\|[0-9][0-9] TU\|DEBUG:FULL\|build-id=sha1\|EHs-c-\|HAS_EXCEPTIONS\|--cached --others\|WarningsAsErrors\|PRE_TEST\|ctest --preset\|run-clang-tidy -p\|cmake --build --preset" \
  .claude/skills/vase-cpp-engineering/ | grep -v cpp-core-guidelines.md
```

**规矩 1–6 讲代码，规矩 7 讲文档。** 它自己同样适用：本文件与技能冲突时以本文件为准，并回头改技能。

### 格式化与静态检查


配置：`.clang-format`、`.clang-tidy`。

**`.clang-format`** —— 基线为 LLVM style、标准 C++20，但有多处**显著偏离**（逐项以 `.clang-format` 文件为准），不要按「LLVM 风格」的直觉去写代码。最容易写错的三项：`Allman`、`PointerAlignment: Left`，以及构造初始化表的 `PackConstructorInitializers: Never` + `BreakConstructorInitializers: BeforeComma`——**每个初始化式各占一行、逗号在行首**，哪怕整表一行放得下。同理 `BreakTemplateDeclarations: Yes`：`template <...>` 头与 `class` / 函数签名永远分两行（不是超列宽才拆）。另两项 `BreakAfterAttributes: Leave` 与 `ConstructorInitializerIndentWidth: 4` 只是沿用默认的锚定。

**一个例外：宏参数内部的花括号对不受 `BreakBeforeBraces` 管辖。** `Tests/Smoke/CrossDllSmoke.cpp`
的两个 `TEST(...)` 体在本配置下**只能是单行**（实测：把它们折成 Allman，`clang-format --dry-run --Werror`
报 3 处 violation 并 exit 1；`clang-format -i` 会把它们折回单行）——那是格式门要求的输出，不要「修」它。
同一文件里 `namespace` 的花括号仍是 Allman，两者不矛盾。

#### 命名规范


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

## 四、怎么验

tidy 与 format 怎么跑、退出码为什么单独不够、基数怎么读。宣布完成前读。

### 静态检查与格式


```bash
run-clang-tidy -p build-win/win-x64-clang-debug          # clang-cl 线（77 个 TU）
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
不必再手工 grep 日志。基数见「核这些门禁时，退出码单独用是不够的」那节的实测表，脚本不复制阈值。
实测两侧输出与该表逐位对上：Linux 78 TU / 208584 / NOLINT 2751，Windows 两线各 77 TU /
501215 / NOLINT 2701。

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
  Running clang-tidy in 12 threads for 77 files out of 77 in compilation database ...
  5014 warnings generated.
  Suppressed 5076 warnings (5014 in non-user code, 62 NOLINT).
  ```

  所以「无新 warning」的判据是三条一起：**退出 0 + 正文 `error:` 0 条 +
  正文 `warning:` 0 条**，再连摘要行一起读。只 grep `warning:` 会漏掉全部被抑制的量。
  实测基数（终审修复波复测，2026-09-24；两处代码级出路后的三线全绿记录，未新增任何 NOLINT；
  全部落在第三方头、gtest 模板与被 NOLINT 豁免的自有位形/宏机制代码里，
  我们自己的代码零正文 warning）：

  | 线 | 文件数 | 摘要行 Suppressed 合计 | 单 TU 最小 / 最大 |
  |---|---|---|---|
  | clang-cl（`win-x64-clang-debug`） | 77 | 501215 | 998 / 45609 |
  | cl.exe（`win-x64-msvc-debug`） | 77 | 501215 | 998 / 45609 |
  | Linux（`linux-x64-clang-debug`） | 78 | 208584 | 359 / 11337 |

  **Linux 比 Windows 多 1 个 TU**，就是 `Tests/HotSwap/fixtures/NoBuildIdPlugin`——它在
  `if(NOT WIN32)` 里（与按线基数那 +2 条同源）。两侧的 `*Windows.cpp` / `*Posix.cpp` 是成对的，
  不贡献差。文件数比编译数据库的条目数少 1：`fixtures/LoadProbe/LoadProbe.cpp` 同时编进
  `LoadProbe` 与 `UnloadProbe` 两个 fixture，`run-clang-tidy` 按路径去重（两条线都一样）。

  数字变了不一定是错，但**要看它变在哪一类**——摘要行里还会出现 `N NOLINT`
  （本轮的抑制命中次数：Windows 两线 2701、Linux 2751；**这不是仓库里的抑制处数**，
  同一个抑制会被每个包含它的 TU 各计一次——M2a 的 `Value.h` 与 `ConfigMacros.h` 两处
  NOLINTBEGIN 区因此被几十个 TU 各计一遍，数目看着大是这一乘法的形状），以及 gtest 模板实例化带来的巨量非用户代码告警。
  三线正文里的 `ThirdParty/` 路径 diagnostic 实测 **0 条**（`ExcludeHeaderFilterRegex: '.*ThirdParty.*'` 如期生效）。

- **`ctest` 在一个测试都没发现时同样返回 0。** 所以「测试全绿」不能只跑
  `ctest --preset <p>`，必须另跑一次 `ctest --preset <p> -N`，把 `Total Tests`
  与「构建与测试」一节那张**按线分账的基数表**逐位对上（终审修复波复测六线全部对上，见该表）。`gtest_discover_tests` 用的是 `DISCOVERY_MODE PRE_TEST`，
  枚举发生在 ctest 运行时——测试被漏注册时，`ctest` 会一声不吭地报成功。
