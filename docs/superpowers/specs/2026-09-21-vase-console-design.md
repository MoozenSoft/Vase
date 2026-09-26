# VaseConsole 设计 —— 可交互 + 可回放的热插拔验证台

日期：2026-09-21～09-22　分支：`feat/vase-cli`
范围：新增 `Tools/VaseConsole` 与 `Samples/HelloPluginPrime`；`ThirdParty/cli` = `MoozenSoft/cli` 的 **git submodule**（`daniele77/cli` 的无异常无 asio fork）；新增 `Cmake/VaseThirdParty.cmake`；`.clang-tidy` 加一条第三方排除。**根 `CMakeLists.txt` 的 `VaseBuildOptions` 一行未改**，只多一句 `include()`。

> **更名注记（2026-09-23）**：本验证台原名 `VaseCli`、住在 `Samples/VaseCli/`；因「CLI」不体现它是**交互式**验证台，更名为 `VaseConsole` 并移入 `Tools/VaseConsole/`。**本文已按新名同步全文**——含 ctest 用例名（`VaseConsole*`）、CMake 变量（`VASE_CONSOLE_*`）与 fixture / 资源锁名。源码注释里的「spec §N」指的就是本文；分支名仍是不随之改的 `feat/vase-cli`。按本文回放历史提交时，当时那个路径是 `Samples/VaseCli`。

本文是设计定稿 + 已实施部分的实测记录。**命令、preset 与各类基数的真值来源始终是根 `CLAUDE.md`**（规矩 7）——本文引用它而不复制它。

---

## 1. 定位与非目标

### 1.1 定位

`Tools/VaseConsole` 是一个**交互式命令行验证台**，人肉驱动 M1 已落地的热插拔机制，且**同一套命令实现既能敲、也能从文件回放**：

```
交互：  VaseConsole                          # 人逐条敲，第一条是 pod new <planFile>
回放：  VaseConsole --script run.txt; echo $?
        退出码即全程是否 Clean           # 可进 CI
```

**开局只有 `pod new <planFile>` 一个入口，没有 `--plan` 这条命令行选项。** 一个动作两个入口必然分叉（谁先建局？两者混用怎么办？），而 `pod new` 单独就能表达同一件事，还让"什么时候开局"在脚本里可见。命令行只剩 `--script`（不给就是交互）。

这是它相对 `Samples/Embedding` 的实质增量：`VaseEmbedding` 的 `play | loop <N> | swapdemo` 是硬编码的四条分支，动作序列写死在 C++ 里；验证台的命令序列在文本里，人可以在任意两步之间插入任意多步，也可以在两步之间**做库外的动作**（手工重编译插件、覆盖 DLL）。后者是 gtest 结构上做不到的。

### 1.2 非目标

- **不提供业务逻辑、不做编辑器、不做引擎适配**（项目定位不变）。
- **不新增 Vase 机制。** 命令面严格落在 `PluginHost` 现有公开 API 上（§3）。任何"命令想要但 API 不给"的项一律砍掉或退回 v1，不绕过账本去凑。
- **不替代 `Tests/HotSwap/`。** 测试是断言，验证台是观察；它给的是 gtest 拿不到的场景（跨进程、人在中间动手），不是把已有用例换个壳。
- **不改动全项目的异常策略。** 新增的每个 TU 都关异常（这一点如何达成见 §5——答案是改库，不是开例外）。
- v0 不做行编辑与 Tab 补全（§5.5：那是 fork 的 v1 项）。

---

## 2. 实测基座（本文所有取舍的依据）

以下都是**跑出来的**。被测对象：`daniele77/cli` **v2.2.0** 与 fork 的 `master`（`769c5fa`）——两者差异见 2.13，**本表全部结论对 fork HEAD 同样成立**。

> **Git Bash 的坑（本任务期间撞到四次）**：`/EHsc` 这类斜杠开头的参数会被 MSYS 改写成 `D:/Program Files/Git/EHsc`，编译器**根本没收到**该 flag，命令照跑、结果全是假的；而 `export MSYS2_ARG_CONV_EXCL='*'` 又会反过来让 `/tmp/x.cpp` 不再被翻译，编译器报 "no such file"——**两种错都长得像"测过了"**。本节所有编译器探针都在 `MSYS2_ARG_CONV_EXCL='*'` 下用 `cygpath -w` 的绝对路径跑，并**先拿一个不含 cli 的 `tiny.cpp`（裸 `try{throw;}catch{}`）自证 flag 真的生效**（`/EHsc` → 0 error、`/EHs-c-` → 2 error）。涉及 `$` 的 WSL 命令一律落成脚本文件再 `bash <脚本>`（CLAUDE.md 已警告过，本轮又实证一次）。

