# Vase M3：多插件（判据 3a 全量形 / 3d 进程级状态）（spec）

**日期**：2026-09-26 · **状态**：brainstorming 定稿（需求方五问五裁）→ **grilling 两轮收敛**（R1 六问 + R1′ 四问全裁；R1′ 的三问前提被事实取证推翻后重问，见下方改判注） · **实施计划**：spec 复审通过后由 writing-plans 另立 · **分支**：`m3-multi-plugin`

**上游权威**：`wiki/vase-architecture.md` v3 的 §5.6（进出判定流与四条补角）/ §9.1（静态状态分层与进程级状态登记）/ §9.2（诊断基线）/ §9.3（无法被自动化验证的承诺）/ §3.1、§3.3（描述符与两个版本号）/ §12.1 判据 3a 与 3d / §12.3 里程碑表（其 M3 行已于 2026-09-26 按磁盘口径补全为 3a + 3d + 完整属主追踪器——**本文 D97 把第三项核查后否决**，该行注记随 M3 收口一并改）；
`2026-09-23-vase-m2a-assembly-foundation-design.md` 的 D42（级联空壳的中途进出）与第 8 节延期表第 8 条（「拆未活成员」的三档证据 shape 属 3a/M3）；
`2026-09-25-vase-m2b-wave2-loading-verification-design.md` 的 D89（`.Config` 忘写的静默点收口形态——本文 §3.3 的同构先例）与 D72/D74（比对域、版本闸与「读取处即闸」）。
历史决定 D1–D89 不改史，本文决定从 **D90** 续号。

**本文的地位**：兑现 §12.3 的 M3 行（第三项经核查否决）。凡本文与磁盘代码冲突处以磁盘为准（仓库惯例）；本文与架构文档冲突处以本文为准并回头挂勘误（第 7 节清单）。

> **[grilling 改判注（2026-09-26）]** 三轮事实取证推翻了本 spec 初稿的四处前提，逐条改判如下：
> ① **属主追踪器整笔撤掉（D97）**。初稿把「归属化残留点名」当成新收益——**错**：`PodTestPeer::InjectLeakedScope` 造的本就是真 `EffectScope`（真 `ScopePool`、真计数、走完整 `CreateRaw`），报告内容**今天已是**「owner + 活的 `EffectCount`」。而真泄漏的网是 `~PluginHost` 的 Debug 五项归零断言（`PluginHost.cpp:211-221`），比按 Pod 分桶更硬；「拆局漏 Dispose」那一支已由 `PodReport::Clean()` 的 `CountersDiff.Scopes` 覆盖。加上 §4.2 的可达性核查（插件拿不到 Pod 的 `ScopePool`），追踪器**不新增任何可达的检出情形**。故 M3 = **3a + 3d 两笔**。
> ② **初稿 §4.5「测试缝语义反转」不成立**。把 `InjectLeakedScope` 改成「造一只真孤儿」会推进 `Scopes`/`Effects` 且永不归还 ⇒ Debug 下必触发 `~PluginHost` 的断言——那个形态**在结构上不可表达**（`PodTestPeer.h:3-6` 已自述）。测试缝原样保留。
> ③ **初稿 §2.2 的邻居地址判据（R1 Q2 选项 c）撤销**。`HotSwapLoopTests.cpp:133-134` 已有实测结论：`// B 实例指针在这里不能判别：新对象会落回被释放的堆块（实测同一二进制时红时绿）`。不再引入。
> ④ **初稿 §2.3 点错了证人**。`Eject.UnknownOrStaleRejected`（`EjectTests.cpp:113-120`）**一个字都不匹配消息文本**，只钉 `IsOk()==false`。`not in pod` 的真实证人只有 `RecursiveTeardownTests.cpp:100`（空壳）与 `Tools/VaseConsole/CMakeLists.txt:432` 的 `unknown_id.txt` 回放（未知 Id）——而 `PluginHost.cpp:822` 是**同一条分支**吞下这两种情形，代码分不开。D94 只会动到前一个证人。
> ⑤ **两份 `PrintEjectReport` 从来只打 6/14 个字段**（注释却自称「把报告的每个判据都打出来」）；`Status`/`Consumers` 在 Console 侧由调用方兜住，三档一字段（`LedgerHadNoIncomingEdges`/`ScopeEmptied`/`CrossPodInstancesZeroed`）**在任何通道下都不打印**。见 D100。
> ⑥ **比对器没有「全字段」的机制守卫**：`ManifestCompare.h:13` 只有一句注释自述，全仓无 static_assert、无字段枚举、无「漏一个即红」的测试。见 D101。

