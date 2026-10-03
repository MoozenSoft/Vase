# Vase M4：热替换全谱（换件谱扩描述符维 / Windows 档三负例 / 语义依赖知情位）（spec）

**日期**：2026-09-27 · **状态**：brainstorming 定稿（需求方五问五裁）→ **grilling 两轮收敛**（R1 十一问 + R2 七问全裁，见下方裁定注） · **实施计划**：spec 复审通过后由 writing-plans 另立 · **分支**：`m4-hot-replacement`

**上游权威**：`wiki/vase-architecture.md` v3 的 §12.3 里程碑表（M4 行「v2『Reload』扩为 HotSwap 全谱：双版本 fixture、三平台档二/档三、语义依赖提示面」）/ §8.2（三档证据链与平台落点）/ §11.2（宿主重编译单个插件的四步流程）/ §9.1（进程级状态登记与「共享值必须走服务边」硬契约）/ §12.1（判据 3 与 3a）；
本文的**直接前身是一句代码注释**：`Tests/HotSwap/AdoptTests.cpp:249` 明文写「Windows 的映射文件覆盖语义另成一题——v3 §12.1『某平台不可行就回来改这节』同样适用于 Win 的 sharing 规则，**该侧端到端验证登记到 M4/M5**」——本文第 3 节就是还这笔账；
`2026-09-26-vase-m3-multi-plugin-design.md` 的 D91/D99（Reset 触发条件与三分支口径——本文 §4 的置位条件与它同构）、D100（报告加字段则前端补打点，本文 D111 同例）、D103（「先让证据说话」的实测纪律，本文 §6 与 D116 同例）。
历史决定 D1–D103 不改史，本文决定从 **D104** 续号。

**本文的地位**：兑现 §12.3 的 M4 行，**macOS 腿顺延 M5**（与 M0–M3 口径一致：Win / Linux 跑通，macOS 与移动端归 M5）。凡本文与磁盘代码冲突处以磁盘为准（仓库惯例）；本文与架构文档冲突处以本文为准并回头挂勘误（第 7 节清单）。

> **[事实取证注（2026-09-27）]** 本文的前提都是**读盘或实测核出来的**，不是推的：
> ① **`#ifndef _WIN32` 圈宽了**。`AdoptTests.cpp:194-288` 的闸把 `RenameReplacementCaughtByTierThree` 与 `MissingIdentityFeatureRejectedWithPointer` 一起圈住，但**前者不依赖任何 Linux-only fixture**——它用的 `Vase.LoadProbe` / `Vase.UnloadProbe` 两平台都有（`Tests/CMakeLists.txt` 里只有 `NoBuildIdPlugin` 在 `if(NOT WIN32)` 内）。前者被圈是顺带的。
> ② **缺身份特征的错误臂已存在**。`Source/Host/ImageInspectCommon.cpp:80` 的 `MissingCodeViewError()` 文案已指向 `/DEBUG:FULL`（同文件 `:88` 是 Linux 的 `--build-id` 臂）——Windows 侧对位**不需要新文案**。且解析器是**逐条找 `kDebugTypeCodeView`**、找不到才报错，故 MS 链接器线上 `/DEBUG:NONE` 残留的那条 `POGO` 条目会被正确跳过。
> ③ **50 轮循环的承重性质是「描述符逐字节不变」**。`HotSwapLoopTests.cpp` 的 `LoopAdoptRequest` 注释与 `AdoptExpectations.h:47` 都写明：A/A′ 描述符逐字节相同，**一份期望跑完 50 轮**。M4 补的是这条性质的**补角**，不是它的替代。
> ④ **`VersionedA` 目前没有 `Config`**。它的描述符只有 `Id` / `DisplayName` / `Version` / `Provides` 四项——描述符维谱能扩的维度是 `Version` 串与 `Provides` 版本两条，`Config` 默认值这一维要另起 `VASE_CONFIG` 机器（本文不做，见 §0 不做表）。
> ⑤ **换件语境的负例已经存在**。`Adopt.RequestDisplayNameDriftRejected`（`AdoptTests.cpp:365`）与 `RequestConfigKeyMissingInExpectationRejected`（`:378`）已走真 Adopt 轨证明「期望与二进制描述符不符 → `manifest/binary mismatch` 拒」。本文的负例与它们的差别只在**漂移方向**（那边改期望、这边换字节），故 §2 把重型放在**正例**（D105）。
> ⑥ **`AdoptImpl` 的判定序是 `EnsureResident` 先于 `CompareDescriptor`**（`PluginHost.cpp:1027` vs `:1046`）。故「用错期望 Adopt 被拒」这一次调用**已经把新二进制装载驻留了**——它「留在局外」但**不是「什么都没发生」**。这条直接决定 §2.3 的事件序。
> ⑦ **P1 探针（实测，2026-09-27）**：Windows **允许**重命名一个正被 `LoadLibrary` 映射的 DLL；改名后原路径空出、落新字节成功——「驻留镜像 = 旧字节、同名路径 = 新字节」在 Windows 上可造。**但直接覆盖不行**：文件仍映射在原路径上时 `MoveFileExW(REPLACE_EXISTING)` 报 `ERROR_ACCESS_DENIED`、`Copy-Item -Force` 报 `ERROR_SHARING_VIOLATION`、`Rename-Item -Force` 报 `ERROR_ALREADY_EXISTS`。所以可行路线是「先改名腾路径、再落新字节」两步，**不是我们选的，是平台只剩这一条**。附带：改名后 `GetModuleFileNameW` 仍返回**原路径**（此刻已是新字节）。
> ⑧ **P2 探针（实测，2026-09-27）**：`/DEBUG:NONE` 后手压过 `/DEBUG:FULL`——lld-link 线调试目录整个为空；MS `link.exe` 线 CodeView/RSDS 消失。语义是严格的「**后写的赢**」，不是「NONE 恒赢」（反序照样产出 RSDS）。仓库侧 `LINK_OPTIONS` 排在 `CMAKE_*_LINKER_FLAGS` 组之后是 **CMake 生成器保证**（M1 实验已用 GNU ld 钉死，见 `docs/superpowers/plans/2026-09-16-vase-m1-pod-loop-hotswap.md:4698`），这条保证与「后写者胜」合起来给出 Windows 侧的完整链条。

