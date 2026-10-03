# CLAUDE.md


This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 目录

四组按「先弄清要做什么 → 再动手 → 最后宣布完成」排。

**节名是稳定接口**：`.claude/skills/vase-cpp-engineering/` 里有几十处按名字指向本文件各节（`构建与测试`、`静态检查与格式`、`工具链 flag 是承重的`、`规矩 N`…）。**改这些标题要同步那些链路**，否则技能里的指针当场变孤儿（见规矩 7）。

- **一、这个项目是什么** —— 定位、现状，以及本文件与技能 / wiki 的分工。第一次接触仓库先读这一组。
- **二、怎么跑** —— 八个 preset 的命令、按线基数、产物落位、第三方依赖的两条入口，与两条承重的工具链 flag（macOS 无对位 flag，见「工具链 flag 是承重的」）。
- **三、怎么写** —— 语言与提交约定、七条规矩、格式与命名的偏离项。改代码前读。
- **四、怎么验** —— tidy 与 format 怎么跑、退出码为什么单独不够、基数怎么读。宣布完成前读。

找命令与基数去 `构建与测试`；找七条规矩去 `在这个仓库里干活要知道的规矩`；跑门禁与读基数表去 `静态检查与格式` 与 `核这些门禁时，退出码单独用是不够的`（这两节原先嵌在 `构建与测试` 里，现独立成组）。

## 一、这个项目是什么

定位、现状，以及本文件与技能 / wiki 的分工。第一次接触仓库先读这一组。

### 技术前提（由需求方指定）


- 语言标准：**C++20**。
- 项目性质：**plugin 插件管理能力库**——只负责发现插件、解析依赖、装配、运行、干净关停；不提供业务逻辑，不提供编辑器，不提供引擎适配。
- 目标平台：**Win x64 / Linux x64 / macOS x64**（开发 + 发布）、**Android arm64 / iOS arm64**（仅发布）。macOS 原为 arm64，需求方 2026-10-01 裁定改 x64、**arm64 永久移出**——将来若要 arm64 是新增一套 preset + triplet，不是改这一套（macOS 腿 spec D157）。编译器与 STL 矩阵见架构文档 8.5——注意 Windows 上 `cl` 与 `clang-cl` 各有一条 preset 线、**同一个构建树内不得混用**（要么全 `cl`、要么全 `clang-cl`；需求方 2026-10-03 裁定），Linux/macOS/Android/iOS 用 `clang` + `libc++`。
- 许可证：MIT，版权归 MoozenSoft。
- **不使用 C++ 异常**：全项目以关闭异常的方式编译（Windows `/EHs-c-`、Linux/macOS `-fno-exceptions`）。错误一律经 `Result<T>` / `Error` 显式返回，**不写 `throw` / `try` / `catch`**。连带约束（标准库与第三方库的抛错 API 改用不抛形式、`nlohmann/json` 开 `JSON_NOEXCEPTION`、`EXPECT_THROW` 在本项目 TU 不可用）见架构文档 0.3 原则 7 与 13.2，以及 M0/M1 设计文档第 9 节。

### 设计意图（摘自 README）


> Vase is a plugin framework that treats every module like a branch in flower arranging — carefully selected, gracefully placed, and cleanly removed.

即核心关注点是**模块的选取、挂载与干净卸载**。M1 已把它的最小形落成代码，M2a 补了装配地基那一层，M2b 两波补了清单解析/Catalog 求解与加载期比对执法（见「项目状态」；具体机制以磁盘上的头文件与实现为准，`wiki/vase-architecture.md` 里其余部分仍是提案）。**不要在文档或代码注释中把提案写成既定事实**——新增或变更机制前先与需求方确认。

### 项目状态：M0、M1 完成；M2a 完成（M2 第一波），M2b 波 1 完成（清单解析/Catalog/Solve/Preset），M2b 第二波完成（加载期比对/Adopt 单轨/enum+bump/前端），M3 完成（判据 3a 全量形/3d 进程级状态登记），M4 完成（换件谱描述符维含回退/Windows 档三负例还清 M1 登记账/语义依赖知情位），M5 第一波完成（VaseCli scan/validate），M6 完成（VaseCli plan/doctor，四命令共享同一套退出码），macOS x64 平台腿完成（八线全绿：档二 _dyld 映射清单/档三 LC_UUID/无对位承重 flag 由负例把守），小账清理波完成（2026-10-03：verify 脚本 cd 泛化 / cl.exe 线 `/we4062` / Mach-O 三枚对抗证人 / 文书四笔），CI/CD 与对外分发两条「不做」裁定（2026-10-03）


