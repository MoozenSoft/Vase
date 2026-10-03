# Vase 插件产物布局：一插件一目录（spec）

**日期**：2026-10-03 · **状态**：brainstorming 定稿 → **grilling 三轮收敛（Q1–Q9 全部裁可，两处事实更正由子代理掀出并已并入）** · **实施计划**：spec 复审通过后由 writing-plans 另立 · **分支**：`plugin-artifact-layout`（自 main `5fd5586` 起）

**上游权威**：`wiki/vase-architecture.md` v3 的 §10（目录布局）、§11.1（VaseCli 与构建后置步骤）、§11.2（宿主重编译单插件流程）、§3.3（清单字段与 `Binary` 缺省回退）、§8.2（档二/档三与身份特征）；`CLAUDE.md` 的「构建与测试」产物落位句、规矩段那条 pin 与「工具链 flag 是承重的」段；M2b spec 的 D54（`Binary` 缺省 = 子目录名）与 D57（清单逐字手写义务）；M5 spec 的 D122/D134（`scan` 回写语义）、D125（CMake 后置步骤记账）与 D128（validate 对缺清单只 warn、不计退出码）；M6 spec 的 `doctor` ③④（目录写探针与残留锁探针）；macOS 腿 spec 的取证注⑫（`install_name` = `@rpath/libVasePod.dylib`）。
历史决定 D1–D179 不改史，本文决定从 **D180** 续号。

**本文的地位**：本波改的是**产物布局**这一既有承重约束（`CLAUDE.md` 那句「可执行与 DLL 同处 `bin/`」要改准），并据此把 D125 结案。**零公开面增量**：`kHeaderVersion` 不动、导出符号与描述符布局不动、`Result<T>` / `Error` 不动。

凡本文与磁盘代码冲突处以磁盘为准（仓库惯例）；本文与架构文档冲突处以本文为准并回头挂勘误（§9 清单）。