---

## 0. 范围

**M3 做**（描述符动、清单动、Catalog 动、Host 动、前端动、测试补）：

1. **判据 3a 全量形（§2）**：三插件局 50 轮 Eject/Adopt，断言相邻实例**从第一步起全程未受扰**（新 fixture `NeighborC` 使「其余实例」复数可断言；每轮派拍，见 D98）。
2. **空壳可拆（§2.3，D94）**：`EjectPlugin` 的 ①' 分支扩为「Failed 记录 ∪ 空壳条目」，三档证据与跨局闸同一套。
3. **判据 3d（§3，D91/D92/D99）**：`ProcessStateDesc` 进描述符、`kHeaderVersion` **3→4**、Eject 按「无活实例」口径自动 Reset 并写入 `EjectReport`，三条 Eject 分支一律跑。
4. **清单侧 `processStates`（§3.3，D93 + D101）**：顶层新键 + Name 集双向等值比对（D89 同构），外加构造期绊线。—— 这一笔把「作者忘写 `.ProcessStates`」从静默点变成主链上加载被拒。
5. **前端（§3.5，D100）**：两份 `PrintEjectReport` 补 `ProcessStatesReset` 打点，并把它们自述「每个判据」的注释改准。

**M3 不做**（各归其位，不许顺手）：

| 项 | 去向 |
|---|---|
| **完整属主追踪器** | **经核查否决（D97）**——不新增可达检出情形，且其宣称的两项收益今天都已存在（§4） |
| `ProcessStateDesc.Describe`（供诊断展示） | 判据 3d 不要它、无消费方；YAGNI，需要时另起一次 bump（D92） |
| 服务/订阅的**逐条**点名 | 要动 `IEffect`（又一个 ABI 面），收益只是更细；且随 D97 一并失去动力 |
| 三档一字段与 `RemovedEdges` 的打印缺口 | 独立的取证决策（Console 回放资产要重取），另立一笔（D100） |
| 对插件作者的可达执法（§9.3 三条） | **无可达形态**（§4.2 的核查结论），§12.1 判据 19 维持「无法自动化验证」 |
| 级联热替换（档 ②）、`epoch` | §13.4 已明文否决，M3 不动 |
| macOS 证据链、Android/iOS | M5 |

---

## 1. 决定表