**M0（构建地基）、M1（Pod 闭环 + 热插拔骨架）、M2a（M2 第一波：装配地基——配置面、`LoadPlan` 定形、
多插件装配与跳过/碰撞执法、递归拆除、报告结构化）、M2b 第一波（清单解析/Catalog/Solve/Preset）
与第二波均已完成**。第二波的五件：**加载期全字段比对**（`ManifestExpectation` 进 Host、
`CreatePod` 带期望即比 / `AdoptPlugin` 必比，D67/D68/D72）、**Adopt 单轨**（`AdoptRequest` 改签名、
`KnownBinaries` 路径账退役、`AdoptInto` 单点重读，D69/D71/D81）、**`enum` 配置型与 choices schema +
`kHeaderVersion` 2→3**（七型、`FieldInfo` 尾追加两槽，D75/D76/D80）、**D19 前端双入口**
（Console `pod new <目录> [preset]` 主链 + `pod new-raw` 旁路 + `catalog` 命令组；Samples 补
`DependentPlugin` / `FailingPlugin`）、**handoff 账四条还账**（Solve 归因两段制 D82、Preset 根级
unknown 键 D83、`Directory()`/Plan 再绑定用例、`PluginMeta` 不收两槽 D73）均已完成，
六线基数与 tidy 计数已随收口波落账（见「构建与测试」）。**波 2 的两笔留账亦已还**
（账①：`VASE_CONFIG` 补「enum 默认 ∈ choices」编译期闸——`FieldInfo.h` 的 `DefaultInChoices`
取件器 + CHECK 点第三条 static_assert，**描述符布局未动**（其时 `kHeaderVersion` 仍 3，3→4 是 M3 的 D92）；
账②：`Solve` 的 Notes 去重键契约——键仍为 4 元组，同键必同 Message，违反即 `ProgrammerError`）。
**M3（判据 3a 全量形 + 3d 进程级状态登记）也已完成**（spec `docs/superpowers/specs/2026-09-26-vase-m3-multi-plugin-design.md`，
D90–D103）。本波两笔：**3d 进程级状态登记**——`ProcessStateDesc{Name, Reset}` 进 `PluginMeta` 尾槽
（`kHeaderVersion` **3→4**，D92）、清单顶层 `processStates` 名字数组与描述符按 Name 集**双向等值**比对
（D93，D89 对 `.Config` 的同构收口——忘写 `.ProcessStates` 而清单列了名字 ⇒ 主链加载即拒，不再是静默点）、
`EjectPlugin` 三条分支（活实例/级联空壳/Failed 记录）按「这份 binary 在任何 Pod 无活实例」口径自动 Reset
并逐条写入 `EjectReport::ProcessStatesReset`（D91/D99）、比对器入口的结构化绑定绊线守「加了字段没进绑定」
（D101 改判形——构造点即绊线的原案因全仓零个指定初始化点被替换，见 plan 偏离登记）、前端两份
`PrintEjectReport` 补打点（D100）；**3a 全量形**——三插件局 50 轮 `HotSwap.FiftyRoundsBehaveLikeFirstTime`
（新 fixture `NeighborC`；每轮派 2 拍、循环内断言两只邻居累计 `Beats()`，D98/D103），连带**级联空壳可被
Eject**（D94——`not in pod` 自此只剩「本局无此 Id」一支，见规矩 6）。**完整属主追踪器经核查否决（D97）**：
§9.3 契约束无机制可拦，插件侧「绕道注册」无可达形态（拿不到 Pod 的 `ScopePool`），磁盘四处「属 M3 完整属主追踪器」
的注释与技能同口径句已随文书义务波改准（`Evidence.h` 那处 M3 归属随 3d 实现落地时即已改）。
**六线基数与 tidy 计数已落账，共两段三轮**——收口复测（83bd297 删树重配全量 + b1a0cc5 后增量复测，2026-09-27）
与 **M3 终审修复波**（2026-09-27：③ 全局闸计入级联空壳第三类持有者——C1，spec §2.3「走同一套全局闸」兑现；
Reset 闸维持只数活实例，与 §3.4 记载的 kept-resident×重置并存组合对上——并补两枚并存证人
`Eject.ProcessStatesResetCoexistsWithKeptResidentImage`/`…Shell`，前者是 `state.Reset()` 调用点的**首个可证伪
证人**——原单局证人因全卸重装、读数落在全新镜像上而对该调用失明，勘误见 plan 偏离登记 6）。现值见「构建与测试」两张表。
**M4（热替换）也已完成**（spec `docs/superpowers/specs/2026-09-27-vase-m4-hot-replacement-design.md`，
D104–D116；**macOS 腿顺延 M5**，与 M0–M3 的平台口径一致）。三条腿：**换件谱扩到描述符维含回退方向**
——五步阶梯（`VersionedA` →（代码）→ `VersionedAPrime` →（`Version` 1.0.0→1.1.0）→
`VersionedAStampDrift` →（`Provides` v1→v2）→ `VersionedAServiceDrift` → 回退两步），
**每步只差一维**（回滚拆两步即为守住这条），一条用例走完
（`HotSwap.DescriptorDriftLadderSwapsBothWays`），每步三条证人——负例（一维 tamper）/ 复用分支
正例 / 全新装载正例，`ReusedResidentImage` 的真假两值因此在一趟里都有证人；两个新 fixture 同 `OUTPUT_NAME`、
各自 stage 目录（D105/D106/D113）；**Windows 侧档三负例还清 M1 登记账**——换件序列两平台统一为
「改名离开（`P → P.old`）+ 落新字节」，拆掉 `#ifndef _WIN32` 那道闸，**Linux 侧实跑亦绿**故该统一序列成立、
无任何为它而设的 `#ifdef`；`NoBuildIdPlugin` 跨平台化并改名 `NoIdentityPlugin`（Windows 侧 per-target
`/DEBUG:NONE`、Linux 侧维持 `-Wl,--build-id=none`，**两平台各摘各的身份特征**；`P.old` 残留归
`ProbeSwapGuard` 析构清理，D108/D109/D114）；**语义依赖知情位**——`EjectReport` 尾追加
`bool SemanticDependencyPossible`，置位 = `ProcessStatesReset` 非空 ∧ 进程内任一 Pod 仍有活插件实例
（只数活实例），**不是检出**、如实上报不可知；`Status == kRejectedConsumers` 时恒 `false`（那一路没有
重置发生），而 **kept-resident 分支可以合法为 `true`**——Reset 闸问的是「这份 binary 还有没有活实例」，
这一位问的是「进程内还有没有别的活实例」，不是同一个问题；前端两份 `PrintEjectReport` 只在
`kEjected` 时打点（D110/D111/D115）。连带 **判据 3f** 入 v3 §12 判据表（与 3d 分立，D115）。
**后果之一是平台形状变了**：`linux − win` 的 +2 差值归零——那两条档三负例不再 Linux-only，
**平台差没了，六条线只在 `debug − release` 那一维上有差**（这一形状自 M4 起维持至今，现值见「构建与测试」两张表）。
**M5 第一波（VaseCli `scan` / `validate`）也已完成**（spec `docs/superpowers/specs/2026-09-27-vase-m5-vasecli-scan-validate-design.md`，
D117–D140；**M4 遗留同域两笔随 T14 还清**——`AdoptManifestTests` 恒真断言删除、D110 补跨 Pod falsifier）。
本波四件：**`scan` 取「装载读」**——`Loader` 载入 → 读描述符 → 立即卸下，驻留期 `DllMain` 与静态构造会跑是
明写接受的代价（D117，wiki §1.1/§3.1 同口径句随文书义务挂勘误）；连带 `VASE_PLUGIN` 尾追加第四个符号
**`VasePlugin_Descriptors`**——只给工具的枚举面，纯追加、不动 `PluginMeta` 布局、**不 bump `kHeaderVersion`**；
缺入口的老二进制照常加载而扫不了，报错点名原因、不静默返回「零个插件」（D118/R1-Q1，这处不对称记在
wiki §13.3）。**清单保真回写**——`schemaVersion` 恒写 1、`binary` 由 `LibraryStem` 从文件名恢复、
`enabledByDefault` 保真（D122），写盘单文件原子 + 逐插件尽力而为、输出按 Id 排序（D134/D136）。
**`validate` 四项**——快照能否建成、逐插件清单↔描述符比对、依赖面如实透传 `Solve` 裁定（环与被阻塞不另分列、
`kDisabled` 不算未过，D127/D131）、服务前缀自洽 + 宿主越界（D120/D132）；前缀规则自此定死为
**完整 Id**，`Samples/DependentPlugin` 服务随之改名 `Vase.Farewell` → `Vase.Dependent.Farewell`（D121）；
`--host-provides <name>@<version>` 可重复（D131）；退出码是**两命令共享的同一套**三档 0/1/2、零个插件的树都算用法/环境错（D135——`scan` 与 `validate` 同一律）（M6 起四命令同一律，见下 M6 段）。
**未接与推后的**：CMake 后置步骤本波不接（D125）、`plan` / `doctor` 与 macOS 平台腿归后续波次（D119/D138）。
六线基数与 tidy 计数已随收口波落账（M5 收口复测 2026-09-28，见「构建与测试」两张表）。
**M5 波末还账（2026-09-28）也已完成**——把收口时记名的四笔清干净：**M4 跨域两笔**（M5 spec D139 明写
不搭顺风车的那两笔）其一为 `Tests/HotSwap/AdoptTests.cpp` 的 `ProbeSwapGuard` **中止残留加固**——abort
不走析构，旧实现的 ctor 会把 `.orig`/`.old` 两份原件都删掉、以**换过的** Target 为新基线，静默把替换字节
立成「LoadProbe」的常态；现改为「`.old` 无条件清（它是换件序列的中转名）、`.orig` 是锚且只在缺失时创建，
锚还在即先用它把 Target 还原」，两种残留态各有判据、合成一枚确定性证人
`Adopt.ProbeSwapGuardHealsAbortResidue`；其二为**前端字段计数无机器守卫**——两份 `PrintEjectReport`
各加**结构化绑定绊线**（M3/D101 同例：`EjectReport` 加字段即该行硬编译失败，实测
`binds to 16 elements, but only 15 names were provided`），`VaseConsoleBehaviourEjectReason` 的正则由钉前缀
扩到**钉整行字段名序列**，并给此前**零 ctest 消费**的 `Samples/Embedding` 补 `EmbeddingSwapDemo`
（两条 pin 各做过「抽掉一个字段打印」的变异取证）。**M5 记名文书债两条**：wiki §3.3 清单样例与 §6.1
`IDamageSystem::kName` 的裸名 `Vase.DamageSystem` 改准为 `Vase.Combat.…`（§6.1 那条勘误注随之由
「三点」改「四点」）、§11.2 流程行的「CMake 后置步骤自动跑」补现状指针（D125 未接）。
**M6（VaseCli `plan` / `doctor`）也已完成**（spec `docs/superpowers/specs/2026-09-29-vase-m6-vasecli-plan-doctor-design.md`，
plan `docs/superpowers/plans/2026-09-29-vase-m6-vasecli-plan-doctor.md`，D141–D155）——§11.1 **四条子命令至此全部落地**，
零公开面增量（`kHeaderVersion`、导出符号与描述符布局都不动）。两腿：**`plan` = 零装载 Solve 预览**
（D145：链上只有 `Refresh`/`LoadPreset`/`Solve`——装载不了的二进制照出计划；形 `plan <插件目录> [presetFile] [--host-provides …]`，
提案原文 `plan <preset>` 缺目录，勘误入账，D141；输出文本与 Console `catalog solve` 各持一份映射，D149；argv 腿
`--host-provides` 自 `Validate.cpp` 提入 `Cli.{h,cpp}` 共用、行为逐字不变，D146；「plan 与 validate③ 同判」是结构论证
非跨命令断言，D154）；**`doctor` = 收窄四项定形**（D143）：① 磁盘读身份特征在场（承重 flag 被摘的症状自运行期响亮失败
提前到诊断时刻）、② 装载读 HeaderVersion（节名 `check 2 (load & header version)`，D153；描述符计数打 info 不假设 1，D151；
**收集序 ①③④→② 置后**，D152——② 是全工具唯一装载步，其 kept-resident 镜像否则会自喂 ④ 的假阳性，结构消除）、
③ 目录写探针（根 + 各插件子目录创建→关→删，删除失败也算 FAIL，D150）、④ 平台锁探针（探针住工具层文件内 static、
不提 VaseHost，D148；**Windows 写开独占**——T4a 首测证伪原读开形：驻留映射下读开实测 gle=0、写开才 gle=32，
T4b 需求方裁定 2026-09-30 探针改按 `PlatformReopenWritable`/`copy_file` 同律的 install 语义、头号场景证人恢复真断言
（单点归因 `checks 4`），④ 的 FAIL 面随之变宽（非共享冲突类 open 失败如只读属性现报 `probe failed (Win32 error 5)`），
反向「允许写但拒绝读」的持有者不见于 ④ 属裁定明记接受（install 照样落字节，不构成换件障碍）；Linux 支 info 行
不拖退出码，D144 如实报）；T4 风险 2（delete-pending 目录 ⇒ ③ FAIL）亦首测证伪——属平台事实，Windows 支按协议
`GTEST_SKIP` + 登记、Linux chmod 555 支为真实证人，③ 的 Windows 候选构造（ACL deny-Write）待裁。**D135 同一律
扩到四命令共享同一套退出码三档 0/1/2**（D142/D144）；清单↔二进制逐字段等值 doctor 明文不做（D155，validate①/D72
的执法位）。**D147 为本波新记名一笔**（编译器/版本/CRT 剩余子项记账不实现——两平台可读面不对称，有后果可判的部分才进退出码；**该笔已于 2026-10-03 经需求方裁定划入 wiki §13.4「明确不做」**，见「项目状态」段末）。
**M5 记名欠账五件本波清「`plan`+`doctor`」一件、余四件原位**（macOS 平台腿、D125、VasePack+C4251、CI）。
新测试文件 2 个 `Tests/Integration/VaseCli{Plan,Doctor}Tests.cpp`（plan 12 + doctor 8：T3 共享 6 + T4 平台证人 2 系
体内 `#ifdef`、两平台各注册各的同名一条计数对称；plan 的 12 含下述 T6 证人）+ ctest 级用法腿 6（plan 2 + doctor 4）。覆盖如实记：③ 的 `put('x')`
返回值未查、探针名取 steady_clock ns（并发两 doctor 理论可同名假 FAIL，brief 原形、接受）；doctor 的四条环境腿
（not-a-dir / 零插件 / 快照未建 / header 版本行）无自动 pin——plan 侧有同形证人、validate 用法腿先例同形，
doctor 环境腿本轮不加新证人、如实标注。**六线基数与 tidy 计数随收口波落账（M6 收口复测 2026-09-30**：六线删树重配全量两轮
全绿零假红；三线 tidy 首跑捕正文 warning 10 条、全落本波新代码，代码级修复后复跑归零，见 plan 偏离登记 T5 条；
规矩 6 的 `HotSwap|Eject|Adopt` 族 doctor② 是 Loader 新调用方、六线全量跑过族内全部用例为调用面回归证据）。
**M6 终审修复波（T6，2026-09-30，verdict：With fixes）已跟上**：`Plan.cpp` notes 环补 Cause 支——
`providerSkipped`/`versionMismatchProvider` 两 kind 空 Key、归因在 Cause（D82 两段制的产出位），缺支即丢归因、
破 D149 形状忠实副本承诺，照 `Console.cpp:417-420` 逐字镜像补；证人
`VaseCliPlan.ProviderSkippedNoteCarriesCauseAttribution`（Alpha provides + preset 禁之 ⇒ Beta 硬跳，note 行两侧
点名；变异取证：抽掉 Cause 支该枚转红）。④ 缺件文件由 `unlocked` 改 `absent (check 1 owns it)` info 支
（spec §3.4「文件不存在 → info 跳过」兑现、不计 Failures）。六线各净 +1（331/330）与 tidy 复测值随 T6 落账——
现值见「构建与测试」与「静态检查与格式」两张表。
**macOS x64 平台腿（2026-10-02）也已完成**（spec `docs/superpowers/specs/2026-10-01-vase-macos-x64-platform-leg-design.md`，
plan `docs/superpowers/plans/2026-10-01-vase-macos-x64-platform-leg.md`，D156–D179，plan 偏离登记十条）——
**M5 记名欠账至此清「macOS 平台腿」一件、余三件原位**（D125、VasePack+C4251、CI），`LibraryFileName` 的
`.dylib` 分支随 T6 还清 **D138**。目标平台改口：macOS 只做 **x64**、arm64 **永久移出**（需求方 2026-10-01 裁，D157——
本仓库唯一一处需求方指定前提的变更；将来若要 arm64 = 新增一套 preset + triplet，不是改这一套）。形状一句话：
**八线全绿**（Win4 + Linux2 + macOS2，debug 334×4 / release 333×4、平台差 0——该波读数，现值见「构建与测试」）+ tidy 四线 + format 双判；
构建面 MacPorts `clang++-mp-23`（非 Apple clang 16——SDK 头树不同源，故 overlay triplet `x64-osx-libcxx.cmake`
钉编译器为头号理由，`dynamic` 链接只属「与另三条线一致」，D158/D160；**`pkg-config` 为 macOS 独有环境前提**，
缺它 gtest port 倒在 `vcpkg_fixup_pkgconfig`，脚本起手响亮检查、不出 shim，D178）；
**档二** macOS 证据 = `_dyld_image_count` / 路径枚举（`ImageInspectDarwin.cpp`，**匹配前路径必须规范化**——
dyld 记规范化路径，`/tmp`→`/private/tmp`，D164）；**档三** 身份 = `LC_UUID`，**macOS 无对位承重 flag**（链接器必写），
承重性由 `NoIdentityPlugin` 的 `-Wl,-no_uuid` 负例**反向把守**（D159/D167，档三两条自此三平台同跑）；
公开面增量：`IdentityKind` 追加 `kMachOUuid` 并**去掉默认值**（「忘了赋 Kind 静默贴错标签」的埋伏拆除，D163/D175，
`kHeaderVersion` **不动**）、`FirstUnresolvableImport` 的 `bool isPe` 换 `ImageFormat{kPe,kElf,kMachO}`（枚举使格式集合
成为编译期穷举点，`Loader.cpp` 平台分支从三处降到一处，D165/D174）、三解析器契约统一「返回文件原文、不做路径归一」，
`@rpath/` 与带路径名归一在**比对点**——修的是通用形态（ELF 的 `DT_NEEDED` 同样可带路径，Linux 上潜伏的同形缺陷
顺手堵掉，D176）；`ImageInspectPosix.cpp` 更名 `ImageInspectLinux.cpp`、`ImageInspectMachO.cpp` **无条件编译**
（八线合成就链它，D161/D162）；Mach-O 解析走合成字节用例 `ImageInspect.MachO{UuidExtracted,DylibNamesListed,
WithoutUuidFailsLouder}` **八线同幅**（净 +3 = 本波全部用例增量，T10 起无增减；路径匹配一层只有 macOS 线可测，如实记）；
`doctor` ④ 的 macOS 形态 = 与 Linux 同属「不可探测」支，点亮后确认**无需改码**（D170，M6 预告至此兑现）；
neverUnload 实测结论**带形态限定**入技能——纯 C++/libc++、无 ObjC/Swift 元数据、无 `RTLD_NODELETE` 形态不触发，
清单其余项未测，**不是「macOS 不会 neverUnload」**（D171）；「改名离开 + 落新字节」的换件序列在 macOS 上还压着
一条比 Linux 空转更硬的理由：in-place 截断仍被映射的镜像 ⇒ 旧映射页失效、触碰即 `SIGBUS`——不许把它优化回去。
八线基数与 tidy 四线计数随收口波落账（macOS 腿 T10 复测 2026-10-02，见「构建与测试」与「静态检查与格式」两张表；
规矩 6 族选择子 macOS 首次入账 58——与 win/linux 同值）。
**小账清理波（2026-10-03）**：四笔定点清理——两个 verify 脚本的机器路径 `cd` 泛化为 SCRIPT_DIR 形、
cl.exe 支补 `/we4062`（两 msvc 线的穷举 switch 守卫自此真 armed，探底 + 变异探针取证）、`LoaderTests` 增三枚
Mach-O 对抗证人（第三条经加固后八线可证伪）、文书四笔（`T3` 消歧 / 节名去「两侧」/ M0 spec 挂 CMake 下限勘误注 /
README 补 vcpkg 下载兜底）。八线 337 × 4 与 336 × 4、tidy 四线三判据全过，见两张表。**D125 与 D147 经判不属于本波**
（前置均为设计决策，原位）。**CI/CD 由需求方 2026-10-03 裁定永久不做**——自 M5 记名欠账划入 wiki §13.4「明确不做」，
M5 欠账自此余**两件**（D125、VasePack）——五件里 `plan`+`doctor` 与 macOS 腿已还、CI 裁掉、
「VasePack + C4251」那件的 **C4251 半边销账**（见下），只剩 VasePack 本体。
**不对外分发 Vase（需求方 2026-10-03 裁定，全口径）**：插件永远由本仓库的构建产出、住在同一棵
构建树内——「发布」仍指 VasePack 组合库随产品走。连带销掉 **`/wd4251` 那笔债**（树外不再有插件
作者面，29 处 STL 成员没有受害者）、把「外部工具链构建的插件可能 `throw`」从**契约束降为机制保证**
（树内插件 `throw` 编译期就编不过）、§8.4 的 C ABI 启用条件**前两条**标注**永不触发**（只剩「跨语言」）——三处账目见
`wiki/vase-architecture.md` §13.3/§13.4。**VasePack 本身照旧**（发布期组合、加载、LTO、iOS `.a`）。
**D147 与「Windows 两编译器不得混用」两条裁定（需求方 2026-10-03）**：`doctor` 的「工具链一致性（编译器 / 版本 / CRT）」细读**不做**——划入 wiki §13.4「明确不做」，M6 记名的 D147 一笔自此销账（提案 ① 项已兑现为**身份特征在场**，覆盖「有后果可判」的那一半；剩余子项三平台无可靠读面、且失败形态在树内不可达）；Windows 上 `cl` 与 `clang-cl` **不得在同一构建树内混用**（要么全 `cl`、要么全 `clang-cl`，订正入 wiki §8.5 三处早先「同平台 + 同 STL」的写法）。**纯文书改动**：零代码、零公开面增量，八线全绿结论不受影响。欠账至此余 **VasePack 一件**（D125 于本次插件产物布局波以「拷贝」形态结案，见下笔）。
**插件产物布局波（2026-10-03，分支 `plugin-artifact-layout`，spec D180–D195，一步到位）**：全仓 44 个插件目标改为一插件一目录——`vase_add_plugin_fixture`（规矩 5 的唯一出口）给 target 设 `RUNTIME/LIBRARY_OUTPUT_DIRECTORY = <产物根>/<target 名>`，宿主可执行与框架库仍在 `bin/`(win)/`lib/`(posix) 平铺、`Versioned` 四兄弟维持各自 stage 目录（D194）；新增可选 `MANIFEST <path>` 参数，POST_BUILD 把清单**拷贝**进 `$<TARGET_FILE_DIR>/plugin.json`（D186）；三份描述 Samples 插件的清单自 `Tests/Integration/fixtures/manifests/` **搬家**至 `Samples/<X>/plugin.json`（D182，原地剩 3 份 fixture 清单不动）。承重理由改准：由「必须同处一目录」换成「按平台的依赖解析契约」——Windows 靠「加载器与被依赖框架库同进程」、POSIX 靠链接期烘入的 `RUNPATH`/`LC_RPATH`（D187，三平台各实测；该加载器 flag 同时升为承重条款 D195）；规矩 6 增第六类触发「产物布局」（D191）。新增两处证人（D189）：gtest `Tests/Unit/ArtifactLayoutTests.cpp` 三条 + ctest 级 `VaseCliValidateProductRoot`（指着产物根跑 `validate`、rc=0，D128 缺清单只 warn 不拖 rc）——**收口波八线实测：八线 debug 341 × 4 / release 340 × 4**（自 337/336 净 +4），平台差 0、`release − debug` 恰为 M1 T3 那条 death test，八线用例名集合 debug 四条与 release 四条各两两全等（此前只重建 `win-x64-clang-debug` 一棵树、其余按不变量推得的半句至此由实测兑现）。**D125 以「拷贝」形态兑现并结案**（spec §5：构建后置步骤自此就是这条 POST_BUILD 拷贝，`scan` 不接后置步骤）——M5 记名欠账自此余 **VasePack 一件**。**收口波 tidy 四线复跑即全绿**：TU 117 与基数对上、三判据全过、正文双 0（**首跑**曾捕得正文各 3 条 `readability-trailing-comma`，落本波 01115a8 改写的 `Tests/Integration/AssemblyFromSolveTests.cpp:69,72,75`——多行指定初始化尾缺逗号；收口波内补逗号修复后复跑归零，见「静态检查与格式」两张表）。
当前仓库里有什么：

