# Vase M6：`VaseCli plan` / `doctor`（加载计划预览与环境诊断）（spec）

**日期**：2026-09-29 · **状态**：brainstorming 定稿（需求方四问四裁 + 设计六节全圈，2026-09-28/29）→ **grilling 一轮收敛（R1 四问四裁全部裁可，见下方裁定注）** · **实施计划**：spec 复审通过后由 writing-plans 另立 · **分支**：`m6-vasecli-plan-doctor`

**上游权威**：`wiki/vase-architecture.md` v3 的 §11.1（`VaseCli` 四子命令提案与「validate 与 plan 必须与运行时同一套规则」注）、§4.4（静态跳过与工具/运行时同规则）、§8.2（档三身份特征与两条承重工具链 flag）、§12.3（M5 行的「`plan` / `doctor` 归后续波次」）；
`docs/superpowers/specs/2026-09-27-vase-m5-vasecli-scan-validate-design.md` 的 D117（装载读与其代价）、D118（枚举入口缺失点名）、D119（本波范围的来源裁定）、D124（库薄工具厚）、D130/D131/D133/D135/D137（validate 的判定与 CLI 口径）；
M2b 波 1 的 D47/D62（Notes 结构化）、D50/D58（快照事务与宽容跳过）、D60（HostProvided 语义）、D82（归因两段制）；M4 的 D108/D109（`NoIdentityPlugin` 两平台各摘各的身份特征）。
历史决定 D1–D140 不改史，本文决定从 **D141** 续号。

**本文的地位**：兑现 §11.1 四子命令的**后两条**与 D119 挂账的「M5 第二波」。本波**零公开面增量**：不动 `kHeaderVersion`、不新增导出符号、不碰描述符布局——`plan` 是把 Console 已在走的 Solve 链搬到批处理前端，`doctor` 的四项全部站在 M5 之前已存在的读取面上。M5 spec §9 记的「后续波次」五件中，本波只清「`plan` + `doctor`」一件；平台腿、D125、VasePack、CI 原位不动。凡本文与磁盘代码冲突处以磁盘为准（仓库惯例）；本文与架构文档冲突处以本文为准并回头挂勘误（第 7 节清单）。