> **[grilling 裁定注（2026-09-27）]** R1（Q1–Q11）与 R2（Q12–Q18）全部裁可，逐条落点：
> **腿一改重型（D105）**：负例与既有 `RequestDisplayNameDriftRejected` 同形，边际信息少，重型移到**正例**；负例降级为每级的附属断言，且用**一维 tamper** 构造（把本级期望沿本级那一维改回上一级的值），不依赖「上一级整份期望」这个形态。
> **阶梯形态（D113）**：一条用例走完，含**回滚级**（Q4）；回滚**拆两步**以维持 D105 的「每级只差一维」不变式（写回时发现原案一次退两维会破坏该不变式）；每级三条证人（Q11 选 (c)）——负例、复用分支正例、全新装载正例，`ReusedResidentImage` 的真假两值都有证人。
> **知情位口径（D110/D111）**：「还有别人」只数**活实例**（Q5，空壳与 `Failed` 记录攥不住值，且与 D91 的 Reset 闸同口径）；**`Status == kRejectedConsumers` 时恒 `false`**（Q6）；前端**只在 `kEjected` 时打**（Q7，bool 没有「空」，靠「没打 = 这局没成功」区分）。
> **fixture 命名（D106/D109）**：新版本用描述性名 `VersionedAStampDrift` / `VersionedAServiceDrift`；`NoBuildIdPlugin` 全改名为 `NoIdentityPlugin`（含 Id 与事件名），理由是它的存在理由本就是平台中性的「身份特征缺失」（Q10/Q12）。
> **`P.old` 清理（D114）**：扩 `ProbeSwapGuard`（M1 立它就是为「失败早退不留脏树」），不改用工作副本目录（会牵动 `EnsureResident` 的绝对路径去重键）。
> **判据 3f（D115）**：新增一行，不与 3d 合并。
> **耗时实测（D116）**：照 M3-D103，先测后写进 §6。

---

## 0. 范围

**M4 做**（fixture 动、测试动、报告动、前端动，**描述符布局与 `kHeaderVersion` 都不动**）：

1. **换件谱扩描述符维（§2，D105/D106/D113）**：五步阶梯 `VersionedA → VersionedAPrime → VersionedAStampDrift → VersionedAServiceDrift → 回退两步`，**每步只差一维描述符**（含回退方向）；每步三条证人。2 个新 fixture、2 条新期望工厂。
2. **Windows 侧档三负例（§3，D108/D109/D114）**：还 `AdoptTests.cpp:249` 那笔登记账——把「驻留镜像是旧字节、磁盘同名路径是新字节 → 档三必须拒」在 Windows 上做出来（P1 已证可行）；连带 `NoIdentityPlugin` 跨平台化（P2 已证可行）。
3. **语义依赖知情位（§4，D110/D111）**：`EjectReport` 增一个 bool，如实上报「本次重置了进程级状态，且进程内仍可能有持派生值者」——**不假装查过**（图上本就不可见）。

**M4 不做**（各归其位，不许顺手）：

| 项 | 去向 |
|---|---|
| **macOS 腿**（档二 `_dyld_image_count` / 档三 `LC_UUID` / dyld4 neverUnload 实测） | M5，与 M0–M3 同口径（§8） |
| **行为维谱**（多几个只差代码行为的版本、50 轮里交替 A/A′） | 与现有 50 轮循环同质，多出来的是轮数不是覆盖（D105） |
| **三档齐步走链**（把散在 `EjectTests` / `AdoptTests` 的孤立断言串成一轮链） | 需求方 2026-09-27 裁：本波只补覆盖，不改形态 |
| **`Config` 默认值维漂移** | 要另起 `VASE_CONFIG` 机器（事实取证注④），收益与 `Provides` 版本维重复；YAGNI |
| **插件自述「哪些进程级状态被跨插件共享」** | 知情位取「如实上报不可知」形态（D110）；自述面会给人「查过了」的错觉，与 D97 否决属主追踪器同一类论证 |
| **把「档二在 Windows 上被蒙骗」升格成断言**（事实取证注⑦） | tier-3 用例走 Adopt 轨、不产出 Eject 报告，硬要断言得配一次 Eject，会把用例从一件清晰的事变成两件事拼的（Q16）——写进用例注释当存在理由即可 |
| **三档一字段与 `RemovedEdges` 的打印缺口** | M3-D100 已另立一笔，M4 不搭顺风车 |
| 级联热替换（档 ②）、`epoch` | §13.4 已明文否决 |
| Android / iOS | M5 |