- **构建系统已建立**：`CMakeLists.txt`、`CMakePresets.json`、`Cmake/Toolchains/`（四个工具链文件，macOS 腿起）、
  `Cmake/Triplets/`（两件：`x64-linux-libcxx` / `x64-osx-libcxx`），依赖经 vcpkg manifest 模式拉取。
- **三个动态库 target**：`VasePod`、`VaseHost`（后者链前者）与 `VaseCatalog`（链 `VaseHost`，
  D45 链接方向）——「插件不依赖 Host」（D11）是链接期事实，不是约定。插件 target 一律经
  `Cmake/VasePluginHelpers.cmake` 立（见规矩 5）。
- **架构代码已在**（`Include/Vase/` + `Source/{Pod,Host,Catalog}`）：描述符宏/基类与 `HeaderVersion`、
  `Result<T>` / `Error`、效果与作用域（`IEffect` / `EffectScope` / `ScopePool`）、
  服务注册表与事件总线、依赖账本（最小形）、`Pod` / `Context`、`PluginHost`、
  `AdoptPlugin` / `EjectPlugin` 与三档卸载证据、`Loader` 与自写的 PE/ELF/Mach-O 镜像解析（macOS 腿，文件清单见「尚未确定的事项」表目录行）；
  M2a 增**配置面**——`Include/Vase/Config/` 四头（`Value` / `FieldInfo` / `ConfigInfo` /
  `ConfigMacros`，header-only，无 Source 对称实体）+ 宿主拥有层 `ConfigBlob`、`LoadPlan` 定形
  （含 `BinaryPath` 与缺字段回退，D23）、装配预检与 Provides 碰撞执法（两态形，D27/D33）、
  `OnStart` 失败的递归拆除（闭包 + 逆数组序，D28）、`EjectReport` / `AdoptReport` 结构化（D20/D21）；
  M2b 波 1 增 **Catalog 层**——`Include/Vase/Catalog/` 五头（`ManifestView` / `Preset` /
  `LoadRequest` / `PluginCatalog` / `CatalogAdopt`，`Preset` 值形只含标量、清单 `config` 条目自波 2
  起可带 choices）+ `Source/Catalog` 四源（`ManifestJson` / `PresetJson` / `PluginCatalog` /
  `Solve`，另 `Detail/LibraryFileName`、`Detail/ChoiceCoerce.h`——label→value 的唯一转换点），
  **全仓唯一 JSON 消费者**（D55：nlohmann 不出现于任何公开头，也不出现在 `Source/Catalog/` 以外；
  解析面只到结构/语法 D59，类型核对在 `Solve`）；
  M2b 波 2 增**加载期执法**——`Include/Vase/Host/ManifestExpectation.h`（拥有值形期望，宿主面不过
  ABI 界）+ `Source/Host/Detail/ManifestCompare`（比对器与格式器，全字段严格等值 D72，
  `CreatePod` 带期望即比 / `AdoptPlugin` 必比），`CatalogAdopt.h` 的 `AdoptInto` 走单点重读
  （复用公开 `ParseManifestFile`，D81），`AdoptPlugin` 改收 `AdoptRequest`、`KnownBinaries` 退役
  （D69/D71）；配置面自波 2 起是**七型**（`enum` 入列，choices 表作者侧命名、`FieldInfo` 尾追加
  两槽、`kHeaderVersion` 2→3，D75/D76/D77）。
- **测试已建立**：GoogleTest 1.18.0（vcpkg manifest）+ `ctest` + `gtest_discover_tests`，
  分层落成 `Tests/{Smoke,Unit,Lifecycle,Integration,HotSwap,Abi}`（共用 `Tests/TestingSupport`），
  各线基数见「构建与测试」。M0 的跨 DLL 冒烟用例保留。M2a 新增测试文件 7 个
  （`Unit/{ConfigValueTests,ConfigMacroTests,ConfigBlobTests}`、
  `Integration/{MultiPluginAssemblyTests,ConfigApplyTests,RecursiveTeardownTests}`、
  `Lifecycle/MultiPluginCycleTests`），`Tests/Integration/fixtures/` 现共 24 个 fixture 插件（含波 2 的
  `TwoProvidersPlugin`；目录数盘上可核，非测试基数），
  其中 M2a 新增 16 个（Behind\* 五个、Cycle 一对、Dead 三个、StartFail / FailingEdge /
  SharedConsumer2 / Collision / ConfigConsumer / ConfigLayoutMisuse 各一）。M2b 波 1 新增测试文件 6 个
  （`Unit/{CatalogWiringSmoke,ManifestJsonTests,PresetJsonTests}`、
  `Integration/{SolveTests,CatalogScanTests,AssemblyFromSolveTests}`）、共用支撑
  `Tests/TestingSupport/CatalogSandbox.h`，另 `Tests/Integration/fixtures/manifests/` 下 3 个
  纯清单 fixture（复用既有 DLL，不新增插件二进制）。M2b 波 2 新增测试文件 2 个
  （`Integration/LoadTimeComparisonTests`、`HotSwap/AdoptManifestTests`）、共用支撑
  `Tests/TestingSupport/AdoptExpectations.h`（`AdoptRequest` 各厂），扩簇
  `Unit/{ConfigValue,ConfigMacro,ConfigBlob,ManifestJson,PresetJson,Descriptor}Tests`、
  `Integration/{Solve,CatalogScan,AssemblyFromSolve,ConfigApply,MultiPluginAssembly,RecursiveTeardown,FailureSemantics}Tests`、
  `HotSwap/{Adopt,HotSwapLoop}Tests` 与 `Abi/ImportEnforcementTests`，另 `TwoProvidersPlugin`
  fixture；原 `manifests/{dependent,failing}` 两个清单已随插件产物布局波（2026-10-03，D182）搬家至
  `Samples/{DependentPlugin,FailingPlugin}/plugin.json`（六线净 +54 的现值见「构建与测试」基数表）。
- **示例**：`Samples/{HelloCommon,HelloPlugin,HelloPluginPrime,DependentPlugin,FailingPlugin,Embedding}`——
  `VaseEmbedding play | loop <N> | swapdemo`，`HelloPluginPrime` 是换件材料（自波 2 起**必须与清单
  成对覆盖**，D87）。`DependentPlugin`（requires Greeter、经 `IFarewell` 提供 `Vase.Dependent.Farewell`，
  此名自 M5/D121——旧叙述行写的 `Vase.Farewell` 是改名前）与
  `FailingPlugin`（OnStart 返回 `Err`）是波 2 补的依赖链演示位与失败注入位（D86，均经
  `vase_add_plugin_fixture`）。Hello 两个插件自 M2a 起带**真实配置**（`VASE_CONFIG` 的 `Repeats`
  字段），`play` 打出的 `… x1 [安静]` 就是配置默认值走通的肉眼证人；波 2 起再加 `MoodValue`
  （`enum` 型，choices 安静/响亮），其肉眼证人是**两级**的：局内 adopt 后 label 仍跟建局清单走
  （adopted 实例按 D34 回放 plan 时 blob，换件腿打 `[安静]`、`prime v2` 半句证字节真换），
  拆局重建后新 plan 按 prime 清单铸 blob 才打 `[响亮]`（label 随清单与描述符**同步换**才放行比对）
  ——这既是「六型→七型」的展示位，也是加载期比对链的 Sample 侧证人，两级各有一枚自足证人
  （`…ReplayReason`/`…Reason`，逐字输出与状态链见 `wiki/vase-console-use.md` §7）。
- **工具**：`Tools/VaseConsole/`（target 与二进制同名）是热插拔验证台——
  `VaseConsole [--script <file>]`，不给 `--script` 即交互，退出码 = 全程是否 Clean。
  **别与架构 §11.1 的 `Tools/VaseCli` 混为一谈**：那是清单扫描 / 校验工具——§11.1 提议里它确实长期「不存在」，
  M5 第一波起 `scan` / `validate` 两个子命令实有（见下一条），M6 起 `plan` / `doctor` 亦实有（D141–D155）；CMake 后置步骤以「拷贝」形态兑现（`vase_add_plugin_fixture` 的 `MANIFEST` POST_BUILD 步把清单铺进产物目录，`scan` 不接后置步骤，D125 结案）；
  它与本仓库这个交互式验证台仍是**两回事**（一个批处理、一人读文本，一个交互、退出码语义也不同；
  更名与移位的经过见「尚未确定的事项」表）。
- **清单工具**：`Tools/VaseCli/`（target 与二进制同名）自 M6 起是四条腿——
  `VaseCli scan <目录>`（装载读 + 枚举面 + 清单保真回写，D117/D118/D122/D134）、
  `VaseCli validate <目录> [--host-provides <name>@<version>]…`（四项检查，D120/D127/D131/D132）、
  `VaseCli plan <插件目录> [presetFile] [--host-provides <name>@<version>]…`（零装载 Solve 预览，D141/D142/D145/D149）
  与 `VaseCli doctor <插件目录>`（收窄四项：① 磁盘读身份、② 装载读版号（收集序置后 D152）、③ 目录写探针、④ 平台锁探针，D143/D144）；
  **四命令共享同一套退出码三档 0/1/2**，零个插件的树都算用法/环境错（D135 扩到四命令，同一律），用法细节以磁盘上的 `Tools/VaseCli/` 为准。
  形态：内部静态库 `VaseCliCore` + 薄 `main`（D124——检查逻辑住库、gtest 直接链它，`main` 只分发 argv），
  argv 手写**不接** `ThirdParty/cli`、只出人读文本（D137）。它带动的新公开面住库里而非工具里：
  `Include/Vase/Host/Inspect.h` 的 `InspectDescriptors`（枚举读 + 版本闸与「按 Id 读」共用，D123）、
  从 `Source/Host/Detail/` 提升为 VaseHost 公开面的 `CompareDescriptor`（§4.4「同规则」自此由代码承载，D123）、
  `Include/Vase/Catalog/LibraryFileName.h` 的正反两函数 `LibraryFileName` / `LibraryStem`（round-trip 钉住，D129）
  与清单序列化面 `WriteManifestFile`（D122/D134）。`VASE_PLUGIN` 的展开物自此是**四个符号**——工厂、
  唯一命名描述符、统一入口、枚举入口 `VasePlugin_Descriptors`（纯追加，`kHeaderVersion` 不动）。
- **第三方源码依赖一个**：`ThirdParty/cli`（`MoozenSoft/cli` 的 **git submodule**，`daniele77/cli`
  的无异常无 asio fork，BSL-1.0）。它**不走 vcpkg**，接入方式见「构建与测试」的
  「第三方依赖有两条入口」那条。
- **不接 CI/CD**——需求方 2026-10-03 裁定**永久不做**，见 `wiki/vase-architecture.md` §13.4「明确不做」；
  八线删树重配全量、tidy 四线与 format 双判仍由三个开发机脚本手工跑。

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

- [`wiki/vase-console-use.md`](wiki/vase-console-use.md) —— **`Tools/VaseConsole` 的使用文档**：两种运行方式、命令一览（含 M2b 波 2 的双入口与 `catalog` 组、两条清单换件命令）、plan 格式（`pod new-raw` 旁路）、报告字段读法、退出码规则与回放资产清单。与上面那份不同，它描述的是**实有工具**，输出示例为**实测截取**（波 2 收口波自真实回放会话重取，2026-09-26）；命令与退出码行为以磁盘上的 `Tools/VaseConsole/` 为准，构建命令与测试基数仍以本文件为准（它不复制）。

- [`.claude/skills/vase-cpp-engineering/`](.claude/skills/vase-cpp-engineering/) —— **本仓库的 C++ 工程约束技能**：`SKILL.md` 给工程权重、五条不可违反、干活流程、提交前自查与「改什么必须验什么」矩阵；`references/` 按 architecture / ownership-lifetime / error-handling / abi-boundary / plugin-lifecycle / concurrency / performance / portability / verification 分面，另附一份通用 C++20 写法参考。设计、实现或评审任何 C++ 之前先读它。

  **与本文件的关系**：本文件是命令、构建规矩与测试基数的**唯一真值来源**。技能该给的是「什么边界不能跨、该读哪一份、某类改动必须验到哪一档」这一层判断，而不是把本文件的话在远处再说一遍——门禁阈值抄进第二处必腐：M2 每加一条用例，本文件改、技能不改，拿技能当准的人会算出「少了几条」，把健康的构建判成漏注册。

  **债已收（2026-09-23 复测）**：基数与差值算式、preset 名、命令块、工具链文件清单、HotSwap 选择子字符串都已撤出技能（首轮评测实测到的「把数字递到手上，回答只会复述结论」那个后果，其教训现记在 `references/verification.md` §2）。技能里残留的字面值命中规矩 7 的**允许两类**——嵌在论述里的承重 flag 与配置值（各处已带「字面值以本文件为准」的就地指针），以及允许两边各写一份的判据与理由。跑「在这个仓库里干活要知道的规矩」末尾那条自检 grep，预期命中的就是这一合规形态；此后它每命中一处，仍要逐条判类别。冲突时一律**以本文件为准，并回头修正技能**。

  接入方式：Claude Code 从项目级 `.claude/skills/` **自动发现**它，靠 frontmatter 的 `description` 触发，不需要在 `settings.json` 里开任何东西；主会话与 `Agent` 工具起的子代理都会拿到（已实测）。同一个约束在本仓库可能有**三种载体**：源文件自己的头部注释、本文件、技能。**越靠近改动现场越不易腐**——`PluginDescriptor.h` 的 POD 纪律、`Context.cpp` 落账本边那一段，都属于第一类，它们不需要、也不应该在别处再写一遍。

### 尚未确定的事项


下表逐项对照「架构文档怎么说的」与「仓库里实际有什么」。**凡「仓库里的状态」仍写着「无」的，都属未定，请先询问而不是假设**——架构文档里「有决定或提案」不等于「有实现」：

