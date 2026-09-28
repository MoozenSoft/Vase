# Vase M5 第一波：`VaseCli scan` / `validate`（清单生成与四项校验）（spec）

**日期**：2026-09-27 · **状态**：brainstorming 定稿（需求方六问六裁）→ **grilling 三轮收敛（R1 九问 + R2 六问 + R3 三问，全部裁可，见下方两处裁定注）** · **实施计划**：spec 复审通过后由 writing-plans 另立 · **分支**：`m5-vasecli`

**上游权威**：`wiki/vase-architecture.md` v3 的 §11.1（`VaseCli` 四子命令）、§3.4（清单的权威性与生成流程）、§1.1（分层表：`VaseCli` | Catalog）、§3.1（描述符 POD 纪律与「唯一一跳」）、§8.1（`VasePlugin_GetPlugin` 入口）、§6.1（服务命名的前缀自洽与宿主前缀）、§6.3（一服务一实现）、§4.3（脏清单是常态）、§4.4（工具与运行时必须同一套规则）、§13.3（已知静默失败点）；
`docs/superpowers/specs/2026-09-25-vase-m2b-wave2-loading-verification-design.md` 的 D67–D74（加载期比对的域规则、D72 全字段严格等值、D73 清单独有字段、D74 schemaVersion 闸在读取处）、D81（`AdoptInto` 单点重读）；
`docs/superpowers/specs/2026-09-24-vase-m2b-catalog-solving-design.md` 的 D49/D64（未知键闸）、D50（坏清单/缺清单/重复 Id 的处置）、D54（库文件命名收口在 Catalog 层）、D55（JSON 只住 Catalog）、D57（清单逐字义务）、D58/D64a（`Refresh` 的事务性）、D60（宿主声明本局会注册什么）。
历史决定 D1–D116 不改史，本文决定从 **D117** 续号。

**本文的地位**：兑现 §12.3 的 M5 行中**不依赖非本机平台**的那一部分——即 §11.1 的 `scan` 与 `validate` 两条子命令。M5 的平台腿（macOS 档二/档三/dyld4 neverUnload 实测、Android/iOS 冷装配边界）与本波**没有交集**，仍归 M5 后续波次。凡本文与磁盘代码冲突处以磁盘为准（仓库惯例）；本文与架构文档冲突处以本文为准并回头挂勘误（第 7 节清单）。

> **[事实取证注（2026-09-27）]** 本文的前提都是**读盘核出来的**，不是推的：
>
> ① **描述符不是名字可预知的静态对象。** `VASE_PLUGIN` 生成的是函数 `VasePluginDesc_<类名>()`，其体内是**函数内 `static const PluginDescriptor`**（`Include/Vase/PluginDescriptor.h:135-140`）。符号名由类名派生，外部不可预知；拿不到 Id 就无法走 `VasePlugin_GetPlugin`（同文件 `:141-144`）。
> ② **`InspectBinary` 需要 Id。** 它走 `Loader::Symbol(record, "VasePlugin_GetPlugin")`，再以 `id` 调入口（`Source/Host/PluginHost.cpp:64-76`）——而「发现 Id」恰是 `scan` 的活。
> ③ **镜像解析器没有导出目录。** `Source/Host/ImageInspectCommon.cpp` 只解析 PE 的节表、导入目录（`kImportDirectoryIndex = 1`）与调试目录（`kDebugDirectoryIndex = 6`，`:111-113`），**没有导出目录**。故「枚举导出表按前缀找符号」若要走，是新增工作量；本文的 D118 绕开了它。
> ④ **`CompareDescriptor` 是 DLL 内私有面。** `Source/Host/Detail/ManifestCompare.h` 明文：「住 `Source/Host/Detail`：DLL 内私有面、不进公开头、不挂导出宏」。
> ⑤ **Catalog 没有任何 JSON 写面。** `Source/Catalog/*.cpp` 与 `Include/Vase/Catalog/*.h` 里 `dump` / `to_json` / `serialize` **零命中**——`ManifestJson.cpp` 只有解析面。`scan` 的序列化面是全新增量。
> ⑥ **库文件命名的平台知识是纯拼接，且没有反向实现。** `Source/Catalog/Detail/LibraryFileName.h:11-18`：`_WIN32` 支 = `stem + ".dll"`；`#else` 支 = `"lib" + stem + ".so"`。全仓（`Source/` 与 `Tools/`）无扩展名剥离 / `lib` 前缀剥离的反向代码——`IsNativeLibraryFileName` 只活在本文里。**另一处重复**：`Tests/Integration/SolveTests.cpp:104-113` 自建 `ExpectedBinaryPath(...)`，把同一条平台规则逐字又写了一遍（**第三份**）。CMake 侧 `PREFIX` 全仓零命中、`OUTPUT_NAME` 只在换件谱 fixtures 里出现（`Tests/HotSwap/fixtures/CMakeLists.txt:25` 把 `VersionedAPrime` 的产物名设为 `VersionedA`）——**target 名与产物名本来就可以不一致**。
> ⑦ **一条服务只有一个提供者。** §6.3 规定「同一服务被两个插件提供 → 求解失败」，所以「提供的服务必须落在自己的 Id 前缀下」这条约定**没有归属歧义**——多插件引用同一服务是**消费**侧的事，与提供侧前缀无关。
> ⑧ **命名前缀的违规在磁盘上已经有实例**，不止 §3.1 的样例：`Samples/DependentPlugin`（Id `Vase.Dependent`）提供 `Vase.Farewell`（`Samples/DependentPlugin/DependentPlugin.cpp:47`、`Samples/HelloCommon/Farewell.h:13`、`Tests/Integration/fixtures/manifests/dependent/plugin.json`）；`Vase.Test.*` 一族 fixture 同样不合规（Id 如 `Vase.SharedProvider`、`Vase.CollisionProvider`，服务名一律 `Vase.Test.*`）。**注意 `Samples/` 那处是应当合规的样板，fixture 那族是刻意的违规材料。**
> ⑨ **描述符与清单都没有事件槽。** `PluginMeta` 只有 `Requires` / `OptionalRequires` / `Provides` / `Config` / `ProcessStates` 五槽（`Include/Vase/PluginDescriptor.h:52-67`）；`ManifestEntry` 同构（`Include/Vase/Catalog/ManifestView.h`）。事件名今天只活在 `static constexpr kName` 里。故 §6.1 举的 `Vase.Combat.DamageEvent` 那一半**无机器可读载体**。
> ⑩ **环与阻塞在 Solve 里是同一个出口。** `Source/Catalog/Solve.cpp:641` 的报错文本是 `dependency cycle or blocked plugins: …`（D52 口径：硬边与纯 optional 边都咬）。
> ⑪ **测试侧的两件既有资产可直接复用**：`Tests/TestingSupport/CatalogSandbox.h`（temp 下建一次性插件树、析构清理）与 `Tests/CMakeLists.txt:62+` 的 `VASE_FIXTURE_*` 编译定义（fixture 二进制的构建树路径）。
> ⑫ **清单解析对未知键根级与条目级一律 `Err`。** 闸是 `OnlyKeys`（`Source/Catalog/ManifestJson.cpp:222-234`）：根级白名单 11 键（`:570-589`）、`requires`/`optionalRequires`/`provides` 元素（`:252-258`）、`config` 元素（`:331-337`）、enum `choices` 元素（`:381-388`）各有白名单。**故「人手写的未知键被静默丢掉」这条风险不存在**——但反过来，`scan` 序列化出来的键必须全部落在白名单内。
> ⑬ **架构文档在这件事上是二对二的自相矛盾**：§1.1 的分层表行（`VaseCli` | Catalog | 「从不加载任何 DLL」）与 §3.1（「不必执行 DLL 里的任何代码」）说**不加载**；§3.4 的流程图（「加载 DLL / 读出导出的描述符 / 生成 …… / 立即卸载」）与时点表（构建期 | `VaseCli scan` | 是否加载二进制 = **是**（一次性，随即卸载））说**加载**。本文 D117 采后者，前三处**都要挂勘误**（第 7 节）。
> ⑭ **宿主提供的服务会让朴素判定误报。** `Vase.Test.HostOnly` 由 `EdgeConsumerPlugin` 经 `Requires` 声明（`Tests/Integration/fixtures/manifests/edge_consumer/plugin.json:7-8`、`Tests/Integration/fixtures/EdgeConsumerPlugin/EdgeConsumerPlugin.cpp:52`），而它由**宿主**在 Stage0 注册。当 `LoadRequest::HostProvided` 为空 span 时，`Solve` **不返回 `Err`**：该条目落 `kSkip` + `kMissingDependency`，且**不产生 Note**（`Solve.cpp:399-403` 的 `hostProvides` 恒 false → `:436-449` 只翻 `participating` → `:456-474` 归因 → `:698-711` 装配）。`Solve` 里只有两处 `Err`（拓扑环/阻塞 `:640-641`、Provides 碰撞 `:545-547`/`:552-554`）。既有证人：`SolveTests.cpp:466-476` `AbsentProviderSkipsWithoutNote`。
> ⑮ **CMake 顺序与测试接线**：根 `CMakeLists.txt:271-290` 的顺序里 `add_subdirectory(Tools/VaseConsole)`（`:288`）**已排在** `add_subdirectory(Tests)`（`:290`）之前——这就是「在较早子目录建 target、`Tests` 里链接它」的既有范式（`VaseHost`/`VaseCatalog` 同例）。`Tests/CMakeLists.txt` 是**单目标** `VaseTests`、源文件平铺，`gtest_discover_tests(VaseTests DISCOVERY_MODE PRE_TEST)`（`:144-151`）。
> ⑯ **那处断言确实是恒真，但下游已有真证人。** `Source/Host/PluginHost.cpp:1177` 在成功态那一个分支上置 `report.ManifestVerified = true`，而 `Include/Vase/Host/Evidence.h:104` 记着「拒绝态不携证据位」——所以**任何拿得到的 `AdoptReport` 都满足它**。该字段在**下游**另有人守：Console 的 `VaseConsoleAdoptManifestSyncedAdoptReason` 钉着 `manifestVerified=true` 的输出文本。