| # | 决定 | 依据 |
|---|---|---|
| D90 | **M3 范围 = 3a + 3d**（第三项属主追踪器经 D97 否决）。两笔在同一分支、收口一轮六线验证里收 | §12.3；D97 |
| D91 | **Reset 触发条件 = 这份 binary 在任何 Pod 都无活实例**（`crossPodInstances == 0`），**不看镜像卸没卸**；调用点在 `Loader.Unload` 之前（Reset 是镜像内函数） | §9.1 的依据是档三「重载如同首次」，那件事以**实例**为计量；同时避开「清掉别局正在用的共享进程级状态」 |
| D92 | `ProcessStateDesc{ std::string_view Name; void (*Reset)(); }` 最小形，**`Reset` 返 `void`**、无 `Describe`；`PluginMeta` **尾追加** NSDMI 槽 + **`kHeaderVersion` 3→4** | YAGNI；§9.1 已把幂等写成契约；NSDMI + 尾追加 = 既有站点零改动（D26/D75 老办法） |
| D93 | 清单顶层加 `processStates` 字符串数组，比对按 **Name 集双向等值**（多重集、序不敏感）；**`schemaVersion` 保持 1**（纯追加，旧清单缺该键 = 空集，与描述符空集相等） | D89 同构：`.Config` 的静默点正是靠「清单也带」才被收口；D64 的闸是主版本，纯追加不构成主版本变更 |
| D94 | **空壳可被 Eject**：①' 分支从「只认 `FailedBinaries`」扩为「Failed 记录 ∪ 空壳条目」，三档证据与跨局闸同一套。连带 `not in pod` 的子串覆盖面收窄（该分支此后只剩「未知 Id」） | §5.6 四条补角里 Failed 本就可 Eject，而它与空壳同是「无实例的账目残留」；今日两者待遇不同，且空壳挡 Adopt 的 Id，使「本局重试失败插件」只对 Failed 开放 |
| ~~D95~~ | **已作废（grilling 改判）**——原为「属主追踪器取归属化口径」。作废理由见 D97：那个口径**今天已经是事实**，不是待办 | 事实取证①：`InjectLeakedScope` 造的是真 Scope、报告已是 owner + 活条数 |
| ~~D96~~ | **已作废（grilling 改判）**——原为「正查不变式 + 测试缝退役」。作废理由：正查不变式与残留报告是同条件（Debug 下断言先炸、报告填不出），且真泄漏已由 `~PluginHost` 的 Debug 五项断言覆盖；「测试缝反转」在结构上不可表达 | 事实取证② |
| D97 | **完整属主追踪器不做**。理由三条：① 它宣称的「归属化」今天已在（事实①）；② 真泄漏的网是 `~PluginHost` 的 Debug 五项归零断言（`PluginHost.cpp:211-221`），「拆局漏 Dispose」已由 `PodReport::Clean()` 覆盖——它不新增可达检出情形；③ §4.2 的可达性核查表明插件侧「绕道注册」无形态。**关账，不建机器**：结论写进 §9.3 勘误，磁盘上五处「属 M3 完整属主追踪器」的注释一并更正 | 事实取证①；§12.1 判据 19；仓库先例：无证人的那一步要明说（Solve Notes 键契约那笔） |
| D98 | **3a 的 50 轮每轮派固定 k 拍、循环内每轮断言** B/C 的累计 `Beats()` 与 A 的行为；**不用实例地址做判据** | 不派拍则等式两端皆 0、断言恒真（`TimerNoReentryTests` 注释点名的假绿世界）；地址判据已被 `HotSwapLoopTests.cpp:133-134` 的实测否决 |
| D99 | **Reset 在三条 Eject 分支（活实例 / 空壳 / Failed）卸货前一概跑**，条件同一个（这份 binary 在任何 Pod 无活实例） | §9.1 的幂等契约的用途正是不区分「用没用过」都能安全重置；只跑活实例那一支会留下「OnStart 失败留了半初始化态、又永不被清」的口子 |
| D100 | **前端只补 `ProcessStatesReset`**（有内容才打，空则整行不输出，与 `HotSwapNote` 同形）；三档一字段与 `RemovedEdges` 的打印缺口另立一笔。**顺手把两份打印器自述「把报告的每个判据都打出来」的注释改准**（事实⑤：实际 6/14） | 报告加了字段而前端不打，本波的肉眼证人缺一半；三档一字段属独立取证决策，不搭顺风车 |
| D101 | **`ManifestExpectation` 去掉各成员的 NSDMI**，让 `-Wmissing-designated-field-initializers`（本工具链是 error）在**每个构造点**强制写全字段——加字段就编不过，构造点即绊线。**边界写明：它只守构造、不守比对**；「比对器漏读无机制可拦」记进 §13.3 | 事实⑥：今天全靠人读；而 M3 正要加第一个新字段，是这条纪律最脆的时刻。相比「字段清单 golden 测试」，它是零成本的真绊线，且不制造第二份人工知识的副本（规矩 7） |
| D102 | **提交形态**：分轮提交（便于回退定位），**收口时折成一笔**；六线全量只在收口跑一轮，每笔按规矩 6 跑对应选择子与分层 | 与 M2b 波 2 同例；五笔互相有依赖（清单闸等描述符槽、空壳等 Reset 挂点），分轮提交让回退有落点 |
| D103 | **50 轮保留为单一用例**，先实测耗时并把数字写进 §6 的风险行；若超预算再回来改判，改判记进 plan 的偏离登记。**不预先降轮数、不拆成多条短局** | 仓库既有做法是「先让证据说话」；拆成多条各 10 轮的短局测的是「反复建销」而非「同局内反复进出」，是另一件事 |