| 项 | 架构文档里的状态 | 仓库里的状态 |
|---|---|---|
| 构建系统 | 已决定：CMake + vcpkg（13.2） | **已建立**：`CMakeLists.txt`、`CMakePresets.json`、`Cmake/Toolchains/`（4 个）、`Cmake/Triplets/`（2 件）、`Cmake/VasePluginHelpers.cmake`，八个 preset 全绿（见「构建与测试」） |
| 目录布局与模块划分 | **已确认**（第 10 节） | **磁盘上是这些**：`Include/Vase/`（`Plugin.h` / `PluginDescriptor.h` / `Config` / `Catalog` / `Effect` / `Service` / `Event` / `Pod` / `Host` / `Detail`）、`Source/{Pod,Host,Catalog}` 三 target（`Source/Host/` 的镜像解析自 macOS 腿起四件：`ImageInspectMachO.cpp` **无条件编译**——八线合成就链它（D161）；平台桥三件 `ImageInspect{Windows,Linux,Darwin}.cpp`，其中 `Linux` 由 `Posix` 更名（D162）；共用字节助手 `Detail/ImageBytes.h`）、`Samples/{HelloCommon,HelloPlugin,HelloPluginPrime,DependentPlugin,FailingPlugin,Embedding}`、`Tools/{VaseConsole,VaseCli}/`、`Tests/{Smoke,Unit,Lifecycle,Integration,HotSwap,Abi,TestingSupport}`、`ThirdParty/cli`；第 10 节里此前未出现的 `Catalog/` 实体已于 M2b 波 1 立起（清单解析面住在 `Source/Catalog`，`Host/` 下不另立）。**但别把这一格读成"§10 是当前状态的描述"**：§10 的 `Samples/` 子树那两格空位（`DependentPlugin` / `FailingPlugin`）自 M2b 波 2 起**提案已成真**（D86，该树随之标注），其时漏列的实有两目录（`HelloCommon` / `HelloPluginPrime`）亦已随勘误补进树；它提议的 `Tools/VaseCli/`（清单扫描、校验、索引生成）自 M5 第一波起 **scan / validate 已实有**，`plan` / `doctor` 亦于 M6 落地（2026-09-30，D141–D155）（索引生成仍未接；CMake 后置步骤以「拷贝」形态兑现——`vase_add_plugin_fixture` 的 `MANIFEST` POST_BUILD 步把清单铺进产物目录，`scan` 不接，D125 结案）——它与实有的 `Tools/VaseConsole/`（交互式验证台）**不是一回事**——两者同在 `Tools/` 下、名字只差一个词，别混；后者原名 `Samples/VaseCli`，2026-09-23 因「CLI 这个名字不体现它是交互式验证台」而更名并移出 `Samples/`，同一轮的 spec / plan 在 `docs/superpowers/` 下同步改名为 `vase-console-*`。**改 §10 本身要需求方过目**（它标着"已确认"），不要顺手修 |
| 测试框架与运行方式 | 有分层与验证策略（12 节） | **已落成**：GoogleTest 1.18.0（vcpkg manifest）+ `ctest` + `gtest_discover_tests`；分层即 12.2 那五类（Unit / Integration / Lifecycle / HotSwap / Abi，外加 M0 的 Smoke），各线基数见「构建与测试」的按线分账表。12 节里依赖 M2 起的部分：Catalog 求解链已于 M2b 波 1 落成、加载期比对与 Adopt 单轨已于波 2 落成（判据 13 自此有现形证人）；**判据 3b 亦已落地**（凭声明执法 + 解析落账在 `Source/Pod/Context.cpp` 的 `ResolveRaw`，Eject 反查与拒绝点名消费者在 `PluginHost::EjectPlugin`，证人 `Tests/Integration/LedgerSemanticsTests.cpp` 含未声明解析的 death test；D20 的「薄面」是刻意收窄外向 API，不是欠账）——本行原先记的「账本 3b 完整执法仍未起」与磁盘不符，2026-09-26 更正。§12 判据表的 M3 两项亦已落：**3a 全量形**——三插件局 50 轮 `HotSwap.FiftyRoundsBehaveLikeFirstTime`（新 fixture `NeighborC`，每轮派拍、循环内断两只邻居累计 `Beats()`）；**3d 进程级状态登记**——`Eject.ProcessStatesResetMakesReloadLikeFirstTime` 与 `Eject.ProcessStatesKeptWhenOtherPodHoldsLiveInstance`（`Evidence.h` 的 `ProcessStatesReset` 自此有内容，不再恒空），空壳可拆证人 `RecursiveTeardown.TornShellCanBeEjectedAndThenReAdopted`（D94）。属主追踪器经核查否决（D97）。**M4 新增判据 3f 亦已落**（§12 判据表：Eject 如实上报「语义依赖可能已陈旧」而不宣称查过）——三条证人 `Eject.SemanticDependency{PossibleWhenResetAndOthersLive, AbsentWhenNothingElseLive, AbsentWithoutProcessStateReset}` 的真假两侧都钉住；**空壳局一支无证人可写**（空壳攥不住值、条件本就不该置位），如实标注而非跳过。判据表至此无未起项（判据 19 按设计即「无法自动化验证」的契约束，维持原判） |
| 插件接口 / ABI 约定 | 有完整设计（第 3、8 节） | **已落地最小形**：§3.1 的宏（`VASE_PLUGIN`）、插件基类、描述符结构体与 `kHeaderVersion` 都在 M1；M2a 补配置面（`VASE_CONFIG`、`ConfigInfo`/`ConfigBlob`，其时为六型）；M2b 波 1 已落 **清单/`Preset` 格式与 `Catalog` 解析**（`Source/Catalog`，结构/语法段 D59 + Solve 两段）；M2b 波 2 已落 **`enum`（七型）与 `kHeaderVersion` 2→3**（`FieldInfo` 尾追加 choices 两槽，D75/D76）、**加载期全字段比对**与 **`AdoptRequest` 单轨**（D67/D69/D72）；M3 已落 **`ProcessStateDesc{Name, Reset}` 进 `PluginMeta` 尾槽 `ProcessStates` 与 `kHeaderVersion` 3→4**（D92），连带清单顶层 `processStates` 名字数组的双向等值比对（D93）。接口以磁盘上的 `Include/Vase/Plugin.h` 与 `PluginDescriptor.h` 为准，第 3、8 节其余部分仍是提案 |

**不要从文档推断出可用的命令、路径或接口签名**——文档写的是「打算怎么做」，本文件负责说明「现在有什么」。
## 二、怎么跑

八个 preset 的命令、按线基数、产物落位、第三方依赖的两条入口，与两条承重的工具链 flag（macOS 无对位 flag，见「工具链 flag 是承重的」）。

### 构建与测试


**八个 preset，全部已实测可用**。基数的最近一次复核（**插件产物布局波，2026-10-03**）：八条 preset 线各自删树重配
→ configure → build → ctest → `ctest -N`，**八线全绿、构建零警告**（`Scripts/win-verify.cmd` 与
`Scripts/linux-verify.sh` 两脚本并行、`Scripts/macos-verify.sh` 在 macOS 开发机上跑）。本波用例净 +4
（gtest `Tests/Unit/ArtifactLayoutTests.cpp` 三条 + ctest 级 `VaseCliValidateProductRoot` 一条，八线同幅、
无 debug 门无平台门）——八线 debug **341 × 4** / release **340 × 4**、平台差 0、`release − debug` 仍恰为
M1 T3 那枚 death test 一条；八线逐对比过，**debug 四条线名集合两两全等、四条 release 线亦两两全等**。
**该轮不是「零重跑」**：Linux debug 线首跑 `ctest` 返回 RC=8 且**正文零测试输出**（只有 `Test project` 一行，
ctest 没跑到任何一条），按「先重跑再判」处置——单线删树重跑一次即 341/341 全绿。**macOS 两条线首跑亦未成**：
SMB 挂载下 `rm -rf build-macos/…` 撞瞬时 `Resource busy`（`bin/` 下四个可执行 + `CMakeFiles`）、configure 在删不干净的
树上倒下，`pkill` 复核确无进程持有后重试 `rm -rf` 成功、预清两棵树重跑方绿。**首跑即绿的只有 Windows 四条与
Linux release 一条，其余三条均系重跑读数**。
**本波 tidy 四线复跑即全绿**（TU 117、三判据全过、正文双 0；**首跑**曾捕得正文各 3 条
`readability-trailing-comma`、收口波内补逗号修复，见「静态检查与格式」；macOS RC 口径另有勘误一笔，见那里）。
**小账清理波（2026-10-03）保留为历史**：八线删树重配全量、八线全绿、构建零警告、零重跑；本波用例净 +3
（三枚 Mach-O 对抗证人，八线同幅、无 debug 门无平台门）——八线 debug 337 × 4 / release 336 × 4、平台差 0、
`release − debug` 恰为 M1 T3 那枚 death test、debug 四条与 release 四条名集合两两全等。
**macOS 腿 T10（2026-10-02）保留为历史**：其时八条 preset 线各自删树重配 → configure → build → ctest →
`ctest -N`，八线全绿、构建零警告（`Scripts/win-verify.cmd` 与 `Scripts/linux-verify.sh` 两脚本并行一次跑完、
`Scripts/macos-verify.sh` 在 macOS 开发机上跑——见下 macOS 块的脚注；**该轮撞到那类 `z-applocal` 文件锁假红
（第 5 次）**：`win-x64-clang-debug`、目标 `BehindFarPlugin.dll`，按协议单线删树重跑一次即归零，其余七线首跑
即绿，见该段）。该波用例净 +3（macOS 腿 T3 的 Mach-O 合成字节证人，八线同幅）——八线 debug 334 × 4 /
release 333 × 4、平台差 0、release − debug 恰为 M1 T3 那枚 death test 一条；T10 收口轮本身零用例增减，
计数与 T9 落账一致（差集复测见下）。
**M6 终审修复波 T6（2026-09-30）保留为历史**：其时六条 preset 线（macOS 两线尚未存在）各自删树重配全量、
六线全绿、构建零警告、`z-applocal` 假红零撞零重跑；本波代码是终审三条修复（`Plan.cpp` notes 环补 Cause 支、
`Doctor.cpp` ④ 缺件 absent info 支、证人 `VaseCliPlan.ProviderSkippedNoteCarriesCauseAttribution` 一枚），
六线各净 +1（331/330）即这枚证人——无 `#ifndef NDEBUG` 门、无平台门，两平台各线都注册。
**M6 收口（2026-09-30）保留为历史**：同为六线删树重配全量但跑了**两轮**——第一轮在 T1–T4 代码上即全绿，
随后三线 tidy 全量首跑捕得正文 warning 10 条（全部落本波新代码），代码级修复后第二轮再删树重配全量；
两轮都零撞、每一轮都没有重跑。
**M5 波末还账（2026-09-28）保留为历史**：同为六线删树重配全量、同样零撞；其前的 **M5 收口（2026-09-28）**
亦零撞（见该段末条）。
tidy 四条 debug 线各自跑（见「静态检查与格式」；**macOS 腿首跑捕得 21 条正文 warning、全部代码级出路、复跑四线正文双 0**；M6 T6 时三线全量**首跑即三判据全过、正文双 0**——终审修复未带入新正文 warning），format 一条命令（M6 T6 时首跑捕得新代码一行超长注释，折行后重跑 RC=0；**macOS 腿另在 macOS 侧以 `clang-format-mp-23` 复核同一条命令，RC=0 逐字同判**）。
**M4 收口（2026-09-27）保留为历史**：同为六线删树重配全量，Windows 侧 `win-x64-msvc-release` 首跑撞到
该类假红（目标 `VaseEmbedding.exe`，链接已成功、`vcpkg z-applocal` 拷贝步撞锁致 build 步 RC=1，
连带该树 ctest 80 条红；按协议单线删树重跑一次即 266/266 全绿零警告）。更早两段：M3 终审修复波走
增量重配复测、其前的收口复测走删树重配全量（另一棵 `win-x64-clang-release` 撞到同一类假红，
目标 `NeighborC.dll`，单线重跑即绿）。

| 平台 | preset |
|---|---|
| Windows / clang-cl | `win-x64-clang-debug`、`win-x64-clang-release` |
| Windows / cl.exe | `win-x64-msvc-debug`、`win-x64-msvc-release` |
| Linux / clang + libc++ | `linux-x64-clang-debug`、`linux-x64-clang-release` |
| macOS x64 / clang + libc++（MacPorts `clang++-mp-23`） | `macos-x64-clang-debug`、`macos-x64-clang-release` |