| # | 测的东西 | 结果 |
|---|---|---|
| 2.1 | `#include <cli/cli.h>` + `clifilesession.h`，clang-cl `/EHs-c-` | **20 个硬 error**。抛点全在**非模板 inline 定义**里，与规矩 2 的 `EXPECT_THROW` 同形：解析期就过不去 |
| 2.2 | 同上，靠 `-isystem` 把诊断吞掉 | 能编过——**假绿**：cli 的 `catch` 没有 landing pad，"坏参数 → 打用法提示"静默退化成 terminate。已排除。2.16 会发现**本仓库自己就带着一个等价开关** |
| 2.3 | 同上，clang-cl `/EHsc` | **0 error**；`/W4 /WX`（Linux 侧 `-Wall -Wextra -Werror`，`-I` 与 `-isystem` 都试）**零警告**。顺带测出 clang++ 默认**是开异常的** |
| 2.4 | 同上，`/EHsc` **且保留** `_HAS_EXCEPTIONS=0` | **0 error**。cli 抛的是自己的 `bad_conversion : public std::bad_cast`（`fromstring.h:66`），由自己 `catch(std::bad_cast&)` 接住 |
| 2.5 | target 上追加 `/EHsc` 想盖掉 `VaseBuildOptions` 的 `/EHs-c-` | **盖不掉**：interface 的 flag 排在 target 自己的之后，后者胜。目录级 `CMAKE_CXX_FLAGS`、源文件 `COMPILE_FLAGS`、本地 interface 追加——**全部无效** |
| 2.6 | `$<TARGET_PROPERTY:…>` 让 `VaseBuildOptions` 按消费者分叉异常 flag | 技术上**成立**（不声明 → `/EHs-c-` 编不过；声明 `ON` → `/EHsc` 编过）。**本设计不用它**，被"根不该为单个 sample 携带按消费者分支"这条约束否掉（§9） |
| 2.7 | vcpkg 端口能否"关掉 cli 的异常" | **不能**：①端口 header-only，给端口的编译加 flag 是空转；②靠传播宏——**库里没有这样的宏**（v2.2.0 全部条件编译只有 include guard、`CLI_FROMSTRING_USE_BOOST`、`CLI_TELNET_TRACE`） |
| 2.8 | `cli::CliFileSession::Start()` 的线程模型 | 同步循环 `Prompt()` → `getline(in, line)` → `Feed(line)`，**跑在调用线程上**，`in` 是任意 `istream&` → 交互与回放是同一份代码换个流；§1.4 单线程契约**免费成立** |
| 2.9 | `CliLocalSession` 的线程模型 | 键盘层各有一个 `std::thread servant`（`winkeyboard.h:179`、`linuxkeyboard.h:232`）；`commandprocessor.h` 内无 thread，故派发**大概率**在主线程——**未实测**，v0 不用 |
| 2.10 | `detail/split.h`、`colorprofile.h` 在 `-fno-exceptions` 下 | **0 error**（这两个头本来就不需要改） |
| 2.11 | `from_string` 的调用面 | 非 asio 代码里**只有一个调用点**：`cli.h:716`。删数值支路不留悬空调用 |
| 2.12 | `from_string<std::string>` 的实现 | `return s;`——**不可能失败**。于是 `catch(std::bad_cast&)` 在"参数全 string"之下是死代码，**删掉即可**，不需要把 `Select<>::Exec` 从 `void` 重铺成返回失败 |
| 2.13 | fork `master`(`769c5fa`) 与 `v2.2.0`(`80541c4`) 差多少 | 20+ 提交全是 CI/CMake churn。**我们关心的 5 个头字节相同**，异常站点计数两侧都是 `throw=24 catch=6`；差异全在 `CMakeLists.txt`（+90/−69）→ 本表结论无需重测 |
| 2.14 | 非 `string` 参数现在的失败方向 | **链接期**：`lld-link: error: undefined symbol: from_string<int>`。响亮，且不是静默给个 0 |
| 2.15 | `add_subdirectory(ThirdParty/cli)` vs 自立 INTERFACE target | 都 configure/build rc=0。**但只给 include 目录会丢"这库要 C++17"** → `no template named 'optional' in namespace 'std'` 指向第三方头（clang-cl 默认 C++14）。补 `target_compile_features(... cxx_std_17)` 即等价。两条路线 include 都落地为 `-I`（实测零 `-isystem`）。`add_subdirectory` 另外带进 `find_package(Threads REQUIRED)`、Linux 上往消费者挂 `-lpthread`、`install()` 会把第三方头装进我们的分发树、嵌套 `project()` 的政策作用域 |
| 2.16 | "上游 merge 带回 throw" 在三条编译器线上的可见性 | **不等价**，见 §5.4 的表。根因是 `VaseBuildOptions` 里已有的 `/external:anglebrackets`（cl.exe 线专属）把尖括号头标为外部头，`/we4530` 升出的 C4530 一并静音：同一份上游头在 cl.exe 下 `<cli/cli.h>` **0 诊断**、`"cli/cli.h"` **5 × error C4530** |
| 2.17 | cli 是否有"接住整行剩余参数"的 Insert 形状 | **有，原生**：`cli.h:692/695` 的 `FreeformCommand` 接 `(ostream&, const vector<string>&)`，且其 `Exec` **完全不碰 `from_string`**（`:840` 直接切 token 交给处理器）→ 不需要桥接模板，也不需要给 fork 加 API |
| 2.18 | vendored 头会不会吃我们自己的 clang-tidy 检查 | **会**。`.clang-tidy:79` 是 `HeaderFilterRegex: '.*'`，改 fork 头时实测报 `Invalid case style for function 'from_string'`、`misc-no-recursion`、`readability-braces-around-statements` 等 → 已加 `ExcludeHeaderFilterRegex: '.*ThirdParty.*'`（不押路径分隔符） |
| 2.19 | ctest 4.4.3 里 `PASS_REGULAR_EXPRESSION` 与退出码的关系（四格实验，scratch 项目可复现） | **正则命中即 Passed，完全无视退出码**（非零退出 + 命中 = Passed）；与 `WILL_FAIL` **同设**时命中反而判 **Failed**（"Required regular expression found"）。→ 退出码与文本**不能在同一条用例里并立**，故用兄弟用例（见 §7.2）。追加一格：`FAIL_REGULAR_EXPRESSION` **不吞退出码**（不命中 + rc1 仍 Failed），与 PASS_REG 行为不对称 |
| 2.20 | 两条"纸面以为成立、实测不成立"的判定路径 | ① 打错的命令名走 cli 的 `WrongCommandHandler`（`cli.h:930`），**不进任何 Vase 处理器** → `bogus; exit` 实得 **rc=0**：脚本敲错一条命令、其余全成功，会假绿。② `exit` 时留着活局 → `~PluginHost` 在 Debug 断言 `!slot->Alive`（`PluginHost.cpp:155-158`），`pod new; exit` 实得 **rc=3 + assert**，我们那句 `return 1` 根本轮不到 |

---

## 3. 命令面（被现有 API 框死的那一半）

### 3.1 `PluginHost` 给的动作就这些

```
CreatePod(plan, options) → Result<PodHandle>   // KnownBinaries[Id]=path 唯一写入点：PluginHost.cpp:284
EjectPlugin(handle, id)  → Result<EjectReport>
AdoptPlugin(handle, id)  → Result<AdoptReport> // id 不在 KnownBinaries → 拒
DestroyPod(handle)       → PodReport           // 永不失败
Resolve(handle) → Pod*；Pod::{Root,PluginCount,HasPlugin,PluginIds,Failures}
```

`AdoptPlugin` 的拒消息是写死的（`PluginHost.cpp:650-657`）：`adopt refused: unknown plugin id <id> — M1 requires prior registration through a LoadPlan`。

### 3.2 三条推论（承重）

1. **"换件" = 覆盖已登记路径上的文件，不是加载一个新路径。** 而"覆盖同一路径、档三重读新身份"正是 §5.6「就地重读清单」的 M1 代偿形态，也恰是 T12 `InstallPrime()`（`Tests/HotSwap/HotSwapLoopTests.cpp:63`）验的那件事。**命令面不需要向 API 要新东西——M1 给的正好是热插拔要验的那一半。**
2. **要玩任意 DLL，靠 CLI 自己解析 plan 文件**（其**地位**见 §8.2）：**首个空白之前是 Id，其余整段（含其中的空白）都是路径**；`#` 起始行与空行忽略。数组序 = 文件序 = `LoadPlan::Ordered` 序（架构文档 §5.1 那句"数组序就是加载与关停的序"，M1 无求解器，由手写者保证）→ 转成 `vase::LoadPlan` → 交 `CreatePod`。登记仍走正规路径，**不碰 Vase API、不新增机制**。
3. **故意不提供 `load <path>` 这种"加载任意新 DLL 进当前局"的命令。** 硬做就得绕过 `KnownBinaries`，那是"为了把功能做完绕过账本体系"（技能·五条不可违反 #1）。