> **[事实取证注 · 实测（2026-10-03，Windows 11 + WSL Ubuntu）]** 本波的形状是先跑探针、再派子代理清点全仓假设问出来的。探针材料（构建树里的临时目录与脚本）跑完即删，故下面的输出原文是这些结论的唯一记录。
>
> ① **Windows：插件住子目录不影响装载**。把 `HelloPlugin.dll` 与手写清单放进 `build-win/<preset>/bin/ProbeRoot/HelloPlugin/`，跑 `VaseConsole --script`（`pod new ProbeRoot` / `pod list` / `pod destroy`）：
> ```
> vase> pod created index=0 generation=1
>   plugin: Vase.Hello
> vase> pods: 1
>   [0] gen=1 plan=ProbeRoot plugins=1  <- active
> vase> destroy index=0
> clean=true handleWasStale=false
> counters diff (baseline = pod creation): effects=0 services=0 subscriptions=0 pluginInstances=0 scopes=0
> RC=0
> ```
> **机理（grilling 更正，与初稿相反）**：不是「按应用目录搜索」。两条合起来才是真因——(a) `Source/Host/LoaderWindows.cpp:34` 用 `LoadLibraryExW(path, nullptr, **LOAD_WITH_ALTERED_SEARCH_PATH**)`，该 flag 把依赖搜索基改成**被加载模块自己的目录**（即 `bin/HelloPlugin/`）而非应用目录；(b) 解析插件的 `NEEDED: VasePod.dll` 时，Windows 按**已加载模块名**先行命中——而 `VasePod.dll` 必然已在场，因为**执行这次加载的代码本身就住在 `VaseHost.dll` 里**（`Source/Host/Loader*.cpp` 编进 VaseHost，VaseHost PUBLIC 链 VasePod）。**根本没走文件系统搜索。** 这条是**自证的**：不链 `VaseHost` 就调不到加载器（D195）。
>
> ② **产物根按平台分岔**。根 `CMakeLists.txt:47-49`：`RUNTIME→bin/`、`LIBRARY→lib/`、`ARCHIVE→lib/`。Windows 的 DLL 属 RUNTIME 类 → `bin/`；Linux/macOS 的共享库属 LIBRARY 类 → `lib/`。实测：`build-linux/<preset>/bin/` 只有 4 个可执行，`build-linux/<preset>/lib/` 有 43 个 `.so`（3 框架库 + 40 插件）；`build-win/<preset>/bin/` 有 45 个 `.dll`（3 框架库 + 2 个 vcpkg gtest + 40 插件）。
>
> ③ **Linux：插件挪层后依赖仍解析**。`libHelloPlugin.so` 实测 `NEEDED` 含 `libVasePod.so`，`RUNPATH` = `/mnt/d/Git/Vase/build-linux/linux-x64-clang-debug/lib`（configure 期烘死的绝对路径，本仓库既有形态，非新风险；由 CMake 按所链目标的目录算出）；插件挪进 `lib/<名>/` 后 RUNPATH 仍指向 `lib/`，解析不变。
>
> ④ **macOS：插件挪层后依赖仍解析（2026-10-03 实测，macOS 14.8.9 / x86_64）**。dylib 同属 LIBRARY 类、`install_name` 是 `@rpath/libVasePod.dylib`（macOS 腿 spec 取证注⑫）；与 Linux 同机制但不可推，按 D192 在 macOS 线上实测，结论与 Linux 同形——`LC_RPATH` 指向框架库所在的 `.../lib`（configure 期烘死的绝对路径，本仓库既有形态、非新风险），`@rpath/libVasePod.dylib` 经它解析到 `lib/libVasePod.dylib`，插件挪进 `lib/<名>/` 后解析不变。首步探针（D193）在 Windows 侧已跑过同形；macOS 侧本注只记 LC_RPATH 与整条线：`ctest` 全绿、`Total Tests` = 339（与 Windows 侧同值，本波无平台门）。D192 的「失败即停下重裁」未触发。
> ```
> $ otool -l build-macos/macos-x64-clang-debug/lib/HelloPlugin/libHelloPlugin.dylib | grep -A2 LC_RPATH
>           cmd LC_RPATH
>       cmdsize 80
>          path /Users/moozen/WindowsGit/Vase/build-macos/macos-x64-clang-debug/lib (offset 12)
> $ otool -L build-macos/macos-x64-clang-debug/lib/HelloPlugin/libHelloPlugin.dylib
> lib/HelloPlugin/libHelloPlugin.dylib:
> 	@rpath/libHelloPlugin.dylib (compatibility version 0.0.0, current version 0.0.0)
> 	@rpath/libVasePod.dylib (compatibility version 0.0.0, current version 0.0.0)
> 	/usr/lib/libc++.1.dylib (compatibility version 1.0.0, current version 1800.105.0)
> 	/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1351.0.0)
> ```
>
> ⑤ **PDB 跟随目标自己的输出目录组**（实测）。`build-win/<preset>/Tests/HotSwap/fixtures/stageA/` 里是 `VersionedA.dll` + `VersionedA.lib` + `VersionedA.pdb` **三件同处**；全仓**未设** `PDB_OUTPUT_DIRECTORY`。故新布局下 PDB 会跟着进 `bin/<名>/`。档三身份特征（CodeView RSDS）嵌在 DLL 内，与 `.pdb` 落点无关。
>
> ⑥ **现状盘点**：全仓 **44 个插件目标**全部经 `vase_add_plugin_fixture`（无一手写 `add_library(SHARED)` 绕过；三个手写 SHARED 的是框架库 `VasePod`/`VaseHost`/`VaseCatalog`）。其中 **40 个落产物根**，**4 个落各自 stage 目录**（`VersionedA`/`VersionedAPrime`/`VersionedAStampDrift`/`VersionedAServiceDrift`，见 D194）。44 个目标里只有 **6 份手写清单**，其中 3 份（`hello`/`dependent`/`failing`）描述 `Samples/` 的三个插件，另 3 份描述 `Tests/Integration/fixtures/` 下的 fixture。**其余 37 个故意没有清单**（负例材料），`PluginCatalog::Refresh` 对无清单子目录的既定行为是 warning `no plugin.json — skipped`（`PluginCatalog.cpp:189`），负例性质依赖它。
>
> ⑦ **零自动回归覆盖**（实测清点）：全仓**没有任何测试枚举构建产物根**——所有目录枚举都发生在临时沙箱（`CatalogSandbox` / `sandbox.Root`）。所以新布局本身没有既有测试看得见它，D189 的两条证人是它唯一的自动化证据。
>
> ⑧ **运行期定位全部走绝对路径 + 目录树**，没有一处拼到「可执行文件所在目录」：`BinaryPath` 的唯一算式是三级拼接 `SnapshotDirectory / Subdirectory / LibraryFileName(Binary)`，出现在 `Solve.cpp:650,707`、`PluginCatalog.cpp:314`、`Doctor.cpp:57`、`Validate.cpp:129-131`；`EnsureResident` 先 `absolute`（`Loader.cpp:20-41`）再 `PlatformLoad`。**故宿主侧代码零改动。**