---

## 1. 决定表

| # | 决定 | 依据 |
|---|---|---|
| D104 | **M4 范围 = 三条腿**（换件谱描述符维 + Windows 档三负例 + 语义依赖知情位），**macOS 顺延 M5**。三条在同一分支、收口一轮六线验证里收。**〔勘误（2026-10-02 挂，不改史）：顺延的 macOS 腿已于 2026-10-02 落地、口径为 x64（需求方 2026-10-01 裁改、arm64 永久移出——macOS 腿 spec D157/D159：档二 `_dyld` 映射清单 / 档三 `LC_UUID` / 无对位承重 flag 由负例把守；本档头部地位句、不做表与 D104 的「macOS 腿」口径均按此读）〕** | §12.3；需求方 2026-09-27 裁；与 M0–M3 平台口径一致 |
| D105 | **腿一重型 = 正例，负例降级为附属断言**（事实取证注⑤：换件语境的负例已被 `Adopt.RequestDisplayNameDriftRejected` 覆盖，差别只在漂移方向）。换件谱 = **五步阶梯、每步只差一维**（含回退方向）；`PluginId` 全程恒为 `Vase.VersionedA`——「同一个插件的下一版」才成立换件语义 | 每步只差一维才可归因；正例证的是今天**零覆盖**的性质——「描述符也变了的版本能走完 Eject→换字节→换期望→Adopt」 |
| D106 | **新增 2 个 fixture**：`VersionedAStampDrift`（`Version` 1.0.0→1.1.0）与 `VersionedAServiceDrift`（`Provides` v1→v2）；`OUTPUT_NAME` 同为 `VersionedA`、各自独立 stage 目录（同 `VersionedAPrime` 的既有处理，否则 Ninja 报 `multiple rules generate …VersionedA.lib`）。命名取**描述性**（仓库习惯），不用 `V2`/`V3`——后者把「第几个版本」与「差哪一维」混在一个名字里，而本谱的全部意义就是每步只差一维。**不动** `VersionedA` / `VersionedAPrime` / 50 轮循环 | 现状那一步（rung0→rung1，描述符逐字节相同）已被 50 轮循环覆盖，M4 只把它当阶梯的**起始步**（D113） |
| D107 | **不新开测试 TU**：阶梯用例进 `Tests/HotSwap/HotSwapLoopTests.cpp`（同目录同主题）、知情位用例进 `Tests/HotSwap/EjectTests.cpp`（Eject 面）、Windows 档三改动落在既有 `Tests/HotSwap/AdoptTests.cpp`。**tidy TU 预期 +2**（两个新 fixture）+1（`NoIdentityPlugin` 上 Windows） | M3 先例：两条新用例住进既有 `EjectTests.cpp`；少一个 TU 就少一份 nlohmann/gtest 模板量的乘法 |
| D108 | **Windows 档三负例统一成「改名离开 + 落新字节」序列**（`rename(P → P.old)` 再 `copy 新 → P`），**两平台同一路径、不写 `#ifdef`**，并拆开 `#ifndef _WIN32` 的闸。**P1 已证**：Windows 允许改名映射中的 DLL，而直接覆盖被拒——这不是二选一，是平台只剩这一条路（事实取证注⑦） | 仓库明文偏好（`AdoptTests.cpp:393`「两平台同变换、不写 `#ifdef`」）；Windows 允许改名映射文件、**不允许删除或覆盖** |
| D109 | **`NoBuildIdPlugin` 跨平台化并改名 `NoIdentityPlugin`**（Windows 侧 per-target `/DEBUG:NONE`，Linux 侧维持 `-Wl,--build-id=none`），**不新开 Windows-only fixture**——两条既有 Linux-only 用例随之六线同跑，`linux − win` 的差值**由 +2 变 0**（如实登记，见 §6/§7）。**P2 已证**：`/DEBUG:NONE` 后手压过 `/DEBUG:FULL`（事实取证注⑧）。**全改**：fixture 目录/文件名、插件 Id `Vase.NoBuildId` → `Vase.NoIdentity`、事件名、CMake 变量 `VASE_FIXTURE_NOBUILDID` → `VASE_FIXTURE_NOIDENTITY`、期望工厂与用例名——半改会留下「名字说 BuildId、Id 说 NoBuildId」的错位。断言按平台取指路 token（`--build-id` / `/DEBUG:FULL`），与 `ReopenWritableIsMeaningful` 的既有处置同形。**该 fixture 自守**：标志一旦失效，产物就长出身份特征，Adopt 会走到装配并返回 `Ok`，`EXPECT_FALSE(r.IsOk())` 当场变红——静默失效不可能产生假绿（M1 report 已证此性质，Windows 对位同构） | 事实取证注②⑧；本腿的本意是**消掉**平台不对称，新开 Windows-only fixture 反而多一处；自守性质使「探针失效」这一风险自带警报 |
| D110 | **知情位 = `EjectReport` 新增 `bool SemanticDependencyPossible`**，置位条件 = **`ProcessStatesReset` 非空 ∧ 进程内任一 Pod 仍有活插件实例**。**只数活实例**——`Failed` 记录与级联空壳没有实例、攥不住派生值，且与 D91 的 Reset 闸「只数活实例」同口径（③ 闸要计空壳是因为它问的是「镜像能不能卸」，不是同一件事）。**`Status == kRejectedConsumers` 时恒 `false`**（没有 Reset 发生，`ProcessStatesReset` 必空）。**不 bump `kHeaderVersion`**——`EjectReport` 住 `Include/Vase/Host/`，不过插件 ABI 界 | §9.1「共享值必须走服务边」+ 提示面兜底；范围取「进程内」与进程级状态的真实作用域对齐，也与 D91 解 Reset 时「这份 binary 在任何 Pod 无活实例」的口径同构 |
| D111 | **前端两份 `PrintEjectReport` 补打点**（`Tools/VaseConsole/Console.cpp` 与 `Samples/Embedding/main.cpp`），**只在 `Status == kEjected` 时打**（含 `false`）——bool 没有「空」，靠「拒绝态整行不打」把「这次没走到这一步」与「走到了且判定无风险」分开 | D100 同例：报告加了字段而前端不打，本波的肉眼证人缺一半 |
| D112 | **提交形态**：分轮提交（便于回退定位），**收口时折成一笔**；六线全量只在收口跑一轮，每笔按规矩 6 跑对应选择子与分层 | 与 M2b 波 2 / M3-D102 同例；本文三条腿互相独立，分轮提交让回退有落点 |
| D113 | **阶梯 = 一条用例、五步、每步三条证人**。步序：`rung0 →(代码)→ rung1 →(Version)→ rung2 →(Provides)→ rung3 →(Provides 回)→ rung2 →(Version 回)→ rung0`；**回滚拆两步**以维持「每步只差一维」。每步事件序：`Eject → 落本级字节 → 负例（一维 tamper 的期望 → 拒）→ 正例甲（本级期望 → **复用分支**，`ReusedResidentImage == true`）→ Eject → 正例乙（本级期望 → **全新装载**，`ReusedResidentImage == false`）`。**正例甲之所以免额外往返**：负例那次调用已经把二进制装载驻留了（事实取证注⑥），所以紧接着换对期望必然走复用分支——`ReusedResidentImage` 的真假两值因此在一趟里都有证人（今天全仓只有 `Adopt.ReusedResidentImageStillVerifiesIdentity` 一条碰过真值）。阶梯带 B/C 邻居，每步断言邻居未扰 | 「反例先行」的副产品被显式收下而非静默接受（事实取证注⑥）；一条用例走完保住阶梯的**连续性**，定位靠 ASSERT 消息带步号（50 轮循环 `<< "round " << round` 的先例） |
| D114 | **`P.old` 残留归 `ProbeSwapGuard` 清理**：扩它的析构，删 `P.old` 并按需还原 `P` 的原字节。**不改用工作副本目录**——`LoadProbe` 的路径参与 `EnsureResident` 的绝对路径去重键（`Loader.h:86-89`），换目录会牵动 `Plan()` 与兄弟集，不值 | M1 report 已把「失败早退会把换过的树留给之后所有运行」点名为该用例的存在理由之一，守卫就是为它立的；新序列只是多一个要收的文件 |
| D115 | **`wiki` §12 判据表新增判据 3f**（语义依赖知情位的置位可证伪），**不与 3d 合并**——3d 管「Reset 真跑并写进报告」，3f 管「知情位置位条件可证伪」，两件事两处证人，合并会让 3d 变成复合体 | 判据表每行是「承诺 ↔ 验证方式」的一对一 |
| D116 | **阶梯用例的耗时先实测再落账**：把实测数字写进 §6 风险行；超预算再回来改判（改判记进 plan 偏离登记）。**不预先降步数、不预先砍负例** | M3-D103 同例：「先让证据说话」 |