### 3.3 v0 命令表

| 组 | 命令 | 行为 |
|---|---|---|
| **局** | `pod new <planFile>` | 解析 plan → `CreatePod`（`Strict=true`）。回显 `PodHandle{Index,Generation}`、逐条装载结果、`Failures()` |
| | `pod use <index>` | 切换后续命令的目标局 |
| | `pod list` | 活局列表 + 各自 `PluginCount()` |
| | `pod destroy` | 打完整 `PodReport`：`Clean()` / 五项 `CountersDiff` / `Failures` / `Residuals` / **`HotSwapLog`** |
| **进出** | `eject <id>` / `adopt <id>` | 报告**全字段**输出，含 `ReopenWritableIsMeaningful` / `MappingRemovalIsObservable` 两个声明位——不打出来，Windows 上那个 `mappingRemoved=false` 就成了没解释的噪声（§9.2） |
| | `swap <id> <rounds>` | `eject` + `adopt` 跑 `rounds` 次，不碰磁盘：验"同一份字节来回"（T12 `FiveRoundsBehaveLikeFirstTime` 的交互形态）。**原表的 `swap <id>` 与 `swaploop <id> <N>` 两条已合并成这一条**——"跑一次"的写法与 `swaploop` 这个名字一起砍掉：两条命令两套 arity 不值当，`rounds` 必填且必须是正整数 |
| **磁盘面**（测试做不利索的那半） | `file stage <id> <srcPath>` | 把 src 的字节暂存待用，**不覆盖** |
| | `file install <id>` | 把暂存字节用 `std::ofstream`（不带 `app` 即截断）写到 `<id>` 的登记路径，**并把「覆盖证明了什么」按平台声明**（§4） |
| | `file show <id>` | 登记路径、size、mtime，以及磁盘侧身份指纹（`Loader::FileIdentity` 是 public static） |
| **行为面** | `emit <n>` | `Root().Emit(GreetEvent{n})` |
| | `get` | `Root().TryGet<IGreeter>()` 后打串（**不能用 `Get<T>()`：它 `required=true`，缺服务即 `ProgrammerError` 终止**，见 `Context.h:82`）——**"行为真的换了"的判据**：换件后这个串必须变；`eject` 之后它应当报"no provider"而不是崩 |
| **账目** | `plugins` | `PluginIds()` 活集合 + `Failures()` |
| **退出** | `exit` | **拆净所有活局**（逐个 `DestroyPod` 并打完整 `PodReport`），再退出；**退出码 = 全程是否都 Clean** |

两个子菜单：`pod`（`new` / `use` / `list` / `destroy`）与 `file`（`stage` / `install` / `show`）；其余（`eject` / `adopt` / `swap` / `get` / `emit` / `plugins` / `exit`）挂根菜单。**这是 cli 的 menu 嵌套，不是把空格塞进命令名**——子菜单按 `CommandSpec::Group` 惰性建立；cli 先按空白切 token，名字里带空格永远匹配不上。

**退出码规则（四条）**（回放的价值全押在它上面，所以不留默认语义）：以下任一条成立即非 0——
① 任何命令收到 `Err`，**或一条输入根本没匹配上任何命令**；
② 任何一次 `pod destroy`（**含 `exit` 自动拆的那些**）的 `PodReport.Clean()` 为 false；
③ 脚本读到结尾却没走过 `exit`（`ShellStop::kEndOfInput`）；
④ 输入流出错（`ShellStop::kInputStreamError`——一次 I/O 错误不是一次干净的运行）。

要说清的取舍：

- **③ 取代了原先的"`exit` 时仍有活局 → 非 0"。** 原规则与本表 `exit`「拆净所有局」自相矛盾（拆净了就不可能"仍有活局"），且实测**在 Debug 下根本走不到我们的判定**：`~PluginHost` 断言 `!slot->Alive`（`Source/Host/PluginHost.cpp:155-158`），`pod new; exit` 实得 **rc=3 + assert**，不是 rc=1。改为 `exit` 负责拆，判定交回规则 ②。
- **收紧的初衷没有丢。** 当初担心的是"脚本忘了写收尾 → `DestroyPod` 从未被调用 → Clean 与否从未被判定"。现在这条由 ③ 挡住：**没走 `exit` 就是非 0**，而只要走了 `exit`，拆除就一定发生、判定就一定发生。约束从"必须显式拆"变成"必须显式收口"，弱化的只是脚本风格，不是判据覆盖。
- **① 涵盖"未匹配"是实测逼出来的**：打错一个命令名（`ejct Vase.Hello`）走的是 cli 的 `WrongCommandHandler`（`ThirdParty/cli/include/cli/cli.h:930`），**不进我们任何处理器**，实测 `bogus; exit` → **rc=0**。也就是说"一条命令静默没跑、其余全成功"的脚本本来会假绿——这恰恰是验证台最不该有的形状。故经 cli 现成的 setter（`cli.h:169`）把未匹配接进失败状态。
- **`eject` 的拒因文本按路径分两种**（钉拒因文本的用例必须钉对那一条）：`eject refused: ` 只出在"被消费者挡住"那条路径（`Source/Host/PluginHost.cpp:499`）；而 `eject` 一个本局不存在的 Id 走的是 `:531`，文案是 `plugin not in pod: <id>`。验证台钉的是后者——命令层不预查成员资格、直接问 Host，所以那条串才是 Host 的真实拒绝，不是停在我们自己的前置检查上。

`help` 由 cli 提供。**v0 无 `counters`**——中途五项计数只能走 `ForTestCounters()`，那会让一个 sample 消费 `ForTest` 缝（§9）。

### 3.4 参数一律 `std::string`

每个命令处理器接 `const std::vector<std::string>&`，需要数值的地方自己用 `from_chars` 解析（与 `Samples/Embedding/main.cpp:112` `ParseCount` 同写法），失败就报错，不走异常。`emit <n>`、`swaploop <id> <N>`、`pod use <index>` 都收数，但收的是串。

在 fork 之后这条**不再是选择而是结构**：数值支路已被删除（§5.2），`from_string<int>` 根本不存在，误用会撞在链接期（2.14）。

---

## 4. `file install` 的判据必须自带平台声明

根 `CLAUDE.md` 规矩 6 记着一件实测事实：T12 那条"覆盖成功即证明镜像已卸"的断言，**Windows 上是真的 sharing-violation 探针，Linux 上是空转的**——在 Linux 上覆盖一个仍被映射的镜像会成功，同一条断言照样通过。T12 用的是 `copy_file(overwrite_existing)`；`file install` 没有磁盘源文件可整份拷贝，改用 `std::ofstream`（不带 `app` 即截断，见 §3.3），落在同一路径上效果相同：**truncate-in-place、同一 inode**。