**各线 `ctest -N` 基数**（八线现值为 **插件产物布局波 2026-10-03 实测**，八条 preset 线删树重配全量、`ctest` 与
`ctest -N` 一并跑完；相对小账清理波表 337/336 **八线各 +4 = 三枚 gtest**（`Tests/Unit/ArtifactLayoutTests.cpp`
的 `ArtifactLayout.*`）**+ 一条 ctest 级**（`VaseCliValidateProductRoot`，指着产物根跑 `validate`），
八线同幅、无 debug 门无平台门）——八线 debug 341 / release 340；历史口径——小账清理波 2026-10-03 表 337/336 =
相对 macOS 腿 T10 表 334/333 **八线各 +3 = 三枚 Mach-O 对抗证人**
（`ImageInspect.MachO{RejectsGarbageBytes,UsesFirstUuidCommand,SkipsUndersizedDylibCommand}`，同波新加，
八线同幅、无 debug 门无平台门）；macOS 腿 T10 表 334/333 =
相对 M6 终审 T6 表 331/330/331/330/331/330 **六线各 +3 = macOS 腿 T3 的三枚 Mach-O 合成字节证人**
（`ImageInspect.MachO{UuidExtracted,DylibNamesListed,WithoutUuidFailsLouder}`，D173，八线同幅），
macOS 两线自注册起即同值 334/333；其时六线现值 331/330 = 相对 M6 收口表 330/329/330/329/330/329 六线各 +1 = T6 证人）：
M6 相对 M5 波末表（305/304/305/304/305/304）**六线各净 +26**，两笔来源——**gtest 用例六线同幅 +20**
（`VaseCliPlanTests` 12 + `VaseCliDoctorTests` 8：T3 共享 6 + T4 平台证人 2，后两条系**体内 `#ifdef`**、
两平台各注册各的同名一条，计数对称，见规矩 6 那段的判据不对称与本波偏离登记；plan 的 12 含 T6
终审证人 `ProviderSkippedNoteCarriesCauseAttribution`）
**+ ctest 级 +6**（`Tools/VaseCli/CMakeLists.txt` 的用法腿 `VaseCliPlanNoArgs{IsUsageError,Reason}` 与
`VaseCliDoctorNoArgs/TooManyArgs{IsUsageError,Reason}` 四腿，D142/D144）；26 条无一受 `#ifndef NDEBUG` 门或
平台门（差集复测见下）。历史口径——M5 波末还账相对 M5 收口表（303/302/303/302/303/302）**六线各净 +2**
（`Adopt.ProbeSwapGuardHealsAbortResidue`（gtest，HotSwap 目录里那条守卫的自愈证人）+1 与 `EmbeddingSwapDemo`
（ctest 级，前端字段集守卫）+1，两条均无 `#ifndef NDEBUG` 门、无平台门）；
M5 相对 M4 收口表（267/266/267/266/267/266）**六线各净 +36**，两笔来源——**gtest 用例六线同幅 +32**
（按文件：`VaseCliScanTests` +9、`VaseCliValidateTests` +11、`ManifestJsonTests` +5、`InspectTests` +3、
`LibraryFileNameTests` +2、`DescriptorTests` +1、`EjectTests` +1——最后一枚是 T14 跨 Pod 证人
`Eject.SemanticDependencyPossibleAcrossPods`）**+ ctest 级 +4**（`Tools/VaseCli/CMakeLists.txt` 的用法腿
`VaseCliNoArgs{IsUsageError,Reason}` 与 `VaseCliUnknownSubcommand{IsUsageError,Reason}`，D135）；
按 `git diff main..HEAD -- Tests/` 的 TEST 宏增减独立核过，新增用例无一受 `#ifndef NDEBUG` 门或平台门
（全 diff 唯一条件编译在 `LibraryFileName.RejectsForeignAndMalformedNames` **用例体内**，两平台各注册各的一条）。
历史口径：M4 相对 M3 终审修复波表（261/260/263/262）Windows 各线净 +6、Linux 各线净 +4——新增用例六线同幅 +4
（阶梯 `HotSwap.DescriptorDriftLadderSwapsBothWays` 1 条 + 知情位 3 条
`Eject.SemanticDependency{PossibleWhenResetAndOthersLive, AbsentWhenNothingElseLive, AbsentWithoutProcessStateReset}`），
加上档三两条由 Linux-only 转六线同跑（`Adopt.MissingIdentityFeatureRejectedWithPointer`、
`Adopt.RenameReplacementCaughtByTierThree`——Windows 各线 +2、Linux 各线 +0，那两条本来就在
Linux 的计数里，D109）；M3 相对波 2 收口表（249/248/251/250）各线净 +12（T1 +1、T2 +3、T3 +2、T4 +2、T5 +2，
T6 Five→Fifty 与 T7 空壳用例替换均 1:1 改写净 0，M3 终审修复波 +2 为两枚并存证人）；波 2 相对波 1
（194/193/196/195）各线净 +54（枚举 `enum` 七型、加载期比对/Adopt 单轨、Catalog 求解扩簇、Console
双入口与各证人族；其中含收口波 T14a 退役孤儿 `VaseConsoleSwapStage` 的 −1 与 T14c synced 族自足化
新增 `…ReplayReason` 证人的 +1），同日还账波再加账① Unit 用例 1 条、六线同幅 +1
（账② 零用例）；
`ctest` 在没发现测试时同样返回 0，故基数要按线单独记，见「四、怎么验」下的「核这些门禁时，退出码单独用是不够的」）：

| preset | `Total Tests` | 与 debug 基线的差 |
|---|---|---|
| `win-x64-{clang,msvc}-debug`、`linux-x64-clang-debug`、`macos-x64-clang-debug` | 341 | —（四条 debug 线零差） |
| `win-x64-{clang,msvc}-release`、`linux-x64-clang-release`、`macos-x64-clang-release` | 340 | −1：M1 T3 的 death test（`EffectScopeDeath.CreateAfterDisposeTerminates`）受 `#ifndef NDEBUG` 门——四条 release 线恰同这一条，**平台差 0 自 macOS 入场后维持**（M4 的 `linux − win` 归零形状见下，macOS 未引入新差集，spec §5 收口形态） |

**基数差是设计，不是漏注册**：**macOS 入场未引入任何新差集**——macOS 腿全部用例增量（macOS 腿 T3 的合成字节三枚）与
平台无关（D173 的设计点），体内 `#ifdef` 的两枚平台证人自 macOS 腿起在三条非 Windows 线上各注册各的同名一条；
**`linux − win` 的 +2 差值在 M4 归零**——T11 那两条档三用例自 M4/D109 起
**两平台同跑**：fixture `NoBuildIdPlugin` 跨平台化并更名 `NoIdentityPlugin`（Windows 侧 per-target
`/DEBUG:NONE` 摘掉 CodeView，Linux 侧维持 `-Wl,--build-id=none` 摘掉 `.note.gnu.build-id`，**两平台各摘
各的身份特征**；macOS 腿起转**三平台同跑**，macOS 侧 `-Wl,-no_uuid` 摘掉 `LC_UUID`——macOS 无对位承重
flag，该负例因此反向把守它的在场，D167）。**这一格不是「洞被填上」，是刻意的平台不对称被消掉**——D109 的理由正在于此，
所以差值变小是**设计**而非计数异常。debug 与 release 差的 1 条**不变**，仍是 M1 T3 的 death test
（`#ifndef NDEBUG`）——M2a 新增的 `ConfigApply.LayoutMismatchTerminates` 同为 death test 但**不带**
这道门，debug/release 同计（两条 release 线实测全绿），故 −1 不因它而变。M4 新增的净 4 条、那两条
改平台的用例、M5 新增的净 36 条（含 4 条 ctest 级）、M5 波末还账新增的 2 条、**M6 新增的净 26 条
（gtest 20 + ctest 级 6，含 T6 证人）**、**macOS 腿新增的净 3 条（合成字节证人，八线同幅）**、
**小账清理波新增的净 3 条（Mach-O 对抗证人，八线同幅）**与
**插件产物布局波新增的净 4 条（gtest 3 + ctest 级 1，八线同幅）**
**无一受** `#ifndef NDEBUG` 门或平台门——M6 的 `VaseCliDoctor` 里两条体内 `#ifdef` 的平台证人
各平台注册各的同名一条，计数对称（Windows 上其中一条按首测协议走 `GTEST_SKIP`——**SKIP 是运行时行为，
不动 `-N` 注册计数**；macOS 腿起「两侧」是三条非 Windows 线——macOS 上 ③ 的 chmod-555 支为**真实断言**、
④ 打 not-observable info 行，spec §4 要求的这两处 macOS 实测确认随 T9 通过）。
两侧都与预期值逐位对上（用例名集合的**差集本轮实测**——**macOS 腿 T10 2026-10-02 八线逐对比过**：
win-clang ↔ win-msvc 两对、linux ↔ win、macos ↔ win、linux ↔ macos 各双向，**debug 四线与 release 四线
任意方向的差集全空**；`debug − release` 在四条 debug 线上恰为 M1 T3 那一条。M6 终审修复波 T6 2026-09-30 六线逐对比过，
且新证人六线六条注册线全部在内：
debug − release 在三条 debug 线上恰为 M1 T3 那一条、`linux − win` 与 `win − linux` **两个方向都为空**、
clang-debug 与 msvc-debug 名集合逐条相同、两条 release 线的名集合也逐条相同；M6 收口 / M5 波末还账 / M5 收口与更早各轮
（M4 收口、M3 收口/修复波三轮、波 2 收口/还账波 2026-09-26）的同形复测保留为历史。
规矩 6 的选择子 `-R 'HotSwap|Eject|Adopt'` 在 **macOS 腿 T10 重测 win 58 / linux 58 / macOS 58（macOS 首次入账，三平台同值）**、**小账清理波 2026-10-03 三线复测仍为 58 / 58 / 58（该族逐条实跑 58/58 全绿）**、**插件产物布局波 2026-10-03 复算仍为 58**（按 `ctest -N` 名集合导出复算：四线各 57 个名匹配 + ctest 自动拉入的 fixture `VaseConsoleCatalogStage` = 58，win 线 `ctest -R … -N` 实测确认；八线全量 `ctest` 逐条跑过族内全部用例并通过）——与 M6 终审修复波 T6 / M6 收口 / M5 波末同数：
M6 的新命令名 `plan` / `doctor`、T6 证人名与 macOS 腿新用例名（`ImageInspect.MachO*`）都不含选择子串；该枚相对 M5 收口 57 / 57 的增量是波末还账的
`Adopt.ProbeSwapGuardHealsAbortResidue`（名含 `Adopt`；再往前较 M4 的
56 / 56 那一枚是 T14 跨 Pod 证人 `Eject.SemanticDependencyPossibleAcrossPods`，
名含 `Eject`、无 debug 门无平台门，两侧之差依旧为零；较 M3 终审的 50 / 52：+4 是新建用例名含
HotSwap / Eject、+2 是档三那两条不再 Linux-only——两侧之差随之消失），
且八线全量 `ctest` 本就逐条跑过族内全部用例并通过，说明 `gtest_discover_tests` 没有静默漏掉任何一条；
**doctor② 是 Loader 的新调用方（规矩 6 触发条件之一），该族八线全绿自此有调用面回归的证据**。
console 套件（`ctest -R VaseConsole` 六线均 33 条，自 M3 起未变）、M5 新族自 M6 扩面
（`ctest -R '^VaseCli'` 六线均 **50 条** = gtest 用例 40——`VaseCliScan` 9 + `VaseCliValidate` 11 +
`VaseCliPlan` 12 + `VaseCliDoctor` 8——加 `Tools/VaseCli` 的 ctest 级 10 条：既有 4 + plan 2 + doctor 4）
与 Embedding 回放（现 2 条：`EmbeddingLoop20` + `EmbeddingSwapDemo`，macOS 腿未动）
**不含** `#ifndef NDEBUG` 门与平台门，
所以上面的差值不因它们而变；macOS 两线读数系由八线差集全空推得（名集合与既有线全等），T10 未单独点验这两族计数。

**这一档有环境性假红**（2026-09-22 实测）：`win-x64-clang-release` 的首次删树重配在一个
`vcpkg z-applocal` 步上撞到 `The process cannot access the file ... being used by another process`
（目标 `FailingLoadPlugin.dll`），构建中止、连带 4 条 HotSwap / Abi 用例因缺 DLL 而红；同一条线
立即重跑即 94/94 全绿。是 Windows 文件锁层面的抖动，不是仓库缺陷——**但它是重跑才显形的那一类**，
所以删树全量的退出码非 0 时，先重跑那一条线再判。
M2a 收口波（2026-09-24）同一位置再撞过一次（目标 `VersionedA.dll`），单线删树重跑即 138/138 全绿；
M2b 终审修复波复测两轮全量与波 2 收口波复测（2026-09-26）均未再撞。
M3 收口复测（2026-09-27，跑在 83bd297）再撞一次：`win-x64-clang-release` 的 `NeighborC.dll` 链接已成功、
其后 `z-applocal` 拷贝步撞锁致 build 步 RC=1（该树 ctest 当时也 258 全过，仍按协议单线删树重跑一次，
即 258/258 全绿零警告）。
**M4 收口（2026-09-27）同一类步上再撞一次，且第一次落到另一条线**：这次是 `win-x64-msvc-release`
（此前三次都在 clang-release 侧），`VaseEmbedding.exe` 链接已成功、`z-applocal` 拷贝步撞锁致 build 步
RC=1，构建立即中止（后续 target 未产出），该树 ctest 连带 **80 条红**（Not Run 为主，缺的可执行与 DLL
是中止的下游）；按协议**单线删树重跑一次**即 266/266 全绿零警告。**读这一类红时先看是不是缺产物**——
「80 条红」看着像大面积回归，其实是缺文件。
**M5 收口（2026-09-28）未再撞**：六线删树重配全量两脚本并行单跑即全绿，零重跑（波末还账一轮亦零撞）。
**M6 收口（2026-09-30）未再撞**：本波全量跑了**两轮**（tidy 首跑捕得新代码正文 warning、代码级修复前后各一轮），
两轮都是两脚本并行单跑即全绿、零重跑。**M6 终审修复波 T6（2026-09-30）亦未再撞**：六线删树重配全量、
两脚本并行单跑即全绿、零重跑。
**macOS 腿 T10（2026-10-02）第 5 次再撞，落回老位置**：`win-x64-clang-debug` 的 `BehindFarPlugin.dll`
链接已成功、`z-applocal` 拷贝步 Access denied 致 build 步 RC=1（该树 ctest 当时 334/334 已全过——
「缺产物型红」的读法依旧成立）；按协议单线删树重跑一次即归零，其余七线（含 macOS 两条）首跑即绿。
**插件产物布局波（2026-10-03）没撞 Windows 那类锁，但撞到另两形**：**其一**，Linux debug 线首跑 `ctest` 返回 RC=8、
**正文零测试输出**（只有 `Test project` 一行，连一条 `Test #N` 都没有——ctest 压根没跑起来，不是「跑了但有红」），
同一条线 `ctest -N` 与 build 步都 RC=0；按「退出码非 0 先重跑再判」单线删树重跑一次即 341/341 全绿。**其二在 macOS**：
两条线首跑 `rm -rf build-macos/…` 撞瞬时 `Resource busy`（SMB 目录缓存的脾气，`bin/` 下四个可执行 + `CMakeFiles`），
configure 在删不干净的树上倒下；`pkill` 复核确无进程持有后重试 `rm -rf` 成功、预清两棵树重跑方绿——同属**预清理步**的
环境抖动，不是用例回归。**读法**：
这一类红与 Windows 的 `z-applocal` 锁同属环境抖动，**别当成用例回归**——真回归会给出 `Test #N ... Failed` 行，
而不是一片空白。

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
仍以上面那张为准（写进脚本就是第二处真值）。前提是 `VCPKG_ROOT` 已设。macOS 侧的
`Scripts/macos-verify.sh` 与 Linux 侧同模型（逐步骤打 RC 行、脚本退出码恒 0——**按日志的 RC 行判**，
别拿它当 `win-verify.cmd` 的失败步数退出码）。

构建产物落 `build-<平台>/<presetName>/`：**可执行在 `bin/`**（POSIX 侧框架库与静态库在 `lib/`），**每个插件独占 `<产物根>/<target 名>/`**（Windows `bin/`、Linux/macOS `lib/`）。依赖解析的依据：**Windows 靠「加载器与被依赖框架库同进程」**——`VaseHost.dll` 里跑的 `LoadLibraryExW` 带 `LOAD_WITH_ALTERED_SEARCH_PATH`，而插件需要的 `VasePod.dll` 早已被宿主静态导入、按已加载模块名命中（自证；取证注①）；**POSIX 靠链接期烘入的 `RUNPATH` / `LC_RPATH`**（指向框架库所在的 `lib/`；Linux 取证注③、macOS 取证注④ 实测同形）。改回平铺、改动 `vase_add_plugin_fixture` 的输出目录设置、或动 `LoaderWindows.cpp` 的 `LOAD_WITH_ALTERED_SEARCH_PATH`，必须按规矩 6 重跑三平台 `-R 'HotSwap|Eject|Adopt'` 族并各留证据。

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