---

## 2. 换件谱：描述符维（§11.2 的补角）

### 2.1 阶梯

现状 `HotSwap.FiftyRoundsBehaveLikeFirstTime` 证明的是「**描述符逐字节不变**时反复换件」——`MakeVersionedAExpectation()` 一份跑完 50 轮，这条性质本身承重，M4 **不动它**。M4 补的是它的补角：**描述符变了 ⇒ 期望必须跟着换**（§11.2 第 3 步 `VaseCli scan` 重生成清单，在测试里的对应物就是换期望）。

| 级 | fixture | `Version` | `Provides` | 行为 | 与上一级的差 |
|---|---|---|---|---|---|
| rung0 | `VersionedA`（已有） | 1.0.0 | `Vase.Test.Counter` v1 | `1` | — |
| rung1 | `VersionedAPrime`（已有） | 1.0.0 | `Vase.Test.Counter` v1 | `2` | 只差**代码** |
| rung2 | **新增** `VersionedAStampDrift`（D106） | **1.1.0** | `Vase.Test.Counter` v1 | `2` | 只差 **`Version` 串** |
| rung3 | **新增** `VersionedAServiceDrift`（D106） | 1.1.0 | **`Vase.Test.Counter` v2** | `2` | 只差 **`Provides` 版本** |