`file install` 把同一个动作交给用户，就会把同一个陷阱交出去。因此它的输出**不允许只报成功**，必须把声明位一起打出来（那两行判据力文本由 `#ifdef _WIN32` 二选一），形如：

```
install Vase.Hello written=true path=/…/HelloPlugin.dll
  判据力：Windows → 覆盖写不开即「Eject 没真卸」（sharing violation）。本条**有**判据力
  判据力：Linux → 覆盖是 truncate-in-place（同 inode），映射着也写得开。本条**为空转**
```

文案不是装饰：**两平台同跑得到的不是同一件事的两份拷贝**。这与 `UnloadEvidence` 用 `IsMeaningful` / `IsObservable` 表达的是同一设计意图，搬到 CLI 的文本输出上。

---

## 5. Fork：`MoozenSoft/cli`（已改造，以 submodule 进仓）

### 5.1 身份与基线

- 仓库：`https://github.com/MoozenSoft/cli`，`daniele77/cli` 的 fork，`master` = `769c5fa` = 上游 master tip（含 `v2.2.0`）。
- **接入方式**：不以 vcpkg 端口取它，而是**以 git submodule 挂在 `ThirdParty/cli`**，Vase 直接消费其头文件。因此没有 tag / SHA512 / `vcpkg-configuration.json` 这一整摊——**submodule 的 gitlink commit 就是版本钉**。
- 基线取 `master` 而非回退 `v2.2.0`：依据 2.13——我们关心的头两版之间字节相同，差异全在 `CMakeLists.txt`（本来就要改），回退等于永久背一段上游 CMake 的落差。
- 归属与许可：上游 **BSL-1.0**，fork 保留其 `LICENSE` 与全部版权头未改（BSL 的 derivative-works 条款要求保留声明）；Vase 本体 MIT。**这条要进仓库的第三方许可记录**（现在文件就在仓里，比 vcpkg 路线更直接）。

### 5.2 已实施的改动（24 文件、+222 / −2067）

| 文件 | 改动 |
|---|---|
| **删除 16 个 asio 头** | `boostasio*` / `standaloneasio*` 各 3、`detail/{boost,standalone,new*,old*}asio*`、`detail/genericasio*`、`detail/genericcliasyncsession.h`、`detail/server.h`。实测 `clilocalsession.h`（v1 要的）走 `scheduler.h` / `loopscheduler.h` / `commandprocessor.h` 这条与 asio 无关的链，**本地与文件会话不受影响** |
| `detail/fromstring.h` | 283 → 62 行，只剩 `from_string<std::string>`（`return s;`）。主模板**声明不定义**（2.14） |
| `cli.h` | 删 `VariadicFunctionCommand::Exec` 的 `catch(bad_cast&)` 与 `CliSession::Feed` 的两个 catch（2.12 证明是死代码）；删 `Cli::StdExceptionHandler` 那对与 `exceptionHandler` 成员；全局命令 `!` 的 `unsigned` 参数改文本、内部用 `from_chars` 解析；加 `<charconv>` `<optional>` |
| `detail/history.h` | `At()` 返回 `std::optional<std::string>`、`IdToIndex` 返回 `std::optional<std::size_t>`，并**显式挡空 buffer**（上游那句 `id > idOfOldest + size - 1` 在 size 为 0 时回绕、比较恒假、返回越界下标）；`IndexToId` 那个不可达分支的 throw 去掉 |
| `clifilesession.h` | 构造里两处 `throw std::invalid_argument` 删除，有效性交调用方（降级成"不校验"会让坏流静默跑成空会话，所以刻意留给人） |
| `detail/{win,linux}keyboard.h` | `WaitKbHit()` 返回 `bool`、`Get()` 返回 `std::optional`，servant 线程靠返回值退出。**每轮事件顺序与上游一致**（先 `cv.wait(enabled)` 再等键）。顺带修 `linuxkeyboard.h` 两处既有 bug：`select()` 返 −1（终端 resize 的 `EINTR`）时上游掉出函数尾（UB）；`fd_set` 只在循环外初始化一次而 `select` 会改写它 |
| `CMakeLists.txt` | 删两个 asio option 及其整块（含 `CMP0167 OLD` 的 FindBoost 政策回退）；C++14 → **C++17**（`std::optional` 要）。**`find_package(Threads)` 保留**：v1 的键盘层用 `std::thread`，而它不是 `boost-asio` 的来源（那来自 vcpkg 端口 metadata） |
| `README.md` | 顶部新增「这是 fork」一节，逐条列分歧及其**为什么**；Features 里远程会话与异步接口划掉；`## Dependencies` 改为"无第三方依赖"并保留上游原文供对照 |

**不做**的事：不改 `include/cli/` 的目录层级与头文件名，merge 时目录对齐是机械活。**但"仓库里怎么包含它"是另一回事，见 §5.4 末——必须用引号。**

### 5.3 与上游的公开分歧

删 `StdExceptionHandler` 就是删掉上游 `README.md:472` 宣传的公开特性"处理器里抛出的异常会被接住"。这个特性在 Vase 里永远用不到（我们的处理器在关异常的 TU 里，抛不出来），但对读上游文档的人是行为分歧。**已写进 fork 的 README**，否则下一个人或下一次 merge 会照上游预期它存在。

### 5.4 不变式由谁把守 —— 实测：三条线不等价

**不变式**：`include/` 树里不出现 `throw` / `try` / `catch`。今天成立（实测全树 grep 排除注释后**零命中**）。

原方案说"它由我们自己的构建把守，症状是六线全部编译失败"。**这句话在 cl.exe 两条线上原本是错的**，实测（用含 throw 的上游原版头模拟"merge 带回异常"）：

| 线 | 诊断数 | 关是否活着 |
|---|---|---|
| Linux clang++ `-fno-exceptions` + `-I` | 20 error | ✅ |
| 同上但 `-isystem` | 0 | ❌ 吞诊断 |
| Windows clang-cl `/EHs-c-`（拿不到 `/external:*`，那是 cl.exe 专属块） | 20 error | ✅ |
| Windows **cl.exe** + `<cli/cli.h>` | **0** | ❌ **死的** |
| Windows **cl.exe** + `"cli/cli.h"` | 5 × `error C4530` | ✅ |

根因是 `VaseBuildOptions` 里**已有**的 `/external:anglebrackets`：它把尖括号包含的整棵头树标为外部头，`/we4530` 升出来的 C4530 随之静音。

**因此有两条规则，都写在现场**（`Cmake/VaseThirdParty.cmake` 的注释里）：① `VaseThirdPartyCli` 的 include 不得标 SYSTEM / 不得 `/external:I`；② **Vase 侧包含 cli 头一律用引号** `#include "cli/cli.h"`。第 ② 条与规矩 3 是同一机制的**反方向**应用——规矩 3 要引号是"别放过我们自己头的警告"，这里要引号是"别放过第三方头的异常"。写 `Shell.cpp` 的人第一眼会觉得这是笔误，所以注释就是它存在的全部理由。