> **[裁定注 · brainstorming（2026-09-27，六问六裁）]** 逐条落点：
> **腿一 · 范围（D119）**：本波 = `scan` + `validate`。`plan` 与 `doctor` 归第二波——`plan` 是纯 Catalog 面（`Solve` 已存在，`VaseConsole` 的 `catalog solve` 已在做同一件事），`doctor` 四项里「工具链一致性」在架构里只有一句话、「文件锁残留」是平台不对称项，需要单独定定义，与 `scan`/`validate` 不同类。
> **腿二 · `scan` 的读法（D117）**：**装载读**（`Loader` 载入 → 读描述符 → 立即卸下），采 §3.4 口径。理由：§4.4「工具与运行时同规则」走这条路是**代码事实**（共用 `CompareDescriptor` 与 `ParseManifestFile`），走「只读数据段」则是新写**第二个描述符读者**，从此两份实现要同步；且「只读数据段」在描述符满是待重定位指针的前提下等于自写 PE `.reloc` / ELF `.rela.dyn` 重定位器（②③ 决定了连枚举导出表这条捷径也不存在），而 **Linux 侧没有任何「映射但不跑初始化」的手段**，Windows 侧的 `LOAD_LIBRARY_AS_IMAGE_RESOURCE` 若有对位能力也只会造成两平台两套机制。
> **腿三 · 枚举入口（D118）**：新导出入口是**纯追加**的 `extern "C"` 函数，不动 `PluginMeta` 布局、**不 bump `kHeaderVersion`**；组合库由 VasePack 生成同名（§8.6）。
> **腿四 · `validate` 第四项（D120/D121）**：度量定死为「描述符里每条 `Provides.Name` 以 `<插件Id>.` 为前缀」，**只覆盖服务**；事件无载体，记账不发散。`Samples/DependentPlugin` 随之改名。
> **腿五 · 保真回写（D122）**：`enabledByDefault` 保真（人手语义），`binary` 不保真（写观察事实），差异打印。
> **腿六 · CMake 接线（D125）**：本波不接 §11.1 的构建后置步骤，单独记账。