> **[事实取证注（2026-09-28/29）]** 本文的前提都是**读盘核出来的**，不是推的：
>
> ① **`plan` 的链条在磁盘上已有两条完整实现**：`Console::CmdPodNew`（`Tools/VaseConsole/Console.cpp:425-467`）= `Refresh → LoadPreset(可选) → Solve → 拿 plan`；`Console::CmdCatalogSolve`（`:364-423`）= 同链 + 打印。`LoadPreset` 是公开单函数（`Include/Vase/Catalog/Preset.h:30`），`Preset` 值形自包含。`plan` 不需要任何新机制。
> ② **`Loader::FileIdentity(path)` 是 static 的磁盘读**（`Source/Host/Loader.cpp:102`）：PE 侧 `ParsePeCodeView(image, loadedInMemory=false)`（`Source/Host/ImageInspectCommon.cpp:404`）走节表换算，ELF 侧读 `.note.gnu.build-id`；两者缺席时返回既有的 `MissingCodeViewError` 一类错误。**doctor 第①项因此不需要装载**——读身份特征读的是磁盘字节。
> ③ **`InspectDescriptors` 已带版本闸与 D118 点名**（`Source/Host/Inspect.cpp:42-81`）：无枚举入口 → `binary has no VasePlugin_Descriptors entry…`；版本不匹配 → `HeaderVersion mismatch: binary N, host M`（`CheckHeaderVersion`，`:19-27`）。doctor 第②项 = 装载读 + 这个函数，报错文本零新造。
> ④ **计划条目的可打印面**：`LoadPlanEntry{Id, BinaryPath, Decision, Reason, ResolvedConfig, Expected}`（`Include/Vase/Host/LoadPlan.h:38-49`）；`SkipReason` 恰三值、validate③ 已有无 default 穷举 switch 的先例（`Tools/VaseCli/Validate.cpp:227-241`）。`Console.cpp:197-209` 的 `SkipReasonText`/`SolveNoteKindText` 是该 TU 的 static——`plan` 的输出若是同形文本，就是 Tools 层第二份映射（见 D149）。
> ⑤ **argv 形状在磁盘上**：退出码三常量 `kExitOk/kExitCheckFailed/kExitUsage` 住 `Tools/VaseCli/Cli.h:11-13`；`RunValidate` 的手写解析体在 `Validate.cpp:252-279`（目录 + `--host-provides` 成对消费 + 坏值点名）；`ParseHostProvided`（`:31-49`）住 `Validate.cpp` 匿名 namespace——`plan` 要复用就得搬家（D146）。
> ⑥ **doctor 三项的 fixture 现成**：`NoIdentityPlugin`（`Tests/CMakeLists.txt:120-129`，Windows `/DEBUG:NONE` 摘 CodeView、Linux `-Wl,--build-id=none` 摘 build-id——**两平台各摘各的**，M4/D108 的跨平台化）→ 第①项 FAIL 证人；`NoEnumeration`（无枚举入口）与 `StaleEnum`（stale `HeaderVersion`）（`:44-45`、`:131-132`）→ 第②项两枚点名证人；健康侧 `LoadProbe` 两平台身份特征都在（T12 起是主链前提）。
> ⑦ **测试资产**：`Tests/TestingSupport/CatalogSandbox.h`（temp 一次性插件树 + 析构清理 + `CreateDir/WriteFile`）；`VaseCliValidateTests.cpp:21` 的 `StagePlugin` 放 DLL + 写/拷清单的构树法；`Tools/VaseCli/CMakeLists.txt:25-36` 的 ctest 用法腿形制（**WILL_FAIL 与 PASS_REGULAR_EXPRESSION 不可同设**——ctest 4.4.3 实测，命中反判 Failed）。
> ⑧ **平台不对称证人先例**：`LibraryFileName.RejectsForeignAndMalformedNames` 用例体内 `#ifdef` 两平台各注册各的一条；规矩 6 记的 Windows sharing-violation 判据力（镜像映射着 → `copy_file` 失败）与 Linux 空转差——第④项的探测面正好踩在这条既有平台事实上：**Windows 可探测，Linux 对映射文件无手段**。
> ⑨ **`#ifdef _WIN32` 平台探针函数的仓内先例在 Loader 层**：`PlatformReopenWritable` / `PlatformMappingRemoved`（`Source/Host/Loader.cpp:71-82` 调用点）——doctor 第④项的探针形状照它，但**住工具层**，不进 VaseHost（D148）。

> **[裁定注 · brainstorming（2026-09-28，四问四裁 + 设计圈定）]** 逐条落点：
> **Q1 · plan 形状（D141）**：`plan <插件目录> [presetFile] [--host-provides <name>@<version>]…`——提案原文 `plan <preset>` 缺目录参数，无目录即无快照；采 Console `pod new` 同形，且**吃 `--host-provides`**：wiki §11.1 那条「validate 与 plan 必须与运行时同一套规则」要求两命令的 `LoadRequest` 可同样施加，否则宿主补了服务的树会给出两样结论。
> **Q2 · plan 退出码（D142）**：D135 同一律扩到 plan。1 = Solve 硬错误 ∪ `kMissingDependency`/`kVersionMismatch`（与 validate③ 逐字同判）；2 = 目录不存在 / preset 读不成 / 零插件树；`kDisabled` 与 warn Notes 只打不计。
> **Q3 · doctor 定形（D143）**：收窄四项——①身份特征在场（磁盘读）②HeaderVersion 匹配（装载读）③目录可写（写探针）④残留文件锁（Windows 实测 / Linux 如实不可探测）。「编译器/版本/CRT」子项**记账不实现**（D147）。**[勘误（2026-10-03）]** 该子项已由需求方裁定划入 wiki §13.4「明确不做」，D147 销账。
> **Q4 · doctor 退出码（D144）**：`doctor <插件目录>` 单参；同一律三档，任一 FAIL = 1；Linux 的「不可探测」是 info 行，不计成败也不静默丢。