两条都成立时，这道关在**四条开发线**上都是响亮的；fork 侧 CI 因此仍然不是承重件。

### 5.5 v1 的行编辑与 Tab 补全（fork 的存在理由）

v0 用 `CliFileSession`：裸 `getline`，无行编辑无补全。要那些得用 `CliLocalSession`，其键盘层多一个 `std::thread servant`（2.9），与 §1.4 的关系**尚未实测**——fork 之后这是"可以修"而不是"只能绕"。

**新发现的第二个 v1 前置**：`clifilesession.h:46` 给 `CliSession` 传的 history size 是 **1**（实测：`! 2` 报"没有该条目"、`history` 只列得出当前这条）。所以想要像样的历史，`Shell.cpp` 得直接用 `CliSession` 自己传大小，而不是用 `CliFileSession`。这不是 fork 引入的，是上游既有。

### 5.6 消费方式：为什么是自立 INTERFACE target

`add_subdirectory(ThirdParty/cli)` 实测可行（configure/build 均 rc=0），但会把上游的构建决策变成我们的风险，而那恰是上游改动最勤处（v2.2.0 之后那串 `Fix policies for old versions of cmake` / `cmake minimum set to interval 3.8...3.27` / boost 组件增减全在此）：`find_package(Threads REQUIRED)` 是硬 configure 依赖、Linux 上还往每个消费者挂 `-lpthread`、`install()` 会把第三方头装进我们的分发树、嵌套 `project()` 带进一层政策作用域。

改为 `Cmake/VaseThirdParty.cmake` 里自立 `VaseThirdPartyCli` INTERFACE，只贡献两样：include 目录 + `cxx_std_17`。实测代价说清：**标准要求搬到了我们手里**，漏掉那一行的症状是 `no template named 'optional' in namespace 'std'` 指向第三方头（clang-cl 默认 C++14）——响亮，但看不懂，故注释必留。两条路线的 include 都实测落地为 `-I`（零 `-isystem`），§5.4 的关都不被削弱。

---

## 6. 目录与 target 结构

```
ThirdParty/
  cli/                         # git submodule → MoozenSoft/cli（§5）
Cmake/
  VaseThirdParty.cmake         # VaseThirdPartyCli INTERFACE：include 目录 + cxx_std_17
Samples/
  HelloCommon/Greeter.h        # 现有
  HelloPlugin/                 # 现有：Id=Vase.Hello, Greet()="hello from Vase.Hello"
  HelloPluginPrime/            # 新增：同 Id，Greet() 带 " [prime]" —— file install 的换件材料
  VaseConsole/
    CMakeLists.txt
    main.cpp                   # --script（回放）/ 无参（交互）两条入口 + 退出码合成
    Commands.h / .cpp          # 命令表 + 会话状态（累计判据住这里）+ 调 Vase API
    Shell.h / .cpp             # 命令表 ↔ cli 的桥；唯一包含 cli 头的 TU（用引号，§5.4）
    PlanFile.h / .cpp          # plan 文件解析 → vase::LoadPlan，失败返回 Result
                               # 回放资产（*.txt）由 file(GENERATE) 直出到 <build>/replay/，无 .in 模板（§7.2）
```

```cmake
add_executable(VaseConsole main.cpp Commands.cpp Shell.cpp PlanFile.cpp)
target_link_libraries(VaseConsole
    PRIVATE VaseBuildOptions VaseHost VaseThirdPartyCli)
# get 要认识 samples::IGreeter（TryGet 的类型实参），它住在 Samples/HelloCommon。
target_include_directories(VaseConsole PRIVATE "${PROJECT_SOURCE_DIR}/Samples/HelloCommon")
```

**四个 TU 无一例外关异常**，都链 `VaseBuildOptions` 拿全部 8 样。没有属性开关、没有独立 static 库、没有"某个 TU 允许写 throw"——这是 fork 路线买到的东西（对比被否的 §9「异常策略」①②③）。`HelloPluginPrime` 经 `vase_add_plugin_fixture` 立（规矩 5）。

分层：`Shell.cpp` 不认识 Vase（只碰 `CommandSpec` 与 cli），`Commands.cpp` 不认识 cli。`Shell.h` 的对外面：

```cpp
struct CommandSpec {
    std::string Group;                 // 空 = 挂根菜单；非空 = 挂同名子菜单（"pod" / "file"，§3.3）
    std::string Name;                  // 子命令名，单 token（"new"，不是 "pod new"）
    std::vector<std::string> Params;   // 参数名列表，供 usage 打印；值一律 string（§3.4）
    std::string Help;
    std::function<void(std::ostream&, const std::vector<std::string>&)> Handler;
};

enum class ShellStop { kByExitCommand, kEndOfInput, kInputStreamError };
// onUnmatched：cli 的 WrongCommandHandler 桥，签名同 cli::Cli::WrongCommandHandler；空 = 维持 cli 默认。
ShellStop RunShell(const std::vector<CommandSpec>& commands, std::istream& in, std::ostream& out,
                   const std::function<void(std::ostream&, const std::string&)>& onUnmatched);
```

`ShellStop` 只说明循环为何结束；**累计判定（哪条命令收到 `Err`、哪些局从未 `pod destroy`）全部住在命令层的会话状态里**，`main.cpp` 把两者合成退出码。两层不互相以为对方在管。

`CommandSpec::Handler` 的签名与 `FreeformCommand` 原生吻合（2.17），所以 `Shell.cpp` 里**不需要 arity 桥接模板**——注册就是 `Insert(name, parDesc, handler, help)`。附带好处：这条路上根本没有类型转换，§5.4 的"异常不会出现在我们帧上"由构造成立而非由约定成立。

---

## 7. 回放用例与依赖接入

### 7.1 vcpkg

**不变。** `vcpkg.json` 的 `dependencies` 仍是 `["gtest"]`——cli 走 submodule + 自立 target（§5.6），不经过 vcpkg。`boost-asio` 的问题不再是"摘掉"而是**根本不相干**。

### 7.2 回放用例

**回放资产全部由 `file(GENERATE)` 生成到 `${CMAKE_CURRENT_BINARY_DIR}/replay/`，仓库里没有 `.in` 模板，也没有静态文本。** 原因很硬：plan 文件的内容是插件 DLL 的**绝对路径**，而那些路径只有构建系统知道（`$<TARGET_FILE:HelloPlugin>` 一类的生成器表达式）——写死在仓库里就等于把产物路径抄成第二处真值，换 preset 即失效。

**`configure_file` 这条路走不通**（本文原先写的就是它，已按实测改掉）：它是 configure 期命令，而生成器表达式要到 generate 期才求值，两者接不上。`file(GENERATE CONTENT ...)` 在 generate 期求值内容里的生成器表达式，一步到位，故不需要 `add_custom_command`。