> **[裁定注 · grilling 三轮（2026-09-27，R1 九问 + R2 六问 + R3 三问全部裁可）]** 逐条落点：
> **R1-Q1（D118 就地修订）**：不 bump `kHeaderVersion`；缺枚举入口时**响亮报错且在文本里点名原因**（「binary 早于枚举入口」），**不许**静默返回「零个插件」。这是一处**刻意接受的不对称**：`HeaderVersion` 相同而工具能力不同。
> **R1-Q2（D133）**：响亮只留给**真歧义**（有库但读不出 / 一个子目录两个库）；「既无库也无清单」的子目录**跳过 + 一行打印**，与 D50 的宽容同调。
> **R1-Q3 / R3-Q18（D134）**：单文件**原子写**（临时文件 + rename 覆盖）；整次 `scan` **尽力而为**——逐插件写、失败逐条报、已写的保留、退出码非零。不采两阶段（它不是真事务，却要额外持有全部条目；而 `scan` 幂等可重跑）。
> **R1-Q4 / R3-Q17（D135）**：退出码三档（0 / 1 有检查未过 / 2 用法或环境错误），**不把四项编码进不同码位**；**零个插件的树 = 2**（本仓库对「静默成功」过敏的先例：`ctest` 零测试返回 0）。
> **R1-Q5（D139）**：M4 遗留**同域两笔**并进本波（恒真断言删、D110 补跨 Pod 正例）；跨域两笔（前端字段计数守卫、`ProbeSwapGuard` 的 abort 残留）不搭顺风车。
> **R1-Q6（D136）**：枚举数组**顺序不是契约**；`scan` 自己的输出**按 Id 排序**——确定性由工具保证，不由库承诺。
> **R1-Q7（D129 就地修订）**：**加反向函数** `LibraryStem(fileName)`，与 `LibraryFileName(stem)` 同址、随 D129 一起提升为公开面；**用 round-trip 测试钉住两侧互为反函数**；顺带收编 `SolveTests.cpp:104-113` 那份第三副本。
> **R1-Q8（D122 就地修订）**：「清单独有字段」是**三个**不是两个：`schemaVersion`（恒写 1，不读旧值）、`binary`（观察事实）、`enabledByDefault`（保真）。
> **R1-Q9（D131）**：`validate` 加可重复的 `--host-provides <name>@<version>`，喂进 `LoadRequest::HostProvided`（D60 的既有模型）；**`kMissingDependency` / `kVersionMismatch` 一律算第三项未过**，`kDisabled` 不算。
> **R2-Q10 / R3-Q16（D132）**：第四项加「**越界**」子判断——宿主提供的名字**不得落在任何插件 `Id.` 之下**；§6.1 末句「不占用 Vase 的命名空间」**记账不实现**（`Vase.` 在本仓库既是框架前缀也是插件 Id 前缀，判不出「占的是谁的」）。
> **R2-Q11 / R2-Q12（D137）**：`VaseCli` **手写 argv 解析**（不接 `ThirdParty/cli`）；**只出人读文本**，不另给 `--json`（机器判据已有退出码，而其消费者 D125 已推后）。
> **R2-Q13（D139）**：恒真断言**直接删**（下游已有真证人，事实取证注⑯）；跨 Pod falsifier **新增独立一条用例**。
> **R2-Q14（D138）**：`LibraryFileName` 的 macOS 分支**本波不写**——本机无 macOS，写了就是未验证的代码；且 §6 风险 8 已承诺本波不产生平台不对称。
> **R2-Q15（D140）**：spec §3.4 那条「扫 `Tests/Integration/fixtures/` 应报出违规」**降级为一次性人工观察**记进计划，不进自动化判据。

---

## 0. 范围

**本波做**（宏动、Host 与 Catalog 的公开面各长一点、新增 `Tools/VaseCli`、`Samples/DependentPlugin` 改名、M4 遗留同域两笔；**描述符布局与 `kHeaderVersion` 都不动**）：

1. **新导出入口（§2.1，D118）**：`VasePlugin_Descriptors`——一个库里 N 个描述符的枚举面。纯追加。
2. **Host 侧的两个公开原语（§2.2，D123）**：读二进制全部描述符；`CompareDescriptor` 提升为公开面；`HeaderVersion` 闸抽成两条读法共用的一个函数。
3. **Catalog 侧的两件（§2.2–2.3，D129 + 序列化）**：库文件命名的**正向与反向**两个函数一并从 `Detail/` 提升为公开面（`LibraryFileName` / `LibraryStem`）；清单**序列化**面（全新增量，事实取证注⑤）。
4. **`scan`（§2.3–2.6，D122/D133/D134/D136）**：目录约定与跳过口径、清单独有字段的保真回写、写盘语义、退出码。
5. **`validate`（§3，D120/D121/D127/D128/D131/D132/D135）**：四项检查、`--host-provides`、`Samples/DependentPlugin` 改名、输出与退出码。
6. **`Tools/VaseCli` 的内部形态与 CLI 面（§4，D124/D137）**：内部静态库（可被 gtest 直接链）+ 薄 `main`；手写 argv 解析；只出人读文本。
7. **M4 遗留同域两笔（§5.1，D139）**：删掉那处恒真断言；补 D110 的跨 Pod falsifier。

**本波不做**（各归其位，不许顺手）：

| 项 | 去向 |
|---|---|
| **组合库（一库 N 插件）的扫描与枚举** | VasePack 那一波（D126）。`VasePlugin_Descriptors` 的形状已为它留好（同名生成即可），但 Catalog 的「一子目录一清单」模型装不下它——那是发布期的目录模型问题 |
| **§11.1 的 CMake 后置步骤**（构建后自动 `scan`） | 单独记账（D125）。接线的前置是先把「哪些目录允许自动重生成」划清，而那要与 D57 的逐字手写义务一起看——`Tests/Integration/fixtures/manifests/` 下那 6 份是刻意手写的两侧样本，自动重写等于把反例证人变成正例 |
| **`plan` 与 `doctor`** | M5 第二波（D119） |
| **事件名的前缀检查** | 待描述符加「发出的事件名」槽时再补（事实取证注⑨）。加槽要 bump `kHeaderVersion` 并牵动比对器的绑定绊线与清单双向等值 |
| **`LibraryFileName` 的 macOS `.dylib` 分支** | M5 的 macOS 腿那一波（D138）：本机无 macOS，未验证的代码不落；届时与 `LoaderPosix` 的 Darwin 分支一起做 |
| **§6.1 末句「宿主不占用 Vase 的命名空间」** | 记账不实现（D132）：`Vase.` 在本仓库既是框架前缀也是插件 Id 前缀 |
| **`--check` / `--dry-run` / `--json` 模式** | `validate` 是前两者的对偶面；机器判据已有退出码（D137） |
| **`scan` 的编号索引 / `list` 之类的附加子命令** | §11.1 没有；YAGNI |
| macOS 腿、Android / iOS、CI | M5 后续波次（§12.3 与 `CLAUDE.md` 记的「仍无 CI（M5）」） |