> **[裁定注 · brainstorming（2026-10-03）]** 六问六裁：
> **Q1 · 范围（D180）**：**全仓一律**——40 个落产物根的插件目标全走新布局，不做「只有 Samples 特殊」。
> **Q2 · 清单范围（D181）**：**按需**——只有 `Samples/` 的三个（及原本手写的那三份 fixture 清单）带 `plugin.json`；其余保持无清单。
> **Q3 · 真值源（D182）**：**搬家**——`fixtures/manifests/{hello,dependent,failing}` 迁到对应 `Samples/<X>/plugin.json`，不留副本。
> **Q4 · 目录名（D183）**：**二进制名**（target 名）——与 D54「`Binary` 缺省 = 子目录名」天然对齐；D194 把措辞严格钉成 **target 名**。
> **Q5 · `HelloPluginPrime`（D184）**：**只放库，不放清单**——它与 `HelloPlugin` 同 Id（`Vase.Hello`），放清单会让 `Refresh` 因重复 Id 直接 Err（`PluginCatalog.cpp:198-206`，整棵树刷新失败而非告警）。
> **Q6 · 产物根（D185）**：**跟随平台惯例**——Windows 插件在 `bin/<名>/`，Linux/macOS 在 `lib/<名>/`。

> **[裁定注 · grilling（2026-10-03）]** 九问九裁，全部落点：
> **D188 · 产物根的定位（Q1）**：接受 37 条 `no plugin.json — skipped`，把产物根定名「**一棵不 clean 但可用的插件树**」，写进 spec；不补清单、不加过滤。
> **D189 · 本波的两条证人（Q2/Q9）**：① ctest 级 `VaseCli validate <产物根>`（期望 rc=0，默认退出码判据即足够——D128 明写缺清单只 warn、不计退出码）；② gtest `Tests/Unit/ArtifactLayoutTests.cpp` 断言代表性目标的 `$<TARGET_FILE_DIR:...>` 符合 `<根>/<target 名>`。**§8 的「基数零增减」按此改准。**
> **D190 · console 回放树不动（Q3）**：`replay/<case>/<Plugin>/` 保留副本、一个字不改——影子纪律（写盘家族独占一棵根）＋ ctest 并行 ＋ 产物根不该被用例写脏。
> **D191 · 规矩 6 增第六类触发（Q4）**：「产物布局」进 `CLAUDE.md` 规矩 6 的触发清单，与 Loader / 依赖账本 / Eject·Adopt 路径 / 描述符布局 / `HeaderVersion` 并列。
> **D192 · macOS 失败的处置（Q5）**：**明写「失败即停下重裁」**，不预置 `-Wl,-rpath,@loader_path/..` 一类补丁——预置补丁在机制本已成立时也会生效，等于盖住真因。
> **D193 · 落地节奏（Q6）**：**一步到位**（一个 spec / 一个 plan），但 plan 的**第一个验证点**是「按新形态复跑取证注①的探针」——先在一棵线上跑通，再铺八线。
> **D194 · `Versioned` 四兄弟维持现状（Q7）**：四个 target 同为 `OUTPUT_NAME = VersionedA`，靠各自显式设三项输出目录到 `Tests/HotSwap/fixtures/stage{A,APrime,StampDrift,ServiceDrift}` 隔离（Ninja 生成期 `multiple rules generate .../VersionedA.lib` 的既有处置，`Tests/HotSwap/fixtures/CMakeLists.txt:25-55`）。它们**不进产物根**；D183 的目录名因此可严格钉成 **target 名**。副作用记名：函数内新设的输出目录会被那四段**在调用之后**的显式设置覆盖——今天就是这个顺序，别调换。
> **D195 · 「加载器与框架库同进程」升为承重条款（Q8）**：写进取证注① + `CLAUDE.md` 的「工具链 flag 是承重的」段（与 `/DEBUG:FULL`、`--build-id=sha1` 并列），**不加机器守卫**——前提自证（不链 `VaseHost` 就调不到加载器），加守卫等于给恒真式写断言。进承重段的理由是**它会被误改**：摘掉 `LOAD_WITH_ALTERED_SEARCH_PATH` 或把 `Loader` 挪出 `VaseHost`，症状都是插件加载全线失败，而这两个动作看起来都无害。