两条实测事实要记：

- **`file(GENERATE)` 在 Windows 上产出 CRLF**（`"exit\n"` 落成 `65 78 69 74 0d 0a`）。而 cli 的 `detail::split` **不认 `\r` 为分隔符** → 每行带尾 `\r` 进 `Feed`，`exit` 变 `exit\r` 匹配不上，回放用例首跑即红。修在**消费点**（`RunShell` 剥行尾 `\r`）而不是给生成物钉 `NEWLINE_STYLE LF`：人手工写的 CRLF 脚本、以及 Linux 侧跑到一个 CRLF 脚本，都同样被兜住。
- 生成物路径本身在 configure 期就已知（`${CMAKE_CURRENT_BINARY_DIR}/...`），所以脚本里引用 plan 文件不需要第二个生成器表达式。

用例的**判据形状**由 2.19 的实测钉死：**退出码与文本不能在同一条用例里同时断**。所以规则是——每条用例默认判据只钉退出码；需要钉文本时**另起一条兄弟用例**用裸 `PASS_REGULAR_EXPRESSION`（注释写明它无视退出码、码由兄弟用例钉）；`WILL_FAIL` 与 `PASS_REGULAR_EXPRESSION` **绝不同设**。且只在"退出码看不见那个信息"时才配兄弟用例：`VaseConsolePodCycle` 不配（`rc=0` 已经蕴含 `clean=true`，因为规则 ② 就在算它），而换件串、`no provider`、`hotswap log (N)`、两条拒因文本要配。

> **落地勘误（终审写回，2026-09-26）**：案例名已随 M2b 波2 迁移；现行清单见 `wiki/vase-console-use.md` §10。
> 本节与下节的列举按历史读，不逐项写回。

已注册用例（逐条见 `Tools/VaseConsole/CMakeLists.txt`）：`VaseConsoleExitOnly`、`VaseConsolePodCycle`、`VaseConsoleNoExitIsFailure`、`VaseConsoleBadPlan`(+`…BadPlanReason`)、`VaseConsoleUnknownCommandIsFailure`(+`…Reason`)、`VaseConsoleSwapLoop`(+`…Reason`)、`VaseConsoleSwapStage`（`FIXTURES_SETUP`：把 `HelloPlugin.dll` 刷进影子 `swap_target.dll`）、`VaseConsoleHotSwap`(+`…Reason`、+`…ShowReason`、+`…AdoptReason`)、`VaseConsoleBehaviour`(+`…Reason`、+`…EjectReason`、+`…PluginsReason`、+`…EmitReason`)、`VaseConsoleRefusedIsFailure`(+`…Reason`)。四条换件用例 `FIXTURES_REQUIRED` 那个 setup 并共享一把 `RESOURCE_LOCK`，所以 `-R VaseConsoleHotSwap` 会把 setup 一并拉进来——那是设计，不是多算一条。

回放资产分两类、**不是同一条链路**：plan 文件（`plan.txt` / `swap_plan.txt` / `bad_plan.txt`）与其余按关注点各一份的脚本（`exit_only.txt` / `pod_cycle.txt` / `no_exit.txt` / `bad_plan_script.txt` / `unknown_command.txt` / `swaploop.txt` / `hotswap.txt` / `behaviour.txt` / `unknown_id.txt`）。其中完整的换件链路（`pod new` → `get` → `eject` → `file stage` → `file install` → `adopt` → `get`（串必须变）→ `file show` → `pod destroy` → `exit`）**只属 `hotswap.txt` 一份**。

判据**以退出码为主**（与 `EmbeddingLoop20` 同形态，不新增测试代码）。`--script` 是显式入口而非 shell 重定向，因为 ctest 的 `COMMAND` 不支持重定向。

> **已实测定稿**：用 `file(GENERATE CONTENT …)` + `$<PATH:CMAKE_PATH,$<TARGET_FILE:…>>`，仓库里没有 `.in` 模板（§11 第 6 桩已打掉）。`configure_file` 走不通——它是 configure 期命令，接不上 generate 期才求值的生成器表达式；`add_custom_command` 也不必，`file(GENERATE)` 一步到位。

---

## 8. 已知限制（写出来，不藏）

### 8.1 `KnownBinaries` 是 Host 级、不按 Pod 分账

`PluginHost::KnownBinaries` 是宿主成员（`PluginHost.h:107`），不在槽内。所以第二个局 `CreatePod` 灌进去的 Id，第一个局里 `adopt` 也查得到——**能领回一个只属于别局的 Id**。

这是既有形态（M1 的"以计划登记代替清单"），本文不修。但多局并存会让它**容易被用户撞到**，所以 `pod list` 与 `adopt` 的输出都带上"该 Id 登记自哪次 plan"。**不得**为了好看在 CLI 侧另建一张 Id→局 的表——那会是第三份进程级真值（规矩 7 的同族问题）。

### 8.2 plan 文件格式是 CLI 私有

**不声称**它是 `wiki/vase-architecture.md` §5.1 的插件清单格式。M2 的 `PluginCatalog` 接手 `KnownBinaries` 时，这个文件与那处"M2 还债"注释一起退场。命名上避开"manifest / 清单"字样，就叫 plan file。

### 8.3 我们 Own 一份第三方 fork

这是本设计引入的**最重的长期承诺**。它意味着：上游修的真 bug 要手工挑进来；上游改的接口要我们判断是否跟；`ThirdParty/cli` 的构建与提交由我们维护。

三条缓解：① diff 以删除为主（§5.2），重放成本低；② 不变式由我们的构建把守、失效响亮（§5.4，**前提是引号包含那条被守住**）；③ 不改头文件路径与名字，merge 是机械活。

判据是 §5.5：fork 的存在理由是 v1 的行编辑与补全——那是自写 REPL 要碰平台 console API 的真活。若 v1 后来决定不做，这个承诺就失去回报，届时该改成把删除版定版进仓库、不再跟上游。

### 8.4 v0 没有行编辑与 Tab 补全

`CliFileSession` 是裸 `getline`。原因不是"少个功能"而是 §1.4 单线程契约的实测前置项没做。列为 fork 的 v1 项（§5.5）。

### 8.5 `ThirdParty/` 是新的顶层目录

架构文档 §10 的目录布局是"已确认"的（CLAUDE.md 如此陈述），新增顶层目录是对它的一次扩写。本文按需求方指示这么做，**但 §10 与 CLAUDE.md 都要补一句**，否则下一个人照着 §10 立目录会看不见它（§10 第 11 桩）。

### 8.6 没有用例产出**不干净**的 `PodReport`（规则 ② 只被检视、未被失败方向压过）