---

## 1. 决定表

| # | 决定 | 依据 |
|---|---|---|
| D117 | **`scan` 取「装载读」**（`Loader` 载入 → 读描述符 → 立即卸下），并把 §1.1 / §3.1 / §10 的同口径句挂勘误改准。代价明写：扫描会跑 `DllMain` 与静态构造 | 事实取证注⑬（文档二对二，采 §3.4）；§4.4；Linux 无「映射不初始化」手段 |
| D118 | **新导出入口 `VasePlugin_Descriptors(std::uint32_t* outCount)`**，返回指向静态数组首元素的 `const PluginDescriptor* const*`；`extern "C"` + `VASE_EXPORT`；**纯追加**，不动布局、不 bump `kHeaderVersion`。**缺该入口的二进制 ⇒ 响亮报错，报错文本点名原因**（「binary 早于枚举入口」），不许静默返回「零个插件」（**R1-Q1 修订**） | 与既有家族（`VasePluginDesc_X` / `VasePlugin_GetPlugin`）同形；§8.6 的 VasePack 分发表是它的天然实现；事实取证注③ |
| D119 | **本波范围 = `scan` + `validate`**；`plan` 与 `doctor` 归第二波 | 裁定注腿一 |
| D120 | **`validate` 第四项的度量 = 描述符里每条 `Provides.Name` 以 `<插件Id>.` 为前缀**；**只覆盖服务**，事件记账 | §6.1；事实取证注⑦、注⑨ |
| D121 | **`Samples/DependentPlugin` 的服务改名 `Vase.Farewell` → `Vase.Dependent.Farewell`**；前缀规则自此定死为**完整 Id**（§3.1 自己的样例也违反该规则，一并挂勘误） | §6.1；事实取证注⑧ |
| D122 | **`scan` 的回写：三字段三类处置**（**R1-Q8 修订**）——`schemaVersion` 恒写 `1`（不读旧值）、`binary` **不保真**（用 `LibraryStem` 从观察到的文件名恢复 stem；与旧值不同则**改写 + 打印**）、`enabledByDefault` **保真**（旧清单无该键 ⇒ 省略，输出因此稳定可重复跑） | D73/D74；事实取证注⑥；「保真 `binary` 会留下一个指向不存在文件的清单」 |
| D123 | **`CompareDescriptor` 从 `Source/Host/Detail` 提升为 VaseHost 公开面**；`HeaderVersion` 闸抽成「按 Id 读」与「枚举读」共用的一个函数 | §4.4（同规则唯一能保证的形态）；事实取证注④ |
| D124 | **编排住 `Tools/VaseCli`（库薄工具厚）**：库里只长 D118/D123/D129 与清单序列化**四件**必须的；目录遍历、四项检查、报告与退出码住工具。工具内部再分两层：内部静态库（可被 gtest 直接链）+ 薄 `main` | `Tools/VaseConsole` 的先例（1127 行胖前端）；`AdoptInto` 那个先例成立是因为它**宿主与工具共用**，而本波没有第二个程序化消费者 |
| D125 | **本波不接 §11.1 的 CMake 后置步骤**，单独记账 | 不做表第二行 |
| D126 | **组合库（一库 N 插件）本波不做** | 不做表第一行 |
| D127 | **`validate` 第三项如实透传 `Solve` 的裁定**：环与被阻塞**不另分列** | 事实取证注⑩；另造分类即第二处真值 |
| D128 | **子目录缺 `plugin.json` 沿用 D50**（跳过 + Warning）：打印成 Warning 行，**不计入退出码** | §4.3；`validate` 报的是「存在的东西不对」，不是「少了个东西」 |
| D129 | **库文件命名的识别面从 Catalog 的 `Detail/` 提升为公开面，且**正反两函数一并**（**R1-Q7 修订**）：`LibraryFileName(stem)` 与 `LibraryStem(fileName)` 同址、互为反函数、**由 round-trip 测试钉住**；顺带收编 `Tests/Integration/SolveTests.cpp:104-113` 那份第三副本 | 事实取证注⑥；§13.1 第二条、D54 |
| D130 | **`validate` 第二项失败时快照不建成** ⇒ 只报该项 + 非零退出，**不假装**其余三项通过，输出里说清「快照没建成」 | `PluginCatalog::Refresh` 的事务性（D58/D64a） |
| D131 | **`validate` 加可重复的 `--host-provides <name>@<version>`**，喂进 `LoadRequest::HostProvided`（D60 的既有模型）；**`kMissingDependency` / `kVersionMismatch` 一律算第三项未过，`kDisabled` 不算** | 事实取证注⑭；§6.3 的承诺不能在工具面失效 |
| D132 | **第四项加「越界」子判断**：宿主提供的名字**不得落在任何插件 `Id.` 之下**。§6.1 末句「不占用 Vase 的命名空间」**不实现**、记账 | §6.1；事实取证注⑧（`Vase.` 在本仓库既是框架前缀也是插件 Id 前缀） |
| D133 | **边界口径：响亮只留给真歧义**（有库但读不出 / 一个子目录两个库）；「既无库也无清单」的子目录**跳过 + 一行打印**，不计入退出码 | D50 的宽容同调；目录里混着非插件内容是常态 |
| D134 | **写盘语义：单文件原子**（临时文件 + rename 覆盖）；**整次 `scan` 尽力而为**——逐插件写、失败逐条报、已写的保留、退出码非零。不采两阶段 | D58 的事务性精神；`scan` 幂等可重跑 |
| D135 | **退出码三档**：0 = 全过；1 = 有检查未过；2 = 用法或环境错误。**不把四项编码进不同码位**。**零个插件的树 = 2** | 本仓库对「静默成功」的过敏（`CLAUDE.md` 记的 `ctest` 零测试返回 0） |
| D136 | **枚举数组顺序不是契约**；`scan` 的输出**按 Id 排序**（确定性由工具保证，不由库承诺） | 顺序是组合库的定序问题，现在钉就是把还不存在的用例的形状写死 |
| D137 | **`VaseCli` 手写 argv 解析**（不接 `ThirdParty/cli`）；**只出人读文本**，不另给 `--json` | `cli` 的强项是交互会话与补全，批处理用不上；机器判据已有退出码，其消费者已推后（D125） |
| D138 | **`LibraryFileName` 的 macOS `.dylib` 分支本波不写**，记账 | 本机无 macOS ⇒ 未验证代码；§6 风险 7 |
| D139 | **M4 遗留同域两笔并进本波**：删 `Tests/HotSwap/AdoptManifestTests.cpp:73` 那处恒真断言（事实取证注⑯）；补 D110 的跨 Pod falsifier（新增独立一条用例） | 边际成本近零；下游已有真证人 |
| D140 | **§3.4 的「扫 `Tests/Integration/fixtures/` 应报出违规」降级为一次性人工观察**，记进计划；保留稳定那半句「扫 `Samples/` 合规插件零违规」 | 那目录刻意堆了多种违规，任何 fixture 增删都会让「应报出 N 项」腐掉 |