---

## 1. 目标形态

规则一句话：**每个插件独占一个以 target 名命名的子目录，里面是它的库文件（与平台伴生物）和（若有）`plugin.json`；宿主可执行与框架库仍在各自平台的产物根下平铺。**

```text
build-win/<preset>/
  bin/                                     ← Windows 的 RUNTIME 类
    VaseTests.exe  VaseConsole.exe  VaseEmbedding.exe
    VasePod.dll  VaseHost.dll  VaseCatalog.dll        ← 框架库，平铺
    gtest.dll  gtest_main.dll                          ← vcpkg，平铺
    HelloPlugin/       HelloPlugin.dll       plugin.json
    HelloPluginPrime/  HelloPluginPrime.dll             ← 换件材料，无清单（D184）
    DependentPlugin/   DependentPlugin.dll   plugin.json
    FailingPlugin/     FailingPlugin.dll     plugin.json
    LoadProbe/  LoadProbe.dll
    …（其余 36 个插件目标，无清单）
build-linux/<preset>/
  bin/    VaseTests  VaseConsole  VaseEmbedding             ← 可执行，平铺
  lib/    libVasePod.so  libVaseHost.so  libVaseCatalog.so  libVaseCliCore.a   ← 框架与工具库，平铺
          HelloPlugin/  libHelloPlugin.so  plugin.json
          …（同 Windows 的分目录规则）
build-macos/<preset>/   ← 同 Linux 的 bin/lib 分工（lib/<名>/libX.dylib）
```

全仓 44 个插件目标 = 产物根下 40 个 + `Versioned` 四兄弟（各落 `Tests/HotSwap/fixtures/stage*`，D194）。

三条明确**不在**本波射程内：

- 框架库（`VasePod` / `VaseHost` / `VaseCatalog`）**不**成目录——它们不是插件，是宿主侧；
- `libVaseCliCore.a`（VaseCli 的内部静态库，ARCHIVE 类）与 vcpkg 的 gtest **不**成目录；
- 可执行**不**成目录。

**产物根的定位（D188）**：它是一棵**不 clean 但可用**的插件树——指着它能起局（取证注① 就是这个形态），但会拿到 37 条 `no plugin.json — skipped` warning。那 37 条是「无清单插件被跳过」这条既有语义的证据面，不消。

## 2. 清单：手写、随插件住

- **新增**（自 `Tests/Integration/fixtures/manifests/` 迁入，内容逐字不动）：`Samples/HelloPlugin/plugin.json`、`Samples/DependentPlugin/plugin.json`、`Samples/FailingPlugin/plugin.json`；
- **删除**源位置的三份（D182：搬家，不留副本）；
- **原地不动**：`fixtures/manifests/{shared_provider,shared_consumer2,edge_consumer}/plugin.json`——它们描述的是 `Tests/Integration/fixtures/` 下的 fixture 插件，不是 Samples；
- **`HelloPluginPrime` 不带清单**（D184）。它的换件清单仍是 CMake 生成的 `replay/manifests/hello_prime.json`，那份材料是「与二进制成对覆盖」（D87）用的，不是给目录树 `Refresh` 用的——**console 那条链一个字不改**；
- **其余 37 个插件目标**（44 减去上面三个带清单的，**含** `HelloPluginPrime`）**不带清单**（D181）。

消费者改指（grilling 期实测清点，共四处，比初稿多三处）：