#### macOS（在 macOS 开发机上跑）


前提：MacPorts 的 `/opt/local/bin` 在 PATH（`clang++-mp-23` / `cmake` / `ninja` / `pkg-config` /
`clang-format-mp-23` / `run-clang-tidy-mp-23` 都在那里）、`VCPKG_ROOT` 已设（macOS 上住 `~/.zshenv`）。
**`pkg-config` 是本平台的硬前提**——缺它 vcpkg 的 gtest port 无条件倒在 `vcpkg_fixup_pkgconfig`、整条腿
什么都编不出（MacPorts `pkgconf` / Homebrew `pkg-config`；`Scripts/macos-verify.sh` 起手检查存在性、缺了
响亮报错指名，D178——仓库不出 shim）。编译器是 MacPorts clang 而非 Apple clang（版本闸因此零改动，D158；
环境清单见 `README` 的 macOS 段）：

```bash
cmake --preset macos-x64-clang-debug
cmake --build --preset macos-x64-clang-debug
ctest --preset macos-x64-clang-debug      # 两条线同构：debug/release
```

构建产物落 `build-macos/<presetName>/`。删树重配全量是 `Scripts/macos-verify.sh`、tidy 是
`Scripts/macos-clang-tidy.sh`（见「静态检查与格式」）。

**macOS 开发机上的仓库路径是 `~/WindowsGit/Vase`**——它是 Windows 开发机的 `D:\Git` 经 SMB 挂载而来，
**同一份工作树、无同步步骤**（`.git` 也是同一份：Windows 侧提交后 Mac 侧立刻可见，Mac 的 `build-macos/`
落回共享树并被 ignore）。挂载自带的脾气：SMB 目录缓存可致陈旧快照读——写后立刻回读的文件枚举以重读为准。

> **脚注（D179）**：本仓库的 macOS 线通常由 macOS 开发机执行；从 Windows 开发机触发走 ssh——脚本化
> 调用**必须**带 `-o BatchMode=yes`（无 TTY 时它失败而不是挂住），主机别名见本机 ssh config（**不入库**）。
> 与 D6「clang 不写死安装路径」同取向：随仓库分发的文档不写机器专属值。

#### 第三方依赖有两条入口，gtest 与 nlohmann-json 走 vcpkg

- **要编译的依赖 → vcpkg manifest**（现在是 `gtest` 与 `nlohmann-json`）。`vcpkg.json` 的 `dependencies` 就是它的清单。
  nlohmann-json 虽是 header-only 库，仍走这条：它经 vcpkg 的 **CMake config 包**出口
  （`find_package(nlohmann_json)` 消费者，target 名 `nlohmann_json::nlohmann_json`）——与下面
  submodule + INTERFACE 那条 `ThirdParty/cli` 的区别就在这里，不在「要不要编译」。
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


四个工具链文件里有两条 flag **不是可选的优化，而是身份特征的构建要求**（§8.2 档三 / 13.2 末行）：

- **Windows 两条线**（`Cmake/Toolchains/windows-x64-clangcl.cmake`、`windows-x64-msvc.cmake`）：
  `CMAKE_SHARED_LINKER_FLAGS_INIT` 追加 **`/DEBUG:FULL`**——没有它就没有 PE 调试目录里的
  CodeView(RSDS)，`AdoptPlugin` 会直接拒绝该二进制（响亮的失败，不做静默降级）；
- **Linux**（`linux-x64-clang-libcxx.cmake`）：`CMAKE_SHARED_LINKER_FLAGS_INIT` 追加
  **`-Wl,--build-id=sha1`**——`.note.gnu.build-id` 是与 RSDS 对位的身份特征，档三在内存镜像
  与磁盘文件上比对的就是这段 desc；
- **macOS**（`macos-x64-clang-libcxx.cmake`）：**无对位 flag**（D159）——身份特征 `LC_UUID` 由
  ld64 **链接必写**、实测无需任何追加；这条空格不是漏写，承重性改由**负例反向把守**：
  `NoIdentityPlugin` 的 macOS 支用 `-Wl,-no_uuid` 主动摘掉它、档三必须拒（D167）。工具链文件里
  写明了这件事实及其后果，别去那里找该加的 flag。
- **另有一条承重 flag 不在工具链文件里、住源码**：`LOAD_WITH_ALTERED_SEARCH_PATH`（`Source/Host/LoaderWindows.cpp`，D195）。
  它与上面两条并列，但管的是**依赖解析**、不是身份特征，来源是源码而非工具链文件；下方「**这两条**」仍只指
  `/DEBUG:FULL` 与 `--build-id=sha1`。插件住 `bin/<名>/` 而框架库住 `bin/`，Windows 上这两者能对上，靠的是
  「**加载器与被依赖的框架库同进程**」——该 flag 把依赖搜索基改成被加载模块自己的目录，而 `VasePod.dll` 早已被
  宿主静态导入、按已加载模块名命中，根本没走文件系统搜索。摘掉这个 flag、或把 `Loader` 挪出 `VaseHost`，
  症状都是**插件加载全线失败**，而两个动作看起来都无害。

**摘掉它们的症状是运行期的响亮失败，不是编译失败**：构建照常全绿，直到 Adopt 全线拒绝才暴露。
所以摘除或改动这两条，**必须重跑 T12 主循环**（`-R HotSwap`，双平台各自留证据）——macOS 那条无
flag 可摘，它的同位纪律是**别摘负例**：改动 `LC_UUID` 解析或 `NoIdentityPlugin` 的 macOS 支，
同样按规矩 6 的选择子跑族并三平台各留证据。

**另有一条 configure 期的坑（T9 实测）**：`CMAKE_*_FLAGS_INIT` **只在工具链首次 configure 时
进入缓存**。工具链后来才加上 `/DEBUG:FULL`、而某棵树在那之前就配过，那棵树的
`CMAKE_SHARED_LINKER_FLAGS` 就是空的、`LoadProbe.dll` 没有 `.pdb`，
`Loader.MemoryIdentityMatchesFileIdentity` 当场失败。**修法是删掉那棵树重新 configure**，
不是用 `-D` 钉一个永久的手工 override——后者会把后续所有工具链改动一起遮住。
**macOS 上有两形同族坑（T8 实测，推导见 plan 偏离登记 5）**：① MacPorts 的 `clang++-mp-23` 是
wrapper 而非符号链接，工具链里「解析真实目录」那步空转，而 `/opt/local/bin` 没有裸名
`clang-scan-deps`——CMake 会把字符串 `NOTFOUND` 原样塞进 CXX 扫描的 try-compile，症状是 exit 127
伪装成「Could NOT find Threads」；处置是 `find_program(clang-scan-deps-mp-23 clang-scan-deps)` +
缺位 `FATAL_ERROR`。② `port select` 漂版本时 `clang++` 与 `clang++-mp-23` 同存一个目录，目录重选
候选的**次序**就是钉版本身（`clang++` 排前会静默降级，D158 胜在显式次序上）。两条共同的读法：
症状出现在 configure/try-compile 面时先怀疑缓存与名字解析，删树重配，不是查代码。

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
一条代价已知并接受：该 flag 是整 TU 免检，所以新增「导出类带 STL 成员」**不再有逐处把关**。
**它原来还有第二条代价——「出不了这棵树」——已随 2026-10-03 的裁定销账**：需求方裁定不对外
分发 Vase 供第三方插件作者构建，插件永远由本仓库的构建产出，树外不再有插件作者面
（wiki §13.4）。**若 §8.5 那条前提破掉**（允许插件用不同版本的 MSVC STL 构建），全局豁免即
失去依据，届时改 pimpl 或把 STL 成员移出导出面。站点数（29 处，实测）与那笔账的来龙去脉，
记在 [`wiki/vase-architecture.md`](wiki/vase-architecture.md) 的 13.3；`/wd4251` 本身照旧。

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
`/external:*` 那一段与 `_HAS_EXCEPTIONS` 那一段。（`Cmake/` 下的四个工具链文件与两个 triplet
只讲编译器定位、vcvars 与 STL 选型，**不涉及异常设置**，别去那里找。）

#### 5. 插件 target 一律经 `vase_add_plugin_fixture`


`Cmake/VasePluginHelpers.cmake` 的 `vase_add_plugin_fixture(name SOURCES … LINK_LIBRARIES …)`
是**插件形态的唯一出口**——示例与测试 fixture 都走它（`Samples/HelloPlugin/CMakeLists.txt` 亦然）。
它承载两件手搭 `add_library(SHARED)` 会漏掉的事：**只链 VasePod**（D11：「插件不依赖 Host」
是链接期事实而非约定）与**可见性收紧**（`CXX_VISIBILITY_PRESET hidden` + `VISIBILITY_INLINES_HIDDEN`）。
手搭的插件 target 视为违规——那也正是「某个 target 忘了链 `VaseBuildOptions`」（规矩 1）的复发点。

#### 6. `Tests/HotSwap` 按主干对待（它的判据力按平台不对称）


v3 §12.2：v2 里 `Tests/Reload` 是「不必每次提交都跑」的尾部测试；热插拔升为主干承诺后，
**任何改动 Loader、依赖账本、Eject / Adopt 路径、描述符布局、产物布局或 `HeaderVersion` 的提交，
必须跑 `Tests/HotSwap/` 的全部用例，且 Windows、Linux 与 macOS 各自留证据**（macOS 自 macOS 腿
2026-10-02 起为现役平台）。这几类改动正是「八线全绿」最靠不住的地方。**本波（2026-10-03 插件
产物布局）就是活证**——一次布局改动同时影响所有 Loader 调用方与三平台依赖解析，单看某一条线的绿
推不出别的线。

> **选择子是 `-R 'HotSwap|Eject|Adopt'`，不是 `-R HotSwap`。** `Tests/HotSwap/` 的用例名按套件
> 分三类：`HotSwap.*`（主循环）、`Eject.*`（拆除路径）、`Adopt.*`（领回路径）。**`-R HotSwap`
> 只选到第一类**，会把守 Eject / Adopt 的边界用例（点名消费者、kept-resident、failed-record、
> 同文件两 id……）整套漏掉——照规则做的人会拿到几条绿灯，然后从没跑过那一堆。
> `-R 'HotSwap|Eject|Adopt'` 是「该目录全部用例」的**超集**：它另捎上
> `Abi.AdoptRejectsBinaryThatImportsSiblingPlugin`（也走 Adopt 路径，跑上没有坏处），
> 以及 `Tools/VaseConsole` 的换件链证人族 `VaseConsoleAdoptManifestSynced*`（波 2 的
> `file install` + `file install-manifest` 双覆盖链，下方「第二实例」那条；含 `…Reason` /
> `…ReplayReason` / `…ShowReason` / `…AdoptReason` 四条只钉文本的兄弟用例——T14c 自足化后
> 五条名全含 Adopt、两级 label 证人各吃脚本自己造出的态，整族被本选择子拉进且逐条单跑自足）
> 与执法/比对拒绝回放证人 `VaseConsoleEjectBlocked*` / `VaseConsoleAdoptBlocked*` /
> `VaseConsoleAdoptManifestMismatch*` / `VaseConsoleRawAdoptRefused*` 四族（各 `…IsFailure` 钉退出码、
> `…Reason` 点名文本——Eject/Adopt 诸族名恰含对应子串，正是被本选择子拉进来的），并经其 fixture
> 拉进 `VaseConsoleCatalogStage`（catalog 家族刷回；M1 raw 面的 `VaseConsoleSwapStage` 自 T12 起
> 无用例 require，已于 T14 随 `swap_plan.txt` / `swap_target.dll` 一并退役）。这里不写条数——
> 写死的数会随用例增删漂移，规则要的是「该目录全部用例」。

**判据子串的收窄与迁移（M2a 收口 2026-09-24；M2b 波 2 D88 续账；M3 D94 再收窄）**：这族用例匹配的消息子串里，
`already in pod` 与身份类子串仍是契约；**`not in pod` 自 M3/D94 起只剩「本局无此 Id」一支**——Failed 记录
与级联空壳都可被 Eject（空壳证人 `RecursiveTeardown.TornShellCanBeEjectedAndThenReAdopted` 已改按报告字段
断言，不再钉这条子串）；**`unknown plugin id` 自波 2 随路径账从
Adopt 面退役**（D69/D71——Host 无账可查，别再按旧账找它）。新契约 token：**误用两枚**
`request/expectation id mismatch` 与 `manifest expectation required`（Host 侧）、**快照归因一枚**
`not in catalog snapshot`（Catalog 侧 `AdoptInto`）、**比对失败一枚** `manifest/binary mismatch`
（一个子串即够，字段细节留给人，spec §6）。**`provided by [ … ]` 与
`unresolved declarations` 两族自 M2a 起改按 `EjectReport` / `AdoptReport` 字段断言**
（迁移完成于 T9/T10，通道边界见 M2a spec §5.3）——别再为这两族新增消息子串匹配。

**但别把「八线全绿」读成「八线等价」**（T12 复评的判定；macOS 腿入场后 macOS 与 Linux 同类，macOS 腿 spec §0 注④）：

- `Tests/HotSwap/HotSwapLoopTests.cpp` 里 `InstallPrime()` 的覆盖断言在 **Windows 上是真的
  sharing-violation 探针**——镜像还映射着时 `copy_file(overwrite_existing)` 失败，直接暴露
  「Eject 没真卸」（T12 实测：注释掉 `EjectPlugin` 那一行，该断言报
  `The process cannot access the file because it is being used by another process.`）；
- 在 **Linux 上它是空转的**——`copy_file(overwrite_existing)` 是 truncate-in-place（同一 inode），
  镜像还映射着也会覆盖成功、随后读到新字节，同一条断言照样通过。
- **macOS 与 Linux 同类，但多一条更硬的理由不许就地覆盖**：in-place 截断一个**仍被映射**的镜像会让
  旧映射的页失效，进程再触碰就是 `SIGBUS`（macOS 腿 spec §3）——「改名离开 + 落新字节」的 M4 序列
  天然避开，**不许把它优化回 in-place**；macOS 上压住「没真卸」的是档二 `_dyld` 映射清单与档三
  `LC_UUID` 的内存↔磁盘比对（取证注④：驻留时磁盘身份变、内存身份不变，正是这一对看见的）。