---

## 2. `scan`

### 2.1 宏侧：新导出入口（D118）

```cpp
extern "C" VASE_EXPORT const ::vase::PluginDescriptor* const*
VasePlugin_Descriptors(::std::uint32_t* outCount)
{
    static const ::vase::PluginDescriptor* const kAll[] = { VasePluginDesc_##Type() };
    *outCount = 1;
    return kAll;
}
```

- **纯追加**：既有生成物（工厂、`kVaseMeta_X`、`VasePluginDesc_X`、`VasePlugin_GetPlugin`）一个字节不变；`PluginMeta` 布局不动 ⇒ **`kHeaderVersion` 不动**。
- 选**函数**而非纯数据导出（`extern "C" VASE_EXPORT const MetaTable kVasePlugin_MetaTable;`）：与既有家族同形，且 §8.6 的 VasePack 分发表就是它。
- **同一库里写两次 `VASE_PLUGIN` 会撞名**——与既有那条同源（§3.1「组合库里没有这一项」），不是本波引入的新限制。
- **缺入口是一条新的失败面**（D118）：老二进制照旧能**加载**（`VasePlugin_GetPlugin` 还在），但**扫不了**。报错文本必须点名原因，测试必须钉住「响亮，不是零个插件」（§4）。
- **不扩 §8.1 的「唯一一跳」**：装载跳仍是 `VasePlugin_GetPlugin`，宿主侧一行不改；新入口是**只给工具用的枚举面**。这条要靠文档说清（第 7 节），否则它会被读成「宿主也可以走两条路」。
- **顺序不是契约**（D136）：单插件库恒 1 条；组合库那一波由 VasePack 定序。

### 2.2 Host 与 Catalog 侧的原语（D123 / D129）

新增公开头 `Include/Vase/Host/Inspect.h`：

```cpp
// 读这个二进制里的**全部**描述符；内部 = Loader::Symbol("VasePlugin_Descriptors") + 逐条 HeaderVersion 闸。
// 返回的指针**指向镜像只读段（借用）**，寿命 = 该 BinaryRecord 的驻留期——见下方寿命纪律。
VASE_HOST_API Result<std::vector<const PluginDescriptor*>> InspectDescriptors(const detail::BinaryRecord& record);
```

- **装载 / 卸载的配对与归属留给调用方**（工具自己 `EnsureResident` 与 `Unload`），与 `CreatePod` 同形——`Loader` 已经是公开面，不必再包一层。
- **寿命纪律（借用面，必须点名）**：返回的 `PluginDescriptor*` 与其内部的 `string_view` / `MetaArray` 全部**指向镜像只读段，借用**。调用方必须在 `Unload` **之前**把需要的东西**物化成拥有形**（`scan` → `ManifestEntry`；`validate` → 期望与结论）。这条不是建议：`Unload` 之后解引用是悬垂。与 `LoadPlan` 的 Id 借用窗口、`ManifestExpectation` 的拥有值形同一纪律。
- **`HeaderVersion` 闸抽成一个共用函数**：`InspectBinary`（按 Id 读）与 `InspectDescriptors`（枚举读）过的是同一段代码；**两条读法各自的失败文案保持一份**。
- **`CompareDescriptor` 提升**：声明并入 `Include/Vase/Host/ManifestExpectation.h`（它就是「期望 vs 描述符」这一个关系的两条边），定义仍住 `Source/Host/Detail/ManifestCompare.cpp`，加 `VASE_HOST_API`。原私有头随之退役（不留转发头）；`ManifestCompare.h` 的「DLL 内私有面」注释要改准——它现在是对外契约的一部分。
- **库文件命名的正反两函数**（D129）：`LibraryFileName(stem)` 与新增的 `LibraryStem(fileName)` 同址、一并从 `Source/Catalog/Detail/` 提升为公开面。`LibraryStem` 是**唯一**能从磁盘实际文件名恢复 `binary` 值的实现（`lib` 前缀与扩展名两侧都要剥）。**round-trip 测试是它存在的判据**：对两侧平台各自的合法文件名，`LibraryStem(LibraryFileName(s)) == s`。`SolveTests.cpp:104-113` 那份副本随之收编。

### 2.3 目录约定与跳过口径（D133）

`PluginCatalog::Refresh` 要求「子目录缺 `plugin.json` → 跳过（D50）」，而 `scan` 的活恰恰是**生成** `plugin.json`——**直接吃 `Refresh` 的快照来发现插件是鸡生蛋**。所以发现逻辑独立于 Catalog：

- 遍历**一级**子目录；每个子目录按三条处置：

| 情形 | 处置 |
|---|---|
| 恰有一个库 | 正常路径 |
| **两个及以上库** | **响亮报错**（真歧义，不猜） |
| 有库但读不出描述符 / 无枚举入口 / `HeaderVersion` 不符 | **响亮报错** |
| 既无库也无清单 | **跳过 + 一行打印**，不计入退出码（与 D50 同调） |
| 无库但有清单 | **响亮报错**——清单在指一个不存在的库，是明确的坏树 |