---

## 2. 判据 3a 全量形

### 2.1 三插件局与 `NeighborC`（新 fixture）

§12.1 判据 3a 要的是「**其余实例**的计数与行为不变」——复数。现有局只有 `VersionedA`（叶，换件对象）+ `NeighborB` 两只，断言对象不足。

**决定**：新增 `Tests/HotSwap/fixtures/NeighborC`，与 `NeighborB` **同形**（心跳 + 一个自有服务）、另一个 `Id`（`Vase.NeighborC`）。**不借 `TimerPlugin`** 当第三方——它的目的是 3c 的无再入证人，借来会把那族的语义搅混。

**服务标识必须自带**（不是选择题）：`ServiceRegistry::Add` 对重复键在两个构建里都 `ProgrammerError`（「一服务一实现」），一个 Pod 里两个 `IHeart` 提供方当场终止。故 `NeighborC` 不能复用 `samples_fixture::IHeart`，要有自己的接口类型与 `kName`。**事件可以共用** `samples_fixture::TickEvent`——订阅没有唯一性规则。

代价（须落账）：tidy 文件数 Win 93→94 / Linux 94→95；六线 `ctest -N` 基数各 +N；CLAUDE.md 的基数表与 tidy 表随本轮更新。

### 2.2 5 轮 → 50 轮（D98）

**改 `HotSwap.FiveRoundsBehaveLikeFirstTime` 的常量**（并改名 `FiftyRoundsBehaveLikeFirstTime`），不新增用例——两条用例测同一件事是重复登记。

每轮的断言形态：
- **每轮派固定 k 拍**（题面取 2），50 轮共 100 拍；
- **循环内每轮**断言 B、C 的 `Beats()` 恰为**累计**拍数——「没被碰过」因此是一条非零两端的等式，且失败能定位到轮次；
- A 的行为按 `prime` 翻转（现值 1 / 2）；
- 每轮走完 `Eject → 覆盖 → Adopt` 三段，覆盖那一步本身仍是「锁真解了」的断言（Windows 上的 sharing-violation 探针，Linux 上靠档三与清单比对——平台不对称的判读照旧，规矩 6）。

**不用实例地址做判据**：`HotSwapLoopTests.cpp:133-134` 已有实测结论——新对象会落回被释放的堆块，同一二进制时红时绿。判据由精确计数承担。

### 2.3 空壳可拆（D94）

**现状**：`EjectPlugin` ①' 分支只查 `pod.FailedBinaries`；空壳（级联拆除留下的 `Instance == nullptr` 条目、镜像在架、不在 `FailedBinaries`）落进 `not in pod` 的 `Err`。注意 `PluginHost.cpp:822` 是**一条分支吞两种情形**（未知 Id ∪ 空壳），代码分不开——这是 D94 必须动它的原因。

**改成**：①' 分支匹配「Failed 记录 ∪ 空壳条目」。空壳的拆除 = 从 `Instances` 摘掉条目 + 走同一套全局闸（③ 的跨局持有者计数）决定是否 `Unload`，报告 `HotSwapNote` 注明「torn shell ejected」；三档证据字段按既有 Failed 分支的同形填（无实例 ⇒ 必无入边、必无存活 Scope）。

**连带（已核，两个证人的覆盖面不同）**：

| 证人 | 触发情形 | D94 之后 |
|---|---|---|
| `RecursiveTeardownTests.cpp:100`（`EXPECT_NE(...find("not in pod"), npos)`） | **空壳** | **必改**——空壳不再走这条字符串。改按报告字段断言（`Ok` + `Status == kEjected` + 三档证据） |
| `Tools/VaseConsole/CMakeLists.txt:432` 的 `VaseConsoleRefusedReason`（`unknown_id.txt` 回放） | **未知 Id** | **不受影响**——该分支此后只剩它这一支 |