- **第二实例：同一个动作交给用户之后**（`Tools/VaseConsole` 的换件链走
  `file install` + `file install-manifest` 双覆盖）判据力同样按平台不对称——Windows 上靠"写不开"
  证明「Eject 没真卸」，Linux 上覆盖是空转、它绿的原因是 `adopt` 的档三身份比对与加载期清单比对
  （`file install` 的输出本身就把这条声明打出来，见 spec §4）。反半句是波 2 的新证人：**只换二进制
  不更清单 → adopt 被比对拒**（`VaseConsoleAdoptManifestMismatch*` 族）——那是 §11.2 链路的本意，
  不是缺陷。

因此 Windows 比 Linux/macOS 多一层文件锁证据；多平台同跑得到的不是同一件事的多份拷贝。

#### 7. 同一件事只准有一处字面真值（本文件与技能的叠层口径）


本文件、`.claude/skills/vase-cpp-engineering/`、以及源文件自己的头部注释，是同一个约束的**三种载体**。判据不是字数重不重，是**仓库变一次，这个副本要不要跟着变**：

| 类别 | 例 | 处置 |
|---|---|---|
| **阈值与基数** | `ctest -N` 的各线 `Total Tests`、tidy 的 TU 数与抑制合计 | **只住本文件**，技能里出现即违规。除了腐烂，实测还有一个更实的害处：把数字直接递到手上的那份回答，只复述了「差值是设计不是漏注册」，没读技能的那份反而给出了成因 |
| **可整段引用的块** | 八个 preset 的命令块、WSL 登录 shell 那三条、`git ls-files` 那串 | **只住本文件**，技能写指针。整段抄过去换不到任何可读性 |
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
run-clang-tidy -p build-win/win-x64-clang-debug          # clang-cl 线（117 个 TU，见下面实测表）
run-clang-tidy -p build-win/win-x64-msvc-debug -extra-arg=-Wno-unused-command-line-argument
run-clang-tidy -p build-linux/linux-x64-clang-debug      # Linux 线**必须单独跑**
Scripts/macos-clang-tidy.sh                              # macOS 线：在 macOS 开发机上跑（脚本形态的理由见下）
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' \
  | xargs -0 clang-format --dry-run --Werror
```

`clang-tidy` 与 `clang-format` 全平台**同为 23.1.x**：Windows/Linux 是 23.1.0（同源 commit
`ea7d852a`），macOS 经 MacPorts 是 **23.1.2**（`clang-tidy-mp-23` / `clang-format-mp-23`，PATH 上无裸名）。
判据一致要的是**同一套检查器与规则版本**——patch 级差异不构成反例（macOS 腿 D172 的口径收窄）；
观测支持它：T10 的 format 门两侧同一条命令 RC=0 逐字同判、tidy 四线正文双 0。`git ls-files` 那串
`--cached --others --exclude-standard` 不能省——
**尚未 `git add` 的新文件会被静默跳过**，门禁照样绿（本仓库实测过）。

**四条 tidy 线都有脚本形态**：`Scripts/linux-clang-tidy.sh`（Linux，经登录 shell）、
`Scripts\win-clang-tidy.cmd`（Windows 两条 debug 线，参数 `clangcl` / `msvc` 可单跑一条）与
`Scripts/macos-clang-tidy.sh`（macOS，本机执行——MacPorts 只给 `clang-tidy-mp-23`，而 `run-clang-tidy`
按裸名找 tidy 二进制，脚本解析 `command -v clang-tidy-mp-23 || command -v clang-tidy` 并以
`-clang-tidy-binary` 显式递入，找不到时 **rc=2 带名响亮拒绝**；T10 首跑的指纹「0 秒即终止、RC=1、
正文双 0」就是这个包装器缺口而非代码）。四者同契约：日志落在脚本旁边（`*.log`，已被 gitignore），
stdout 打全「退出码 + 正文 `error:` 条数 + 正文 `warning:` 条数」三判据与摘要计数，**退出码非 0 即门禁未过**，
不必再手工 grep 日志。（**macOS 线的 RC 与正文 warning 不耦合**——2026-10-03 勘误，见「核这些门禁时…」那节。）
基数见「核这些门禁时，退出码单独用是不够的」那节的实测表，脚本不复制阈值。
实测输出与该表对上（**插件产物布局波 2026-10-03 复跑值**）：**四线各 117 TU** / Windows 两线 **776999**、
Linux **319226**、macOS **449889** / NOLINT 命中 Windows 两线 **4895**、Linux **4890**、macOS **4891**——
**四线全量复跑三判据全过、正文双 0**。TU 117 = 上表 116 **+1 = `Tests/Unit/ArtifactLayoutTests.cpp`**
（四线同幅）；单 TU 极值三线（Windows 998/45609、Linux 359/11337、macOS 278/7278）**一字未动**，NOLINT
命中三数亦与小账清理波逐数相同（本波新增代码 NOLINT 净增 0），Suppressed 上跳
（771606→776999 / 315892→319226 / 445702→449889）属新 TU 的乘法形状。**收口修记**：该波**首跑**四线正文各
捕得 3 条 `readability-trailing-comma`（`Tests/Integration/AssemblyFromSolveTests.cpp:69,72,75`——01115a8 折行
后尾缺逗号），**收口波内已修**（补尾逗号、只改那三处），四线复跑即双 0、抑制合计与单 TU 极值随之逐数不变
（登记见 plan 偏离登记第 6 条）。**另记一形（macOS RC 口径勘误）**：首跑 macOS 线**正文 3 条 warning 而
退出码仍为 0**——「macOS 线正文有 warning 即 RC=1」那句**不成立**，已改准，见「核这些门禁时…」那节。
**本波 format 门读数**：三线（Windows / Linux / macOS）同一条
`git ls-files -z --cached --others --exclude-standard '*.h' … | xargs -0 clang-format --dry-run --Werror`
全 **RC=0**（`--cached --others --exclude-standard` 三件俱在，未 `git add` 的新文件不被静默跳过）。
历史口径——**小账清理波 2026-10-03 复跑值**：**四线各 116 TU** / Windows 两线 **771606**、
Linux **315892**、macOS **445702** / NOLINT 命中 Windows 两线 **4895**、Linux **4890**、macOS **4891**——
**四线全量复跑三判据全过、正文双 0**；相对 macOS 腿 T10 表三线 Suppressed 各 +3、NOLINT 命中与单 TU 极值
**一字未动**（本波新增代码的 NOLINT 净增 0，canonical 位点维持 6），故属整体平移而非新诊断。
历史口径——**macOS 腿 T10 2026-10-02 复跑值**：同 116 TU / 771603 / 315889 / 445699 / 4895 / 4890 / 4891，
四线全量复跑三判据全过、正文双 0；**该波首跑合计捕得 21 条正文 warning**（按 clang-cl 5 + Linux 6 +
macOS 10 计，cl.exe 线与 clang-cl 同组；macOS 的 10 条里两条系 SMB 挂载时序的陈旧快照，T10 报告 §1.4
如实记），全部走代码级出路（`ImageIdentity` 三构造点指定初始化——不回填 NSDMI、守 D175 本意，
`ForEachLoadCommand` 的 `Call&&` 传值化，`Put8`，POSIX 两支重排，`#ifdef`，删孤儿 `<cstring>`），
新增抑制仅两枚、各带就地理由（canonical 位点 4→6，见下）。历史口径——M6 终审修复波 T6 三线各 115 TU /
766651 / 314262 / 4895 / 4890，**三线全量首跑即三判据全过、正文双 0**（终审修复未带入任何新正文
warning；M6 收口那轮「三线全量两轮、首跑捕得同一组 10 条正文 warning、
全部落本波新代码 `Plan.cpp` 与 `VaseCliPlanTests.cpp`、代码级修掉后复跑归零」保留为历史，
详见「核这些门禁时…」那节与该 plan 偏离登记 T5/T6 条）。
T6 本波账面：TU 数 115 未动（本波零新源，只改三个既有源）、NOLINT 命中两侧零动（4895 / 4890 与 M6 收口
逐数相同——本波新增代码 `git diff HEAD -U0 -- '*.h' '*.cpp' | grep '^+.*NOLINT'` **零命中**，canonical 位点维持 4）、
Linux max 11337 未动、唯抑制合计两侧各 +1 落非用户代码桶（与单 TU 极值不动并存的乘法形状，如实记）。
M6 波本体：TU +4 = `Tools/VaseCli/Plan.cpp` / `Doctor.cpp` 与 `VaseCli{Plan,Doctor}Tests` 四个新源，
两侧同幅、无平台专属新源（M6 的平台分支都在函数体内），**TU 平台差维持 0**；抑制合计的上跳
（680329→766651 / 301480→314262）是这 4 个新 TU 各带一份 nlohmann/gtest 模板量的乘法形状；
**单 TU 极值 Windows 998 / 45609 不动**，Linux min 359 不动、max 11334 → 11337 又落回本波未碰的
`ImageInspectPosix.cpp`——与 M5 收口前读数同值，属上一轮记的未归因漂移**再现**，如实记；
**本波新增代码 canonical NOLINT 位点 4 处**（`Doctor.cpp` 两个 `misc-include-cleaner` 区：windows.h
include 区与 `ProbeExclusiveOpen` 函数体；`VaseCliDoctorTests.cpp` 两个：同形 include 区含 `#undef CopyFile`
尾行与 `LoadLibraryW`/`FreeLibrary` 使用区——伞形头处理的 canonical 推导在 `Source/Host/LoaderWindows.cpp`，
四处均只活在本波新代码里、就地一句带过），命中增量（Windows +185、Linux +168）主体是既有位点乘上
4 个新 TU 的乘法形状，账面仍以 canonical 位点清单为准。
M5 收口 2026-09-28 口径为 Windows 两线 680325、Linux 301475、三线首跑即三判据全过——较 M4 收口口径
（三线各 99 TU / 617742 / 269117 / 4394 / 4405）的上跳 =
**三线同增 12 个编译数据库条目**（本波 12 个新源文件：`Source/Host/Inspect.cpp`、`Tools/VaseCli/` 五件、
`VaseCli{Scan,Validate}` 与 `Inspect`/`LibraryFileName` 四个测试文件、枚举面 fixture 插件
`NoEnumeration`/`StaleEnum` 两个）各带一份 nlohmann/gtest 模板量的乘法形状；两侧同幅、无平台专属新源，
故 TU 平台差维持 0。
M5 收口那波 canonical 净新增抑制位点 **7 处**（`git diff main..HEAD -U0 -- '*.h' '*.cpp' | grep '^+.*NOLINT'`
可数，其中一行是「不需要 NOLINT」的论述注释不计位点）：`readability-identifier-naming` 4、
`union-access` / `reinterpret-cast` / `misc-const-correctness` 各 1，均就地带理由。
**该波单 TU 极值未动**。更早几段保留为历史：M4 收口较 M3 终审修复波（Windows 96 TU / 602497 / 4197，
Linux 97 TU / 264835 / 4273）的 +3 / +2 是 `VersionedAStampDrift` / `VersionedAServiceDrift` 两个新
fixture 与 `NoIdentityPlugin` 跨平台化，该波零 NOLINT 位点；M3 两轮各捕得一条 `readability-trailing-comma` 并补
尾逗号归零；b1a0cc5 尾逗号修复前后各跑一轮、合计逐数相同（尾逗号不是抑制位点）；M3 收口较还账波
（Windows 587140 / 3895，Linux 258300 / 3970）的上跳是 `StatefulPlugin` 与 `NeighborC` 两个新 TU
各带一份 nlohmann/gtest 模板量；还账波 2026-09-26 较波 2 收口各 +1 的成因（逐 TU 定位到
`ConfigMacroTests.cpp` 一个 TU 的**非用户代码桶**：新取件器的实例化拖出的系统头模板实例化，
打开 header filter 在我们自己的头上查不到任何诊断））。

> **tidy 的树必须是构建过的**（T8 实测坑）：compile_commands 里每条命令以 `@...modmap` 引用
> CMake 模块扫描的响应文件，那是**构建产物**——只 configure 未 build 的新树里它不存在，
> clang-tidy 当场报 `clang-diagnostic-error`、run-clang-tidy 退出 1，而正文 `error:`/`warning:`
> 这类报错行**不带 `文件:行:列:` 前缀**，脚本按 `": error: "` 计数抓不到——三判据里
> 退出码 1 而正文双 0，正是它的指纹（也再次说明退出码与正文计数谁都不能单独信）。
> 修法是先把那棵树 build 一遍，不是查代码。

但**别指望 configure 帮你守住版本**：`CMakeLists.txt` 的版本校验对象是
**CXX 编译器**，只查 clang 的 `23.x` 主版本；cl.exe 分支更是只查平台、不查版本。
**它管不到 `clang-format` / `clang-tidy` 这两个独立可执行文件。**
「CXX 编译器的 clang 是 23.x」不等于「tidy / format 也是 23.x」——
后者由环境（PATH 最前）保证，不是 configure 保证的。

#### 同一版 LLVM 是必要条件，不是充分条件


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