退出码规则 ② 说「任一次 destroy（含 `exit` 自动拆的那些）的 `PodReport.Clean()` 为 false 即非零」，但今天**没有任何一条用例产出不干净的局**：回放脚本要么全程 Clean、要么在建局前就失败，六条线里没有一次 destroy 走失败方向。把 `Commands.cpp` 里那两处 `if (!report.Clean()) MarkFailed();` 删掉，六线仍全绿——规则 ② 只有代码上的覆盖，没有被实测压过。

要压它就得造一份带残余（未清的 Effect / 服务 / 订阅）的局，而那要借 `Tests/` 的换件材料——§9 已判定 Samples→Tests 构建耦合不该有（fixture 语义为断言服务，与 sample 语义本就不同），同一理由在此适用。故这是一条**写下来、不补**的已知限制，不是待办。

---

## 9. 决策记录

| 议题 | 决定 | 被否掉的选项与理由 |
|---|---|---|
| **异常策略** | **fork 掉异常用法本身**（§5） | ①「`VaseBuildOptions` 按消费者属性分叉」——技术上实测成立（2.6），被需求方约束否掉：**根不该为单个 sample 携带按消费者分支**；②「把 `VaseBuildOptions` 整块搬进 `Cmake/VaseBuildOptions.cmake`，根只留一行 `include`」——字面满足①但没消除机制，只是让它不可见，且要搬动那几处承重的"位置要求"注释；③「整个 VaseConsole 开异常」——削弱面正落在调 Vase API 的文件上；④「不链 `VaseBuildOptions` 自带 flag」——踩规矩 1 点名的复发形态（8 样复制成第二处真值，且丢 `/utf-8` → `C1019` 那种与编码毫无字面关联的炸法）；⑤「靠 `-isystem` 吞诊断」——假绿（2.2）；⑥「vcpkg 端口关异常」——做不到（2.7）；⑦「自写 REPL」——v0 可行的最低价，但 §5.5 判定 v1 要行编辑与补全，那是碰平台 console API 的真活 |
| fork 怎么进仓 | **git submodule** 挂 `ThirdParty/cli` | ① 裸 `git clone` 到 `ThirdParty/` 再 `git add`——实测记下的是一条 **mode 160000 gitlink**、跟踪文件数 1、且指向**未含我们修改的**上游 commit：本地能编、别人 clone 到原版 → 六线全红而自己全绿。git 自己警告 "Clones of the outer repository will not contain the contents"；② vcpkg 端口钉 fork 的 tag——要等建仓 + 打 tag + SHA512，且在关键路径上多一个人类动作；③ vendored 成普通文件（`rm -rf .git`）——可行且最简单，代价是与上游的关系退化成手工 import |
| fork 基线 | `master`（`769c5fa`，= 上游 tip） | 「回退到 `v2.2.0` 打基线」看着更像"钉在已发布版本上"，但 2.13 实测两版之间我们关心的头字节相同、差异全在 `CMakeLists.txt`，回退等于永久背一段上游 CMake 的落差 |
| 消费方式 | `Cmake/VaseThirdParty.cmake` 自立 INTERFACE | `add_subdirectory(ThirdParty/cli)`（实测可行但把上游构建决策变成我们的风险，见 §5.6）；vcpkg imported target（会把 include 降级成 `-isystem`，正撞 2.2 / §5.4） |
| cli 头的包含写法 | **引号** `#include "cli/cli.h"` | 尖括号——与"第三方库用尖括号"的直觉一致，因而**默认会被写错**；实测它在 cl.exe 线上让异常关失效（2.16 / §5.4）。规矩 3 是同一机制的反方向 |
| `boost-asio` | 随 asio 整体删除而不存在 | 保留 = 把整个 boost 拖进 `x64-linux-libcxx` 的 libc++ 编译；那个 triplet 文件已记着 vcpkg 依赖走 libc++ 踩过的坑（`clang-scan-deps` 报 `'cstdint' file not found`） |
| 换件材料 | 新增 `Samples/HelloPluginPrime`（同 Id、不同行为） | 「引用 `Tests/HotSwap/fixtures` 的 `VersionedA`/`VersionedAPrime`」= Samples→Tests 构建耦合，fixture 改名即碎，且 fixture 语义（为断言服务）与 sample 语义（给人看）本就不同 |
| `counters` 命令 | v0 不做 | 中途五项计数只能走 `ForTestCounters()`（`PluginHost.h:62`，名字与注释都写着"测试缝"）。让 sample 消费它会制造先例。真要提供，正路是提升为正式诊断 API——**那是变更 Vase 公开面**，按「新增或变更机制前先与需求方确认」得单独走。v0 靠 `get`（行为）+ `plugins`（活集合）+ `PodReport.CountersDiff`（拆局时）覆盖同一问题 |
| 命令是否含 `load <path>` | 不含 | M1 API 不支持；硬做就是绕过 `KnownBinaries`（§3.2 推论 3） |
| 数值参数交给库转换 | 不交，一律 string + 自己 `from_chars` | fork 删掉数值支路后这条**已无选择**（§3.4）；记录在此是为了说明它当初是一个选择 |

---

## 10. 落地影响（本文**不**给新数字：实测值一律落根 `CLAUDE.md`）

| 门禁 | 会动的东西 |
|---|---|
| `ctest -N` 按线基数 | ✅ 已实测：`VaseConsole*` 用例（清单见 §7.2）使六行基数整体上移，**实测值见根 `CLAUDE.md` 的按线基数表**，本文不复写。既有两处差值不变——新用例既无 `#ifndef NDEBUG` 门、也无平台门：release 的 −1 仍是 T3 的 death test，Linux 的 +2 仍是 T11 的两条 Linux-only |
| clang-tidy 三条 debug 线 | ✅ 已实测：文件数与 `Suppressed` 合计都变了，**实测值见根 `CLAUDE.md` 的 tidy 表**。`ExcludeHeaderFilterRegex` 如期生效（三条线正文零 `ThirdParty/` diagnostic）。另新增一条**整检查禁用**：`clang-analyzer-optin.core.EnumCastOutOfRange` 由 MSVC 自己的 `xfilesystem_abi.h` 触发、无代码级出路，理由就地写在 `.clang-tidy` |
| clang-format | ✅ 已实测 rc=0，新文件均已 `git add`（`git ls-files` 那串是实测过的坑）。**`ThirdParty/` 里的头不被这条门禁枚举——实测 `git ls-files 'ThirdParty/*'` 只返回 gitlink 本身 `ThirdParty/cli`**，所以不需要 `ThirdParty/.clang-format`。（若哪天真要放行内容，退路是 `ThirdParty/.clang-format` 写 `DisableFormat: true`，在 /tmp 实测过：一份违规文件从 rc=74 变 rc=0，且**不必改 CLAUDE.md 里那条命令**） |
| 根 `CLAUDE.md` | ✅ 已回填：①第三方接入方式（submodule + `Cmake/VaseThirdParty.cmake`）记在「构建与测试」，「引号包含」这条反直觉规则记在规矩 3；②两张基数表换成实测值；③「项目状态」的 Samples 一行 + 顶层 `ThirdParty/`，并把「尚未确定的事项」表里那格"已按第 10 节落成"改成实况 + §10 `Samples/` 子树未同步的提醒；④第三方许可记 BSL-1.0 的 `cli`；⑤规矩 4 那句"`/external:W0` 管不着 C4530"已按 2.16 收窄；⑥规矩 6 补了 `VaseConsoleHotSwap` 这个第二实例（§4） |
| 架构文档 §10 | ✅ 目录布局已新增 `ThirdParty/`（§8.5）——**经需求方过目后落笔**，因为 §10 是标着"已确认"的那部分 |
| 技能 `vase-cpp-engineering` | ✅ 已定（§9 表末行那条悬着的问题）：**基数不写**，只住 `CLAUDE.md`；"引号包含"写进 `SKILL.md` 提交前清单，且**必须带指回 `CLAUDE.md` 规矩 3 / `Cmake/VaseThirdParty.cmake` 的链路**（规矩 7 的"嵌在论述里的单个字面值"这一类——容许副本，不容许没有链路的副本）；"我们 Own 一份 fork、其 core 路径必须无异常"写进 `references/architecture.md` 作**判据**（为什么 fork 而不是自写 / 端口），不抄实现细节（规矩 7 的"判据与理由"这一类——两边各写一份、不需要指针） |