| 位置 | 现状 | 改为 |
|---|---|---|
| `Tests/CMakeLists.txt:111` `VASE_FIXTURE_MANIFESTS` | 指向 `Tests/Integration/fixtures/manifests` | 保持（剩三份仍住那里）；另加指向 `Samples/<X>/plugin.json` 的宏 |
| `Tests/Integration/AssemblyFromSolveTests.cpp:58,75,91,95` | `manifests / item.Dir / "plugin.json"` 单一宏拼路径 | roster 改成「清单路径按项给」 |
| `Tests/Integration/VaseCliPlanTests.cpp:86,98`、`VaseCliValidateTests.cpp:160,177`、`HotSwap/AdoptManifestTests.cpp:51` | 同上的单宏拼路径 | 同上 |
| `Tools/VaseConsole/CMakeLists.txt:21` `VASE_CONSOLE_HELLO_MANIFEST` | `Tests/Integration/fixtures/manifests/hello/plugin.json` | `Samples/HelloPlugin/plugin.json` |

## 3. 实现：一处出口

插件形态的唯一出口是 `vase_add_plugin_fixture`（CLAUDE.md 规矩 5，44 个目标全走它、无例外），所以实现也只有一处：

1. **输出目录**：在函数内给 target 设
   `RUNTIME_OUTPUT_DIRECTORY = ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name}`、
   `LIBRARY_OUTPUT_DIRECTORY = ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${name}`
   （各自平台那一类生效，零散改无）。
2. **清单拷贝**：新增可选参数 `MANIFEST <path>`，给了就挂一条 POST_BUILD：
   `${CMAKE_COMMAND} -E copy_if_different <path> "$<TARGET_FILE_DIR:${name}>/plugin.json"`
   （`$<TARGET_FILE_DIR:...>` 自动跟随第 1 条，不需要第二处知道路径规则）。

**不动的**：`ARCHIVE_OUTPUT_DIRECTORY`（Windows 的导入库 `.lib` 与 Linux 的 `.a` 留在 `lib/`）；框架库与可执行的 target 属性。

四处要复核的副作用：

- **`Versioned` 四兄弟**：函数内设的输出目录会被 `Tests/HotSwap/fixtures/CMakeLists.txt:30-55` 在调用**之后**的显式设置覆盖（D194）——今天就是这个顺序，别调换；
- **PDB**：跟随目标输出目录组进 `bin/<名>/`（取证注⑤ 实测）。`NoIdentityPlugin` 的 per-target `/DEBUG:NONE`、`ProbeSwapGuard` 的中转名（`.orig` / `.old` 与 Target 同目录）都要按新路径复核；
- **同源双 target**：`LoadProbe` 与 `UnloadProbe` 由同一个 `LoadProbe.cpp` 编出，各得一个目录，互不干扰；
- **测试内部复制**：`CatalogSandbox` / `Sandbox.CopyFile` 一族走 `$<TARGET_FILE:X>` 的完整路径与 `filename()`，布局搬动对它们透明；`$<TARGET_FILE_NAME:X>` 同（用于 console 回放 staging）。

## 4. 既有约束改准

`CLAUDE.md` 现钉着：「构建产物落 `build-win/<presetName>/`，可执行与 DLL 同处 `bin/`——这是 Windows 能找到 DLL 的前提，不要改 `CMAKE_RUNTIME_OUTPUT_DIRECTORY`。」本波把这条**改写**为：

> 构建产物落 `build-<平台>/<presetName>/`：**可执行在 `bin/`**（POSIX 侧框架库与静态库在 `lib/`），**每个插件独占 `<产物根>/<target 名>/`**（Windows `bin/`、Linux/macOS `lib/`）。依赖解析的依据：**Windows 靠「加载器与被依赖框架库同进程」**——`VaseHost.dll` 里跑的 `LoadLibraryExW` 带 `LOAD_WITH_ALTERED_SEARCH_PATH`，而插件需要的 `VasePod.dll` 早已被宿主静态导入、按已加载模块名命中（自证；取证注①）；**POSIX 靠链接期烘入的 `RUNPATH` / `LC_RPATH`**（指向框架库所在的 `lib/`；Linux 取证注③、macOS 取证注④ 实测同形）。改回平铺、改动 `vase_add_plugin_fixture` 的输出目录设置、或动 `LoaderWindows.cpp` 的 `LOAD_WITH_ALTERED_SEARCH_PATH`，必须按规矩 6 重跑三平台 `-R 'HotSwap|Eject|Adopt'` 族并各留证据。

这不是放宽——是**换了承重理由**：从「必须同处一目录」换成「按平台的依赖解析契约」。上文那条 Windows 理由同时**升为承重条款**（D195），进 `CLAUDE.md` 的「工具链 flag 是承重的」段。