另两条**不构成证人**（初稿点错的地方）：`Eject.UnknownOrStaleRejected`（`EjectTests.cpp:113-120`）只钉 `IsOk()==false`、不匹配文本；`eject on stale pod handle` 走的是更早的句柄闸（`PluginHost.cpp:748-752`）且全仓**无文本证人**（`PluginHost.h:72` 自述「前者无匹配方」）。

同 Id 再入局的路径**已存在**：`RecursiveTeardownTests.cpp:102` 用**手写期望**（`AdoptExpectations.h` 的 `MakeBehindStrictUnusedExpectation`）喂 `AdoptRequest`，不依赖 JSON 清单——所以「空壳 Eject 后同 Id 可再 Adopt」可直接写成断言。

**规矩 6 的子串契约清单要同步改**：`not in pod` 此后只剩「未知 Id」一支。`CLAUDE.md` 规矩 6 那节与 `PluginHost.h:70-72` 的契约注释同步改字。

---

## 3. 判据 3d · 进程级状态登记（描述符 + 清单）

### 3.1 描述符面（D92）

新 `struct ProcessStateDesc`，住 `Include/Vase/PluginDescriptor.h`，与 `ServiceRef` 同格（它是**作者侧**描述符面，不是宿主面——`Host/` 下是宿主面）：

```cpp
struct ProcessStateDesc
{
    std::string_view Name;   // 点名与比对用（§3.3）
    void (*Reset)();         // 幂等契约（§9.1）；返 void——没有可恢复的失败形态
};
```

纯 POD、函数指针（**不放 `std::function`**：跨 DLL 且描述符是只读数据段，M5 的 `VaseCli scan` 要只读数据段取全量元信息）。

`PluginMeta` **尾追加**一槽：

```cpp
MetaArray<ProcessStateDesc, 16> ProcessStates{}; // NSDMI 为刻意：既有站点零改动
```

NSDMI `{}` 是指定初始化省略豁免的前提（同 `OptionalRequires` / `Config` 的先例，`-Wmissing-designated-field-initializers` 在本工具链是 error）。

**`kHeaderVersion` 3→4**（唯一理由 = `PluginMeta` 布局变更）。连带：所有 fixture / 示例 / 消费方重编（源码构建，无签入产物需改）。

### 3.2 作者侧写法

```cpp
VASE_PLUGIN(CombatPlugin){
    .Id = "Vase.Combat",
    // ...
    .ProcessStates = {{ .Name = "Vase.Combat.TypeRegistry", .Reset = &TypeRegistry::Clear }},
};
```

**内联花括号形**，与 `Requires` / `Provides` 的既有写法同形。理由：`MetaArray` 只提供 `initializer_list` 构造函数，**没有**「从 C 数组合成」的入口（`MetaArray.h` 磁盘为准）——所以「作者侧 `constexpr` 数组 + 传引用」那套是 `FieldInfo::Choices` 的形态（它存的是指针 + 条数，D76），**不适用于**自持存储的 `MetaArray`。

`kProcessStates` **不走宏生成**：宏实参里判不出元数（D77 的计划期改判同因）。

两条作者侧约束（与 `ServiceRef` 同格）：
- `ProcessStateDesc` 两字段都是必写项（`Name` 无 NSDMI → 省略即 `-Wmissing-designated-field-initializers` error）。这是刻意的：`Name` 是 §3.3 比对的身份，不能默认。
- 超过 16 条是**编译错误**（`MetaArray` 的容量出口），不静默截断——同 `Requires` / `Provides` 的既有约定。

### 3.3 清单面（D93，D89 同构）+ 构造期绊线（D101）

清单顶层新增 `processStates` 键：**字符串数组**，内容是各 `ProcessStateDesc::Name`。

- `OnlyKeys` 白名单加 `processStates`；缺键 = 空集（向后兼容，`schemaVersion` **保持 1**）。
- `ManifestExpectation` 加 `std::vector<std::string> ProcessStates`。
- 比对器（`Source/Host/Detail/ManifestCompare`）按 **Name 集双向等值**：多重集、序不敏感、含条数。域规则与 D72 的 `requires/provides` 三类同格。