`Provides` 的版本漂移选 `Vase.Test.Counter` 而非新开服务名：`ExpectedService` 的比对含版本，开新服务名会把「版本漂移」与「服务集漂移」混作一件（后者已被 `LoadTimeComparison.ServiceMissingRefused` 单点覆盖）。

**rung3 必须注册与声明同版本的接口**（写计划时核出的实现要求）：`Context::Provide<T>()` 的注册键是 `ServiceKey{T::kName, T::kVersion}`——来自**接口常量，不是描述符**。所以 rung3 要新增 `ICounterV2`（`kName` 同 `Vase.Test.Counter`、`kVersion = 2`）并注册它；该步的行为读数也经 v2 读（v1 在那一刻已不在册）。
**核过的事实**：全仓**没有**「声明 vs 实际注册」的一致性检查（`PluginHost` 读的一律是 `Meta->Provides`，注解还明说「插件注册了未声明的服务」也在册）。因此「只改声明、不改注册」也能跑通——**但那是让 fixture 撒谎**（描述符说提供 v2、实际只提供 v1），是本仓库最不容的形态，故取前一条。

### 2.2 每步三条证人

**正例是主体**（D105）：它证的是「描述符也变了的版本能走完 `Eject → 落字节 → 换期望 → Adopt`，行为翻转、邻居未扰」——今天零覆盖。**负例是附属**：与既有 `Adopt.RequestDisplayNameDriftRejected` 同形，边际信息少，故只作为每步的伴随断言（事实取证注⑤）。

```text
Eject(A)                          ← 三档证据照常结算
落本步的字节到 A 的位置            ← 与 50 轮循环同一动作
├─ 负例：本步期望沿本步那一维 tamper 回上一级的值 → Adopt 被拒
│    断言：含 `manifest/binary mismatch`（D88 口径：一个子串即够，字段细节留给人）
├─ 正例甲：本级期望 → 通过，走**复用分支**（ReusedResidentImage == true）
│    ← 免额外往返：负例那次调用已经把二进制装载驻留了（事实取证注⑥）
Eject(A)                          ← 第二次卸，为下面腾出「全新装载」分支
└─ 正例乙：本级期望 → 通过，走**全新装载**（ReusedResidentImage == false）
     断言：ReusedResidentImage == false、行为 == 本步应有的值、B/C 邻居未扰
```

### 2.3 步序（D113）

| 步 | 从 → 到 | 差哪一维 | 期望工厂 |
|---|---|---|---|
| S1 | rung0 → rung1 | 代码 | `MakeVersionedAExpectation()`（两份通用） |
| S2 | rung1 → rung2 | `Version` 串 | 本级 = `…StampDriftExpectation()`，负例用 `Version` 回 tamper 的那份 |
| S3 | rung2 → rung3 | `Provides` 版本 | 本级 = `…ServiceDriftExpectation()`，负例用 `Provides` 回 tamper 的那份 |
| S4 | rung3 → rung2 | `Provides` 版本（**回退**） | 同上，方向相反 |
| S5 | rung2 → rung0 | `Version` 串（**回退**） | 本级 = `MakeVersionedAExpectation()` |

**回滚拆两步**是为了维持「每步只差一维」——一次退两维会让「是哪个字段被拒的」说不出来，而那正是负例唯一的产出。

**例外：S1 没有负例**（写计划时发现，D113 的「每步三条证人」以本条为准）。S1 那一维是**代码**——rung0 与 rung1 的描述符逐字节相同，**没有描述符字段可漂**，硬造一条就得去篡改一个与「本步漂移」无关的字段，那测的是比对器而不是换件谱。所以 S1 只做两条正例（甲/乙），其余四步三步齐。这不是遗漏：S1 无负例**正是**「描述符不变 ⇒ 期望不变」那条性质的另一面，而它已由 50 轮循环全程覆盖。

### 2.4 为什么只扩描述符维（D105）

行为维（再多几个只差代码的版本）与现有 50 轮循环**同质**：多出来的是轮数，不是覆盖面。真正与既有测试互补的是描述符维——它把 M2b 的加载期比对链接回换件链，让「换件谱」的完整形（代码变、元信息也变、**回退方向**也能走通）第一次有端到端证人。

---