**规矩 6 增第六类触发（D191）**：「产物布局」与 Loader / 依赖账本 / Eject·Adopt 路径 / 描述符布局 / `HeaderVersion` 并列。本波就是活证——一次布局改动同时影响所有 Loader 调用方与三平台依赖解析，单看某一条线的绿推不出别的线。

## 5. D125 结案

新布局让 `bin/`（Windows）/ `lib/`（POSIX）第一次成为**宿主可以指着它起局的插件目录树**——取证注①就是拿这个形态跑的。

但清单**一律来自手写源**：构建期只**拷贝**，从不**重生成**。于是 M5 spec 里 D125 那句「接线的前置是先把『哪些目录允许自动重生成』划清」有了确定答案：**一个都没有**。

结论：§11.1 的「构建后置步骤」以**拷贝**形态兑现（§3 的 `MANIFEST` POST_BUILD 步），`scan` 仍只是人的工具、**不接**后置步骤。D125 一笔自此**结案**——不是划入「明确不做」，是**以另一形态兑现**；`scan` 的破坏性回写（D122/D134）保持原样，不新增护栏。

## 6. 决策表

| 编号 | 决定 | 理由 / 依据 |
|---|---|---|
| D180 | 布局变更**全仓一律**，不做 Samples 特例 | `bin/` 里一半分目录一半平铺要让人记两套规则；且 Samples 的三个目标本来就在给测试当 fixture |
| D181 | 清单**按需**：只给 Samples 三个（与原本手写的） | 37 个插件无清单是负例材料；补上正确清单会消掉 `no plugin.json — skipped` 与「清单↔二进制不一致」那族证人的前提 |
| D182 | 三份清单**搬家**到 `Samples/<X>/`，不留副本 | 清单与它描述的插件同址是本波立意；两处副本的漂移没有任何门禁拦得住 |
| D183 | 目录名 = **二进制名**（target 名，D194 严格化） | 与 D54「`Binary` 缺省 = 子目录名」天然对齐；且不会有两个 target 争同一路径 |
| D184 | `HelloPluginPrime` **只放库、不放清单** | 与 `HelloPlugin` 同 Id，清单同树即 `Refresh` 整棵树 Err（`PluginCatalog.cpp:198-206`） |
| D185 | 产物根**跟随平台惯例**（Windows `bin/`、POSIX `lib/`） | CMake 既有的 RUNTIME/LIBRARY 分工即 Unix 惯例；统一进 `bin/` 会空掉 `lib/` 并制造第二套「库在哪」的答案 |
| D186 | 后置步骤以**拷贝**形态兑现，`scan` 不接后置步骤 | §5；D125 结案 |
| D187 | 承重约束由「必须同处一目录」改准为「按平台的依赖解析契约」 | §4；理由换了但没放宽 |
| D188 | 产物根定名「**不 clean 但可用的插件树**」，接受 37 条 warning | 那 37 条是「无清单插件被跳过」既有语义的证据面 |
| D189 | 本波加两条证人：ctest `VaseCli validate <产物根>` + gtest `ArtifactLayoutTests` | 新布局零既有回归覆盖（取证注⑦）；两条腿各管「清单↔二进制一致」与「布局规则本身」 |
| D190 | console 回放树**保留副本**，不改建到产物根 | 影子纪律 + ctest 并行 + 产物根不该被用例写脏 |
| D191 | **规矩 6 增第六类触发**：产物布局 | 一次布局改动同时影响所有 Loader 调用方与三平台依赖解析 |
| D192 | macOS 实测**失败即停下重裁**，不预置 rpath 补丁 | 预置补丁在机制已成立时也生效，会盖住真因 |
| D193 | **一步到位**；plan 首验证点 = 复跑取证注① 的探针形态 | ①单独落地没有消费者，「清单住 Samples」的收益也只有在②之后才成立 |
| D194 | `Versioned` 四兄弟**维持各自 stage 目录**，不进产物根 | 四个同 `OUTPUT_NAME`，那四段显式设置是「绕开 Ninja 同名冲突」的既有处置；D183 因此可严格钉成 target 名 |
| D195 | 「加载器与框架库同进程」**升为承重条款**（进 CLAUDE.md 承重段），不加机器守卫 | 前提自证；但摘 `LOAD_WITH_ALTERED_SEARCH_PATH` 或把 `Loader` 挪出 `VaseHost` 都看起来无害而症状是全线加载失败 |