**构造期绊线（D101）**：`ManifestExpectation` 去掉各成员的 NSDMI（连带删掉 `NOLINTBEGIN(readability-redundant-member-init)` 那一对——它们的理由随 NSDMI 一起消失）。此后**每个**构造点省略任何字段都是编译错误，加字段即编不过。**边界**：它只守构造、不守比对——比对器仍可能忘读新字段，那一条**无机制可拦**，记进 §13.3。现状是「靠人读」（`ManifestCompare.h:13` 只有一句注释自述「全字段严格等值」，全仓无 static_assert、无字段枚举、无「漏一个即红」的测试）；本决定把构造那一半机器化，比对那一半如实入档。

**清单改动面（预期很小，别照「清单也带」四个字放大）**：现有六份签入清单（`Tests/Integration/fixtures/manifests/*/plugin.json`）对应的 fixture **都没有进程级状态**，缺键 = 空集 = 与描述符空集相等 → **一份都不用改**，`schemaVersion` 也不动。这一笔真正需要的是一只**带进程级状态的 fixture 且其清单列名**作正面证人，另加一条**清单侧篡改**用例作反面。

> **计划输入（实施时要核的耦合）**：这六份清单是**纯清单 fixture 复用既有 DLL**——所以给某只既有 fixture 的描述符加进程级状态，会让**每一份指向那只 DLL 的清单**都必须列出该名字。选哪只 fixture 时先数它的清单引用数，倾向选**只被一份清单引用**的那只；`Tests/HotSwap/fixtures/` 侧供 Eject/Reset 那组用例，不需要清单。

**收口效果**：作者忘写 `.ProcessStates` 而清单列了名字 → 加载期比对拒（主链上），不再是静默失效。这是 D89 对 `.Config` 的同构做法。

### 3.4 Host 侧登记与 Reset（D91/D99）

- `detail::BinaryRecord` 加「进程级状态表」面：描述符数组的指针 + 长度，**随记录生死**，不复制（镜像驻留 ⊇ 记录存活）。
- **Reset 的调用点**：`EjectPlugin` 里、`Loader.Unload` **之前**（Reset 是镜像内函数，镜像一卸就调不到）。
- **触发条件**：`crossPodInstances == 0`（这份 binary 在任何 Pod 都无活实例）。注意这个条件在**真卸货**那条分支上是**恒真**的（否则早已提前返回），所以它的实际约束力全在 **kept-resident 分支**：别局只剩 `Failed` 记录/空壳时不阻止 Reset（无人用），而别局有活实例时不 Reset。
- **覆盖面（D99）**：三条 Eject 分支（活实例 / 空壳 / Failed）一律跑；空壳与 Failed 尤其需要——OnStart 失败那类**跑过 OnLoad**，进程级状态可能是半初始化态。
- **报告**：逐条把 `Name` **复制**进 `EjectReport::ProcessStatesReset`（报告是返回值，必须拥有——`FieldInfo` 侧的 `string_view` 借用教训同格）。
- **合法组合要写明**：`ProcessStatesReset` 非空与 `BinaryActuallyUnloaded == false` **可以并存**（kept-resident 分支 + 只被 Failed 记录持有）；报告读侧不得把两者当互斥。
- 未走 Eject 的 binary，其 `.Reset` 仍是宿主手工调用（§9.1 跨局语义原样）。

### 3.5 前端（D100）

两份 `PrintEjectReport`（`Tools/VaseConsole/Console.cpp:153`、`Samples/Embedding/main.cpp:133`）补 `ProcessStatesReset` 打点，**有内容才打、空则整行不输出**（与 `HotSwapNote` 同形）。空壳注记走既有 `note:` 行，**零代码**。

**顺手改准两处注释**：它们自称「把报告的每个判据都打出来」，实际只打 14 个字段里的 6 个（`Status`/`Consumers` 在 Console 侧由调用方兜住；`LedgerHadNoIncomingEdges`/`ScopeEmptied`/`CrossPodInstancesZeroed` 在任何通道下都不打印）。注释改成如实描述打了哪些、哪些不在本函数；缺口本身另立一笔（§0 的「不做」表）。

