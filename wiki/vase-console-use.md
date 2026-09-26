# VaseConsole 使用文档

> 本文描述**仓库里实有**的工具：`Tools/VaseConsole/`，CMake target 与二进制同名 `VaseConsole`，
> 是 Vase 的**交互式热插拔验证台**。M2b 波 2（T12）把命令面改成了**双入口**，本文的命令表与
> 报告字段按 `Tools/VaseConsole/Console.cpp` 的 `Commands()` 表对齐，输出示例为**实测截取**
> （2026-09-26 收口波自删树重配后的 win-x64-clang-debug 构建树回放会话重取，绝对路径前缀折成
> `<回放根>`；与格式串逐字一致，其中 §7 的状态链读法务必看那节的更正）。
>
> - 设计依据：`docs/superpowers/specs/2026-09-21-vase-console-design.md`（下文以 spec 指代）与
>   `docs/superpowers/specs/2026-09-25-vase-m2b-wave2-loading-verification-design.md` 的 §5.1（D85）。
> - 构建命令、测试基数、各线前提：以 [`CLAUDE.md`](../CLAUDE.md)「构建与测试」为准，本文不复制。
> - 机制本身（Loader、依赖账本、三档证据、加载期比对）以磁盘上的头文件与实现为准，本文只讲「怎么用、输出怎么读」。
> - **不要与架构 §11.1 的 `Tools/VaseCli` 混为一谈**——那是尚不存在的清单扫描 / 校验工具，
>   与本验证台是两回事（CLAUDE.md「项目状态」一条有同样的提醒）。

## 目录