> **[裁定注 · grilling R1（2026-09-29，四问四裁全部裁可）]** 逐条落点：
> **R1-Q1（D152）**：doctor 收集序定死 **①③④ → ②**、输出仍按 check 1–4 固定序——② 的装载读会对自己留下 kept-resident 的镜像（本仓已知现象，`UnloadEvidence.ReopenWritable` 存在的理由）把 ④ 的「上一轮卸载没干净」**自喂成假阳性**；先探锁、后装载，结构上免疫。
> **R1-Q2（D153）**：② 节名改 `check 2 (load & header version)`，装载失败/无入口/版本不匹配三类发现都算这项——validate① 节名 `manifest vs binary` 装下 `binary not loadable` 是同形先例。
> **R1-Q3（D154）**：「plan 与 validate③ 同参同判」从运行时断言**降为结构论证**（同一个 `Solve` + D146 共用 argv）；跨命令 rc 相等结构上不成立（validate 的 rc 还可能被 check 1/4 拖到 1），测试各自钉各自的行为，论证进代码注释。
> **R1-Q4（D155）**：「doctor 不做清单↔二进制逐字段等值」**明写进 §0 不做表**——doctor 判环境事实（能不能换/装不装得上/版号对不对），内容等值归 validate① 与加载期比对（D72）；「清单漂移但一切可装载」的树 doctor 全绿是边界设计，不是漏项。

---

## 0. 范围

**本波做**（全在 `Tools/VaseCli/` 与测试/文书层；**库与公开面零增量**）：

1. **`plan` 子命令（§2，D141/D142/D145/D146）**：`Refresh → LoadPreset(可选) → Solve → 打印`，不 CreatePod、不装载；argv 与 validate 共用搬出来的解析腿。
2. **`doctor` 子命令（§3，D143/D144/D148）**：收窄四项的执法面、逐项输出、退出码映射、平台不对称处置。
3. **`--host-provides` 解析腿搬家（§4，D146）**：`ParseHostProvided` + 成对消费循环提入 `Cli.{h,cpp}`，validate 与 plan 共用一份；validate 的既有 argv 行为逐字不变（回归证人 = `MalformedHostProvidesIsAUsageError` 等既有用例）。
4. **测试（§5）**：`VaseCliPlanTests` / `VaseCliDoctorTests` 两个 gtest 文件 + ctest 用法腿 + usage 文本扩面。
5. **文书义务（§7）**：wiki §11.1/§12.3 现状注、`CLAUDE.md` 项目状态与两张基数表。

**本波不做**（各归其位，不许顺手）：

| 项 | 去向 |
|---|---|
| 「工具链一致性」的编译器/版本/CRT 细读 | 记账不实现（D147）。PE 侧无可靠读面、ELF 侧 `.comment` 只覆盖 clang 版本，两平台可读面不对称——身份特征子集（①）才是有后果可判的部分（缺了 ⇒ 档三 Adopt 必拒）。**[勘误（2026-10-03）]** 已由需求方裁定划入 wiki §13.4「明确不做」，D147 销账 |
| §11.1 的 CMake 后置步骤 | 不动，仍按 D125 单独记账 |
| macOS 腿与 `LibraryFileName` 的 `.dylib` 分支 | 仍归 macOS 那一波（D138）；doctor 第④项在 Apple 平台形态同 Linux（不可探测支），届时随平台腿一并点亮，本波不预写 |
| `--json` / 机器可读输出 | 仍按 D137 只出人读文本；消费面（D125）未起 |
| 组合库（一库 N 插件）的专项处理 | 仍归 VasePack 那一波（D126）；doctor② 打印描述符**计数**不假设 1，为该形状留了读法，但不为它做专项证人 |
| 运行时跳过的预测 | 结构上不存在——运行时跳过不进计划（§4.4，`LoadPlan.h:29-36` 注释同口径）；plan 只打静态跳过 |
| Console `catalog solve` 的退役或合并 | 不做。两前端文本各印各的（D149），判据同源（都是 `Solve`）已满足 §11.1 那条注 |
| doctor 内的**清单↔二进制逐字段等值** | 不归这里（D155）——那是 validate① 与加载期比对（D72）的执法位；doctor 判**环境事实**（能不能换、装不装得上、版号对不对），重报等值就是第二处真值。「清单漂移但一切可装载」的树 doctor 全绿是边界设计，不是漏项 |

---

## 1. 决定表