1. **四条 debug 线（clang-cl / cl.exe / Linux / macOS）各自跑、各自读正文**，别用一条推另一条；
2. **平台专属 TU（`ImageInspectWindows.cpp` / `ImageInspectLinux.cpp` / `ImageInspectDarwin.cpp`）天然只在
   一条线上被检查**——它们的告警只有对应平台线看得见，抑制数目也因此不对称（`LoaderPosix.cpp`
   由 Linux 与 macOS 两线共用，不在此列；macOS 腿 T10 实测的 `performance-no-automatic-move` 只在
   libc++ 两侧报、Darwin 的 include-cleaner 归因也只有 macOS 线现形，都是这一不对称的现例）。

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
  正文一条 `error:` / `warning:` 都没有；以下系旧运行示例（77 TU 时代），数字只示形态、与本轮基数无关）：

  ```
  Running clang-tidy in 12 threads for 77 files out of 77 in compilation database ...
  5014 warnings generated.
  Suppressed 5076 warnings (5014 in non-user code, 62 NOLINT).
  ```

  所以「无新 warning」的判据是三条一起：**退出 0 + 正文 `error:` 0 条 +
  正文 `warning:` 0 条**，再连摘要行一起读。只 grep `warning:` 会漏掉全部被抑制的量。
  **macOS 线曾记「正文有 warning 即 RC=1」（2026-10-03 勘误，原句作废）**：该形态**不成立**——插件产物布局波
  收口时 macOS 线正文有 3 条 warning 而 `run-clang-tidy-mp-23`（23.1.2）退出码仍为 **0**，与三条 23.1.0 线同形；
  原句的出处是 macOS 腿 T10 首跑的**包装器缺口**（`rc=1` 且正文双 0，见 `Scripts/macos-clang-tidy.sh` 注释与
  该腿 plan 偏离登记第 9 条），不是「warning 推高 RC」。**四线的 RC 与正文 warning 不耦合**，方向一致——
  但仍别只信 RC：RC=0 也不蕴含无抑制量，三判据要一起读。
  实测基数（**macOS 腿 T10 2026-10-02，四线全量复跑三判据全过、正文双 0；该波首跑捕得 21 条正文 warning、
  全部代码级出路——账见「静态检查与格式」段**；M6 终审修复波 T6 2026-09-30 为三线全量首跑即双 0；
  M6 收口那轮为三线全量两轮；波 1 遗留的正文 warning
  自 M2b 终审修复波起保持清零，M3 两轮曾在 `readability-trailing-comma` 上连捕两条
  （`HotSwapLoopTests.cpp` / `EjectTests.cpp`）并均补尾逗号归零；M5 波末还账三线首跑各捕得正文 warning
  （Linux 3 条与 clang-cl 1 条，同一份源、两 STL 不同：libc++ 侧不报那条 `bugprone-signed-bitwise`），
  四处均代码级修掉（该波首跑非双 0；M4/M5 两波收口是三线首跑即双 0）。**M6 收口首跑回到非 0**：三线首跑捕得**同一组
  10 条**正文 warning，全部落本波新代码——`Plan.cpp` 九条（`misc-include-cleaner` 7：`SolveNoteKind`/
  `is_directory`/`std::move`/`ServiceRef`/`SolveOutcome`/`std::vector`/`SolveNote`；`performance-move-const-arg` 1：
  `const Result<Preset>` 上的 `std::move` 空转；`bugprone-branch-clone` 1：两硬跳过 case 同体）与
  `VaseCliPlanTests.cpp` 一条（`std::vector` 直接包含），均走代码级出路（补直接包含 / 照 Console.cpp
  的 `auto loaded` 消费形 / 合并两 case 保穷举），复跑三线三判据全过、正文双 0）。**M6 终审修复波 T6 三线全量首跑即三判据全过、正文双 0**——
  终审修复未带入正文 warning（本波新代码仅一枚用例与两处分支）。正文双 0 之外，
  抑制仍全部落在第三方头、gtest/nlohmann 模板与被 NOLINT 豁免的自有位形/宏机制代码里，
  我们自己的代码零正文 warning：

  | 线 | 文件数 | 摘要行 Suppressed 合计 | 单 TU 最小 / 最大 |
  |---|---|---|---|
  | clang-cl（`win-x64-clang-debug`） | 117 | 776999 | 998 / 45609 |
  | cl.exe（`win-x64-msvc-debug`） | 117 | 776999 | 998 / 45609 |
  | Linux（`linux-x64-clang-debug`） | 117 | 319226 | 359 / 11337 |
  | macOS（`macos-x64-clang-debug`） | 117 | 449889 | 278 / 7278（**macOS 腿首读数 278/7278 未动**） |

  **TU 数四线同为 117**（插件产物布局波 2026-10-03 复跑，「117 files out of 117」四线一致：小账清理波的
  116 → **+1 = `Tests/Unit/ArtifactLayoutTests.cpp`**——本波唯一新源，四线同幅、无平台专属新源，**TU 平台差维持 0**）。
  历史读数——macOS 腿 T10 复测「116 files out of 116」四线一致：M6 的 115 → +1 =
  `Source/Host/ImageInspectMachO.cpp`——**无条件编译**、八线合成就链它（D161），与 `ParseElfBuildIdFile`
  今天在 Windows 上照样编译同构；平台 TU 每线一换一：win `ImageInspectWindows.cpp`、linux
  `ImageInspectLinux.cpp`（由 `Posix` 更名，D162）、macos `ImageInspectDarwin.cpp`。
  更早读数：M6 T6 与 M6 收口同为「115 files out of 115」；M5 的 111 → +4 =
  `Tools/VaseCli/Plan.cpp` / `Doctor.cpp` 与 `Tests/Integration/VaseCli{Plan,Doctor}Tests.cpp` 四个新源——
  两侧同幅、无平台专属新源，M6 的平台分支都在函数体内、不成独立 TU）。每个源都是独立条目，
  `run-clang-tidy` 的按路径去重只合并同一文件多次入库（如 LoadProbe 双 target 那一例）；
  **文件数比编译数据库条目数少 1**（T10 复测 json 实测 117 条目 / 116 唯一路径，四线相同，与 M6 T6 的
  116/115 同形；LoadProbe 双 target 形状自 M6 以来不变），原因是
  `fixtures/LoadProbe/LoadProbe.cpp` 同时编进 `LoadProbe` 与 `UnloadProbe` 两个 fixture（四条线都一样）。
  历史口径：M5 收口两侧同为 111（M4 的 99 → +12 = `Source/Host/Inspect.cpp`、`Tools/VaseCli/` 五件
  （`main`/`Cli`/`Scan`/`Validate`/`ManifestMerge`）、`VaseCli{Scan,Validate}Tests` 与 `Inspect`/`LibraryFileName`
  四个测试、枚举面 fixture 插件 `NoEnumeration`/`StaleEnum` 两个）；M4 收口两侧同为 99（Windows 96 → +3 = `VersionedAStampDrift` / `VersionedAServiceDrift`
  两个新 fixture（D106）+ `NoIdentityPlugin` 跨平台化，Linux 97 → +2，M3 时「Linux 多 1 个 TU 即
  `NoBuildIdPlugin`」的平台差随之消失）；更早——波 2 时 87→93 / 88→94（新增六个条目）、M3 再 +2 至
  95 / 96、M3 终审修复波再 +1 至 96 / 97。

  **插件产物布局波 2026-10-03 复测（现值）**：三线 Suppressed 上跳（Windows 771606→**776999**、
  Linux 315892→**319226**、macOS 445702→**449889**），**单 TU 极值 Windows 998/45609、Linux 359/11337、
  macOS 278/7278 三线一字未动**即证是一个新 TU（`ArtifactLayoutTests.cpp`）乘出的模板量、而非新诊断
  （首跑那 3 条 `readability-trailing-comma` 已在收口波内修复、复跑双 0——见上段）。
  **小账清理波 2026-10-03 复测（当时）**：三线 Suppressed 各 **+3**（Windows 771603→**771606**、
  Linux 315889→**315892**、macOS 445699→**445702**）——三枚新 gtest 用例拖出的模板量；**单 TU 极值
  Windows 998/45609、Linux 359/11337、macOS 278/7278 三线一字未动**即证是整体平移而非新诊断。
  历史口径——**macOS 腿 T10 复测（当时）**：Windows 两线 766651→**771603**（净 +4952）、Linux 314262→**315889**
  （净 +1627）、macOS 445699（新线首读数）——一个 `ImageInspectMachO.cpp` 新 TU 乘出的量，
  **单 TU 极值 Windows 998/45609、Linux 359/11337 一字未动**，仍是乘法形状；macOS 侧极值 278/7278 无历史
  可比，作为基线记下，此后按「变的是极值还是整体平移」的老判据读。历史口径——Suppressed 合计的上跳
  （680329→766651 / 301480→314262——M6 收口读数为 766650 / 314261，T6 复测两侧各 +1、
  落非用户代码桶（NOLINT 命中与单 TU 极值零动，属乘法形状漂移，如实记）；更早 M5 收口那轮为 617742→680325 /
  269117→301475）**是新 TU 各带一份 nlohmann/gtest 模板量的乘法形状**，**单 TU 极值基本未动
  （Windows 998 / 45609 与 Linux min 359 自 M3 以来逐数相同；Linux max 由上一轮的 11334 回到 11337，
  又落在本波未碰的 `ImageInspectPosix.cpp` 上、与 M5 收口前读数同值——未归因漂移再现，如实记）即为证**——
  这条判据比合计数本身可靠：真出现新诊断时，变的是某个 TU 的极值，而不是所有 TU 一起平移。

  数字变了不一定是错，但**要看它变在哪一类**——摘要行里还会出现 `N NOLINT`
  （**插件产物布局波 2026-10-03 三线复测：Windows 两线 4895、Linux 4890、macOS 4891——与上一轮逐数相同，
  本波新增代码 NOLINT 零净增**；历史口径——小账清理波 2026-10-03 三线 4895 / 4890 / 4891 与 macOS 腿 T10
  逐数相同，其 win/linux 两数一字不动的机理一句话**：
  本波新增的两处抑制位点与 open 行的检查扩表全部落在 `__APPLE__` 支或 macOS 线独有的 include-cleaner
  归因上——Linux 线上那一行没有 include-cleaner 可抑制，不编译到某线就不动某线的命中。macOS 线自身
  首跑 4888→复跑 4891，+3 恰是三条被新抑制的诊断。**canonical 位点账 4→6**：macOS 腿净新增 2 处
  （`ImageInspectDarwin.cpp` 的 span cast【计划原文自带、T7 落地】与 typed cast【T10——dyld API 返回
  通用 `mach_header*`、「宽类型收窄」没有免转写法】，均只活在本波新代码、就地带一句理由；`LoaderPosix.cpp`
  open 行是**既有 vararg 位点扩 checks 列表**，不是新位点）。**T6 波口径的命中次数为：Windows 两线 4895、Linux 4890——与 M6 收口逐数相同，T6 零新增位点
  （`git diff HEAD -U0 -- '*.h' '*.cpp' | grep '^+.*NOLINT'` 零命中）、canonical 位点维持 4。M6 波新增代码 canonical 位点 4 处**
  （`git diff main..HEAD -U0 -- '*.h' '*.cpp' | grep '^+.*NOLINT'` 可数，全部是 `misc-include-cleaner` 的
  NOLINTBEGIN 区、全部只在 Windows 侧编译：`Doctor.cpp` 的 windows.h include 区与 `ProbeExclusiveOpen`
  函数体两区、`VaseCliDoctorTests.cpp` 的同形 include 区（含 `#undef CopyFile` 尾行）与
  `LoadLibraryW`/`FreeLibrary` 使用区两区——四处均就地带一句理由，canonical 推导留在
  `Source/Host/LoaderWindows.cpp`，见 plan 偏离登记 T3/T4b 条）；命中增量（Windows +185、Linux +168）
  主体是既有位点乘上 4 个新 TU 的乘法形状，Windows 侧另含这四处位点自身的命中贡献（Linux 上
  `#ifdef _WIN32` 内不编译，故两侧增量不等）。历史：M5 波末还账 4710 / 4722——与 M5 收口的
  4710 / 4722 逐数相同，该波新增代码 `git diff -U0 -- '*.h' '*.cpp' | grep '^+.*NOLINT'` **零命中**，
  抑制合计的 +4 / +5 全落在非用户代码桶。M5 收口那波新增代码**净新增 canonical
  位点 7 处**（`git diff main..HEAD -U0 -- '*.h' '*.cpp' | grep '^+.*NOLINT'` 可数，均就地带理由，
  见「静态检查与格式」段），而命中增量（Windows +316、Linux +317）全属既有位点乘上 12 个新 TU 的
  乘法形状。更早：M4 收口 4394 / 4405——该波新增代码**零 NOLINT 位点**
  （`git diff main..HEAD -U0 -- '*.h' '*.cpp' '*.cmake' '*.txt' | grep '^+.*NOLINT'` 零命中），
  增量全属既有位点乘上新 fixture TU 的乘法形状：Windows +3 个 TU 乘出 +197，Linux +2 个乘出 +132；
  M3 终审修复波 4197 / 4273，M3 收口 4131 / 4207，还账波 3895 / 3970（较波 2 一个数没动），
  波 2 3895 / 3970（较波 1 各 +997 / +1022）。**这不是仓库里的抑制处数**：同一个抑制会被每个包含它的
  TU 各计一次，而 M2b 波 2
  的 10 处新位点里有三处在被广泛包含的头上（`PluginHost.h` / `LoadPlan.h` 各有二十多个直接包含者、
  `ManifestExpectation.h` 十六个，传递包含再放大），一处指令乘出几百次命中是这一乘法的形状——
  账面以 canonical 位点清单为准，命中数只核方向。**M2b 波 2 那 10 处**（`git diff main..HEAD -U0 |
  grep '^+.*NOLINT'` 可数，排除 docs/ 计划文本）是：`readability-redundant-member-init` 3
  （`ManifestExpectation.h` 与 `PluginHost.h` 两个 NOLINTBEGIN 区 + `LoadPlan.h` 一处 NEXTLINE——`= {}`
  NSDMI 是指定初始化省略豁免的前提）、`performance-enum-size` 5（D75 钉 int32 基型：`Mood.h`、
  `ConfigConsumerPlugin.cpp` 与 `ConfigValue/ConfigMacro/ConfigBlob` 三个 Unit 测）、
  `misc-include-cleaner` 1（`AdoptManifestTests.cpp` 的 `<system_error>` 镜像位，T9 勘误入账）与
  `cppcoreguidelines-pro-type-union-access` 1（`ConfigValueTests.cpp` 位形测试）；M3 侧净新增 2 处
  （`PluginDescriptor.h` 尾追加槽 NSDMI 的 `readability-redundant-member-init`，被广泛包含的头，乘出
  增量大头；与 `DescriptorTests.cpp` 仿 `VASE_PLUGIN` 生成名的 `readability-identifier-naming`），
  均就地写了理由；M4 侧 0 处。M2a 的 `Value.h` 与 `ConfigMacros.h` 两处 NOLINTBEGIN 区同理），
  以及 gtest 模板实例化带来的巨量非用户代码告警。
  四线正文里的 `ThirdParty/` 路径 diagnostic 实测 **0 条**（`ExcludeHeaderFilterRegex: '.*ThirdParty.*'` 如期生效，macOS 腿 T10 四线复测同）。

- **`ctest` 在一个测试都没发现时同样返回 0。** 所以「测试全绿」不能只跑
  `ctest --preset <p>`，必须另跑一次 `ctest --preset <p> -N`，把 `Total Tests`
  与「构建与测试」一节那张**按线分账的基数表**逐位对上（macOS 腿 T10 复测八线全部对上，见该表）。`gtest_discover_tests` 用的是 `DISCOVERY_MODE PRE_TEST`，
  枚举发生在 ctest 运行时——测试被漏注册时，`ctest` 会一声不吭地报成功。