## 3. Windows 侧档三负例（还 M1 登记的账）

### 3.1 待还的账

`Tests/HotSwap/AdoptTests.cpp:249` 原话：「Windows 的映射文件覆盖语义另成一题——v3 §12.1『某平台不可行就回来改这节』同样适用于 Win 的 sharing 规则，**该侧端到端验证登记到 M4/M5**，不在 M1 赌。」

现状：档三的**负例**（改名替换骗过档二、只有特征比得分得出新旧）**只有 Linux 有证人**；Windows 侧档三只有正例。

### 3.2 平台事实与统一序列（P1 已证）

| | 改名（rename）映射中的镜像 | 删除 / 覆盖映射中的镜像 |
|---|---|---|
| Linux | 允许（`rename(2)` 换目录项，旧 inode 仍映射） | 允许（truncate-in-place，同一 inode） |
| Windows | **允许**（P1 实测成功） | **拒绝**（`ERROR_ACCESS_DENIED` / `ERROR_SHARING_VIOLATION`，P1 实测） |

于是统一序列：

```text
统一序列（D108）：rename(P → P.old);  copy 新字节 → P
```

两者结局相同、也正是本用例要造的局面：**驻留镜像是旧字节，磁盘同名路径是新字节** → 档三必须拒。统一成一条路径后，`#ifndef _WIN32` 的闸拆开（事实取证注①：闸圈宽了）。

**这条序列在 Windows 上必然造出档二被蒙骗的局面**（P1 附带事实：改名后 `GetModuleFileNameW` 仍返回原路径，此刻该路径已是新字节；而 Windows 档二的口径正是「路径可写开」）——那正是 §8.2「档二会被改名替换骗过」那段论证的活体实例。**不为此加断言**：本用例走的是 Adopt 轨、不产出 Eject 报告，硬要断言档二为真就得另配一次 Eject，会把用例从一件清晰的事变成两件事拼的（Q16）。写进用例注释当存在理由即可。

> **Linux 侧换序仍以实测为准。** P1 证的是 Windows 可行，不是 Linux 回归通过。换序后 `Adopt.RenameReplacementCaughtByTierThree` 必须重跑；若行为变了，退回两平台各自序列，并把「为什么不能统一」写进注释——不是把不成立的那条删掉。

### 3.3 `NoIdentityPlugin`（D109）

Linux 侧原 `NoBuildIdPlugin`（`-Wl,--build-id=none`，住 `Tests/CMakeLists.txt` 的 `if(NOT WIN32)` 内）。跨平台化后的形态：

| 平台 | 摘特征的链接选项 | 指路 token |
|---|---|---|
| Linux | `-Wl,--build-id=none` | `--build-id` |
| Windows | `/DEBUG:NONE`（后手压过工具链 `INIT` 的 `/DEBUG:FULL`） | `/DEBUG:FULL` |

P2 已证「后写者胜」；`LINK_OPTIONS` 排在 `CMAKE_*_LINKER_FLAGS` 之后是 CMake 生成器保证（M1 已钉）。**该 fixture 自守**：标志一旦失效，产物就长出身份特征，Adopt 会走到装配并返回 `Ok`，`EXPECT_FALSE(r.IsOk())` 当场变红——静默失效不可能产生假绿。

连带后果（如实登记，不是缺陷）：

- 两条既有 Linux-only 用例（`Adopt.RenameReplacementCaughtByTierThree` 经 §3.2、`Adopt.MissingIdentityFeatureRejectedWithPointer` 经本节）**六线同跑**；
- **`linux − win` 的基数差值由 +2 变 0**——CLAUDE.md 基数表里承重的一格，属**预期内的形状变化**（§6、§7）；
- 断言里的指路 token 按平台取——平台差异写进断言而非写进 `#ifdef`，与 `ReopenWritableIsMeaningful` / `MappingRemovalIsObservable` 的既有处置同形。

**改名是全改**（D109）：fixture 目录/文件名、插件 Id、事件名、CMake 宏、期望工厂、用例名。半改会留下「名字说 BuildId、Id 说 NoBuildId」的错位，而它的存在理由本就是平台中性的「身份特征缺失」。

### 3.4 探针登记（两条都已实测，结论已落回 D108/D109）

| 探针 | 问什么 | 结论 |
|---|---|---|
| P1 | Windows 上 `rename` 映射中的 DLL 是否成功；成功后原路径落新字节 | **成功**（且实测了三条直接覆盖路径全部被拒）→ D108 走**统一序列** |
| P2 | per-target `/DEBUG:NONE` 能否压过工具链 `INIT` 的 `/DEBUG:FULL` | **能**（lld-link 调试目录整个空；link.exe 剩 POGO，但 CodeView 消失且解析器逐条找 CodeView，POGO 会被跳过）→ D109 走**有 fixture 证人** |

两条探针都过了，退路（v3 §12.1「某平台不可行就回来改这节」）本轮**未启用**。原始证据（P1 的头字节对位表、P2 的三份产物 `llvm-readobj --coff-debug-directory` 输出）记在 plan 的偏离登记里。

---