| # | 决定 | 依据 |
|---|---|---|
| D141 | **`plan` 形状 = `VaseCli plan <插件目录> [presetFile] [--host-provides <name>@<version>]…`**；位置参数至多两个，`--host-provides` 起后全为成对 flag | 裁定注 Q1；事实取证注①；§11.1「同一套规则」注 |
| D142 | **`plan` 退出码 = D135 同一律扩面**：0 = 可解且无 FAIL 级跳过；1 = Solve 硬错误 ∪ `kMissingDependency`/`kVersionMismatch`（与 validate③ 逐字同判）；2 = 目录不存在 / preset 文件 `LoadPreset` 失败 / 零插件树。`kDisabled` 与 warn Notes 只打不计 | 裁定注 Q2；D131（disabled 不算未过）、D130 推理（「没开始建快照才算环境错」） |
| D143 | **`doctor` 四项收窄定形**：①身份特征在场（逐插件 `FileIdentity`，**磁盘读**）②HeaderVersion 匹配（装载读 `InspectDescriptors`，复用 D118/版本闸文本）③目录可写（根 + 各插件子目录真写探针）④残留文件锁（Windows 独占打开实测；Linux info 行） | 裁定注 Q3；事实取证注②③⑥⑧⑨；D119 预告的「需要单独定义」在此兑现 |
| D144 | **`doctor` argv = `VaseCli doctor <插件目录>`；退出码同一律三档**——任一 FAIL = 1（**快照未建成也算 1**，D130 同律：那是判定级发现，不是环境档）；2 = 目录不存在 / 零插件树；**Linux ④ 的「不可探测」不计成败**，但必须打成 info 行（不许静默） | 裁定注 Q4；D135 |
| D145 | **`plan` 是全工具链唯一零装载子命令**：链上只有 Catalog 面（`Refresh`/`LoadPreset`/`Solve`），`LoadRequest::HostProvided` 借 argv 串（D60 窗 = 本次调用）、`Plan.Ordered[i].Id` 借快照（D61），打印在调用内完成——无悬垂面 | 事实取证注①；`plan` 的「预测不执行」语义（§4.4：求解不执行代码） |
| D146 | **`--host-provides` 解析腿搬进 `Cli.{h,cpp}` 共用**：`ParseHostProvided` 出匿名 namespace，成对消费循环提为 `Cli` 层函数；validate 行为逐字不变，回归由既有 validate 用例钉 | 事实取证注⑤；同一份 argv 规则住两处必腐（规矩 7 精神在工具层的同调） |
| D147 | **编译器/版本/CRT 细读记账不实现**：提案第①项兑现为身份特征子集；剩余子项若要做，前置是各平台可读面的独立取证 | 裁定注 Q3；事实取证注②（有后果可判的部分才进退出码）。**[勘误（2026-10-03）]** 需求方裁定划入 wiki §13.4「明确不做」，D147 销账（取证：三平台无可靠读面 + 失败形态在树内不可达） |
| D148 | **④ 的锁探针住 `Tools/VaseCli/Doctor.cpp` 文件内 static**，`#ifdef _WIN32` 单函数，形状照 `PlatformReopenWritable`（事实取证注⑨）；**不提进 VaseHost**——Host 侧没有它的第二个消费者（D124 的库薄工具厚判据再验一次） | D124；YAGNI |
| D149 | **`plan` 输出文本与 Console `catalog solve` 各持一份映射**（`SkipReasonText`/`SolveNoteKindText` 形状），不抽公共前端库——前端文本原则（M3/D100、M4/D110 两份 `PrintEjectReport` 同例）；「同一套规则」钉的是**判定**（同一个 `Solve`），不是输出形状 | 事实取证注④；技能 abi-boundary / 前端先例 |
| D150 | **③ 写探针文件名 = 时间戳唯一**（`.vase-probe-<ns>.tmp`，ns 取 steady_clock），创建→立即删除；**删除失败也算 FAIL**，文本点名残留路径交人清理 | 见 §3.3；探测不得留下不可见副作用 |
| D151 | **② 打印描述符计数、不假设 1**（与 validate① 的「恰一描述符」不同判——validate 比对的是清单↔描述符的**一一对应**，doctor 问的是**版本兼容**）；计数是 info | §3.2；D126 组合库的前瞻 |
| D152 | **doctor 收集序 = ①③④ → ②，输出按 check 1–4 固定序（收集后打印）**——② 是全工具唯一装载步，置后使 ④ 不被自喂（kept-resident 镜像会撞上「残留文件锁」判据） | R1-Q1；§8.2 档二与 `UnloadEvidence` 的既有事实 |
| D153 | **② 节名 = `check 2 (load & header version)`**：装载失败、无枚举入口、版本不匹配三类发现都归这项；节头加一行 `host header version: <kHeaderVersion>` 的 info（人不必去翻常量） | R1-Q2；validate① 节名先例（事实取证注③） |
| D154 | **「plan 与 validate③ 同判」是结构论证不是运行时断言**：同一个 `Solve`（§11.1 那条注的兑现位）+ D146 共用 argv；跨命令 rc 相等不成立（validate 的 rc 含 check 1/4），测试各自钉各自、注释指论证 | R1-Q3 |
| D155 | **doctor 不做清单↔二进制逐字段等值**：等值归 validate①/D72 加载期比对；doctor 只判环境事实，「漂移但可装载」全绿是边界 | R1-Q4；规矩 7（第二处真值） |