`wiki/vase-console-use.md` 的回放示例按规矩重取（它是实测截取）。

---

## 4. 属主追踪器：核查结论 = 不做（D97）

这一节记录**为什么不做**，供日后有人再提时省一次核查。

### 4.1 它宣称的两项收益今天都已存在

| 宣称 | 事实 |
|---|---|
| 「把残留从计数差分升级为归属点名」 | `PodTestPeer::InjectLeakedScope`（`PodTestPeer.cpp:38-47`）造的是**真** `EffectScope`——挂在 Pod 真的 `detail::ScopePool` 上、经 `CreateRaw` 取真内存、推进真计数；`DestroyPod` 的 `Residuals` 填的就是它的 `OwnerLabel()` 与**活的** `EffectCount()`。归属点名**今天就是事实** |
| 「正查不变式：抓框架自身漏拆」 | ① 真泄漏（永不析构的 Scope）已由 `~PluginHost` 的 Debug 五项归零断言把守（`PluginHost.cpp:211-221`）——它比按 Pod 分桶更硬；② 「拆局漏 Dispose」那一支已由 `PodReport::Clean()` 的 `CountersDiff.Scopes != 0` 覆盖；③ 而初稿那条「拆完本 Pod 桶必空」的断言与 §4.2 的残留报告是**同一个条件**——Debug 下断言先炸、报告永远填不出，两者不能都成立 |

### 4.2 插件侧「绕道注册」没有可达形态

插件要制造「宿主不认识的、且参与 Pod 注册的 Scope」，必须拿到 **Pod 自己的 `ScopePool`**；而 `Pod::Pool` 私有、`Context` 不暴露它，插件只有 `ctx.GetScope()`（拿到的是那只已归位的 Scope 引用）。`detail::ScopePool` 虽是公开可默认构造的导出类，但自己 new 一个池等于造一只**完全游离的** Scope——它的 Effect 不进任何 `ServiceRegistry` / `EventBus`，肉眼不可见、也无害。

因此 §9.3 的三条仍是**契约束**，§12.1 判据 19 仍是「无法自动化验证」。这一笔**关账**：把结论写进 §9.3 勘误，并更正磁盘上五处按 M3 归属它的注释（`Include/Vase/Effect/EffectScope.h`、`Include/Vase/Pod/Pod.h`、`Include/Vase/Host/Evidence.h`、`Tests/TestingSupport/PodTestPeer.h`、`Tests/Lifecycle/DiagnosticAttributionTests.cpp`）与技能 `references/architecture.md` 的同口径句。

### 4.3 测试缝原样保留

`Pod::LeakedScopesForTest` 与 `PodTestPeer::InjectLeakedScope` **不动**。初稿设想的「反转成语义制造真孤儿」在结构上不可表达：那只 Scope 拿的是活 host 的 `Counters` 指针，永不归还就会在 Debug 下触发 `~PluginHost` 的断言（`PodTestPeer.h:3-6` 已自述这一点）。它今天的定位（`#10/D14` 的**报告机器**证人）就是它该有的定位。

---

## 5. 测试分层与验收面

| 判据 / 面 | 用例位置 | 形态 |
|---|---|---|
| 3a 全量形 | `Tests/HotSwap/HotSwapLoopTests.cpp` | 三插件局 50 轮；每轮派 2 拍、**循环内**断言 B/C 累计 `Beats()`；A 行为每轮翻转 |
| 3a 空壳可拆（D94） | `Tests/Integration/RecursiveTeardownTests.cpp` | 空壳 Eject → `Ok` + `kEjected` + 三档证据；同 Id 可再 Adopt（手写期望路径） |
| 3d Reset（D91/D92/D99） | `Tests/HotSwap/`（新用例） | 有进程级状态的 fixture：Eject → `ProcessStatesReset` 点名 + 状态真归零；三条分支（活实例 / 空壳 / Failed）各验 |
| 3d kept 判读 | 同上 | 别局有活实例 → **不** Reset；只剩 Failed/空壳 → Reset（且此时 `BinaryActuallyUnloaded == false` 与 `ProcessStatesReset` 非空并存） |
| 3d 清单闸（D93） | `Tests/Integration/LoadTimeComparisonTests.cpp` | 清单 `processStates` 与描述符 Name 集不等 → `CreatePod` 走 Failed 记录拒；Adopt 侧走 `Err`（D70 通道分置） |
| 描述符布局 | `Tests/Unit/DescriptorTests.cpp` | `ProcessStates` 槽的偏移/默认值；NSDMI 零改动断言 |
| 构造期绊线（D101） | `Tests/Unit/` 或编译期 | 无运行时证人可写（它是**编译错误**）——按仓库口径明说：探针式验证，记在 plan |