- 「这是不是一个库」的判据 = 提升后的命名面（D129），平台扩展名知识**只有一份**。
- 输出位置 = `<目录>/<子目录>/plugin.json`（与 D54 / 456078d 的「与 DLL 同目录即配对」一致）。
- **整棵树一个插件都没找到 ⇒ 退出码 2**（D135）。
- **注意与 D128 的方向差**：D128 是 `validate` 遇到「有库无清单」（跳过），这里是 `scan` 遇到「有清单无库」（报错）——前者清单缺失是常态，后者是明确的坏树。

### 2.4 保真回写（D122）

`scan` 生成一份 `ManifestEntry` 并发给序列化面。字段分两类**三小类**：

| 类 | 字段 | 处置 |
|---|---|---|
| **描述符背书** | `Id` / `DisplayName` / `Version` / `Requires` / `OptionalRequires` / `Provides` / `Config` / `ProcessStates` | 从描述符生成（`displayName` 缺省 = Id、`version` 缺省 = `""` 等缺省语义沿用既有解析器的口径） |
| **清单格式常量** | `schemaVersion` | **恒写 `1`**，不读旧值（它的语义是「这份清单的**格式**版本」，由**写者**决定；保真反而荒谬） |
| **清单独有**（D73） | `binary` | **不保真**：用 `LibraryStem`（D129）从**观察到的**文件名恢复 stem；旧值与之不同 ⇒ **改写 + 打印这处改动** |
| | `enabledByDefault` | **保真**：读既有清单取旧值；旧清单无该键 ⇒ **省略**（等价 true，输出因此稳定、可重复跑） |

- 「保真 `binary`」被否的理由：保真会把旧值留下，而加载期按 `binary` 拼路径——结果是「清单指向一个不存在的文件」。**那是比改名更坏的失败**（响亮，但方向错了：事实是文件就叫这个名字）。
- **「清单独有字段」这张清单只含这三个成员名**，与 D73/D74 同源、**只写在一处**（序列化点旁边）。不新立一张表——多一张表就是多一处真值。
- **序列化面是全新增量**（事实取证注⑤）：住 Catalog（JSON 是它的独占物，D55），与解析面同址。**它写出的键必须全部落在 `OnlyKeys` 白名单内**（事实取证注⑫）——这是它唯一需要继承的 schema 知识。
- **JSON 的键序与缩进不是契约**（谁都没读过它、也不该读）；契约是**重新解析回来等值**。测试按「scan → 解析 → 与描述符比对」判，不按字节判。

### 2.5 写盘语义（D134）

- **单文件原子**：写 `<名字>.tmp` 再 `std::filesystem::rename` 覆盖（两侧都是覆盖语义）。盘上任何时刻不会出现半个 JSON——而那份 JSON 恰是加载期要读的权威副本。
- **整次 `scan` 尽力而为**：逐插件处理，失败逐条报出，**已写的保留**，退出码非零。不采两阶段（它不是真事务，却要额外持有全部条目；而 `scan` 幂等可重跑，重跑即修复）。

### 2.6 CLI 与退出码

```
VaseCli scan <插件目录>
```

写盘、逐插件打一行（路径 + `Id`，若有 `binary` 改动则附一行说明）——**按 Id 排序输出**（D136）。退出码按 D135 三档、零插件树为 2。

---

## 3. `validate`

### 3.1 前提与流程

`validate` 校验的是「清单这份**副本** vs 二进制这份**权威**」，所以前提是 `plugin.json` 已存在。流程：

```
建 PluginCatalog 快照（Refresh）
   ├─ Err  ⇒ 第二项的裁定（D130）：只报该项，非零退出，说明「快照没建成」
   └─ Ok   ⇒ 逐子目录：EnsureResident → InspectDescriptors → ①④ 两项 → Unload
             然后 Solve（HostProvided = --host-provides 喂入，无 preset）⇒ 第三项
```

### 3.2 四项各自的落点

| 项 | 落点 | 形状 |
|---|---|---|
| ① 清单↔二进制一致、`schemaVersion` 兼容 | `schemaVersion` 闸在**读取处**（`ParseManifestFile`，D74）；清单条目 → 期望走既有的 `BuildExpectation`（D84）；一致性用**提升后的 `CompareDescriptor`** | 逐插件遍历完，**不短路**；每处不符打印字段路径与两侧值 |
| ② 插件 Id 重复 | `Refresh` 的构造期硬报错（已有） | 见 D130 |
| ③ 依赖图 / 循环 / 版本 | 调 `Solve`（`HostProvided` = `--host-provides`，D131） | `kSkip` + `kMissingDependency` / `kVersionMismatch` **算未过**；`kSkip` + `kDisabled` **不算**；`Notes` 四类逐条打印；`Err` 时透传消息（D127） |
| ④ 服务命名前缀自洽 | 读**描述符**的 `Provides`（与 ① 同一次装载里拿到，权威在二进制）；另加**宿主越界**子判断（D132） | 列出**全部**违规条，**不遇错即停** |

第四项的两条判据：
- `Provides[i].Name` 必须以 `Id` + `.` 为前缀（逐条独立判定，不因前一条违规而跳过）。
- `--host-provides` 喂进来的每个名字**不得落在快照里任一插件 `Id` 的前缀之下**（越界占用）。宿主名字**不做**「符合自家前缀」的判定——宿主没有 Id 可对照（D132）。

**一处已知的覆盖缺口**：第四项只看**描述符**的 `Provides`。若某插件的**清单**里写了描述符没有的服务名，那个名字的合规性不在第四项视野内——但那种漂移**已被第一项拦下**（`CompareDescriptor` 全字段严格等值，D72），所以缺口被包含关系吸收，不另设机制。

### 3.3 第三项要记账的收窄（D127）

§11.1 把「循环依赖」单列，但 `Solve` 把「环」与「被阻塞」从**同一个出口**报出来（事实取证注⑩）。本波**如实透传**：`validate` 打印 `Solve` 的原文并列出被点名的 Id 集合，**不另造「环」这个分类**。要分列得先改 `Solve` 的公共消息——那会动公共面与既有证人（`SolveTests` 有钉该文本的用例），属独立事项。这条写进 §6 风险行。

### 3.4 `Samples/DependentPlugin` 改名（D121）

`Vase.Farewell` → `Vase.Dependent.Farewell`。**已核的牵连范围**（grep 全仓，已排除 build 树）：