1. [它是什么、能干什么](#1-它是什么能干什么)
2. [拿到二进制与启动](#2-拿到二进制与启动)
3. [两种运行方式与菜单机制](#3-两种运行方式与菜单机制)
4. [命令一览](#4-命令一览)
5. [plan 文件格式（`pod new-raw` 旁路）](#5-plan-文件格式pod-new-raw-旁路)
6. [最小会话：建局与拆局](#6-最小会话建局与拆局)
7. [换件全流程：双 install 同步链](#7-换件全流程双-install-同步链)
8. [报告怎么读](#8-报告怎么读)
9. [退出码规则](#9-退出码规则)
10. [自带的回放资产与 ctest](#10-自带的回放资产与-ctest)
11. [已知边界与坑](#11-已知边界与坑)

## 1. 它是什么、能干什么

Vase 只负责发现插件、装配、运行、干净关停；`VaseConsole` 把这条链路交给**人**操作：

- 用**两种入口**建局（`pod`）：**主链** `pod new <插件目录> [preset文件]`（Refresh→Solve→CreatePod，
  条目自动带清单期望，**加载期比对默认生效**，D84）；**旁路** `pod new-raw <planFile>`
  （老 plan 文件整体续命，`Expected=nullptr`、无比对）——「绕过 Solve 直接喂 Host」的能力是
  本验证台的独有价值（D85）；另有会话级预览目录 `catalog refresh/solve` 做求解展示；
- 对局里的插件**卸（eject）/ 领回（adopt）/ 循环换（swap）**，每一步打印三档证据报告；
  `adopt` 走 Catalog 的 `AdoptInto` 单点重读清单再交 Host 比对（D81），**只有 catalog 来源的局能用**；
- 把新字节**写回**磁盘登记路径（`file stage` / `file install`），并可成对地换**清单**
  （`file stage-manifest` / `file install-manifest`，仅 catalog 局）——波 2 起这是「真换件」的
  **双覆盖**：二进制与清单必须同换，只换一边会被加载期比对拒（D87）；
- 拆局时打印残留计数，**退出码即「全程是否 Clean」**——同一套命令既能敲也能 `--script` 回放，
  所以一份会话记录就是可重放的回归证据。

材料以 `Samples/` 的 Hello 家族为主：`HelloPlugin.dll`（Id `Vase.Hello`，服务 `Vase.Hello.Greeter`）
与 `HelloPluginPrime.dll`（同 Id 的换件材料）。波 2 起 Samples 还多了 `DependentPlugin` /
`FailingPlugin`（依赖链演示位与失败注入位，D86）——它们的消费面在 **Tests**（判据 8 的 Sample 面
材料与判据 4 后半的失败注入，随 `manifests/{dependent,failing}` 清单 fixture 一起被消费），
**不在** `VaseEmbedding` 的 play/loop/swapdemo 素材里（那条链只吃 Hello 一家）；console 的
碰撞/消费者回放另吃 `Tests/Integration/fixtures/` 的三个探针插件。
它不提供业务逻辑，也不是发布物的一部分。

> 与 `VaseEmbedding` 的分工：Embedding 演示的是**嵌入方 API**（`play | loop <N> | swapdemo`，
> 纯代码驱动）；VaseConsole 是**用户可操作**的验证台。两者的判据面不同，不互相替代。

## 2. 拿到二进制与启动

按 [`CLAUDE.md`](../CLAUDE.md)「构建与测试」任选 preset 构建即可，`VaseConsole` 是六条线都建的
target。产物与所有插件 DLL 同处构建树的 `bin/`（这是 Windows 能找到 DLL 的前提，布局不要动）。

```
usage: VaseConsole [--script <file>]
```

- **不带参数**＝交互模式，从 stdin 逐行读命令；
- **`--script <file>`**＝回放模式，逐行执行 `<file>` 里的命令；
- 其他任何参数形态、或脚本打不开 → 打印 usage 并返回 **2**（还没进会话，谈不上判定）。

从 `bin/` 目录里跑最省事（plan 文件里的相对路径就按当前工作目录解析）。

## 3. 两种运行方式与菜单机制

### 交互模式

提示符是 `vase> `。要预先知道的两件事：

- **没有 Tab 补全，也没有上箭头翻历史**——本工具的循环是自己写的
  `Prompt → getline → Feed`（`Shell.cpp`），没有走 cli 的 readline 会话。
  翻历史用内建命令：`history` 列出本会话最近 100 条，`! N` 重放第 N 条（**下标从 0 起**）。
- **退出请用 `exit`**。Ctrl+D / Ctrl+Z 之类的 EOF 离席同样会拆局，但按规则「脚本没走 exit」
  判 **rc=1**（见[第 9 节](#9-退出码规则)）。

### 回放模式（`--script`）

一行一条命令。实测行为：

- 容忍 **CRLF**（行尾 `\r` 在消费点剥掉——构建生成的回放资产本身就是 CRLF）；
- 末行**没有换行也算数**；读完后若没遇到 `exit`，判「判定未完整发生」→ rc=1；
- 输出会带每行的 `vase> ` 提示符，但**不回显命令本身**——提示符后面直接出现的行全是报告。
  注意**别把报告行读成命令**：好几条报告以命令名开头、且**省略组前缀**
  （`pod destroy` 打 `destroy index=…`、`file install` 打 `install … written=…`、
  `file install-manifest` 打 `install-manifest … written=…`）——
  那是 ctest 按子串钉着的输出契约，不是漏了 `pod` / `file`。逐行的对应关系见第 8 节开头的表；
- 本文各输出块末尾的 `rc=N` 行是示例附注的**进程退出码**（shell echo），不是工具的输出。

一份失败样例（plan 里的路径不存在；这份脚本由构建生成在回放目录，见第 10 节）：

```
vase> pod new-raw: plan line 1: binary not found: /definitely/not/here.dll
rc=1
```

### 菜单机制：根菜单与子菜单

命令分两层：根上的 6 条（`eject` `adopt` `swap` `get` `emit` `plugins`），
子菜单 `pod` 的 5 条（`new` `new-raw` `use` `list` `destroy`），子菜单 `file` 的 5 条
（`stage` `install` `stage-manifest` `install-manifest` `show`），子菜单 `catalog` 的 2 条
（`refresh` `solve`）。在根提示符下**两段名直接敲**即可：

```
pod new ../Tools/VaseConsole/replay/cat_hot
file show Vase.Hello
```

也可以单独敲 `pod` / `file` / `catalog` **进入子菜单**（提示符变成 `pod> `）。进了子菜单要留意：

- 只有该子菜单自己的命令能用，根命令**不会上浮**——在 `pod>` 里敲 `get` 会得到
  `wrong command: get` 并判非零（实测）；
- 回到根菜单用 **`..`**；`back` 不存在，`..` 在根提示符下也算错命令（同样判非零）；
- 四条内建命令（`help` / `exit` / `history` / `!`）在全局作用域，**任何子菜单里都能用**。

`help` 列出全部命令。一个无害的显示怪相：`get` 与 `plugins` 会显示成
`get <list of strings>`——那是 cli 对「未声明参数的 vector\<string\> 处理器」的固定文案，
这两条命令实际**不吃参数**（多余参数被忽略，不会报错）。

## 4. 命令一览

| 命令 | 参数 | 作用 |
|---|---|---|
| `catalog refresh <pluginDir>` | 1 | 刷**会话级预览目录**（事务性 D58：成功才换预览），逐行列出 Id 与 warning |
| `catalog solve [presetFile]` | 0–1 | 求解预览目录，逐行打计划（`load` / `skip <原因>`）与 Notes（§4.4 卖点展示面；不建局） |
| `pod new <pluginDir> [presetFile]` | 1–2 | **主链建局**并设为活动局：这一局**自持一份 catalog**、当场 Refresh→Solve→CreatePod（D85），条目自动带期望（D84），加载期比对默认生效；逐局以 `Strict=true` 装配 |
| `pod new-raw <planFile>` | 1 | **旁路建局**：M1 的 plan 文件格式整体续命（第 5 节），`Expected=nullptr`、无比对 |
| `pod use <index>` | 1 | 把已存活的局选为活动局（index 来自 `pod list`） |
| `pod list` | 0 | 列出所有活局：`[index] gen= plan= plugins=`，活动局标 `<- active`；`plan=` 对两态局统一显示其来源路径（目录或 plan 文件） |
| `pod destroy` | 0 | 拆掉活动局并打印完整报告；报告不 Clean 判非零 |
| `eject <id>` | 1 | 从活动局卸下插件，打印 §8.2 三档证据报告（不依赖 catalog，raw 局照用） |
| `adopt <id>` | 1 | 把已卸插件领回活动局：走 `AdoptInto` 单点重读该清单（D81）。**仅 catalog 来源的局可用**，raw 局响亮拒绝（见第 8 节） |
| `swap <id> <rounds>` | 2 | eject+adopt 同一插件 N 轮（`rounds` 必须是正整数，没有默认值）；同受「仅 catalog 局」约束 |
| `get` | 0 | 打印活动局里 `Vase.Hello.Greeter` 提供方的问候串 |
| `emit <n>` | 1 | 向活动局派发 `GreetEvent{n}` |
| `plugins` | 0 | 列出活动局在装插件的 Id 与失败记录 |
| `file stage <id> <srcPath>` | 2 | 把 `<srcPath>` 的字节读进**二进制暂存槽**，不落盘 |
| `file install <id>` | 1 | 把暂存字节覆盖写到该 Id 登记的路径（两态局的路径都取自随局条目表） |
| `file stage-manifest <id> <srcPath>` | 2 | 把清单源字节读进**清单暂存槽**（独立槽，与二进制槽各一条，grilling Q4 裁）；**仅 catalog 局** |
| `file install-manifest <id>` | 1 | 用暂存清单覆盖该 Id 插件目录里的 `plugin.json`；**仅 catalog 局** |
| `file show <id>` | 1 | 打印登记路径、大小、mtime 与磁盘上的身份指纹 |

内建命令：`help`、`exit`、`history`、`! <history entry index>`。

参数形状不对（多给、少给、`swap 3` 这种漏了 rounds）会打一行 `usage: …  (got N argument(s))`
并判非零；子菜单就 `pod` / `file` / `catalog` 这三组（上面「分两层」指**深度**：根与子菜单，勿与此处的**组数**混读）。除 `pod new/new-raw/use/list`、`catalog *`
和四条内建命令外，其余命令都需要**存在活动局**：`eject` / `adopt` / `swap` / `get` / `emit` /
`plugins` 会先撞一句 `no active pod (use: pod new <pluginDir> or pod new-raw <planFile>)` 并判非零；
`file stage/install` 与两条清单命令的无局分支**不打字只判码**，`file show` / `file install`
则报 `… is not in the active pod's plan`，`file install-manifest` 报
`… is not in the catalog snapshot`。

## 5. plan 文件格式（`pod new-raw` 旁路）

`pod new-raw` 吃一份纯文本 plan：每行一条「Id ↔ 二进制路径」。解析规则（`PlanFile.cpp`）：

- 每行**首个空白之前是 Id**，其余整段是路径——**路径里可以带空格**，不需要引号；
- `#` 只在**行首**（去除前导空白后）是注释；行中出现会被当作路径内容的一部分；
- 空行与注释行忽略；Id 或路径为空 → 报错，路径指向的文件不存在 → `binary not found` 报错，
  整份文件没有一条有效记录 → 报错。以上都判非零，且**在建局之前**发生；
- 数组序 = 文件序 = 装配序。

示例（路径写成相对 `bin/` 的形式）：

```
# 一局一个 Hello
Vase.Hello HelloPlugin.dll
```

> **旁路不是遗物**（D85/D68）：老 plan 格式在波 2 没有退场，而是整体移进 `pod new-raw` 续命——
> 它产的局 `Expected=nullptr`，加载期比对**不生效**。这份「绕过 Solve 直接喂 Host」的能力正是
> 验证台的独有判据面：判据 4 后半的分歧证人与 Host 执法材料都吃这条旁路。代价随之而来：
> raw 局没有清单来源，`adopt` 与两条清单命令都会响亮拒绝（第 8 节），不是静默降级。

## 6. 最小会话：建局与拆局

主链最短一回：拿回放目录 `cat_hot`（内含 `HelloShadow/`：`HelloPlugin` 二进制 + 配对
`plugin.json`）建局再拆。会话起在构建树的 `Tools/VaseConsole/` 目录下（`replay/` 资产就在旁边，
可执行经 `../../bin/VaseConsole` 或 PATH 拿到；下文路径皆相对这个目录）。先敲这四行：

```
pod new replay/cat_hot
pod list
pod destroy
exit
```

输出形态——`destroy index=0` 是 **`pod destroy` 打的报告行**，不是敲出来的命令（第 3 节那条警告）：

```
vase> pod created index=0 generation=1
  plugin: Vase.Hello
vase> pods: 1
  [0] gen=1 plan=replay/cat_hot plugins=1  <- active
vase> destroy index=0
clean=true handleWasStale=false
counters diff (baseline = pod creation): effects=0 services=0 subscriptions=0 pluginInstances=0 scopes=0
hotswap log (0):
rc=0
```

四个读法要点：

- `index=` / `generation=` 是 `PodHandle` 的两半。`pod list` 里的 index 就是 `pod use` 的参数；
- `counters diff` 的基线是**建局时刻**：一局建了又拆干净，五个计数应回到 0——
  任何非零都是一处没还干净的账（残留在 `residual:` 行逐个点名）；
- `hotswap log (0)` 空着，因为这一局没有进出；有进出时每条记 `eject:<id>` / `adopt:<id>`；
- 主链建局带**静态跳过**时会在 `  plugin:` 行后补 `  skip: <id> (<原因>)` 行（`pod new` 只打静态段；
  完整计划与 Notes 的展示面在 `catalog solve`）。

多局并存时 `pod new` / `pod new-raw` / `pod use` 切换活动局，`eject`、`adopt`、`get` 等命令
**只作用于活动局**。catalog 随局归属（D85）：每局自持一份快照，第二局换目录不影响第一局的 adopt。

## 7. 换件全流程：双 install 同步链

这是本验证台相对 gtest 用例的**唯一增量**：把新字节与新清单真正写上磁盘、再让宿主领回，
最后**拆局重建**看新清单铸的 plan（spec §3.2 推论 1 + 波 2 D87 + T14c 两级证人）。回放资产
`hotswap.txt` 的十五条命令（按脚本行数计，含 `exit`；影子材料在 `cat_hot/HelloShadow/`，
由 `VaseConsoleCatalogStage` 每次 ctest 开头刷回原始字节）：

```
pod new replay/cat_hot
get
eject Vase.Hello
file stage Vase.Hello ../../bin/HelloPluginPrime.dll
file install Vase.Hello
file stage-manifest Vase.Hello replay/manifests/hello_prime.json
file install-manifest Vase.Hello
adopt Vase.Hello
get
file show Vase.Hello
pod destroy
pod new replay/cat_hot
get
pod destroy
exit
```

下面是**从刚刷回的 (base, base) 影子起跑**的实测输出（`staged` / `install` / `install-manifest` /
`show` / `destroy` 开头的行是报告行，组前缀不在，属契约——第 3 节；`<…>` 为回放根下的实际路径；
末行 `rc` 是进程退出码的注记，非工具输出）：

```
vase> pod created index=0 generation=1
  plugin: Vase.Hello
vase> get: hello from Vase.Hello x1 [安静]
vase> eject Vase.Hello binaryUnloaded=true mappingRemoved=false reopenWritable=true mappingRemovalIsObservable=false reopenWritableIsMeaningful=true
vase> staged <N> bytes for Vase.Hello (not written yet)
vase> install Vase.Hello written=true path=<回放根>/cat_hot/HelloShadow/HelloPlugin.dll
  判据力：Windows → 覆盖写不开即「Eject 没真卸」（sharing violation）。本条**有**判据力
vase> staged manifest <M> bytes for Vase.Hello (not written yet)
vase> install-manifest Vase.Hello written=true path=<回放根>/cat_hot/HelloShadow/plugin.json
vase> adopt Vase.Hello registeredBy=catalog <回放根>/cat_hot
adopt Vase.Hello reusedResidentImage=false identityVerified=true importEnforcementPassed=true outgoingEdges=0 manifestVerified=true
vase> get: hello from Vase.Hello prime v2 x1 [安静]
vase> show Vase.Hello path=<…> size=<N> mtimeFileClockS=<秒>
  identity kind=pdbCodeView bytes=<hex>
vase> destroy index=0
clean=true handleWasStale=false
counters diff (baseline = pod creation): effects=0 services=0 subscriptions=0 pluginInstances=0 scopes=0
hotswap log (2):
  eject:Vase.Hello
  adopt:Vase.Hello
vase> pod created index=0 generation=2
  plugin: Vase.Hello
vase> get: hello from Vase.Hello prime v2 x1 [响亮]
vase> destroy index=0
clean=true handleWasStale=false
counters diff (baseline = pod creation): effects=0 services=0 subscriptions=0 pluginInstances=0 scopes=0
hotswap log (0):
rc=0
```

这条链的读法是**两级**的（T14 实测勘误、T14c 按此重立证人）：**局内 label 跟建局清单走**——
第二次 `get` 打出 `hello from Vase.Hello prime v2 x1 [安静]`，`prime v2` 半句证二进制
**真的**换掉了，但 adopted 实例的配置按 D34 回放**建局时刻**的 plan 时 blob（波 2 spec 第 1 节
明载「Adopted 插件的配置回放语义原样保留、零改动」，本局建局时影子还是 base 清单、默认「安静」）
——回放语义非缺陷。**重建局才翻 label**：拆局后对同一棵根再 `pod new`，重 scan 见 (prime, prime)
的盘上双半，Solve 按 prime 清单的 default「响亮」铸新 plan，第三次 `get` 即打
`prime v2 x1 [响亮]`——「label 随清单与二进制同步换」（D87）的本意正落在这条 scan 重生成链上。
ctest 的两级证人各自钉这两行（`…ReplayReason` 钉局内 `prime v2 x1 [安静]`、`…Reason` 钉重建局
`prime v2 x1 [响亮]`，各自吃**脚本自己**造出的态，单跑自足——状态链纪律见第 10 节）。链路上的分工：

- `file stage` / `file stage-manifest` 只读进各自的内存槽，**不碰磁盘**（两槽独立，不串型）；
- `file install` / `file install-manifest` 才是落盘的步子，各自只能 install 到**暂存时绑定的
  那个 Id** 的登记路径 / 其快照目录里的 `plugin.json`（`nothing staged` 即没绑上）；
- `adopt` 的前置行 `registeredBy=catalog <目录>` 之后，`AdoptInto` **当场重读该 plugin.json**
  构造期望（不吃建局时的旧快照内容，§5.6①），连同快照兄弟名集交 `AdoptPlugin`；
- 成功行的 `manifestVerified=true` 是「比对确实发生过且通过」的字段凭证（D70），
  `identityVerified=true` 仍是档三身份比对——Linux 上「换件真的发生」靠这两条而不是文件锁（规矩 6）。

**反面材料：只换二进制、不换清单，adopt 会被加载期比对拒**（`mismatch_1/2.txt` 演的就是这个）：

```
vase> adopt Vase.Hello registeredBy=catalog <回放根>/cat_mm1
adopt failed: manifest/binary mismatch for "Vase.Hello": displayName — manifest "示例插件" != binary "示例插件（prime 版）"
rc=1
```

格式器住 `Source/Host/Detail/ManifestCompare.cpp`；字段路径点名到格（实测首个 offender 即
`displayName`；其余如
`provides[...] missing in binary`、`config "MoodValue".choices[2].label`），判据只钉
`manifest/binary mismatch` 一个 token（spec §6）。这不是缺陷，是 §3.4 / §11.2 一致性的本意：
**换件必须两半同换**。不 eject 直接 install 会得到 `written=false`——Windows 判据力
（镜像还映射着 = 「Eject 没真卸」）正在工作，Linux 上该步为空转（`file install` 报告行自带
这条平台声明）。

> ⚠ **手工跑过 `hotswap.txt` / `mismatch_*.txt` 之后，影子会留在脏态**（刷回只发生在 ctest 开头的
> fixture）。立刻再跑，起点就不是 (base, base) 了——synced 家族留 (prime, prime) 一致脏态尚可
> 自洽重放，mismatch 家族换过一边就复现不出「单边脏」的拒绝路径。手工重刷与 fixture 同一动作：
> `cmake -P <构建树>/Tools/VaseConsole/replay/stage_catalog.cmake`。
> （synced 家族起跑态决定 `响亮` 是否出现在换件腿之前——见上面 §7 正文的状态链一段。）

`eject` 的「真卸下」在**真实开发回路**里恰好是 linker 需要的东西，且那条回路不需要 file 组——
前提是清单不用动：

```
（pod new <插件目录> 建局，目录里 plugin.json 与编译产物配对）
eject Vase.Hello
（另开终端：改代码 → cmake --build --preset win-x64-clang-debug）
adopt Vase.Hello          ← 原路径就地重读：新字节 + 重读的清单一起比
get                        → 新行为
```

描述符里改了东西（加配置字段、动服务版本）就必须同步改 `plugin.json`，否则比对拒——
响亮失败，正是设计。而机制对「换进来的与旧的相同」没有任何要求：install/adopt 从不记得旧字节，
adopt 只查 Id 相符、`HeaderVersion` 相符（波 2 起为 3）、身份特征在、导入表干净、期望比对过。

## 8. 报告怎么读

### 先对表：哪行输出来自哪条命令

工具不回显命令（第 3 节），所以读日志要靠这张表。特别注意 `destroy` / `install` /
`install-manifest` 行**开头没有组前缀**（`eject` / `adopt` / `show` 同样以命令名开头）——
那是被 ctest 按子串钉着的输出契约，不是命令写漏了字：

| 输出行（开头形态） | 来自 |
|---|---|
| `  plugin: <id>` 若干（+ `  warning: <子目录> <msg>`） | `catalog refresh <pluginDir>` |
| `  <n>. <id> load` / `  <n>. <id> skip <原因>` / `  note: <kind> <id> [key/cause]` | `catalog solve [presetFile]` |
| `pod created index=…` + `  plugin: <id>` 若干（+ `  skip: <id> (…)`） | `pod new <pluginDir> [preset]` 与 `pod new-raw <planFile>` |
| `pods: N`（`  [i] gen= plan= plugins=`，活动局标 `<- active`） | `pod list` |
| `active pod: index=… generation=…` | `pod use <index>` |
| `destroy index=…` + 完整拆局报告 | `pod destroy`；`exit` / 脚本读完时的**收尾拆局**打同样的行 |
| `eject <id> binaryUnloaded=…`（有 note 时另起 `  note: …`） | `eject <id>`；`swap` 每轮各打一次 |
| `adopt <id> registeredBy=…` 一行 + `adopt <id> reusedResidentImage=…` 一行 | `adopt <id>`；`swap` 每轮各打一次 |
| `adopt requires a catalog-backed pod (use: pod new <pluginDir>)` | raw 局的 `adopt` 与 `swap` 的 adopt 腿 |
| `file stage-manifest requires a catalog-backed pod (use: pod new <pluginDir>)` | raw 局的 `file stage-manifest` |
| `file install-manifest requires a catalog-backed pod (use: pod new <pluginDir>)` | raw 局的 `file install-manifest` |
| `emitted GreetEvent seq=…` | `emit <n>` |
| `get: …`（问候串或 `no provider …`） | `get` |
| `plugins live=…` | `plugins` |
| `staged N bytes for <id> (not written yet)` | `file stage <id> <srcPath>` |
| `install <id> written=…` + 一行 `  判据力：…` | `file install <id>` |
| `staged manifest N bytes for <id> (not written yet)` | `file stage-manifest <id> <srcPath>` |
| `install-manifest <id> written=…` | `file install-manifest <id>` |
| `show <id> path=… size=… mtimeFileClockS=…` + `  identity …` | `file show <id>` |
| `--- round <k> ---` | `swap <id> <rounds>` 每轮的开头（其后的 eject/adopt 行同上两行） |
| `usage: <规范形态>  (got N argument(s))` | 任何参数形状不对的命令 |
| `wrong command: <原样输入>` | 任何未匹配的输入（含 `help pod`、根上的 `..`） |

### `pod destroy`（及收尾拆局）的报告

字段集合与 `Samples/Embedding` 的 `PrintReport` 同源：

| 行 | 含义 |
|---|---|
| `clean=true/false` | 本次拆局是否干净。计数未归零、有失败记录、有残留，都会是 false |
| `handleWasStale=` | 拆的时候句柄是否已失效（§5.1：句柄失效是预期内，不是错误） |
| `counters diff …` | effects / services / subscriptions / pluginInstances / scopes 相对建局的净增 |
| `failed: <id> [phase] <msg>` | 装配期失败记录。**波 2 起 `phase=load` 多了「带期望比对不符」这一族**（D70：与 HeaderVersion 不符同位）；`pod new/new-raw` 走 `Strict=true`，实践中为空（见第 11 节） |
| `residual: <owner> x<n>` | 没还干净的账，按 owner 点名 |
| `hotswap log (N)` | 本局的进出账，每条 `eject:<id>` / `adopt:<id>`（§9.2「每次进出有账可查」） |

### `eject` 一行

`eject <id> binaryUnloaded= mappingRemoved= reopenWritable= mappingRemovalIsObservable= reopenWritableIsMeaningful=`
（有 note 时另起一行 `note: …`，如 `kept resident: other pod holds instances`）。

前三个是 §8.2 档二证据的读数，后两个是**判据力的平台声明位**——它们声明「本平台哪个字段有
判据力」，没有它们，下面的组合在 Windows 上就成了没解释的噪声：

| 平台 | 有判据力的读数 | 实测形态 |
|---|---|---|
| Windows | `binaryUnloaded` + `reopenWritable`（写不开 = 镜像还映射着 = Eject 没真卸） | `mappingRemoved=false` **属正常**，`ReopenWritableIsMeaningful=true` |
| Linux | `mappingRemoved`（§8.2 档二主判 = 映射条目消失） | 文件覆盖是 truncate-in-place，`reopenWritable` 那条判据**为空转**（CLAUDE.md 规矩 6） |

`binaryUnloaded=false` 且无错误 = 其他局还持有该插件的实例，货留在架上（§8.1 kept-resident）。

### `adopt` 两行

第一行 `adopt <id> registeredBy=<来源>` 是前置说明，波 2 起来源**随局分账**（D85；M1 那个
Host 级、不按局分账的 `KnownBinaries` 路径账已退役，D71）：catalog 局打
`catalog <快照目录>`；raw 局打它的 plan 路径（这行只为可见性保留，稍后即拒）；
Id 不在活动局条目里打 `not-in-active-pod`。第二行按走向四选一：

1. 成功——字段行（`Include/Vase/Host/Evidence.h`）：

| 字段 | 含义 |
|---|---|
| `reusedResidentImage` | 是否复用了仍在内存的镜像（§5.6 判定流②）；复用分支同样必须 `identityVerified` |
| `identityVerified` | **档三**：内存镜像的身份特征 == 磁盘文件的身份特征 |
| `importEnforcementPassed` | §8.7：导入表不含兄弟插件（兄弟集 = 快照全量，D71） |
| `outgoingEdges` | 装配后账本上的出边数（「每次解析落账」的凭证） |
| `manifestVerified` | **波 2 新增**：加载期清单比对发生且通过（D70，与 `importEnforcementPassed` 对位） |

2. 执法拒绝（Ok + Status，CLI 现拼）——`adopt refused: unresolved declarations […]` /
   `adopt refused: provides collision […]`；
3. Err（误用 / 环境 / 身份 / 比对）——`adopt failed: <消息>`，消息含
   `not in catalog snapshot`（Catalog 侧归因）、`manifest/binary mismatch for "<id>": …`、
   `request/expectation id mismatch` 与 `manifest expectation required`（D88 契约 token）；
4. raw 局——`adopt requires a catalog-backed pod (use: pod new <pluginDir>)`。

Windows 上 `Adopt` 会直接拒绝**没有 CodeView(RSDS) 调试目录**的二进制——构建侧
`/DEBUG:FULL` / `-Wl,--build-id=sha1` 两条承重 flag 与此对位（CLAUDE.md「工具链 flag 是承重的」）。

### `file show` 一行

`show <id> path= size= mtimeFileClockS=<秒>` + `identity kind= bytes=<hex>`。
两个字段名自陈来历，别读错：`mtimeFileClockS` 是**各平台自己的 `file_clock` 历元**
（Windows 1601 / Linux 1970）起的秒数，只可与同平台读数比；`identity kind` 在
Windows 是 `pdbCodeView`、Linux 是 `elfBuildId`。指纹缺失（`identity: unavailable — …`）
是诊断信息，**不判命令失败**；而 size/mtime 整个读不到才判失败（读不到 = 没做成一次观察）。

### 三档证据的端到端形态

`swap <id> <rounds>` 把 eject+adopt 连跑 N 轮（每轮打 `--- round <k> ---`），全程不碰磁盘；
配合第 7 节的双 install 就是「内存循环换件」与「磁盘真换件（两半同换）」两条路径。

## 9. 退出码规则

`VaseConsole` 的退出码就是它的全部判据（规则编号与 spec §5.2 / 代码注释一致，波 2 未动）：

| 退出码 | 条件 |
|---|---|
| **0** | 全程无失败，**且**以 `exit` 结束 |
| **1** | ① 任何命令收到 Err、或**根本没匹配上**（含内建命令参数不对，如 `help pod`、裸 `!`）；② 任何一次拆局（含收尾）不 Clean；③ 结尾没走 `exit`；④ 输入流出错 |
| **2** | 参数形态不对、脚本文件打不开（会话还没开始，判定没发生） |

几个**打印了像失败、但不判失败**的输出，是这套判据的关键边界：

- `get: no provider for Vase.Hello.Greeter`——eject 之后拿不到服务是一次**有效观察**；
- `emit 2` 在无人订阅时照常成功——空派发合法（§2.4）；
- `file show` 打 `identity: unavailable`——诊断信息，非命令失败。

反过来，**静默的失败路径也要记进退出码**：敲错一行命令不会让脚本中断，但会把整场判成
非零——回放的价值（一份记录可重放、退出码可断言）正建立在这条上。`UnmatchedCommand`
打的 `wrong command: …` 文案被 ctest 按子串钉着，改动要连带改测试。

波 2 新增的**预期失败回放族**都按第 10 节成对钉码钉文：比对拒（adopt failed 链）、
raw 局 adopt/清单命令拒绝（catalog-backed 链）、eject/adopt 执法拒绝两族（M2a 起已有，未动）。

任何停止原因（`exit` / 脚本读完 / 输入流出错）下，**main 都会先拆净所有活局**再判定：
忘写 `pod destroy` 的局会在结尾自动拆掉、打完整报告并照常参与规则 ②。但忘写 `exit`
本身按规则 ③ 判非零。

## 10. 自带的回放资产与 ctest

构建后，`Tools/VaseConsole/CMakeLists.txt` 用 `file(GENERATE)` 在构建树生成一批回放资产，
路径 `build-*/<preset>/Tools/VaseConsole/replay/`。这些文件**本身就是现成的使用教程**，
直接 `--script` 任意一份就能跑。波 2 的资产分两类：**旁路材料**（plan 文件族）与
**catalog 回放树**——后者按「写盘家族独占一棵根」的纪律排布（一个家族跑完留下的脏态
不许成为下一个家族的起点）：

| 资产 | 演示的东西 |
|---|---|
| `exit_only.txt` | 最小会话：空转到 `exit`，rc=0 |
| `plan.txt` / `pod_cycle.txt` | raw 旁路：建局 → 列表 → 拆局（第 6 节主链版的旁路对照） |
| `no_exit.txt` | 规则 ③：脚本没走 `exit` → 非零 |
| `bad_plan.txt` / `bad_plan_script.txt` | plan 路径不存在 → 建局前拒绝 → 非零 |
| `unknown_command.txt` | 敲错一行（`ejct`）→ 非零 |
| `manifests/hello_prime.json` | Prime 的配对清单（D87 双 install 的另一半；展示字段逐字按描述符抄） |
| `cat_hot/HelloShadow/` + `hotswap.txt` | 双 install 同步换件全链 + 拆局重建（第 7 节那段的来源）——`VaseConsoleAdoptManifestSynced*` 一族 |
| `cat_q/HelloShadow/` + `quiet_swap.txt` | 局内 `[安静]` 证人（`…ReplayReason`）的自持根（T14c 自足化：每棵根每次刷回只被它自己的用例写） |
| `cat_mm1|cat_mm2/HelloSwap/` + `mismatch_1|2.txt` | 反面材料：**只换二进制不更清单 → adopt 比对拒**（IsFailure 与 Reason 各吃一棵刚刷回的根） |
| `cat_show/` + `solve_showcase.txt` | `catalog refresh/solve` 展示面：一 load 一 skip |
| `cat_ab/` + `preset/only_*.json` + `adopt_blocked.txt` | 同目录双插件 + 两份 preset 的碰撞拒绝局（各局只放一只进局，双 kLoad 会在计划级执法处整局拒、到不了 Adopt） |
| `swaploop.txt` | `swap Vase.Hello 3`：三档证据的内存循环（波 2 起吃 `cat_hot`） |
| `behaviour.txt` | `emit` / `plugins` / eject 后 `get` 拿不到提供方（波 2 起吃 `cat_hot`） |
| `unknown_id.txt` / `blocked_plan.txt` / `eject_blocked.txt` | eject 侧拒绝：不存在的 Id、提供方被消费者拦下（eject 不依赖 catalog，留 raw） |

对应地，ctest 里有一族 `VaseConsole*` 前缀的用例把这些脚本钉成回归基线（条数见
CLAUDE.md「构建与测试」的基数表，此处不复制）。窄跑用 `-R VaseConsole`。家族形状沿用
M2a 的「IsFailure 钉退出码 + Reason 钉文本」成对制（两者不可合并，同设反而出反结果）：
`VaseConsoleAdoptManifestSynced{,Reason,ReplayReason,ShowReason,AdoptReason}`（换件链 rc +
重建局 `[响亮]` + 局内 `[安静]`（cat_q 自持根）+ `file show` 报告形状 + `manifestVerified=true`）、
`VaseConsoleAdoptManifestMismatch{IsFailure,Reason}`
（钉 `manifest/binary mismatch`）、`VaseConsoleRawAdoptRefused{IsFailure,Reason}`（钉
`catalog-backed`）、`VaseConsoleSolveShowcase{,Reason,SkipReason}`、
`VaseConsole{Eject,Adopt}Blocked{IsFailure,Reason}` 与既有各族。

所有 catalog 家族的用例 require `VaseConsoleCatalog` fixture（setup 是 `VaseConsoleCatalogStage`：
把每棵回放树从**任何用例都不写**的源刷回初始态），并共享 `RESOURCE_LOCK VaseConsoleCatalogTree`。
**自足化（T14c，R9 先例对齐）**：synced 家族的两级 label 证人各吃**脚本自己**造出的态——
`…Reason` 钉 `prime v2 x1 [响亮]`，fresh (base, base) 起跑出自 hotswap.txt 尾段的拆局重建腿，
(prime, prime) 起跑出自第一条 get，两态恒真；`…ReplayReason` 钉局内 `prime v2 x1 [安静]`，
吃自持根 cat_q（每次刷回 (base, base)、只被该用例写）。二者单跑（窄 `-R` 选中单条）都自足，
不再依赖同族用例的先后；各棵根「独占一棵」的纪律（mismatch 家族先例）由此推广到全家族，
`-R VaseConsoleAdoptManifestMismatch` / `SolveShowcase` / `RawAdoptRefused` 等亦逐族单跑核实过。
M1 raw 面的 `VaseConsoleSwapStage` / `swap_plan.txt` / `swap_target.dll` 自 T12 起无任何用例
require，已随 T14 收口波退役（CLAUDE.md 规矩 6 同步）。

> ⚠ **手工跑过 `hotswap.txt` / `mismatch_*.txt` 之后，影子会留在脏态**（刷回只在 ctest 开头的
> fixture 里发生）。手工重刷：`cmake -P <构建树>/Tools/VaseConsole/replay/stage_catalog.cmake`。
> mismatch 家族尤其吃起点——换过一边就复现不出「单边脏」的拒绝路径了。

## 11. 已知边界与坑

- **plan 格式没退场，但只是旁路**（第 5 节，D85/D68）：raw 局 `Expected=nullptr`、不比对，
  且 adopt 与清单命令都拒绝它。跨会话持久化、按局分账仍不在其职责内——catalog 与路径条目
  随局分账是波 2 起的新事实（D85），raw 局的条目表只服务 `file install/show` 的路径定位。
- **暂存槽是会话级的、且是两条**（二进制槽 + 清单槽，grilling Q4 裁），不随局走：`pod destroy`
  不清它。同一 Id 的暂存字节可能被装到之后某局的路径上——stage 与 install 之间的局切换要自己
  心里有数；两槽不串型（清单装不进 `file install`，反之亦然）。
- **建局走 `Strict=true`**：带失败记录的局根本建不出来（一局要么完整要么失败），
  所以 `failed:` 行实践中为空。框架默认是宽容模式，这里只是不开放它。
- **拒绝文案自 D21/T10 起有两个来源，波 2 起各源里的成员变了**：执法拒绝（被消费者挡 /
  声明不齐 / Provides 碰撞）走 Ok+Status，`eject refused: ` / `adopt refused: ` 行由 CLI 从报告
  字段现拼；误用、环境/身份与比对失败走 Err，由 `eject failed: ` / `adopt failed: ` 冠词包住
  （内文前缀不齐：`plugin not in pod: <id>` 光秃，Host 的两枚新误用文本自带 `adopt refused: ` /
  裸 `adopt request/expectation id mismatch` 前缀）。`unknown plugin id` 已随路径账退役（D88）。
- **`help <词>` 也算错命令**（内建 `help` 不吃参数），会判非零——想查子菜单在根提示符下敲
  `help`，它会连子菜单一起列。
- **中文输出行（如 `file install` 的「判据力：…」声明）要求终端按 UTF-8 解码**：全仓库以
  `/utf-8` 构建，工具发出的是 UTF-8 字节；在按 GBK（CP936）解码的 `cmd` / 控制台里会花成
  `鍒ゆ嵁鍔?` 一类的字。`chcp 65001` 换码页，或用 Windows Terminal / Git Bash（默认 UTF-8）。
  工具本身没有坏——ASCII 行照常，只有中文行花。（`get:` 的问候串自波 2 起也带中文 label——
  `[安静]` / `[响亮]` 同受这条约束。）
- Debug 构建下若宿主析构时还有活局，`~PluginHost` 的断言先崩（main 已保证不会走到；
  自己改代码接 VaseConsole 时留意这条）。