---

## 2. `plan`

### 2.1 链条与 argv（D141/D145）

```cpp
int RunPlan(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);
int PlanDirectory(const PlanOptions& options, std::ostream& out, std::ostream& err); // 供用例直调，同 ValidateDirectory 形
```

`PlanOptions{PluginDirectory, PresetFile(optional), HostProvided}`。链条 = `catalog.Refresh(dir)` →（若给 presetFile）`vase::LoadPreset(file)` → `catalog.Solve(request)` → 打印。任何一步 `Err` 都按 §2.3 的档位映射、不带病继续，且**没跑到的段要明说没跑到**（「不假装跑过」是 D130 的律；档位本身按 §2.3 各归各——preset 读不成是环境档 2，快照未建成是 FAIL 档 1）。

### 2.2 输出（D149，人读文本，D137）

```text
plan: 3 entries (preset: "<path>")        ← 无 preset 时省略括号段；路径带引号照实定（T5 对齐）
  1. Vase.Hello load
  2. Vase.Greeter skip disabled
  3. Vase.Dependent skip missing-dependency
  note: unknownPluginId Vase.Ghost
plan: FAIL (1 entry skipped with a hard reason)
```

条目行 = `Ordered` 数组序（加载序 = 关停逆序，§5.4），**不重排**——顺序本身就是判据的产物。跳过文案用穷举 switch（无 default，`-Wswitch` 顶新增枚举值，照 `Validate.cpp:227` 先例）。~~Notes 逐条透传 Message（D62 结构化字段在此只消费 Message 与 Kind——归因两段制 D82 不另造）。~~ **[勘误（M6-T6，2026-09-30）：该行对两前端都 disk-false——实定形逐条打的是结构化字段 `Kind`/`PluginId`/`Key`/`Cause`（Key 空时经 Cause 支补位），从不打 `Message`（`Console.cpp:410-422`；`Plan.cpp` 的 notes 环此前缺 Cause 支、终审修复波 T6 补齐后两份同形，证人 `VaseCliPlan.ProviderSkippedNoteCarriesCauseAttribution`）。「D82 归因两段制不另造」仍真**末行 verdict 与人读汇总；validate③ 同树同参时两命令**必然同判**，因为吃的是同一个 `Solve`。

### 2.3 退出码映射（D142）

| 情形 | 档 |
|---|---|
| Solve `Err`（环/阻塞、Provides 碰撞、override 类型/值域错 D80） | 1，错误 Message 透传 |
| 出现 `kMissingDependency` / `kVersionMismatch` 条目 | 1 |
| 仅 `kDisabled` 跳过和/或 warn Notes | 0（只打不计） |
| 目录不存在 | 2（环境；validate 同位先例 `Validate.cpp:106-112`） |
| 快照未建成（坏清单/重复 Id） | 1——点名「snapshot not built」，D130 同律（那是 `validate` 第②项的 FAIL 档，不是环境档） |
| `LoadPreset` 失败（文件坏/schemaVersion 闸拒） | 2 |
| 零插件树 | 2（同一律，D135） |
| argv 形状错（多余位置参数、坏 `--host-provides` 值） | 2 + usage 行 |

---

## 3. `doctor`

四项**互相独立报告、不遇错即停**（validate 同调，人一次修得动）。遍历对象 = 快照条目（Id 字典序）；子目录缺 `plugin.json` 的仍是 D50 宽容 + D128 info warning 行（「存在的东西不对」才执法，「少了东西」不执法）。**收集序 = ①③④ → ②，输出按 check 1–4 固定序**（D152：② 是全工具唯一装载步，置后使 ④ 的锁探针不被自己的 kept-resident 镜像自喂）。

### 3.1 第①项：身份特征在场（磁盘读）

逐插件 `Loader::FileIdentity(catalog.Directory() / entry->Subdirectory / LibraryFileName(entry->Binary))`。ok → `Vase.Hello: identity ok`；Err → FAIL，文本追加后果指针「tier-3 adopt would reject this binary」（承重 flag 被摘的症状从运行期响亮失败提前到诊断时刻，正是 doctor 的存在理由）。**文件不存在也落这项**（读不出字节 = 无特征），文案即 `FileIdentity` 的错误 Message。零装载。

### 3.2 第②项：可装载性与 HeaderVersion（装载读；D153、收集序置后 D152）

节名 `check 2 (load & header version)`，节头一行 `host header version: <kHeaderVersion>` 的 info。借用循环照 `Validate.cpp:140-193`（同单 `Loader` 实例、比对照着驻留期做完才卸）。`EnsureResident` Err → FAIL（load failure 点名，含 `DescribeLoadFailure` 的「missing dependency」句）；`InspectDescriptors` Err → FAIL（无入口 D118 文本 / `HeaderVersion mismatch: binary N, host M` 文本，零新造，事实取证注③）；ok → `Vase.Hello: HeaderVersion matches (N descriptor(s))`（D151）。D117 的装载代价由这一项单独承担——①③④全程无驻留。

### 3.3 第③项：目录可写（写探针）

对象 = 快照根目录 + 各 `entry->Subdirectory`（§11.2 换件链要往这两层写：新字节与重写清单）。每处一次真做：`D150` 命名探针文件创建（trunc）→ 关 → 删。任一环节失败 = FAIL 带 `std::error_code::message()` 与探针路径；删除失败也 FAIL（「probe file left behind」——探测不得留下不可见副作用，D150）。

### 3.4 第④项：残留文件锁（平台不对称，如实报）

```cpp
// 返回 locked 原因串；nullopt = 未锁；"not observable" = 平台支不可探测（info）
[[nodiscard]] static std::optional<std::string> ProbeExclusiveOpen(const std::filesystem::path& binary);
```

Windows 支：`CreateFileW(…, share=0, OPEN_EXISTING)`——镜像被任何进程（含本进程）持有时 sharing-violation → FAIL 点名「上一轮卸载没干净」。**Linux 支：不逐文件测**，整项打一行 `lock probe: not observable on this platform — a resident mapping does not prevent overwrites here`，不计成败（事实取证注⑧：这正是规矩 6 记的 Linux 空转事实的诊断面对账）。文件不存在 → 该文件 info 跳过（①已执法，不双报）。

### 3.5 输出与退出码（D144）

```text
doctor: <abs dir>
check 1 (binary identity features):
  Vase.Hello: ok
  Vase.NoId: FAIL — CodeView record not found … (tier-3 adopt would reject this binary)
check 2 (load & header version):      ← 收集序里 ② 最后跑、输出序里仍是 2（D152/D153）
  host header version: 4
  …
check 3 (directory writability):
  ok: . / Hello / NoId …（逐项一行）
check 4 (file locks):
  Vase.Hello: locked — sharing violation（Windows 实测形）
  lock probe: not observable on this platform（Linux info 形）
doctor: FAIL (checks 1 and 4 reported failures)
```

任一 FAIL = 1（快照未建成 ⇒ `snapshot not built: checks 1-4 were not run`，同律 1）；目录不存在 / 零插件树 = 2；全过（含 info 行）= 0。

---

## 4. 工具层形态（全部增量住 `Tools/VaseCli/`）

- 新文件 `Plan.{h,cpp}`、`Doctor.{h,cpp}` 入 `VaseCliCore` 源表（`CMakeLists.txt:4-8`）。
- `Cli.cpp::Run` 分发包扩到四命令；`PrintUsage` 扩到四行。
- **D146 的搬家**：`ParseHostProvided` + 尾段成对消费从 `Validate.cpp` 提入 `Cli.{h,cpp}`（形如 `bool ParseTrailingHostProvides(args, firstFlagIndex, out, err)`，坏值点名行为逐字保留）。
- doctor 需要的 include 面：`Vase/Host/Inspect.h`、`Vase/Host/Loader.h`、`Vase/Catalog/*`、`<filesystem>`——全是 M5 已链的 target，**CMake 链接面零变化**。
- Windows 探针所需的 `<windows.h>` 只出现在 `Doctor.cpp` 的 `#ifdef _WIN32` 体内（clang-cl 与 cl.exe 两线同 API；`/utf-8` 已由 `VaseBuildOptions` 兜住——新文件中文注释的编码前提，规矩 1 不重犯）。

---

## 5. 测试与验收

### 5.1 gtest（新文件，链 `VaseCliCore`；沙箱与构树法照事实取证注⑦）

**`Tests/Integration/VaseCliPlanTests.cpp`**——

| 证人 | 构造 | 断言 |
|---|---|---|
| 三档 happy 腿 | LoadProbe + Greeter 树 + preset 禁一只 | rc 0；`skip disabled` 行；无 FAIL verdict |
| 缺依赖 = 1 | `StageDependentWithGeneratedManifest` 法（validate 先例） | rc 1 + `missing-dependency` 行 |
| `--host-provides` 补上 = 0 | 同树 + 喂 `Vase.Hello.Greeter@1`（validate 同名证人 `HostProvidedServicesSatisfyTheThirdCheck` 的参） | 各自钉各自：plan 侧 rc 0 + 该 Id 的 `load` 行，validate 侧由既有用例钉（「同判」是 D154 结构论证，不做跨命令断言） |
| preset 覆盖禁依赖方 | 写 preset 关 Dependent 自身 | 计划里 disabled 且 rc 0（disabled 不算未过） |
| 环 = 1 | CycleA/CycleB manifest | Solve Message 透传 |
| 坏 preset = 2 / 目录缺 = 2 / 零插件 = 2 | 各一 | 三档环境腿 |
| **零装载证人** | FailingLoadPlugin + 健康 manifest 同树 | **rc 0**——装载不了的二进制 plan 照出计划（它从不装载） |
| Notes 透传 | preset 引用幽灵 Id | `note: unknownPluginId` 行 + rc 0 |

**`Tests/Integration/VaseCliDoctorTests.cpp`**——

| 项 | 证人 |
|---|---|
| ① | `NoIdentityPlugin` staged → FAIL 点名 + rc 1；`LoadProbe` → ok（两平台各摘各的身份特征，负例两侧同构成立，M4/D108 资产的第二读者） |
| ②（`load & header version` 节，D153） | `NoEnumeration` → D118 文本；`StaleEnum` → `HeaderVersion mismatch: binary 5, host 4` 形文本（该 fixture 的唯一变量是 `HeaderVersion = k+1`，M5/D123 探针注释明写——闸对两个方向都咬）；`LoadProbe` → matches；~~`FailingLoadPlugin` → load-failure 点名~~ **[勘误（M6-T5，2026-09-30）：该行实测证伪——FailingLoadPlugin 败在 OnLoad，② 的装载读面（EnsureResident+InspectDescriptors）不执行它，其 DLL 平台装载正常；素材改为真实 LoadProbe 字节翻 machine 字段的篡改件（用例名与三条断言逐字不变），见 plan `docs/superpowers/plans/2026-09-29-vase-m6-vasecli-plan-doctor.md` 偏离登记 T3 条]** |
| ③ | happy = 沙箱默认；FAIL：Linux 支 `permissions(0500)`，Windows 支 `DELETE_ON_CLOSE` 持目录句柄（delete-pending 目录内创建失败）——**用例体内 `#ifdef` 各注册各的**（事实取证注⑧先例形） |
| ④ | Windows：进程内 `CreateFileW(share=0)` 自持二进制句柄 → FAIL 行（sharing 检查对同进程同样生效——**实施期首条实测**，若不成立改用出锁再入锁的对照）；Linux：info 行逐字钉 + rc 不受影响 |
| 合成 | 全健康树 rc 0；两项同时 FAIL 的树 → 两个 check 节各自点名、verdict 行并列（不遇错即停） |

### 5.2 ctest 级（`Tools/VaseCli/CMakeLists.txt`，腿形照 25-36 行既有两对）

`VaseCliPlanNoArgs{IsUsageError,Reason}`、`VaseCliDoctorNoArgs{IsUsageError,Reason}`、`VaseCliDoctorTooManyArgsIsUsageError`（+文本腿）；usage 文本正则**扩现有 `VaseCliNoArgsReason` 的钉法**——现只钉 `usage: VaseCli` 前缀，本波不动其正则，四行全打由新用例各钉各的命令词。

### 5.3 验收档位

标准档：**六线删树重配全量 + `ctest -N` 对基数 + 三条 debug 线 tidy 全量 + format 一条命令**；HotSwap 选择子族**按惯例跑**（本波零 Loader/账本/Eject 路径改动，规矩 6 的触发条件不适用——但 doctor② 是 Loader 的**新调用方**，族绿作为调用面回归证据入账）。基数与 tidy 计数落账随收口波写回 `CLAUDE.md` 两张表。

---

## 6. 风险

| # | 风险 | 处置 |
|---|---|---|
| 1 | ④ Windows「同进程自锁是否触发 sharing-violation」未实测 | 实施期第一条实测；不成立则换 `CreateProcess` 持锁子进程或如实降级为「契约束 + 跨进程探针手工观察」，**不静默改成永真** |
| 2 | ③ Windows `DELETE_ON_CLOSE` 持目录句柄使创建失败的行为差异 | 同样实施期首测；不成立则该 FAIL 支 Linux 有证人、Windows 如实记「无自动化证人」（M4 档三历史同形），判据本身两平台都真 |
| 3 | doctor② 的装载读留下 kept-resident 镜像（Windows 上 `Unload` 可假成功——这恰是 `ReopenWritable` 存在的理由） | D152 已把 ④ 挪到 ② 之前，**自喂假阳性结构性消除**；残留驻留至进程退出、进程即回收。doctor 不判卸载证据（它不是热插拔链），② 的判定在驻留期内完成、不受影响 |
| 4 | `plan` 与 validate③ 判定漂移 | 结构上不可能——同一个 `Solve`；唯一可漂点 = 两命令 argv 解析分叉，D146 搬家消灭 |
| 5 | preset 文件路径与 `--host-provides` 位置混读（第二个位置参数恰好写成 flag 前缀） | 解析规则定死：arg[2] 以 `--` 起 ⇒ 视为 flag 段开始、presetFile 缺省；坏值点名（validate 现有文案同律） |

---

## 7. 文书义务

| 处 | 动作 |
|---|---|
| wiki §11.1 命令块下方 | 挂 M6 现状注：`plan`/`doctor` 已落（D141–D151）；`plan` 采目录参数形（提案原文 `plan <preset>` 缺目录，勘误入账）；`doctor` 四项收窄 + D147 剩余子项记账；D125 后置步骤指针**不动** |
| wiki §12.3 里程碑表 | M5 行注补「`plan`/`doctor` 已于 M6 落地」指针 |
| `CLAUDE.md` | 「项目状态」加 M6 段；两张基数表落账；「退出码是**两命令**共享的同一套」两处措辞随四命令更新（D135 同一律扩面注记）；「清单工具」条目从「两条腿」改「四条腿」 |
| 技能 `vase-cpp-engineering` | 自查 grep 跑一遍；本波无新承重 flag/字面值入技能（判据与理由类若引 D143/D147 收窄逻辑，按规矩 7 第四类两边各写一份、免指针） |

**账本**：M5 记名欠账五件中本波清「`plan` + `doctor`」一件；余四件（macOS 平台腿、D125、VasePack + C4251、CI）原位。本波新记一笔：D147（工具链一致性剩余子项）。