| 文件 | 处 |
|---|---|
| `Samples/HelloCommon/Farewell.h` | `kName` 常量 |
| `Samples/DependentPlugin/DependentPlugin.cpp` | `Provides` 表 + 文件头注释 |
| `Tests/Integration/fixtures/manifests/dependent/plugin.json` | `provides[0].service` |
| `CLAUDE.md`、`wiki/vase-architecture.md` | 叙述行（第 7 节一并处理） |

**`Tools/VaseConsole` 的回放资产与 `get` 输出都不涉及它**（`*.txt` / `*.cmake` 零命中，已核）——故不牵动任何 `PASS_REGULAR_EXPRESSION` 或 Console 用例。

改名的**验收判据**（稳定那半句）：改名后 `validate` 扫 `Samples/` 的合规插件（`Vase.Hello`、`Vase.Dependent`）**零违规**。「扫 `Tests/Integration/fixtures/` 应报出 `Vase.Test.*` 一族违规」那半句**降级为一次性人工观察**（D140）。

### 3.5 输出与退出码

逐项分节、每项的证据逐条列出（人读文本，D137）。退出码按 D135：0 = 四项全过（Warning 不算失败，D128）；1 = 有检查未过；2 = 用法或环境错误（含零个插件的树）。

---

## 4. `Tools/VaseCli` 的形态

- **内部静态库 + 薄 `main`**（D124）：静态库住编排与四项检查，可被 gtest 直接链；`main` 只做 argv 解析（手写，D137）与打印。
- **CMake 接线**：`add_subdirectory(Tools/VaseCli)` 必须排在 `add_subdirectory(Tests)` **之前**（事实取证注⑮ 的既有范式：`VaseHost`/`VaseCatalog`/`Tools/VaseConsole` 都是这么接的）。`Tests/CMakeLists.txt` 的 `VaseTests` 链接该静态库即可——**不需要新的测试目标，也不需要新的 CMake 铺陈机制**。
- **`cli` 库不引入**（D137）：不链 `VaseThirdPartyCli`，因此也不带进它那套第三方头的暴露面。

---

## 5. 测试分层与验收面

### 5.1 M4 遗留同域两笔（D139）

| 处 | 处置 |
|---|---|
| `Tests/HotSwap/AdoptManifestTests.cpp:73` | **删**那行 `EXPECT_TRUE(...ManifestVerified)`——`PluginHost.cpp:1177` 在成功态那一个分支上置 true、拒绝态不携报告（`Evidence.h:104`），所以任何拿得到的 `AdoptReport` 都满足它；该字段在下游另有真证人（Console 的 `VaseConsoleAdoptManifestSyncedAdoptReason` 钉着 `manifestVerified=true` 的输出文本）。与 M4 删掉的两个兄弟同形 |
| D110 的「进程内 vs 本 Pod」缺 falsifier | **新增独立一条用例**：跨 Pod 场景（pod1 卸 `Vase.Stateful`、pod2 持 `Vase.NeighborB`）。不并进既有知情位用例——M4 的 grilling 就同类问题裁过「不把用例从一件清晰的事变成两件事拼的」（其 Q16） |

### 5.2 `VaseCli` 的用例

**落点**：纯逻辑（命名规则、合并回写、`LibraryStem` 的 round-trip）进 `Tests/Unit/`；需要真二进制的（四项检查、端到端）进 `Tests/Integration/`；CLI 二进制的端到端挂 `Tools/VaseCli/CMakeLists.txt`（与 Console 同例）。沙箱目录在 temp 下运行时建（复用 `CatalogSandbox` 的形态），二进制路径从既有的 `VASE_FIXTURE_*` 取（事实取证注⑪）。

**必须钉住的新失败面**：

| 用例 | 钉什么 |
|---|---|
| 无枚举入口的二进制 | **响亮失败并点名原因**，不是「零个插件」——D118 的静默陷阱 |
| `HeaderVersion` 不符 | 与加载期同一段闸、同一份文案 |
| 子目录两个库 / 有清单无库 | 响亮报错（§2.3 表） |
| 既无库也无清单的子目录 | **跳过 + 打印**，退出码不受影响，但**整棵树零插件 ⇒ 2**（D133/D135） |
| 旧清单带 `enabledByDefault: false` | 回写后**仍是** `false`（D122 的保真） |
| 旧清单的 `binary` 与磁盘不符 | 改写成实际 stem **且打印**（D122 的不保真 + 不静默） |
| `LibraryStem` round-trip | 两侧平台的合法文件名各自 `LibraryStem(LibraryFileName(s)) == s`（D129） |
| 第四项违规 | 列出**全部**违规条，不是第一条 |
| 宿主名字落在插件 `Id.` 之下 | 越界被报出（D132）；同名前缀**不**误报 |
| `SharedProviderPlugin` 单独入沙箱 | **只有第四项红**——单变量反例（事实取证注⑧） |
| `Samples/DependentPlugin` 改名后 | 第四项零违规（D121 的验收） |
| `--host-provides` 喂进宿主服务 | 第三项**不**误报（D131）；不喂时**报**缺失 |
| 写盘中途失败 | 失败逐条报、已写的保留、退出码非零、盘上无半个 JSON（D134） |

**端到端那条是「三处一致的证人」**：沙箱目录上 `scan` 生成清单 → `validate` 四项全过 → **加载期也过**（`CreatePod` 成功）。这条同时证 D117 的连带收益——三处走的是同一份 `CompareDescriptor`。

**基数**：六线同幅增长（无平台门、无 `#ifndef NDEBUG` 门），其中 D139 的跨 Pod 用例与上表各条一同计入。`VaseConsole` 的 33 条不受影响（本波不动 Console）。CLAUDE.md 的两张表随收口波落账。

---

## 6. 风险与预期中的行为性红