## 4. 语义依赖知情位（D110）

### 4.1 它是什么、不是什么

**是什么**：`EjectReport` 的一个 bool，置位时如实说明——本次 Eject 重置了进程级状态，而进程内仍可能有别处持有从它派生的裸值；那条边**账本与导入表都看不见**（§9.1 的硬契约正是为此而立）。

**不是什么**：**不是检出**。它不宣称「查过了、没有语义依赖」，恰恰相反——它把「这件事查不出来」结构化地写进报告。这与 §8.2「缺一个平台字段的『成功』比失败更危险」同一条口径，也与 M3-D100「前端不打就是证人缺一半」同一个理由：不可知的事必须显式可见。

### 4.2 置位条件与范围（D110）

```text
SemanticDependencyPossible  =  !ProcessStatesReset.empty()   ∧   进程内任一 Pod 仍有活实例
```

- **范围取「进程内」**，不取「本 Pod 内」：进程级状态是**进程级**的（跨 Pod 存活，§9.1）。Eject(pod1, X) 的重置发生在「X 在任何 Pod 都无活实例」时（D91），可 **pod2 里的另一个插件 Y**（不同 binary）完全可能攥着从 X 的进程级状态派生的值——pod1 卸空了，Y 照样陈旧。「本 Pod 内」形态会说「没事」而其实有事。
- **只数活实例**：`Failed` 记录与级联空壳没有实例、攥不住派生值。与 D91 的 Reset 闸同口径——③ 闸要计空壳，是因为它问的是「镜像能不能卸」，与这里问的不是同一件事。
- **拒绝态恒 `false`**：`Status == kRejectedConsumers` 时没有 Reset 发生，`ProcessStatesReset` 必空。与既有 `Status` 的「默认悲观：成功态必须显式置上」同形。
- **求值时机**：被卸者已从活集合摘除之后，否则恒真。

### 4.3 为什么不动 `kHeaderVersion`

`EjectReport` 住 `Include/Vase/Host/Evidence.h`（宿主侧），插件只链 `VasePod`（D11），本结构不过插件 ABI 界。宿主侧结构的尾追加字段不构成 `HeaderVersion` 变更——与 D92 那次（`PluginMeta` 是插件 ABI 面，故 3→4）的区别正在这里。

### 4.4 前端（D111）

两份 `PrintEjectReport` 补打点，**只在 `Status == kEjected` 时打**（含 `false`）。bool 没有「空」，不能照搬 `ProcessStatesReset` 的「空则不打」——那样 `false`（判定为无风险）与「根本没走到这一步」在输出上无法区分。拒绝态整行不打，于是读法唯一：**没打 = 这局没成功；`false` = 成功了且判定无风险**。

---

## 5. 测试分层与验收面

| 判据 / 面 | 用例位置 | 形态 |
|---|---|---|
| 换件谱 · 五步阶梯（D105/D113） | `Tests/HotSwap/HotSwapLoopTests.cpp` | 一条用例、五个步，每步三条证人（负例 / 复用分支正例 / 全新装载正例）；带 B/C 邻居，每步断言邻居未扰；定位靠 ASSERT 消息带步号 |
| 档三负例 · 两平台（D108） | `Tests/HotSwap/AdoptTests.cpp` | 统一序列「改名离开 + 落新字节」→ Adopt 拒，错误含 `differ` 与逃生门 `rebuild`（§8.2 政策）；拆开 `#ifndef _WIN32` |
| 档三缺特征 · 两平台（D109） | 同上 | `NoIdentityPlugin`：Windows 无 CodeView → 拒且指路 `/DEBUG:FULL`；Linux 指路 `--build-id` |
| 语义依赖知情位（D110） | `Tests/HotSwap/EjectTests.cpp` | 正例：有重置 ∧ 进程内仍有活实例 → 置位；反例 a：有重置 ∧ 进程内再无活实例 → 不置位；反例 b：无重置 ∧ 有活实例 → 不置位。**空壳局一支无证人可写**（空壳攥不住值、条件本就不该置位），如实标注 |
| 描述符布局 / `kHeaderVersion` | — | **本波不动**，无预期红（D110 的边界） |

**验收口径**：六 preset 全绿 + 各线 `ctest -N` 与基数表逐位对上 + 三条 tidy 线正文双 0（TU 预期两侧同为 **99**：Windows 96 → +2 新 fixture +1 `NoIdentityPlugin`；Linux 97 → +2 新 fixture）+ format 干净。本波动 Eject / Adopt 路径与 fixture → 规矩 6 强制：`-R 'HotSwap|Eject|Adopt'` 选择子**双平台各自留证据**。

**六线基数预期**：新增用例六线同幅；`RenameReplacementCaughtByTierThree` 与 `MissingIdentityFeatureRejectedWithPointer` 由 Linux-only 转为六线同跑（Windows 各 +1）。净结果 = **`linux − win` 的 +2 差值消失，六线同值**；`debug − release` 的 −1（T3 的 death test，`#ifndef NDEBUG`）不受影响。**这两个形状都要在收口时逐位核，不是照抄。**