## 7. 明确不做

- 不改框架库、可执行、`.a` / `.lib` 的落位；
- 不给 37 个无清单插件补 `plugin.json`；
- 不把 POSIX 的 LIBRARY 输出改指 `bin/`；
- 不把 console 回放树改建到产物根；
- 不接 `scan` 后置步骤、不给 `scan` 加「拒绝写源码树」护栏（后者若要立，另立一笔）；
- 不改 `Tests/Integration/fixtures/manifests/` 剩余三份的位置；
- 不改 `Versioned` 四兄弟的 stage 目录；
- 不为「依赖解析」加机器守卫（D195）；
- 不动 `kHeaderVersion`、导出符号、描述符布局。

## 8. 验证要求

- **plan 首步（D193）**：按新形态**复跑取证注①的探针**——先在一棵线上跑通，再铺八线；
- **plan 首步（D192）**：macOS 线上实测 `lib/<名>/libX.dylib` 的依赖解析；**失败即停下重裁**；
- 八线删树重配全量：configure → build → `ctest` → `ctest -N`，八线全绿、零警告、零重跑；
- **基数预期**（D189 改准）：**gtest +3**（`Tests/Unit/ArtifactLayoutTests.cpp`，无 `#ifndef NDEBUG` 门、无平台门；计划把断言拆成三条用例——布局主断言 / `Versioned` 例外 / 清单就位，理由是每条要有自己的失败信号，与仓库既有的 `Eject.SemanticDependency*` 族同款拆法）+ **ctest 级 +1**（`VaseCli validate <产物根>`，根路径取 `if(WIN32) ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} else ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}`）。除此之外八线 `Total Tests` 与 `5fd5586` 现值逐位相同；
  > **勘误（落地期收口）**：本条原写 **gtest +1**（「一条用例内断言多个目标」）。实际落成 **+3**，即 **净 +4/线**（收口波八线实测 341/340）。此项按计划改准为 +3，理由见上；登记见 plan 偏离登记第 1 条。
- tidy：**TU +1**（四线同幅）、四线三判据全过；
- format 双判；
- 三平台各跑 `-R 'HotSwap|Eject|Adopt'` 族并留证据（D191 规矩 6 新触发类）；
- 交付前逐条复核：`bin/`(win) 与 `lib/`(posix) 下 40 个插件各自成目录、三个带清单的目录内容与源一致、无插件库平铺在根下、`Refresh` 能对整棵产物根成功建树。

## 9. 文书义务

- `CLAUDE.md`：产物落位句（§4 改写）、规矩段那条 pin、「工具链 flag 是承重的」段补 `LOAD_WITH_ALTERED_SEARCH_PATH` 条款（D195）、规矩 6 增第六类（D191）、项目状态一笔、M5 记名欠账清单（D125 结案，余 **VasePack** 一件）；
- 根 `CMakeLists.txt:45-46` 的注释——「可执行与动态库放同一目录 …同处 bin/ 才能直接跑起来」就是被改写的承重句本体；
- `README.md:115-116`（「可执行与 DLL 同处 bin/」）与 `:312-321`（VaseEmbedding 段落里「各 DLL 同处 bin/」）；
- `wiki/vase-architecture.md`：§10 目录布局（Samples 子树补 `plugin.json`、说明 bin/lib 的插件子目录）、§11.1 现状注（D125 结案）、§11.2 第 3 步（「CMake 后置步骤自动跑」改为「构建期拷贝」）；
- `wiki/vase-console-use.md:232` 的示例 `file stage Vase.Hello ../../bin/HelloPluginPrime.dll`——新布局下这条相对路径会断，改成 `../../bin/HelloPluginPrime/HelloPluginPrime.dll`；
- `Samples/Embedding/CMakeLists.txt:22` 注释「DLL 由可执行文件同目录（bin/）找到」（措辞会变误导）；
- `.claude/skills/vase-cpp-engineering/references/portability.md:75`（同一条 pin）。

本文与磁盘冲突处以磁盘为准；本文定稿后若要偏离，按仓库惯例在 plan 的偏离登记里记名。