1. **§1.1 / §3.1 / §10 的勘误是本波的既定文书义务**（D117）。它们不是「顺手改文档」——三处说的都是**分层承诺**，改准了才算兑现。实施时先 grep `从不加载` / `不加载任何 DLL` 定位全部命中，若冒出第四处一并处理。
2. **第三项的分类收窄会被人读成缺陷**（§3.3）。处置：输出里如实带出 `Solve` 的原文（其中含 `cycle or blocked`），让读者自己看得到这个合并；spec 与代码注释各写一句为什么不分列。
3. **`CompareDescriptor` 提升会扩大 ABI 面**（D123）。它是既有函数，只是加了导出宏与公开头——`/wd4251` 那类站点不因此增加（函数不是类），但「DLL 内私有面」这条性质要同步改注释。
4. **`scan` 的写盘是破坏性操作**（覆盖 `plugin.json`）。D122 只保住 `enabledByDefault` 一个字段；`displayName` 这类字段若人手写过与描述符不同的值，会被**改写回去**（那是本意：描述符是权威）。**预期中的红**：若有人拿 `scan` 跑 `Tests/Integration/fixtures/manifests/`，那 6 份手写样本会被改写——这正是 D125 不接 CMake 后置步骤的理由之一。本波**不**加护栏，但输出里逐条打印实际写出的文件。
5. **枚举入口侵蚀 §8.1「唯一一跳」的风险**。处置：文档说清职责分工（装载跳 vs 枚举面），且宿主侧一行不改——**没有机制阻止**日后有人让宿主走枚举面，这是一条**契约束**，与 §9.3 同类，如实记录不假装有机制。
6. **`HeaderVersion` 相同而工具能力不同**（D118）：这是刻意接受的不对称。它是**新的第三类不兼容**（前两类是「装不上」与「判据不符」），所以第 7 节的 §8.1/§13.3 义务里要把它写进已知契约面。
7. **`LibraryFileName` 今天在 macOS 上是错的**（D138，事实取证注⑥ 的 `#else` 支）。本波**不修**，理由：未验证的代码不落；但那意味着「本波之后 `scan`/`validate` 在 macOS 上会拼错路径」——记账到 macOS 腿那一波，届时与 `LoaderPosix` 的 Darwin 分支一起做。
8. **本波不产生任何平台不对称**（无 `#ifdef` 计划）。若实现中发现某平台需要，那是新事实，回来改判并记计划偏离登记。

---

## 7. 文书义务（验收时同步；规矩 7 口径：只住 `CLAUDE.md` 的真值不复制进技能）

| 载体 | 处 | 处置 |
|---|---|---|
| `wiki/vase-architecture.md` | §1.1 分层表行（「从不加载任何 DLL」） | 挂勘误：`scan` 是构建期的一次性装载；同一行的**论证**（元数据操作不必碰二进制）仍然成立——它由枚举期读 `plugin.json` 承担 |
| 同上 | §3.1 的「不必执行 DLL 里的任何代码」 | 挂勘误：改为「读描述符需要镜像驻留（`DllMain` 与静态构造会跑）；不实例化插件、不注册任何东西、不进 Pod」；并说明**为什么** POD 纪律仍然必要（固定布局、跨 ABI 可读） |
| 同上 | §3.1 的 POD 段落（`VaseCli scan` 那句） | 与上一条同一处，一次改完 |
| 同上 | §3.1 的描述符样例 | 改掉 `Provides = { { "Vase.DamageSystem", 1 } }` → `Vase.Combat.DamageSystem`——它与 §6.1 互相违反（D121） |
| 同上 | §8.1 的「唯一一跳」表述 | 改准：加一句「`VasePlugin_Descriptors` 是**只给工具的枚举面**，装载跳仍唯一（宿主侧不用它）」 |
| 同上 | §6.1 的命名规范段 | 补「前缀 = **完整 `Id`**」；并记账末句「宿主不占用 Vase 的命名空间」为何**不实现**（D132：`Vase.` 在本仓库既是框架前缀也是插件 Id 前缀） |
| 同上 | §10 若同口径的行 | 与 §1.1 同步（先 grep 定位全部命中） |
| 同上 | §11.1 的 `scan` 段 | 补现状注：CMake 后置步骤**本波未接**（D125）；`validate` 新增 `--host-provides`（D131） |
| 同上 | §12.3 的 M5 行 | 补一句「`VaseCli scan` / `validate` 已落（第一波）；平台腿与 `plan` / `doctor` 归后续波次」 |
| 同上 | §13.3 的已知静默失败点 | 补两条：**枚举入口缺失 ⇒ 工具面扫不了而加载面照常**（D118，响亮但不对称）；**枚举面的契约束**（宿主不得改走它，D118/风险 5） |
| `CLAUDE.md` | 「项目状态」 | 加 M5 第一波；「仓库里有什么」补 `Tools/VaseCli` 与两个新公开原语（`InspectDescriptors` / 提升后的 `CompareDescriptor` 与命名正反函数）；基数表随之更新 |
| 同上 | 「静态检查与格式」与「核这些门禁时……」 | tidy 的 TU 数从 99 上跳（新 TU：`Tools/VaseCli/*`、`Tests/` 下的新用例文件）——随收口波落账 |
| `wiki/vase-console-use.md` | —— | **不动**（本波不碰 Console） |
| `.claude/skills/vase-cpp-engineering/` | `references/architecture.md` 若有同口径 | 只改判据与理由那一层；**阈值与基数不进技能**（规矩 7） |

---

## 8. 与其他里程碑的边界

- **M2b 波 2（D67–D74）**：本波**复用**它的比对域规则与 `schemaVersion` 闸，**不改**任何一条。`CompareDescriptor` 只是换了可见性，行为一字不变。
- **M2b 波 1（D45/D49/D50/D54/D55/D57/D60/D64）**：D49/D64 的未知键闸是 `scan` 序列化面必须满足的约束（事实取证注⑫）；D50 的处置口径被 `validate` **继承而非重写**（D128/D130）；D54 的库文件命名面被**提升可见性并补上反向**（D129）；D55 的「JSON 只住 Catalog」被序列化面**遵守**；D60 的 `HostProvided` 是 `--host-provides` 的直接落点（D131）。
- **M3（D92/D93）**：`ProcessStates` 槽与清单顶层 `processStates` 要进序列化面与比对域——本波**只是把它们带过**（既有字段，不改规则）。
- **M4（D104–D116）**：本波接手它留下的同域两笔（D139）；其余两笔仍记在 `vase-v3-hotswap-status` 记忆里，不搭顺风车。
- **M5 后续波次**：平台腿（macOS / 移动端，含 D138 欠下的 `.dylib` 分支）、`plan` + `doctor`、CMake 后置步骤、VasePack 与 C4251 还债、CI。**本波不为其做任何预备性改动**——尤其不为组合库改 `VasePlugin_Descriptors` 的形状（D126），也不为 macOS 预写分支（D138）。