---

## 6. 风险与预期中的行为性红

| 风险 | 处置 |
|---|---|
| Linux 侧换序后行为改变（D108） | 预期红：`Adopt.RenameReplacementCaughtByTierThree`。**先跑统一版**，红则退回双序列并把原因写进注释，不删用例。P1 只证了 Windows 可行，不是 Linux 回归通过 |
| 两个新 fixture 的 `OUTPUT_NAME` 撞 Ninja | 预期红：生成期 `multiple rules generate …VersionedA.lib`。修法是补各自 stage 目录（同 `VersionedAPrime` 既有处理） |
| 换件谱的正例若实现上不可达 | 断言在 Adopt 通过处红，**先查是不是「文件已被替换但宿主仍判已驻留」这条路径没走到**——那本身就是发现，不是放宽断言的理由 |
| `SemanticDependencyPossible` 在既有 Eject 用例上意外置位 | 预期红：既有用例的邻居局形态会触发它。**这未必是缺陷**——若某条既有用例本就该置位，改断言而不是改条件（并在注释说明） |
| **六线基数形状变化**（D109） | `linux − win` 的 +2 差值消失、六线同值；tidy TU 两侧同为 99。这是**预期内的形状变化**，不是漏注册——收口时逐位核过再落表，并在表里写明差值为何变小 |
| **阶梯用例耗时**（D116） | **实测已填**（同树三轮，per-test 计时，含测试进程启动与 gtest 初始化）：`win-x64-clang-debug` 阶梯 `HotSwap.DescriptorDriftLadderSwapsBothWays` **0.13 / 0.14 / 0.13 s**、对照 50 轮循环 `HotSwap.FiftyRoundsBehaveLikeFirstTime` **0.74 / 0.73 / 0.76 s**；`linux-x64-clang-debug` 阶梯 **0.26 / 0.25 / 0.25 s**、对照 50 轮 **0.93 / 0.88 / 0.90 s**。**按次数**是 13 Adopt 对 50 轮的 100 次（50+50）；**按耗时**只有那个循环的 **1/5（Windows）/ 1/3.6（Linux）**，两个口径别混读。全阶梯共 9 Eject + 13 Adopt——S1 无负例，只有 1 Eject + 1 Adopt，其余四步各 2 Eject + 3 Adopt。无超预算问题 |
| `NoIdentityPlugin` 的 Windows 侧标志失效 | **自守**：标志失效 ⇒ 产物长出身份特征 ⇒ Adopt 走到装配返回 `Ok` ⇒ `EXPECT_FALSE(r.IsOk())` 红。静默假绿不可能 |

---

## 7. 文书义务（验收时同步；规矩 7 口径：只住 CLAUDE.md 的真值不复制进技能）

1. `wiki/vase-architecture.md` **§12 判据表新增判据 3f**：语义依赖知情位的置位可证伪（正例 + 两条反例，空壳局无证人可写如实标注）。
2. §12.1 判据 **3** 的验证方式补一句：档三负例的 Windows 侧证人（本文 §3）。
3. **§12.3 M4 行**标「已落（Win / Linux）」，并注明 macOS 腿顺延 M5。
4. **§9.1** 的「`EjectReport` 提示面兜底」由一句话补成实形：`SemanticDependencyPossible` 的语义、置位条件、以及「它不是检出」这条边界。
5. `CLAUDE.md`：
   - 项目状态（M4 完成 + 本波三笔）；
   - **六线基数表**（含 `linux − win` 差值**由 +2 变 0** 的如实登记——那一节记的不只是数字，还有每处差值为什么存在）与 **tidy TU 表**；
   - 规矩 6 的子串契约清单（如有变动）；
   - `Tests/HotSwap/AdoptTests.cpp:249` 那笔登记账已还（连同该行注释本身改准）。
6. **技能 `.claude/skills/vase-cpp-engineering/references/verification.md:27`** 那行「Linux 的基数高于 Windows」——M4 之后失效，且它**本来就违反规矩 7**（基数真值只准住 `CLAUDE.md`）。**整行删掉，不是改准**：它属「阈值与基数」类，处置是「只住本文件」。当规矩 7 的一笔已知债清掉。
7. memory 的 `vase-v3-hotswap-status` 随收口更新。

---

## 8. 与其他里程碑的边界

- **M3（已完成）**：本文不碰 3a / 3d 的既有证人，也不动 `ProcessStateDesc` 与清单 `processStates`。
- **M5（平台收口）**：macOS 的档二（`_dyld_image_count`）、档三（`LC_UUID`）、dyld4 neverUnload 实测**全归 M5**；`VaseCli scan` 与 VasePack 分发（还 C4251 那笔债）亦归 M5。**本文不实现任何 macOS 代码路径**——`LoaderPosix.cpp` 的 Darwin 分支若已存在，本文不改它。
- **§13.4 明确不做**：档 ②（级联热替换与强制 Eject）、`epoch` / 稳态自动重载 / 依赖方原地重绑定、Android / iOS 的任何热插拔——本文一概不动。
