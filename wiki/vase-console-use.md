# VaseConsole 使用文档

> 本文描述**仓库里实有**的工具：`Tools/VaseConsole/`，CMake target 与二进制同名 `VaseConsole`，
> 是 Vase 的**交互式热插拔验证台**。文中所有输出块都是在 `win-x64-clang-debug`（2026-09-23）
> 实跑截取的，不是示意。
>
> - 设计依据：`docs/superpowers/specs/2026-09-21-vase-console-design.md`（下文以 spec 指代）。
> - 构建命令、测试基数、各线前提：以 [`CLAUDE.md`](../CLAUDE.md)「构建与测试」为准，本文不复制。
> - 机制本身（Loader、依赖账本、三档证据）以磁盘上的头文件与实现为准，本文只讲「怎么用、输出怎么读」。
> - **不要与架构 §11.1 的 `Tools/VaseCli` 混为一谈**——那是尚不存在的清单扫描 / 校验工具，
>   与本验证台是两回事（CLAUDE.md「项目状态」一条有同样的提醒）。

## 目录

1. [它是什么、能干什么](#1-它是什么能干什么)
2. [拿到二进制与启动](#2-拿到二进制与启动)
3. [两种运行方式与菜单机制](#3-两种运行方式与菜单机制)
4. [命令一览](#4-命令一览)
5. [plan 文件格式](#5-plan-文件格式)
6. [最小会话：建局与拆局](#6-最小会话建局与拆局)
7. [换件全流程：eject → file install → adopt](#7-换件全流程eject--file-install--adopt)
8. [报告怎么读](#8-报告怎么读)
9. [退出码规则](#9-退出码规则)
10. [自带的回放资产与 ctest](#10-自带的回放资产与-ctest)
11. [已知边界与坑](#11-已知边界与坑)

## 1. 它是什么、能干什么

Vase 只负责发现插件、装配、运行、干净关停；`VaseConsole` 把这条链路交给**人**操作：

- 用一份 plan 文件**建一局**（`pod`），看它的服务、事件与插件列表；
- 对局里的插件**卸（eject）/ 领回（adopt）/ 循环换（swap）**，每一步打印三档证据报告；
- 把新字节**写回**磁盘上登记的 DLL/SO 路径再领回（`file stage` / `file install`）——
  这是「真换件」，不是内存里过家家；
- 拆局时打印残留计数，**退出码即「全程是否 Clean」**——同一套命令既能敲也能 `--script` 回放，
  所以一份会话记录就是可重放的回归证据。

它管理的材料只有 `Samples/` 下的示例插件：`HelloPlugin.dll`（Id `Vase.Hello`，
服务 `Vase.Hello.Greeter`）与 `HelloPluginPrime.dll`（同 Id 的换件材料，`Greet()` 返回
`hello from Vase.Hello prime v2`）。它不提供业务逻辑，也不是发布物的一部分。

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
  （`pod destroy` 打 `destroy index=…`、`file install` 打 `install … written=…`）——
  那是 ctest 按子串钉着的输出契约，不是漏了 `pod` / `file`。逐行的对应关系见第 8 节开头的表；
- 本文各输出块末尾的 `rc=N` 行是示例附注的**进程退出码**（shell echo），不是工具的输出。

一份失败样例（plan 里的路径不存在；这份脚本由构建生成在回放目录，见第 10 节）：

```
vase> pod new: plan line 1: binary not found: /definitely/not/here.dll
rc=1
```

### 菜单机制：根菜单与子菜单

命令分三层：根上的 6 条（`eject` `adopt` `swap` `get` `emit` `plugins`），
子菜单 `pod` 的 4 条，子菜单 `file` 的 3 条。在根提示符下**两段名直接敲**即可：

```
pod new myplan.txt
file show Vase.Hello
```

也可以单独敲 `pod` 或 `file` **进入子菜单**（提示符变成 `pod> `）。进了子菜单要留意：

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
| `pod new <planFile>` | 1 | 按 plan 文件建一局并设为**活动局**；逐局以 `Strict=true` 装配 |
| `pod use <index>` | 1 | 把已存活的局选为活动局（index 来自 `pod list`） |
| `pod list` | 0 | 列出所有活局：`[index] gen= plan= plugins=`，活动局标 `<- active` |
| `pod destroy` | 0 | 拆掉活动局并打印完整报告；报告不 Clean 判非零 |
| `eject <id>` | 1 | 从活动局卸下插件，打印 §8.2 三档证据报告 |
| `adopt <id>` | 1 | 把已卸插件领回活动局，打印身份比对报告 |
| `swap <id> <rounds>` | 2 | eject+adopt 同一插件 N 轮（`rounds` 必须是正整数，没有默认值） |
| `get` | 0 | 打印活动局里 `Vase.Hello.Greeter` 提供方的问候串 |
| `emit <n>` | 1 | 向活动局派发 `GreetEvent{n}` |
| `plugins` | 0 | 列出活动局在装插件的 Id 与失败记录 |
| `file stage <id> <srcPath>` | 2 | 把 `<srcPath>` 的字节读进**会话暂存槽**，不落盘 |
| `file install <id>` | 1 | 把暂存字节覆盖写到该 Id 在 plan 里登记的路径 |
| `file show <id>` | 1 | 打印登记路径、大小、mtime 与磁盘上的身份指纹 |

内建命令：`help`、`exit`、`history`、`! <history entry index>`。

参数形状不对（多给、少给、`swap 3` 这种漏了 rounds）会打一行 `usage: …  (got N argument(s))`
并判非零；`pod` / `file` 之外没有第三层。除 `pod new/use/list` 和四条内建命令外，
其余命令都需要**存在活动局**：`eject` / `adopt` / `swap` / `get` / `emit` / `plugins` 会先撞一句
`no active pod (use: pod new <planFile>)` 并判非零；`file stage/install` 的无局分支**不打字只判码**，
`file show` 则报 `… is not in the active pod's plan`。

## 5. plan 文件格式

`pod new` 吃一份纯文本 plan：每行一条「Id ↔ 二进制路径」。解析规则（`PlanFile.cpp`）：

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

> **这是 M1 的临时形态，不是架构 §5.1 的清单格式**（D12）：plan 文件与 Host 的
> `KnownBinaries` 一起在 M2 随 Catalog 退场。不要把它当插件清单的雏形去扩展。

## 6. 最小会话：建局与拆局

`plan.txt` 登记 `HelloPlugin.dll` 后的完整一回（回放资产 `pod_cycle.txt`，原文用绝对路径，
这里以相对形式代写）。先敲这四行：

```
pod new plan.txt
pod list
pod destroy
exit
```

实测输出——`destroy index=0` 是 **`pod destroy` 打的报告行**，不是敲出来的命令（第 3 节那条警告）：

```
vase> pod created index=0 generation=1
  plugin: Vase.Hello
vase> pods: 1
  [0] gen=1 plan=D:/Git/Vase/build-win/win-x64-clang-debug/Tools/VaseConsole/replay/plan.txt plugins=1  <- active
vase> destroy index=0
clean=true handleWasStale=false
counters diff (baseline = pod creation): effects=0 services=0 subscriptions=0 pluginInstances=0 scopes=0
hotswap log (0):
rc=0
```

三个读法要点：

- `index=` / `generation=` 是 `PodHandle` 的两半。`pod list` 里的 index 就是 `pod use` 的参数；
- `counters diff` 的基线是**建局时刻**：一局建了又拆干净，五个计数应回到 0——
  任何非零都是一处没还干净的账（残留在 `residual:` 行逐个点名）；
- `hotswap log (0)` 空着，因为这一局没有进出；有进出时每条记 `eject:<id>` / `adopt:<id>`。

多局并存时 `pod new` / `pod use` 切换活动局，`eject`、`adopt`、`get` 等命令**只作用于活动局**。

## 7. 换件全流程：eject → file install → adopt

这是本验证台相对 gtest 用例的**唯一增量**：把新字节真正写上磁盘、再让宿主领回（spec §3.2 推论 1）。
敲的十条命令（回放资产 `hotswap.txt`，原文用绝对路径，这里以相对形式代写）：

```
pod new swap_plan.txt
get
eject Vase.Hello
file stage Vase.Hello HelloPluginPrime.dll
file install Vase.Hello
adopt Vase.Hello
get
file show Vase.Hello
pod destroy
exit
```

下面整段是对应的实测输出。`staged` / `install` / `show` / `destroy` 开头的行分别是
`file stage` / `file install` / `file show` / `pod destroy` 的报告行——组前缀不在，属契约（第 3 节）：

```
vase> pod created index=0 generation=1
  plugin: Vase.Hello
vase> get: hello from Vase.Hello
vase> eject Vase.Hello binaryUnloaded=true mappingRemoved=false reopenWritable=true mappingRemovalIsObservable=false reopenWritableIsMeaningful=true
vase> staged 32256 bytes for Vase.Hello (not written yet)
vase> install Vase.Hello written=true path=D:/Git/Vase/build-win/win-x64-clang-debug/Tools/VaseConsole/replay/swap_target.dll
  判据力：Windows → 覆盖写不开即「Eject 没真卸」（sharing violation）。本条**有**判据力
vase> adopt Vase.Hello registeredBy=D:/Git/Vase/build-win/win-x64-clang-debug/Tools/VaseConsole/replay/swap_plan.txt
adopt Vase.Hello reusedResidentImage=false identityVerified=true importEnforcementPassed=true outgoingEdges=0
vase> get: hello from Vase.Hello prime v2
vase> show Vase.Hello path=D:/Git/Vase/build-win/win-x64-clang-debug/Tools/VaseConsole/replay/swap_target.dll size=32256 mtimeFileClockS=13434607886
  identity kind=pdbCodeView bytes=b01f4db2e95642414c4c44205044422e01000000
vase> destroy index=0
clean=true handleWasStale=false
counters diff (baseline = pod creation): effects=0 services=0 subscriptions=0 pluginInstances=0 scopes=0
hotswap log (2):
  eject:Vase.Hello
  adopt:Vase.Hello
rc=0
```

换件发生了：第一次 `get` 是 `hello from Vase.Hello`，覆盖写字节并 `adopt` 之后
变成 `hello from Vase.Hello prime v2`。链路上的分工：

- `file stage` 只读进内存，**不碰磁盘**——它成功不代表换件发生；
- `file install` 才是落盘的那一步，且只能 install 到**暂存时绑定的那个 Id**
  的登记路径（暂存槽与 Id 绑成一个值，两者不可能错配；`nothing staged` 即没绑上）；
- `adopt` 的档三身份比对（`identityVerified=true`）证明内存镜像与磁盘文件是**同一份新字节**
  ——在 Linux 上，「换件真的发生」这件事就靠它而不是靠文件锁（见下一节）。

不 eject 直接 install 会得到 `written=false`——这不是命令失灵，而是上面说的 Windows 判据
**正在工作**（镜像还映射着 = 「Eject 没真卸」或压根没 eject，写不开、判非零、磁盘一个字节不动）。
正确顺序就是本节脚本那五行：`eject` → `file stage` → `file install` → `adopt`。

上面用预编的 Prime 只是回放道具（ctest 中途不能跑编译器）。**真实开发回路**比这短，
且不需要 `file stage` / `file install`——那两条是给「新字节在别处」准备的：

```
（plan 登记的就是编译产物 bin/HelloPlugin.dll）
eject Vase.Hello
（另开终端：改代码 → cmake --build --preset win-x64-clang-debug）
adopt Vase.Hello          ← 原路径就地重读新字节
get                        → 新行为
```

`eject` 的「真卸下」在这里恰好是 linker 需要的东西：映射一放，重链才写得动这个文件。
而机制对「换进来的必须不同」没有任何要求：install/adopt 从不记得旧字节长什么样，
adopt 只查 Id 相符、`HeaderVersion` 相符、身份特征在、导入表干净，再做**当下**内存↔磁盘的
档三比对。实测：一行代码不改地重链，Windows 的 CodeView 指纹前 8 字节每次链接都换
（GUID 由链接器重新生成；Linux 的 build-id 则是内容 sha1）——都不影响 adopt 通过。

> ⚠ **自己演示换件时，别把 plan 直接登记 `bin/HelloPlugin.dll` 再 install 覆盖它**：
> 覆盖会把它的 mtime 顶到比 `.obj` 新，ninja 判定 up-to-date、`cmake --build` 报
> "no work to do" 而**从不恢复它**（实测，回放资产的影子路径就是为此而设）。
> 真弄脏了就删掉那个 DLL 强制重编，或从另一棵构建树拷回。

## 8. 报告怎么读

### 先对表：哪行输出来自哪条命令

工具不回显命令（第 3 节），所以读日志要靠这张表。特别注意 `destroy` / `install` 两行
**开头没有组前缀**（`eject` / `adopt` / `show` 同样以命令名开头）——那是被 ctest
按子串钉着的输出契约，不是命令写漏了字：

| 输出行（开头形态） | 来自 |
|---|---|
| `pod created index=…` + `  plugin: <id>` 若干 | `pod new <planFile>` |
| `pods: N`（`  [i] gen= plan= plugins=`，活动局标 `<- active`） | `pod list` |
| `active pod: index=… generation=…` | `pod use <index>` |
| `destroy index=…` + 完整拆局报告 | `pod destroy`；`exit` / 脚本读完时的**收尾拆局**打同样的行 |
| `eject <id> binaryUnloaded=…`（有 note 时另起 `  note: …`） | `eject <id>`；`swap` 每轮各打一次 |
| `adopt <id> registeredBy=…` 一行 + `adopt <id> reusedResidentImage=…` 一行 | `adopt <id>`；`swap` 每轮各打一次 |
| `emitted GreetEvent seq=…` | `emit <n>` |
| `get: …`（问候串或 `no provider …`） | `get` |
| `plugins live=…` | `plugins` |
| `staged N bytes for <id> (not written yet)` | `file stage <id> <srcPath>` |
| `install <id> written=…` + 一行 `  判据力：…` | `file install <id>` |
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
| `failed: <id> [phase] <msg>` | 装配期失败记录。`pod new` 走 `Strict=true`，实践中为空（见第 11 节） |
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

第一行 `adopt <id> registeredBy=<plan 路径或 not-in-active-plan>` 是前置说明：
`KnownBinaries` 是 **Host 级、不按局分账**的（spec §8.1），多局并存时看得见来源。
第二行字段（`Include/Vase/Host/Evidence.h`）：

| 字段 | 含义 |
|---|---|
| `reusedResidentImage` | 是否复用了仍在内存的镜像（§5.6 判定流②）；复用分支同样必须 `identityVerified` |
| `identityVerified` | **档三**：内存镜像的身份特征 == 磁盘文件的身份特征 |
| `importEnforcementPassed` | §8.7：导入表不含兄弟插件 |
| `outgoingEdges` | 装配后账本上的出边数（「每次解析落账」的凭证） |

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
配合第 7 节的 `file install` 就是「内存循环换件」与「磁盘真换件」两条路径。

## 9. 退出码规则

`VaseConsole` 的退出码就是它的全部判据（规则编号与 spec §5.2 / 代码注释一致）：

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

任何停止原因（`exit` / 脚本读完 / 输入流出错）下，**main 都会先拆净所有活局**再判定：
忘写 `pod destroy` 的局会在结尾自动拆掉、打完整报告并照常参与规则 ②。但忘写 `exit`
本身按规则 ③ 判非零。

## 10. 自带的回放资产与 ctest

构建后，`Tools/VaseConsole/CMakeLists.txt` 用 `file(GENERATE)` 在构建树生成一批回放资产，
路径 `build-*/<preset>/Tools/VaseConsole/replay/`。这些文件**本身就是现成的使用教程**，
直接 `--script` 任意一份就能跑：

| 资产 | 演示的东西 |
|---|---|
| `exit_only.txt` | 最小会话：空转到 `exit`，rc=0 |
| `plan.txt` / `pod_cycle.txt` | 建局 → 列表 → 拆局（第 6 节那段的来源） |
| `no_exit.txt` | 规则 ③：脚本没走 `exit` → 非零 |
| `bad_plan.txt` / `bad_plan_script.txt` | plan 路径不存在 → 建局前拒绝 → 非零 |
| `unknown_command.txt` | 敲错一行（`ejct`）→ 非零 |
| `swaploop.txt` | `swap <id> 3`：三档证据的内存循环 |
| `swap_plan.txt` / `swap_target.dll` / `hotswap.txt` | 磁盘真换件全流程（第 7 节那段的来源） |
| `behaviour.txt` | `emit` / `plugins` / eject 后 `get` 拿不到提供方 |
| `unknown_id.txt` | eject 一个不存在的 Id → `plugin not in pod` → 非零 |
| `blocked_plan.txt` / `eject_blocked.txt` | 提供方+消费者一局，eject 提供方被账本拦下 → `eject refused:` 点名 → 非零（D21 执法拒绝走 Ok+Status 的 console 形） |
| `collision_reg_plan.txt` / `adopt_blocked_plan.txt` / `adopt_blocked.txt` | 注册局拆掉后在另一局 adopt 碰撞者 → `adopt refused: provides collision [...] by ...` 点名 → 非零（③'/D43 的 console 形；两 plan 分开是因为同 plan 会被 T8 计划级执法整局拒） |

对应地，ctest 里有一族 `VaseConsole*` 前缀的用例把这些脚本钉成回归基线（条数见
CLAUDE.md「构建与测试」的基数表，此处不复制）。窄跑用 `-R VaseConsole`；换件一族
（`VaseConsoleHotSwap*`）带一个 `VaseConsoleSwapStage` fixture，选中任意一条都会自动
在开头把影子 `swap_target.dll` 从 `bin/HelloPlugin.dll` **刷回原始字节**。

> ⚠ **手工跑过 `hotswap.txt` 之后，影子会留在 prime 字节**（fixture 只在 ctest 开头刷）。
> 立刻再跑一遍，第一次 `get` 就会直接打出 `prime v2`——「换件」看起来没发生，其实是被
> 上一次跑掉了。手工跑完记得刷回（与 fixture 同一动作）：
> `cmake -E copy_if_different bin/HelloPlugin.dll Tools/VaseConsole/replay/swap_target.dll`

## 11. 已知边界与坑

- **plan 文件与 `KnownBinaries` 是 M1 过渡物**（第 5 节），M2 随 Catalog 退场；
  跨会话持久化、按局分账都不在其职责内。
- **暂存槽是会话级的**，不随局走：`pod destroy` 不清它。同一 Id 的暂存字节可能被装到
  之后某局的路径上——`file stage` 与 `file install` 之间的局切换要自己心里有数。
- **建局走 `Strict=true`**：带失败记录的局根本建不出来（一局要么完整要么失败），
  所以 `failed:` 行实践中为空。框架默认是宽容模式，这里只是不开放它。
- **拒绝文案自 D21/T10 起有两个来源**：执法拒绝（被消费者挡 / 声明不齐 / Provides 碰撞）走
  Ok+Status，`eject refused: ` / `adopt refused: ` 行由 CLI 从报告字段现拼；误用与环境/身份类走
  Err，由 `eject failed: ` / `adopt failed: ` 冠词包住（内文前缀不齐：`plugin not in pod: <id>` 光秃，
  `adopt refused: … is already in pod` 自带前缀）。
- **`help <词>` 也算错命令**（内建 `help` 不吃参数），会判非零——想查子菜单在根提示符下敲
  `help`，它会连子菜单一起列。
- **中文输出行（如 `file install` 的「判据力：…」声明）要求终端按 UTF-8 解码**：全仓库以
  `/utf-8` 构建，工具发出的是 UTF-8 字节；在按 GBK（CP936）解码的 `cmd` / 控制台里会花成
  `鍒ゆ嵁鍔?` 一类的字。`chcp 65001` 换码页，或用 Windows Terminal / Git Bash（默认 UTF-8）。
  工具本身没有坏——ASCII 行照常，只有中文行花。
- Debug 构建下若宿主析构时还有活局，`~PluginHost` 的断言先崩（main 已保证不会走到；
  自己改代码接 VaseConsole 时留意这条）。