### 验证档位

`Tools/VaseConsole` **不碰** Loader / 依赖账本 / Eject / Adopt 路径 / 描述符布局 / `HeaderVersion`，按规矩 6 **不**触发"必须跑 `Tests/HotSwap/` 全部用例"那一档——它只是那套 API 的消费者。

改动清单里的升级触发器：

1. 改根 `CMakeLists.txt`（哪怕只是加一句 `include`）= 改**所有 target 的构建输入** → 六 preset 全量重跑 + 三条 debug 线各自跑 tidy。**已全量执行**（Task 7）：六棵树删树重配全绿、三条 tidy 线各自读正文、format rc=0，数字见根 `CLAUDE.md`。此前那次"已部分执行"（两条 Windows 线）作废，不再是当前状态。
2. 新增 submodule = 改变仓库依赖 → 六棵树**删树重配**（`Scripts/win-verify.cmd` / `Scripts/linux-verify.sh` 正是这个形态）。**已执行**（Task 7，同上）。

---

## 11. 桩的状态

**已打掉的**：

1. ✅ fork 存在、已改造、以 submodule 进仓（§5.1 / §5.2）。
2. ✅ **fork 的无异常性质在真编译条件下成立**——clang-cl `/EHs-c- /W4 /WX /utf-8`、cl.exe 完整 flag 集、Linux `clang++ -fno-exceptions -Wall -Wextra -Werror -stdlib=libc++` 三处均 **0 诊断**。并顺带量出 §5.4 那张"三条线不等价"的表与引号规则。
3. ✅ **运行期行为**：坏命令 → `wrong command: nope`、坏索引 → `Invalid history index: abc`、越界 → `No such history entry: 999`，进程全程存活、退出 0。
4. ✅ **`Insert` 的 arity 形状**：`FreeformCommand`（`cli.h:692/695`）原生接 `(ostream&, const vector<string>&)`，不需要桥接模板也不需要改 fork（2.17）。
5. ✅ 既有构建与测试未破：六条线全量重编 0 警告、`ctest` 全绿、`ctest -N` 逐位对上按线基数表、
   format 门禁 rc=0（数字见根 `CLAUDE.md`）。**`.clang-tidy` 那条新增（`ExcludeHeaderFilterRegex`）
   在它落地时不产生任何效果**——那时编译数据库里还没有 TU 包含 cli 头（`Tools/VaseConsole` 尚未存在），
   所以摘要计数与改前逐位相同；它真正起作用是 `Shell.cpp` 进库之后（见第 7 条）。另：submodule
   内容不被 format 门禁枚举（实测见 §10 表）。
6. ✅ **回放资产的生成方式**：`file(GENERATE)` + `$<PATH:CMAKE_PATH,$<TARGET_FILE:…>>`，无 `.in` 模板（§7.2）。
7. ✅ **`Tools/VaseConsole` 落地后的门禁全量**（六 preset 删树重配 + 三条 tidy 线 + format）已跑完，
   数字全部落在根 `CLAUDE.md` 的两张基数表里，**本文不复写**（规矩 7）。三条结论：`ExcludeHeaderFilterRegex`
   如期生效（三条 tidy 线正文零 `ThirdParty/` diagnostic）；`ThirdParty/` 对 format 门禁惰性（`git ls-files`
   只认出 gitlink）；首次撞上一个**无代码级出路**的 tidy 检查——`clang-analyzer-optin.core.EnumCastOutOfRange`
   由 MSVC 自己的 `xfilesystem_abi.h:134` 触发，已整条关掉并就地写明理由（`.clang-tidy`）。
   另记一条**环境性假红**：首次删树全量时 `win-x64-clang-release` 的一次 `vcpkg z-applocal` 撞到
   `The process cannot access the file ... being used by another process`（目标 `FailingLoadPlugin.dll`），
   构建中止、连带 4 条 HotSwap/Abi 用例因缺 DLL 而红；同一条线立即重跑即全绿。
   是 Windows 文件锁、不是本设计引入的缺陷——但它是**重跑才显形**的那一类。
8. ✅ **`split.h` 的 5 处 `vector::back()` 已复审**（fork `2fa9dae`）：逐处读上下文确认容器在
   `word` / `sentence` / `escape` 三态下恒非空——上游本就带 `assert(!empty())`，`NDEBUG` 会抹掉
   assert、抹不掉那几行判断；依据写进该文件 `class Text` 头部，不改行为。
9. ✅ **fork 的 `examples/` 与 `test/` 定为有意保留的响亮哨兵**（fork `2fa9dae`）：其中若干用例
   仍注册 `int` 等数值参数，开 `-DCLI_BuildExamples=ON` / `-DCLI_BuildTests=ON` 会在**链接期**失败
   （2.14）。分歧写进 fork 的 `README.md`；Vase 侧不 `add_subdirectory`，碰不到它们。

**还欠的**：

10. `CliLocalSession` 的派发线程（v1 前置，2.9）与 history size = 1 的问题（§5.5）。
11. 架构文档 §10 的 `Samples/` 子树**尚未同步**：它列着两个从未存在的目录、漏掉两个实有的，`Tools/VaseCli/` 也与实有的 `Tools/VaseConsole/` 不是一回事。本轮只落需求方批准的 `ThirdParty/` 一行——改 §10 其余部分要重新过目（它标着"已确认"）。CLAUDE.md 那一侧的同步本轮已完成（§10 表）。