**验收口径**：六 preset 全绿 + 各线 `ctest -N` 与基数表逐位对上 + 三条 tidy 线正文双 0 + format 干净。改动命中**描述符布局**与 **`kHeaderVersion`** → 规矩 6 强制：`-R 'HotSwap|Eject|Adopt'` 选择子**双平台各自留证据**。

---

## 6. 风险与预期中的行为性红

| 风险 | 处置 |
|---|---|
| `kHeaderVersion` 3→4 | 描述符布局变更，**所有插件重编**。清单侧不受影响（`schemaVersion` 独立，D93）。预期红：任何硬编码 3 的断言 |
| 空壳 Eject 语义变更（D94） | `not in pod` 覆盖面变小；预期红：`RecursiveTeardownTests.cpp:100` 那条断言 |
| `ManifestExpectation` 去 NSDMI（D101） | **每个构造点**省略字段即编译错误——预期红：`Source/Catalog` 与 `Tests/` 现有构造点；这正是绊线要的效果，修法是把字段写全 |
| 50 轮 × 覆盖写在 Windows 上 | 每轮一次真实 `LoadLibrary`/`FreeLibrary` + 一次真实文件覆盖。**先测**，把实测耗时数字写进本行；超预算再回来改判（D103），改判记进 plan 的偏离登记 |
| 新 fixture `NeighborC` | 六线基数与 tidy 文件数 +1；CLAUDE.md 两张表随本轮更新 |
| 五处「属 M3 完整属主追踪器」的注释 | 随 D97 一并更正（评论改动，无行为） |

---

## 7. 文书义务（验收时同步；规矩 7 口径：只住 CLAUDE.md 的真值不复制进技能）

1. `wiki/vase-architecture.md` §12.1 判据 **3a / 3d** 的「未落（M3）」→「已落」并点名证人。
2. §12.3 的 M3 行**改回「3a + 3d」**，并加注：完整属主追踪器经核查否决（D97），理由指向本文 §4。
3. §9.3 加勘误：**属主追踪器已核查、结论不做**——三条契约仍不可自动化验证，且插件侧「绕道注册」无可达形态（§4.2）。
4. §13.3 加两条：（a）`.ProcessStates` 已按 D89 同构收口，不再是静默点；（b）**比对器漏读新字段无机制可拦**（D101 的边界——构造期有绊线，比对期没有）。
5. §9.1 的 `.Describe` 标「M3 裁定不做（D92）」。
6. 代码注释五处（§4.2 清单）+ 技能 `references/architecture.md` 的同口径句。
7. `CLAUDE.md`：项目状态（M3 完成 + 本波两笔）、六线基数表、tidy TU 数与抑制合计、`kHeaderVersion` 3→4 的记述、**规矩 6 的子串契约清单**（`not in pod` 那半）、`PluginHost.h:70-72` 的契约注释。
8. memory 的 `vase-v3-hotswap-status` 随收口更新。

---

## 8. 与其他里程碑的边界

- **M4（热替换）**：本文不碰双版本 fixture 的三平台档二/档三扩谱，也不碰「语义依赖提示面」。50 轮循环用现成 `VersionedA` / `VersionedAPrime`，不扩版本谱。
- **M5（平台收口）**：`ProcessStateDesc` 的 POD 形态是为 `VaseCli scan`（M5，只读数据段取元信息）预留的，但本文不实现该工具。
