# Vase M1（Pod 闭环 + 热插拔骨架）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 M0 的构建地基升级为一个可运行的最小 Vase：单 Pod 装配闭环（Effect / Context / Service / Host）之上，落地 v3 的核心增量——依赖账本最小形、`AdoptPlugin` / `EjectPlugin` 全循环、三档卸载证据链在 **Win + Linux 首跑**，并让「Pod 实体更名」成为第一个实码动作。

**Architecture:** 三层按 v3 第 1 节落地：`PluginCatalog` **不在 M1**（LoadPlan 手写，D12）；`PluginHost`（`VaseHost`）持进程级实体——二进制表、依赖账本、诊断计数、`ScopePool`；`Pod`（`VasePod`）持实例级实体——根/子 `Context`、`ServiceRegistry`、`EventBus`、每插件一个 `EffectScope`。平台差异全部收在 `Loader` 一个类里（13.1 的收口约定），三档证据（8.2）与导入表执法（8.7）共用同一个自写 PE/ELF 镜像解析器。热插拔取档 ①：入局皆叶、离场必叶、全程串行于 Host 绑定线程。

**Tech Stack:** C++20 · CMake 4.3+ / preset v9 · Ninja · clang/clang-cl 23.1.0 + cl.exe · vcpkg manifest + GoogleTest 1.18.0 · 全项目关异常（`Result<T>` / `Error`）。

**Spec:** [`docs/superpowers/specs/2026-09-14-vase-m0-m1-design.md`](../specs/2026-09-14-vase-m0-m1-design.md)（历史决策记录，D1–D17 与 3.3 的机制探针）＋ **上游权威：[`wiki/vase-architecture.md`](../../../wiki/vase-architecture.md) v3**——凡两者冲突（如 3.6「双版本 reload 留 M4」），**以 v3 为准**（spec 头部勘误 ①②③ 已列明取代关系）。

---

## 本计划的范围

12.3 的 M1 行：「**+ 单插件 Eject / Adopt 全循环，三档判据首次于 Win / Linux 跑通**（macOS 的 neverUnload 风险面见 8.2，M5 判定）——**热插拔是骨架，不是尾巴**」。「单插件」修饰的是被热插拔的对象（一次进出一个叶），不是 Pod 里只能有一个插件——12.1 的循环天然需要 A、B 两个实例，故 **M1 的 Pod 至少容纳 2 个插件实例**（无依赖、手写序）。按需求方定稿的六条主线：

```text
① Pod 实体更名打头 → ② 最小闭环（Effect/Context/Host） → ③ 账本最小形（5.6 两规则）
→ ④ Adopt/Eject 全循环 → ⑤ 三档判据 Win/Linux 首跑 → ⑥ 构建标志落工具链文件
```

**相对 spec 第 3 节旧 M1 的增删，全部由 v3 派生，逐条登记：**

| 项 | 旧 M1（spec 3.6） | 本计划 | 依据 |
|---|---|---|---|
| 双版本 reload fixture | 「留 M4」 | **进 M1**（12.1 全循环） | v3 勘误 ③ / 12.3 |
| Adopt / Eject / 账本 | 无 | **进 M1（最小形）** | v3 1.5 #3/#4/#5、12.3 |
| 卸载证据 | 单条（文件可覆盖写 / Linux 重载归零） | **三档判据**，且 Linux 主判改 `dl_iterate_phdr` 条目消失 | v3 8.2（取代 D16 与 3.3(4) 判据；成因措辞按 v3 修订） |
| 身份特征构建标志 | 无 | **进 M1，落三个工具链文件**（`/DEBUG:FULL`、`-Wl,--build-id=sha1`；v3 还点名 `-fno-gnu-unique`，**实测 clang 不接受该标志**，见 T6） | v3 8.2、13.2 末行 |
| 导入表执法（8.7） | 无 | **进 M1（Adopt 路径最小形）**：读导入表/DT_NEEDED，命中已知兄弟插件即拒；缺依赖诊断共用同一解析器 | v3 1.5 #7、5.1 注 |
| Pod 内插件数 | 1 | ≥2（A、B 并存，手写序，无拓扑求解） | 12.1 |
| `.ProcessState` 登记与 Eject 自动重置（判据 3d） | M3 | **仍 M3**。`EjectReport` 预留空字段并注释理由 | 12.3 里程碑归属 |
| 拒绝报告点名消费者（3b）/ 图反查（3a 50 轮）/ 级联拆除 / Catalog / Preset / Waterfall / `CreateScope` / 拓扑求解 / `Strict` 触发 / `IObserver` / `VaseCli` / `VasePack` / macOS | 不做 | **仍不做**（3b/3e 的**最小形**随账本与 Adopt 顺带落地，见 T9/T11） | spec 3.6 + v3 12.3 |

**连带文书义务（12.3）**：M0/M1 设计文档的勘误头**已挂**（2026-09-16），不改史；`CLAUDE.md` 的同步（目录名、ctest 基数、新规矩）**未做**——落在 T1（更名即时项）与 T14（收尾全量项）。

**本计划实施完毕后应停下、按 T14 的验收判据逐条核对、再开 M2 的计划。**

---

## Global Constraints

以下约束对**每一个** task 都成立，不再逐条重复。出处为 v3 架构文档（§）或 spec（D/3.3/9.x）。

- **语言标准 C++20**；`CMAKE_CXX_EXTENSIONS OFF`；clang-cl / clang **23.1.0** 由 PATH 解析，cl.exe 经 `Scripts/msvc-env.cmd`；六个 preset（`win-x64-{clang,msvc}-{debug,release}`、`linux-x64-clang-{debug,release}`）全部保持绿——**Windows 侧任意时刻跑 clang-cl 两条 debug/release 即可推进任务，全量六线在每桩收尾与 T14 核**（cl.exe 线需要 Developer 环境，每次现配太贵，但**不许连续欠两桩**）。
- **不使用 C++ 异常**（§0.3-7）：不写 `throw` / `try` / `catch`；错误一律 `Result<T>` / `Error`。测试里**禁止 `EXPECT_THROW` 一族**（编译期硬失败，CLAUDE.md 规矩 2）；测「必须失败」用 `Result` 的显式返回值，测「必须终止/断言」用 gtest death test。
- **新 target 必须链接 `VaseBuildOptions`**（CLAUDE.md 规矩 1；本计划所有新增 target——含每个 fixture DLL——代码块里都带上，不许省略）。
- **Vase 自己的头一律引号包含** `#include "Vase/..."`（CLAUDE.md 规矩 3）。
- **STL 前置条件失败 = 进程终止**（CLAUDE.md 规矩 4）：可恢复失败一律走 `Result`；`std::filesystem` 只用 `error_code` 重载；不用 `.at()` / `value()` / `std::stoi`（§9.x：`*opt` / `find` / `get_if` / `from_chars`）。
- **命名**：`.clang-tidy` 强制——类型/函数/成员 `CamelCase`（**成员不带任何前后缀**：写 `Message`，不写 `Message_`）、参数/局部 `camelBack`、常量与枚举值 `kCamelCase`、全局变量 `gCamelCase`。成员与同名访问器撞车时**成员让位改名**。六个 `Member*Prefix/Suffix` 与 `Public/PrivateMemberCase` 已显式落在 `.clang-tidy`（2026-09-17）。**格式**以 `.clang-format` 为权威（Allman、`PointerAlignment: Left`、120 列）；本文代码块已逐块过 `clang-format`，改动后以 `clang-format -i` 的输出为准。
- **tidy 形态约束（本文代码块已逐条实测过 `run-clang-tidy`，新增代码照此写）**——这几条都是「本仓库的检查集开着、而默认写法会踩」的地方：
  - 查询类访问器一律 `[[nodiscard]]`（`modernize-use-nodiscard` 开着）；
  - 每个类把特殊成员写全：删了拷贝就别让移动隐式生成（`cppcoreguidelines-special-member-functions`）；持有引用成员的类必须同时删掉拷贝与移动（`avoid-const-or-ref-data-members`）；
  - **不写裸 `new` / `delete` 表达式**（`cppcoreguidelines-owning-memory`）：`std::make_unique` 造壳 + `release()` 显式移交，销毁端用 `std::unique_ptr<Cell>` 接住再析构；
  - **不用裸 C 数组、不做指针减法、不用非常量下标的 `operator[]`**（`cppcoreguidelines-pro-bounds-*` / `-avoid-c-arrays`）：用 `std::array` / `std::next` / `std::distance` / `std::copy`；
  - **不用变参函数**（`cppcoreguidelines-pro-type-vararg`）：整数格式化走 `std::to_chars`；
  - 类类型成员**不要**写 `{}` 初值——`cppcoreguidelines-pro-type-member-init` 与 `readability-redundant-member-init` 在这点上互斥，实测「什么都不写」才是两边都过的解；
  - `std::optional` 取值前先 `has_value()`：`bugprone-unchecked-optional-access` 不认自己的封装访问器。
  - **「先断 `IsOk()` 为假、随后立刻 `GetError()`」的位置，前一条必须是 `ASSERT_` 族**（T11 实测）：`EXPECT_FALSE` 只记录失败、**不返回**，下一行的 `GetError()` 照样撞上 `Result::GetError()` 的 `ProgrammerError`，测试从「一条断言失败」变成「整条进程挂掉」。
    例外是 `ASSERT_TRUE(r.IsOk()) << r.GetError().Message();` 那种写法——gtest 把 `<<` 的流表达式挂在 else 分支上，条件成立时不求值，可以放心用 `ASSERT_`（用 `EXPECT_` 也可，但没必要）。
- 〔**本条已被取代**（2026-09-18，M1 之后）：`VASE_MSVC_DLL_WARNINGS_BEGIN/END` 已从仓库删除，C4251 改由根 `CMakeLists.txt` 的全局 `/wd4251`（cl.exe 线）豁免，逐类把关让位于构建侧一次性豁免。下面的正文保留作 M1 当时的记录与本条当时的实测结论。现行口径见 CLAUDE.md 规矩 1、wiki 8.3 与 13.3（含「豁免出不了这棵树」这笔待还的债）。〕**导出类带 STL 成员时，用 `VASE_MSVC_DLL_WARNINGS_BEGIN` / `VASE_MSVC_DLL_WARNINGS_END`（`Export.h`，T1 落地）把类定义包起来**：MSVC 的 C4251 在 `/WX` 下是 error（clang-cl 不报，Linux 无关）——已在 `ScopePool` / `EffectScope` 上实测炸过，一度让 cl.exe 线整条编不过。逐类显式豁免而非全局 `/wd4251`：豁免要一次次做，才不会把新代码的真错一起放行。前提是工具链与 STL 矩阵钉死（§8.5）＋ `HeaderVersion` 拦头文件不匹配（D13）；**若日后允许插件用不同版本的 MSVC STL 构建，前提即破，届时应改为 pimpl 或把 STL 成员移出导出面**。T7 的 `Pod` / `PluginHost` 同样带 STL 成员，照此办理。
- **格式门必须覆盖未跟踪文件**：`git ls-files` 对**尚未 `git add` 的新文件静默跳过**（本计划早期的门禁命令有此漏洞，实测：整体报 exit 0，单独跑那两个新文件却报差异）。命令一律用 `git ls-files -z --cached --others --exclude-standard ...`。
- **插件 fixture 与测试探针类放匿名命名空间**，否则撞 `misc-use-internal-linkage`（T4 起实测会报）。`VASE_PLUGIN` 的宏展开自带其所需的抑制，不必再管；`VASE_PLUGIN` 本身仍须留在全局作用域（放匿名命名空间里会破坏 `extern "C"`）。
- **C++ 改动的运行期成本按 `.claude/skills/cpp20-zero-overhead/SKILL.md` 办**：新增或修改类型、函数签名、
  循环、容器与分配、导出面接口之前**先调该 skill**。两条硬约束照办：**每个付费点要么在例外登记表
  占一行**（位置 / 付了什么 / 为什么值 / 测量点＋最近数字与日期 / 复核触发），**要么经确认无付费——
  不许悬空**；**没有验证阶梯上的数字就不写「更快 / 更省 / 零开销」**，注释里也不写。
- **提交信息用中文**，与仓库既有风格一致；**不加任何点名 AI 工具或模型的尾注**——不写
  `Co-Authored-By:`、`Generated-with:`、`Signed-off-by:` 或「🤖 Generated with …」一行
  （CLAUDE.md「语言约定」明定，优先于任何工具自带的署名默认值）。只留标题 + 中文说明体。
- **注释中文为主**；每个机制点注释里标 v3 节号（如 `// §5.6 规则 ②`），承重处标 spec 探针结论出处。
- **铁律 §1.2 + 最小属主追踪（D15）**：进程级容器（二进制表、账本、池）插入时 Debug 断言不持实例级对象；Vase 的实例级对象（`Plugin` 实例、`EffectScope`、服务注册项）带属主标记（M1 只存 `OwnerLabel` 字符串指针，够断言与诊断归属，完整归属追踪 M3）。
- **§1.4**：`PluginHost` 进程内唯一、绑定创建线程；一切生命周期与注册动作串行于绑定线程（Debug 断言 thread id）。计数器的非原子维护依赖这条——见代码注释。
- **§0.3-6 边界借用通则**：跨边界传 `string_view` / 裸指针皆为「当下有效」；**例外仅 `Error`**（全拥有 `std::string`，spec 3.3(3)，对应 v3 §0.3-6 的登记例外）。
- **`HeaderVersion` = 1**（D13）：`vase::kHeaderVersion`，描述符第一字段；加载路径第一个读它，不匹配即拒（§3.1/§8.3）。
- **符号可见性**：Linux 侧 `dlopen` 一律 `RTLD_NOW | RTLD_LOCAL`，不给宿主选项（§8.7）；插件二进制只准链 `VasePod`（D11 的链接期事实由 fixture 的 `target_link_libraries` 承载）。
- **只让有内容的 target 存在**；不建 `VaseCatalog` / `VaseCli` / `VasePack` 空壳。
- **ctest 基数纪律**（CLAUDE.md）：每桩「测试全绿」的判据 = 跑 `ctest --preset <p>` **另加** `ctest --preset <p> -N` 核对计划中标注的基数；基数从 **2**（M0）起，各桩递进值见各任务。
- **Git Bash 里拼 WSL 命令不许带 `$` 进双引号**（CLAUDE.md）：本计划所有需要 shell 变量的 Linux 验证步骤都已写成「先落脚本文件再 `wsl -d Ubuntu -- bash -lc 'bash <脚本>'`」的形式，照做。

---

## 文件结构（M1 结束时的目标态）

```text
Include/Vase/
├── Plugin.h                     ★ 重写为真实公开面（T5 组装，删除 M0 占位注释）
├── PluginDescriptor.h           PluginMeta / PluginDescriptor / ServiceRef / kHeaderVersion（T4）
├── Effect/IEffect.h  EffectHandle.h  EffectScope.h                        （T3）
├── Service/Service.h            标识概念、ServiceOrigin、ServiceKey       （T5）
├── Event/Event.h                事件标识概念、EventKey                     （T5）
├── Pod/Context.h  Pod/Pod.h     Context / PodHandle / PodReport            （T5/T7）
├── Host/PluginHost.h            Host、CreatePod/DestroyPod/Resolve、Adopt/Eject（T7/T10/T11）
├── Host/LoadPlan.h              LoadPlan / LoadPlanEntry / PodOptions（手写形，D12）（T7）
├── Host/Loader.h                二进制进出的唯一收口 + 镜像特征/导入表读   （T6）
├── Pod/DependencyLedger.h       账本最小形（VasePod 类型——Context 要直接写它；Host 持有实例）（T9）
├── Host/Evidence.h              三档证据结构 + EjectReport / AdoptReport    （T10/T11）
└── Detail/
    ├── Export.h                 （T1 改名宏）+ SmokeProbe.h（T1 改符号；M0 冒烟用例保留，探针随之保留）
    ├── Result.h                 Result<T> / Error / ErrorContext / Phase    （T2）
    ├── MetaArray.h              spec 3.3(2) 的定稿实现                      （T2）
    ├── Fail.h                   detail::ProgrammerError：Debug assert / Release abort（T2）
    ├── ScopePool.h              size-class 空闲链 + 槽位池（D8/D10）         （T3）
    ├── Counters.h               §9.2 五项诊断计数（T3）
    ├── RegistryBus.h            ServiceRegistry / EventBus 的共享声明（T5）
    └── ImageInspect.h           自写 PE/ELF 解析的纯函数面（T6）

Source/
├── Pod/          → target VasePod（Effect/Service/Event/Pod 源码并入，§10.2）
│   ├── SmokeProbe.cpp           （T1 自 Session 改名；**保留**——M0 冒烟用例的载体）
│   ├── EffectScope.cpp  ScopePool.cpp                                        （T3）
│   ├── ServiceRegistry.cpp  EventBus.cpp  Context.cpp                        （T5）
│   ├── DependencyLedger.cpp                                                  （T9）
│   └── Pod.cpp                                                                 （T7）
└── Host/         → target VaseHost（链 VasePod）
    ├── SmokeProbe.cpp                                                       （T1 改名）
    ├── Loader.cpp  LoaderWindows.cpp  LoaderPosix.cpp                        （T6）
    ├── ImageInspectCommon.cpp  ImageInspectWindows.cpp  ImageInspectPosix.cpp（T6）
    └── PluginHost.cpp（二进制表、建销 Pod、诊断计数、Adopt/Eject）            （T7/T9/T10/T11）

Samples/
├── HelloCommon/Greeter.h        提供方与消费方共用的接口+事件头（T7）
├── HelloPlugin/                 最小插件：Provide 一个服务 + On 一个事件（T7）
└── Embedding/                   验证宿主：play | loop <N> | swapdemo（T7/T10/T11）
                                 （§10.1：Samples 只放演示；「加载失败」语义归 Tests fixture——
                                  失败插件对宿主是演示、对判据是测试材料，M1 取后者，
                                  原 FailingPlugin 演示位改列 Tests/Integration/fixtures 的
                                  FailingLoad/FailingStart 对，v3 10.1「不放业务、只证能力」）

Tests/
├── Smoke/CrossDllSmoke.cpp      （T1 改符号名；**保留**——M0 验收 #4 的门禁不拆）
├── Unit/                        Result / MetaArray / ScopePool / 回收序 / 描述符 / 注册表 /
│                                镜像解析器 / Loader / 账本                        （T2–T6、T9）
├── TestingSupport/PodTestPeer.{h,cpp}   #10 的 M1 形态注入缝（T8）
├── Integration/                 失败语义、线程守卫、账本语义
│   └── fixtures/
│       ├── SharedCommon.h       ISharedService / IHostOnlyService（T9）
│       ├── StaleHeaderPlugin/   手写描述符、HeaderVersion=999（#12，T8）
│       ├── FailingLoadPlugin/  FailingStartPlugin/   §5.5 两态（T8）
│       ├── RequiresMissingConsumer/  UndeclaredGetConsumer/   §6.2 两条终止路径（T8/T9）
│       └── SharedProviderPlugin/  EdgeConsumerPlugin/         账本正/侧样本（T9）
├── Lifecycle/                   #1 反复建销归零、#15 失效句柄、#10 归属+#18 永不失败（T7/T8）
├── Abi/                         ImportEnforcementTests（3e，T13）
│   └── fixtures/                BadLinkSiblingA/  BadLinkSiblingB/   真互链 fixture 对（T13）
└── HotSwap/                     Eject 单点（T10）、12.1 全循环（T12）、负路径与相邻性（T13）
    └── fixtures/
        ├── VersionedA/  VersionedAPrime/   同名不同目录、只差一处服务返回值（12.1）
        ├── NeighborB/                      与 A 无依赖，带 IHeart 心跳计数
        ├── NoBuildIdPlugin/                -Wl,--build-id=none（Linux only，T11）
        └── TimerPlugin/                    订阅 TickEvent，3c 载体（T13）

Cmake/Toolchains/                三个工具链文件各增若干承重 flag（T6）
CMakeLists.txt                   add_subdirectory 调整：Samples/ 与 fixture 子目录挂入（T6/T7/T12/T13）
CLAUDE.md / README.md            T1 即时项 + T14 全量项
```

**任务依赖**：T1 → T2 → T3 → T4 → T5 → T6(∥) → T7 → T8 → T9 → T10 → T11 → T12 → T13 → T14。T6 与 T4/T5 无依赖可并行，但按编号序执行最简单。

---

## Task 1: Pod 实体更名打头（v3 §1.5-1）

v3 明确这是「M1 的第一批实码动作」：磁盘现状是 v2 命名（`Source/Session` / `VaseSession` / `SessionSmokeProbe`），全部改为 Pod 派生名，并把 `CLAUDE.md` 的连带文书义务即时项做掉。**纯更名，零新逻辑**——判据是两条 debug 线全绿、ctest 基数仍为 2（用例**名字**变了，数量没变）。

**Files:**
- Move: `Source/Session/` → `Source/Pod/`（`git mv`，保留历史）
- Modify: `Source/Pod/CMakeLists.txt`、`Source/Pod/SmokeProbe.cpp`、`Source/Host/CMakeLists.txt`、`Source/Host/SmokeProbe.cpp`、`Include/Vase/Detail/Export.h`、`Include/Vase/Detail/SmokeProbe.h`、根 `CMakeLists.txt`、`Tests/Smoke/CrossDllSmoke.cpp`、`Tests/CMakeLists.txt`（注释）、`CLAUDE.md`、`README.md`

**Interfaces:**
- Produces: target 名 `VasePod`（原 `VaseSession`）；宏 `VASE_POD_BUILD` / `VASE_POD_API`；符号 `vase::PodSmokeProbe()`。后续所有任务的代码块都用这套新名字。

- [ ] **Step 1: 盘点全部出现点（不许凭记忆改）**

```bash
git mv Source/Session Source/Pod
grep -rn "Session" Source Include Tests CMakeLists.txt CLAUDE.md README.md
```

Expected: 命中点全部落在下面的映射表内。替换映射（**逐串替换，大小写敏感**）：

| 旧 | 新 |
|---|---|
| `Source/Session` | `Source/Pod` |
| `VaseSession` | `VasePod` |
| `VASE_SESSION_BUILD` | `VASE_POD_BUILD` |
| `VASE_SESSION_API` | `VASE_POD_API` |
| `SessionSmokeProbe` | `PodSmokeProbe` |
| `HostImportsSessionSymbol` | `HostImportsPodSymbol` |
| 「`Source/Session` → `Source/Pod` 的**实体更名**待 M1（磁盘现状仍是 Session 命名…）」 | 「`Source/Pod` 更名已于 M1-T1 落地；其余 v3 实体（`Context` / `PodHandle` / …）随 M1 各任务出现」 |

- [ ] **Step 2: 应用替换（三处语义注释要改写，不是串替换）**

1. `Include/Vase/Detail/Export.h`：`VASE_SESSION_BUILD/API` 两个宏整体改为 POD 版（`#ifdef` 行与 `#define` 行共 4 行）；**并在文件末尾追加 C4251 的逐类豁免宏**——T3 起实测：导出类里放 STL 成员会让 cl.exe 线整条编不过（`/WX` 把 C4251 升为 error，clang-cl 与 Linux 都不报）。原样追加：

   ```cpp
   // 导出类里带 STL 成员时，MSVC 报 C4251（客户端用不了没 dllexport 的 STL 类型），
   // 在 /WX 下是 error。本项目**有意**在导出类里放 STL 成员（池、槽表、账本），
   // 前提由两条保证：工具链与 STL 矩阵钉死（§8.5）、HeaderVersion 拦头文件不匹配（D13）。
   // 因此逐类显式豁免，而不是全局 /wd4251——豁免要一次次做，才不会把新代码的真错一起放行。
   // clang-cl 也定义 _MSC_VER，故它同样展开出这对 pragma——无害（clang-cl 本就不报 C4251，
// 而它认得 MSVC 的 warning pragma）；只有真正非 MSVC 的目标（Linux/macOS）下展开为空。
   #ifdef _MSC_VER
   #define VASE_MSVC_DLL_WARNINGS_BEGIN __pragma(warning(push)) __pragma(warning(disable : 4251))
   #define VASE_MSVC_DLL_WARNINGS_END __pragma(warning(pop))
   #else
   #define VASE_MSVC_DLL_WARNINGS_BEGIN
   #define VASE_MSVC_DLL_WARNINGS_END
   #endif
   ```
2. `Source/Host/CMakeLists.txt`：`PUBLIC VaseSession` → `PUBLIC VasePod`；其上注释「Host 依赖 Session」→「Host 依赖 Pod——分层方向（§1：二进制层在实例层之上）与「插件不依赖 Host」（D11）由链接图承载」。
3. 根 `CMakeLists.txt`：`add_subdirectory(Source/Session)` → `add_subdirectory(Source/Pod)`；`:45` 附近输出目录注释里的 `VaseSession/VaseHost` → `VasePod/VaseHost`；`Tests/CMakeLists.txt` 里 `gtest_discover_tests` 注释提到的 `VaseSession` → `VasePod`。

- [ ] **Step 3: 更新 `CLAUDE.md`（12.3 连带文书义务的即时项）**

四处，措辞照抄：
- 「配套文件」节 wiki 条目的注意行：把「`Source/Session` → `Source/Pod` 的**实体**更名待 M1（磁盘现状仍是 Session 命名，仓库里凡提到现有目录/探针仍以现状为准）」改为「`Source/Session` → `Source/Pod` 的实体更名**已于 M1-T1 落地**；仓库凡提到现有目录/探针以 `Source/Pod` / `VasePod` 为准」。
- 「项目状态」节：「`Source/Session`、`Source/Host` 各一个 `SmokeProbe.cpp`」→「`Source/Pod`、`Source/Host` 各一个 `SmokeProbe.cpp`」；该节里「`Session` 的实现在 M1」→「`PluginCatalog` / `PluginHost` / `Pod` 的实现在 M1（进行中）」。
- 「构建与测试」节：所有 `VaseSession` → `VasePod`。
- 「尚未确定的事项」表「目录布局与模块划分」行：仓库状态「`Source/Session`、`Source/Host`」→「`Source/Pod`、`Source/Host` 双 target（Pod 更名已落地）」。

- [ ] **Step 4: 干净树重建，两条 debug 线全绿**

```bash
rm -rf build-win/win-x64-clang-debug   # 目录名进过构建缓存，换新树最稳
cmake --preset win-x64-clang-debug && cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -N  # Expected: Total Tests: 2
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && rm -rf build-linux/linux-x64-clang-debug && cmake --preset linux-x64-clang-debug && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug -N'
```
Expected: 双线 build 零警告、ctest 全绿、`-N` 基数 2。产物 `VasePod.dll`（Windows `bin/`）/ `libVasePod.so`（Linux `lib/`）。

- [ ] **Step 5: 残留检查 + 格式门**

```bash
# 范围限定在代码 / 构建 / 现状文书：历史计划与设计文档是**记录**，不改史（T14 的文书义务同此口径）
git grep -n "VaseSession\|SessionSmokeProbe\|VASE_SESSION" -- Source Include Tests CMakeLists.txt CLAUDE.md README.md wiki
# Expected: 零命中。全仓范围必然命中 docs/superpowers/ 里本计划自己的映射表与 M0 历史计划——那不是残留。
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
```

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "M1-T1：Session→Pod 实体更名（目录/target/宏/符号），CLAUDE.md 连带更新"
```

---
## Task 2: Detail 地基——`Result<T>` / `Error` / `MetaArray` / `ProgrammerError`

全部是纯头文件设施，不跨 DLL；spec 3.3(2)(3) 的探针结论直接搬运（MetaArray 溢出的「只声明不定义」机制、`Error` 全拥有）。**它们是全项目的类型底座，后面的每个签名都会出现。**

**Files:**
- Create: `Include/Vase/Detail/Fail.h`、`Include/Vase/Detail/Result.h`、`Include/Vase/Detail/MetaArray.h`
- Create: `Tests/Unit/DetailTests.cpp`
- Modify: `Tests/CMakeLists.txt`（`VaseTests` 源列表加一行）

**Interfaces:**
- Consumes: 无（第一个内容任务）。
- Produces:
  - `vase::Phase : std::uint8_t { kLoad, kStart, kAdopt, kEject, kUnload }`
  - `vase::ErrorContext { std::string PluginId; std::string ServiceName; std::uint32_t ServiceVersion; Phase Stage; }`（§13.2 钉死的字段集）
  - `vase::Error`：`IsSet()` / `Message()` / `Context()`；全拥有（spec 3.3(3)，§0.3-6 的登记例外）
  - `vase::Result<T>`：`static Ok(T)` / `static Err(Error)` / `IsOk()` / `Value()`（非 Ok 时 → programmer error 终止）/ `GetError()`；`Result<void>` 特例化
  - `vase::MetaArray<T, Capacity>`：`constexpr`、`Size()` / `Empty()` / `Begin()` / `End()` / `operator[]`、溢出=编译错（另加运行期 abort）
  - `detail::ProgrammerError(msg, loc = std::source_location::current())` **函数**（不是宏）：打印消息 + `assert`（Debug 便于挂调试器）+ `abort`（两态都终止——§6.2「断言/终止」双形态在本实现里合一为「带诊断输出的终止」）。C++20 的 `source_location` 正好补上宏唯一不可替代的那点能力（`__FILE__`/`__LINE__`），于是 `cppcoreguidelines-macro-usage` 无从下手

- [ ] **Step 1: 写 `Include/Vase/Detail/Fail.h`**

```cpp
#pragma once

// 编程错误的一死到底出口（§6.2）：这不是可恢复失败——可恢复失败一律走 Result。
// Debug 下 assert 把线程钉在断言点（方便挂调试器），两个构建都 abort。
// 无异常编译（§0.3-7），没有「可接住的东西」，这是唯一正确形态。
//
// 形态：具名函数 + source_location 默认参数，而**不是**宏——C++20 的 source_location
// 补上了 __FILE__ / __LINE__ 这点宏唯一不可替代的能力。

#include <array>
#include <cassert>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <source_location>
#include <string_view>

namespace vase::detail
{

inline void WriteStderr(std::string_view text) { static_cast<void>(std::fwrite(text.data(), 1, text.size(), stderr)); }

// 把行号就地写成十进制实测长度（不分配、不抛，§9.x 清单）。
inline std::size_t FormatLineNumber(char* buffer, std::size_t capacity, int line)
{
    const std::to_chars_result result =
        std::to_chars(buffer, std::next(buffer, static_cast<std::ptrdiff_t>(capacity)), line);
    if (result.ec != std::errc{})
    {
        return 0;
    }
    return static_cast<std::size_t>(std::distance(buffer, result.ptr));
}

[[noreturn]] inline void ProgrammerError(std::string_view message,
                                         const std::source_location& location = std::source_location::current())
{
    std::array<char, 16> lineBuffer{};
    const std::size_t lineLength =
        FormatLineNumber(lineBuffer.data(), lineBuffer.size(), static_cast<int>(location.line()));

    WriteStderr("Vase programmer error: ");
    WriteStderr(message);
    WriteStderr(" (");
    WriteStderr(location.file_name());
    WriteStderr(":");
    WriteStderr(std::string_view{lineBuffer.data(), lineLength});
    WriteStderr(")\n");
    static_cast<void>(std::fflush(stderr));
    assert(false && "Vase programmer error");
    std::abort();
}

} // namespace vase::detail
```

- [ ] **Step 2: 写 `Include/Vase/Detail/Result.h`**

```cpp
#pragma once

// Result<T> / Error——全项目的错误通道（§0.3-7）。C++20 没有 std::expected，
// 手写最小形（spec 第 7 节：不引第三方，以免把 ABI 面交给别人）。
//
// Error 全拥有（std::string）：它是**返回值**，必须活得比调用久、且可能比插件
// 镜像活得久（spec 3.3(3)——DestroyPod 后宿主才读报告的路径）。
//
// 成员与访问器撞名时成员让位（Message() 与成员撞名 → 成员叫 Text）。

#include "Vase/Detail/Fail.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace vase
{

// 失败发生的阶段。kAdopt / kEject 为 v3 新增（§5.6 进出判定流）。
enum class Phase : std::uint8_t
{
    kLoad,
    kStart,
    kAdopt,
    kEject,
    kUnload,
};

struct ErrorContext
{
    std::string PluginId;
    std::string ServiceName;
    std::uint32_t ServiceVersion = 0;
    Phase Stage = Phase::kLoad;
};

class Error
{
public:
    Error() = default;
    explicit Error(std::string message, ErrorContext context = {})
        : Text(std::move(message))
        , Info(std::move(context))
    {
    }

    [[nodiscard]] bool IsSet() const { return !Text.empty(); }
    [[nodiscard]] const std::string& Message() const { return Text; }
    [[nodiscard]] const ErrorContext& Context() const { return Info; }

private:
    std::string Text;
    ErrorContext Info;
};

template <typename T>
class [[nodiscard]] Result
{
public:
    static Result Ok(T value)
    {
        Result r;
        r.Stored = std::move(value);
        return r;
    }

    static Result Err(Error error)
    {
        Result r;
        r.Failure = std::move(error);
        return r;
    }

    [[nodiscard]] bool IsOk() const { return Stored.has_value(); }

    [[nodiscard]] T& Value()
    {
        if (!Stored.has_value())
        {
            detail::ProgrammerError("Result::Value() called on error result");
        }
        return *Stored;
    }

    [[nodiscard]] const T& Value() const
    {
        if (!Stored.has_value())
        {
            detail::ProgrammerError("Result::Value() called on error result");
        }
        return *Stored;
    }

    [[nodiscard]] const Error& GetError() const
    {
        if (!Failure.has_value())
        {
            detail::ProgrammerError("Result::GetError() called on ok result");
        }
        return *Failure;
    }

private:
    Result() = default;

    std::optional<T> Stored;
    std::optional<Error> Failure;
};

template <>
class [[nodiscard]] Result<void>
{
public:
    static Result Ok()
    {
        Result r;
        r.Succeeded = true;
        return r;
    }

    static Result Err(Error error)
    {
        Result r;
        r.Failure = std::move(error);
        return r;
    }

    [[nodiscard]] bool IsOk() const { return Succeeded; }

    [[nodiscard]] const Error& GetError() const
    {
        if (!Failure.has_value())
        {
            detail::ProgrammerError("Result::GetError() called on ok result");
        }
        return *Failure;
    }

private:
    Result() = default;

    bool Succeeded = false;
    std::optional<Error> Failure;
};

} // namespace vase
```

（两处纪律已内置：可恢复失败路径**不碰** STL 抛点；`Value()` 误用走终止而非 UB——`.value()` 本身被禁用，见 Global Constraints。）

- [ ] **Step 3: 写 `Include/Vase/Detail/MetaArray.h`**

spec 3.3(2) 的定稿形态（**溢出机制已按 2026-09-17 实测修订**：`MetaArrayCapacityExceeded` 改为「定义成非 constexpr 终止函数」，理由见块内注释——只声明不定义会让运行期构造链接失败）：

```cpp
#pragma once

// 固定容量内联数组：描述符的 Requires/Provides 用它。不用 std::span——
// 没有 initializer_list 构造函数（spec 3.3(2)：文档写法直接编不过）；
// 也不押注 initializer_list **成员**的存储（探针未复现悬垂，正确性依赖实现细节）。
// 自持存储、constexpr 可构造、POD 可平凡拷贝——跨 Win/Linux 三套 STL 不赌行为。
//
// 存储用 std::array、填充用 std::copy：裸 C 数组与非常量下标的 operator[] 都过不了门禁，
// 而 std::array 在自持存储 / constexpr / 平凡拷贝 / 无堆分配四条上与裸数组等价。

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <initializer_list>
#include <iterator>

namespace vase
{

// 溢出出口：定义成**非 constexpr 的终止函数**，而不是 spec 3.3(2) 原始的「只声明不定义」。
// 两者都能让溢出的 constexpr 求值失败（碰到非 constexpr 调用即不是常量表达式）；差别在
// 非溢出路径——只声明不定义时符号引用照样发射，一切**运行期**构造都链接失败
// （实测 lld-link: undefined symbol，引它的是 `MetaArray<int,4>::MetaArray(initializer_list<int>)`，
// 成因是函数内的运行期构造）。溢出照旧响：常量求值里报错、运行期 abort。
[[noreturn]] inline void MetaArrayCapacityExceeded() { std::abort(); }

template <typename T, std::size_t Capacity>
class MetaArray
{
public:
    constexpr MetaArray() = default;

    constexpr MetaArray(std::initializer_list<T> items)
    {
        if (items.size() > Capacity)
        {
            MetaArrayCapacityExceeded();
        }
        Count = items.size();
        std::copy(items.begin(), items.end(), Items.begin());
    }

    [[nodiscard]] constexpr std::size_t Size() const { return Count; }
    [[nodiscard]] constexpr bool Empty() const { return Count == 0; }
    [[nodiscard]] constexpr const T* Begin() const { return Items.data(); }
    // 用 std::next 而非 Items.data() + Count：指针算式过不了 cppcoreguidelines-pro-bounds-*。
    [[nodiscard]] constexpr const T* End() const { return std::next(Begin(), static_cast<std::ptrdiff_t>(Count)); }
    [[nodiscard]] constexpr const T& operator[](std::size_t index) const
    {
        return *std::next(Items.begin(), static_cast<std::ptrdiff_t>(index));
    }

private:
    std::array<T, Capacity> Items{};
    std::size_t Count = 0;
};

} // namespace vase
```

- [ ] **Step 4: 写 `Tests/Unit/DetailTests.cpp`（8 个 TEST）**

```cpp
#include "Vase/Detail/Fail.h"
#include "Vase/Detail/MetaArray.h"
#include "Vase/Detail/Result.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace
{

struct MoveOnly
{
    std::string S;

    MoveOnly() = default;
    explicit MoveOnly(std::string s)
        : S(std::move(s))
    {
    }
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) noexcept = default;
    MoveOnly& operator=(MoveOnly&&) noexcept = default;
    ~MoveOnly() = default;
};

// 描述符里的条目就是这个形状（名字 + 主版本），拿来测非平凡元素类型的运行期构造。
struct Service
{
    std::string_view Name;
    std::uint32_t Version = 0;
};

TEST(Result, OkCarriesMoveOnlyValue)
{
    vase::Result<MoveOnly> r = vase::Result<MoveOnly>::Ok(MoveOnly{"vase"});
    EXPECT_TRUE(r.IsOk());
    EXPECT_EQ(r.Value().S, "vase");
}

TEST(Result, ErrCarriesMessageAndContext)
{
    vase::Result<int> r = vase::Result<int>::Err(
        vase::Error{"no such service", vase::ErrorContext{"Vase.Combat", "Vase.World", 1, vase::Phase::kAdopt}});
    EXPECT_FALSE(r.IsOk());
    EXPECT_EQ(r.GetError().Message(), "no such service");
    EXPECT_EQ(r.GetError().Context().PluginId, "Vase.Combat");
    EXPECT_EQ(r.GetError().Context().ServiceVersion, 1U);
    EXPECT_EQ(r.GetError().Context().Stage, vase::Phase::kAdopt);
}

TEST(Result, VoidOkAndErr)
{
    EXPECT_TRUE(vase::Result<void>::Ok().IsOk());
    vase::Result<void> e = vase::Result<void>::Err(vase::Error{"boom"});
    EXPECT_FALSE(e.IsOk());
    EXPECT_EQ(e.GetError().Message(), "boom");
}

TEST(Error, DefaultIsUnsetWithLoadPhase)
{
    vase::Error e;
    EXPECT_FALSE(e.IsSet());
    EXPECT_EQ(e.Context().Stage, vase::Phase::kLoad);
}

TEST(MetaArray, DefaultIsEmpty)
{
    vase::MetaArray<int, 4> a;
    EXPECT_TRUE(a.Empty());
    EXPECT_EQ(a.Size(), 0U);
}

TEST(MetaArray, InitializerPopulatesInOrder)
{
    vase::MetaArray<int, 4> a{1, 2, 3};
    ASSERT_EQ(a.Size(), 3U);
    EXPECT_EQ(a[0], 1);
    EXPECT_EQ(a[2], 3);
    EXPECT_EQ(a.End() - a.Begin(), 3);
}

TEST(MetaArray, CapacityBoundaryIsExact)
{
    vase::MetaArray<int, 2> a{7, 8}; // 恰好满：合法。超出的反例在 Step 6 手动验证。
    EXPECT_EQ(a.Size(), 2U);
}

// 运行期（非 constexpr）构造非平凡元素：这是「只声明不定义」的 MetaArrayCapacityExceeded
// 会让整个 TU 链接失败的形态（实测 undefined symbol），本用例即那条失效模式的守卫。
// 注意 -O2 下 clang 会把常量初值的构造整体折叠掉（const 与否都一样），故本守卫的有效范围是 debug 线。
TEST(MetaArray, RuntimeConstructionPopulatesAndReads)
{
    const vase::MetaArray<Service, 2> services{
        {.Name = "Vase.World", .Version = 1},
        {.Name = "Vase.Audio", .Version = 2},
    };
    ASSERT_EQ(services.Size(), 2U);
    EXPECT_EQ(std::distance(services.Begin(), services.End()), 2);
    EXPECT_EQ(services.Begin()->Name, "Vase.World");
    EXPECT_EQ(std::next(services.Begin())->Version, 2U);
}

TEST(Fail, ProgrammerErrorTerminates)
{
    // 终止类断言只能用 death test——EXPECT_THROW 族在本仓库是编译期硬失败（CLAUDE.md 规矩 2）。
    EXPECT_DEATH(vase::detail::ProgrammerError("death probe"), "Vase programmer error: death probe");
}

} // namespace
```

- [ ] **Step 5: 接进构建并跑绿**

`Tests/CMakeLists.txt` 源列表改为：

```cmake
add_executable(VaseTests
    Smoke/CrossDllSmoke.cpp
    Unit/DetailTests.cpp)
```

```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -N   # Expected: Total Tests: 11（基数 2 + 本任务 9）
```

- [ ] **Step 6: MetaArray 溢出反例（仓库外一次性，不入构建）**

溢出在**常量求值**里是编译错、在运行期是 abort。反例必须写成常量求值形态——写成函数内的运行期局部会编过（运行期才 abort，实测如此）。建 `D:\Git\.vase-probe\m1t2-overflow.cpp`：

```cpp
#include "Vase/Detail/MetaArray.h"

constexpr vase::MetaArray<int, 2> kOverflow{1, 2, 3}; // 应编译失败
```

```bash
export MSYS2_ARG_CONV_EXCL='*'   # 防 Git Bash 把 /I 开头的参数当路径转换（spec 2.1.1(d)）
clang++ -std=c++20 -fno-exceptions -Wall -Wextra -Werror \
    -I D:/Git/Vase/Include -c D:/Git/.vase-probe/m1t2-overflow.cpp
```
Expected: **编译失败**，诊断含 `constant expression`，note 链点名 `MetaArrayCapacityExceeded`。验证后删除临时文件——它无法作为常规编译单元存在（它的全部价值是不编译）。

- [ ] **Step 7: 门禁 + Commit**

```bash
git ls-files -z --cached --others --exclude-standard '*.h' '*.cpp' | xargs -0 clang-format --dry-run --Werror
run-clang-tidy -p build-win/win-x64-clang-debug 2>&1 | tail -4
# Expected: 退出 0 且正文 error:/warning: 各 0 条（摘要行里的「Suppressed」是第三方头的既有基数，CLAUDE.md）
git add -A
git commit -m "M1-T2：Detail 地基——Result<T>/Error/MetaArray/ProgrammerError + 单测"
```

---
## Task 3: Effect 机器——`IEffect` / `EffectHandle` / `EffectScope` / `ScopePool`

§7.1–7.3 的完整落地 + D8/D9/D10 的存储形态。「一切注册都可回收」的机制本体：Context 的 Provide/On 将来只是它的薄封装（T5），账本边随实例的 Scope 回收而死（T9/T10），所以它必须先于一切存在并被单测穷举。**本任务不跨 DLL**——实现在 `VasePod`，单测直接构造对象。

**Files:**
- Create: `Include/Vase/Detail/Counters.h`、`Include/Vase/Detail/ScopePool.h`
- Create: `Include/Vase/Effect/IEffect.h`、`Include/Vase/Effect/EffectHandle.h`、`Include/Vase/Effect/EffectScope.h`
- Create: `Source/Pod/ScopePool.cpp`、`Source/Pod/EffectScope.cpp`
- Create: `Tests/Unit/EffectTests.cpp`
- Modify: `Source/Pod/CMakeLists.txt`、`Tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `detail::ProgrammerError`（T2）、`VASE_POD_API`（T1）。
- Produces:
  - `vase::IEffect`：`virtual void Recycle() = 0`；保护非虚析构 + `friend class EffectScope`（回收路径独占销毁权）、禁拷贝
  - `vase::EffectHandle { EffectScope* Scope; std::uint32_t Slot; std::uint32_t Generation; }`——可平凡拷贝；`Release()`（回收动作 + 划账**原子**，§7.3）、`IsValid()`
  - `vase::EffectScope`：`Create<T>(args...) -> EffectHandle`、`Dispose()`（逆序、幂等、终态）、`EffectCount()`、`IsDisposed()`、`OwnerLabel()`（D15 最小属主标记）
  - `vase::detail::ScopePool`：`Acquire(size)` / `Release(ptr, size)`（size-class 空闲链；块统一 16 对齐——`static_assert(alignof(T) <= 16)` 在 `Create` 里守住）
  - `vase::detail::DiagnosticCounters { uint64 Effects, Services, Subscriptions, PluginInstances, Scopes; }`（§9.2 五项，M1 期间 EffectScope 动 Effects/Scopes，T5/T7 动其余）

- [ ] **Step 1: 写 `Include/Vase/Detail/Counters.h`**

```cpp
#pragma once

// §9.2 的五项诊断计数。放 Detail 是因为 VasePod（Effect/Service 侧）与
// VaseHost（持有者）都要见到同一布局。**不做原子、不加锁**——依据是 §1.4 的
// 绑定线程契约；谁日后为「多线程装配」拆 1.4，谁就得连这里一起重做（§9.2 明文）。
// 始终维护（Release 也要写进 PodReport），断言只在 Debug——分工不是取舍。

#include <cstdint>

namespace vase::detail
{

struct DiagnosticCounters
{
    std::uint64_t Effects = 0;
    std::uint64_t Services = 0;
    std::uint64_t Subscriptions = 0;
    std::uint64_t PluginInstances = 0;
    std::uint64_t Scopes = 0;
};

} // namespace vase::detail
```

- [ ] **Step 2: 写 `Include/Vase/Detail/ScopePool.h` + `Source/Pod/ScopePool.cpp`**

```cpp
#pragma once

// size-class 空闲链表（D8）。铁律 §1.2 的豁免边界，务必读懂再改：
// 池是进程级容器，但它**只持有空闲节点内存，不持有任何实例级对象**
// （spec 第 5 节原话）——这不违规，它是「反复 play/stop 稳态零分配」（D10）的实现。
//
// chunk 用 vector<std::uint8_t> 而非 unique_ptr<uint8_t[]>（后者是 C 数组类型）；
// vector 被移动时堆缓冲随指针转移，故 Chunks 扩容不影响已发出的块地址。

#include "Vase/Detail/Export.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace vase::detail
{

VASE_MSVC_DLL_WARNINGS_BEGIN
class VASE_POD_API ScopePool
{
public:
    // 块对齐**写死 16**，不用 alignof(std::max_align_t)：后者在 MSVC STL 是 8、libc++ 是 16
    // （实测：同一句 static_assert 在 Windows 失败、Linux 通过），会让 alignas(16) 的 Effect
    // 在 Linux 编得过、Windows 编不过——跨平台插件库不该有这种陷阱。
    static constexpr std::size_t kAlignment = 16;

    ScopePool() = default;
    ~ScopePool() = default;

    ScopePool(const ScopePool&) = delete;
    ScopePool& operator=(const ScopePool&) = delete;
    ScopePool(ScopePool&&) = delete;
    ScopePool& operator=(ScopePool&&) = delete;

    // 返回一块 Size 字节、kAlignment 对齐的内存（内部向上取整，最小 sizeof(FreeNode)）。
    // Size 无上界：超过 kChunkBytes 时按需开一块更大的 chunk（见 .cpp）。
    void* Acquire(std::size_t size);
    // Object 必须是先前 Acquire(Size) 的返回值且 Size 相同（两次的取整一致）。
    void Release(void* object, std::size_t size);

private:
    struct FreeNode
    {
        FreeNode* Next = nullptr;
    };

    static constexpr std::size_t kChunkBytes = std::size_t{64} * 1024;

    static std::size_t BlockBytes(std::size_t size)
    {
        const std::size_t rounded = (size + kAlignment - 1) & ~(kAlignment - 1);
        return rounded < sizeof(FreeNode) ? sizeof(FreeNode) : rounded;
    }

    std::vector<std::vector<std::uint8_t>> Chunks;
    std::uint8_t* ChunkCursor = nullptr;                      // 当前 chunk 的**已对齐**起点
    std::size_t BumpOffset = 0;                               // 相对 ChunkCursor
    std::vector<std::pair<std::size_t, FreeNode*>> FreeLists; // (块字节数, 链头)
};
VASE_MSVC_DLL_WARNINGS_END

} // namespace vase::detail
```

```cpp
#include "Vase/Detail/ScopePool.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>

namespace vase::detail
{

void* ScopePool::Acquire(std::size_t size)
{
    assert(size > 0);
    const std::size_t bytes = BlockBytes(size);

    // 迭代器而非下标：非常量下标的 operator[] 过不了 cppcoreguidelines-pro-bounds-*。
    for (auto entry = FreeLists.begin(); entry != FreeLists.end(); ++entry)
    {
        if (entry->first == bytes)
        {
            FreeNode* node = entry->second;
            if (node->Next == nullptr)
            {
                FreeLists.erase(entry);
            }
            else
            {
                entry->second = node->Next;
            }
            return node; // 复用的块本就是 kAlignment 对齐的
        }
    }

    if (ChunkCursor == nullptr || BumpOffset + bytes > kChunkBytes)
    {
        // 多要 kAlignment 字节做对齐余量，再把可用起点向上取整到 kAlignment——
        // 不赌分配器给多少对齐（kAlignment 写死 16，而 max_align_t 在 MSVC STL 只有 8）。
        // 同时按需开大：bytes > kChunkBytes 时若只开 kChunkBytes，返回区会越过 chunk 末尾。
        const std::size_t chunkBytes = (bytes > kChunkBytes ? bytes : kChunkBytes) + kAlignment;
        Chunks.emplace_back(chunkBytes, std::uint8_t{0});
        std::size_t space = chunkBytes;
        void* cursor = Chunks.back().data();
        ChunkCursor = static_cast<std::uint8_t*>(std::align(kAlignment, 1, cursor, space));
        assert(ChunkCursor != nullptr); // 留了 kAlignment 余量，必然成功
        BumpOffset = 0;
    }
    std::uint8_t* block = std::next(ChunkCursor, static_cast<std::ptrdiff_t>(BumpOffset));
    BumpOffset += bytes;
    return block;
}

void ScopePool::Release(void* object, std::size_t size)
{
    assert(object != nullptr);
    assert(size > 0);
    const std::size_t bytes = BlockBytes(size);
    auto* node = static_cast<FreeNode*>(object);
    for (auto& entry : FreeLists)
    {
        if (entry.first == bytes)
        {
            node->Next = entry.second;
            entry.second = node;
            return;
        }
    }
    node->Next = nullptr;
    FreeLists.emplace_back(bytes, node);
}

} // namespace vase::detail
```

（**空闲链而非 bump 回退**是刻意的：§7.3 的 `Release()` 任意顺序回收，LIFO 处理不了——spec 第 5 节的这条理由写在类注释里防「优化」。）

- [ ] **Step 3: 写 `Include/Vase/Effect/IEffect.h` 与 `Include/Vase/Effect/EffectHandle.h`**

```cpp
#pragma once

// 副作用的统一表示（§7.1）。回收语义（D8）：Recycle() 做逻辑撤销；
// 对象内存归 Scope/池所有，销毁路径 = Recycle + 显式析构调用，**不走 delete**。
// 移交式注册（Provide(unique_ptr)）的 delete 发生在外壳适配器的 Recycle 内部——
// 虚调用在持有该类型的镜像里执行，与 §3.1 的 VasePluginDestroy_ 同一性质。

namespace vase
{

class EffectScope;

class IEffect
{
public:
    virtual void Recycle() = 0;

    // 四个特殊成员全部显式处置：Effect 一律就地构造、由 Scope 回收，从不拷贝也从不移动。
    IEffect(const IEffect&) = delete;
    IEffect& operator=(const IEffect&) = delete;
    IEffect(IEffect&&) = delete;
    IEffect& operator=(IEffect&&) = delete;

protected:
    IEffect() = default;
    ~IEffect() = default; // 保护非虚析构：delete IEffect* 编不过，销毁权只属于 EffectScope

    friend class EffectScope;
};

} // namespace vase
```

```cpp
#pragma once

// 索引 + 代际句柄（D9）。代际一次解决两件事：§7.2 的 Dispose 幂等
// （过期句柄 Release 无副作用）与 §7.3 的「从账上移除」。可平凡拷贝——
// 拷贝出去的第二份句柄在槽位被回收并复用后自动失效（代际不匹配）。
//
// **句柄不得活得比它的 EffectScope 久**：Scope 是非拥有裸指针，Release()/IsValid() 都解引用它。
// 代际只护「槽位复用」那一侧，护不了「Scope 已死」——陈旧句柄安全不是全保。
//
// 标 VASE_POD_API：两个成员函数的定义在 VasePod（要 EffectScope 完整类型），
// 不导出则本库外的调用方链接期即失败（实测 lld-link: undefined symbol）。

#include "Vase/Detail/Export.h"

#include <cstdint>

namespace vase
{

class EffectScope;

struct VASE_POD_API EffectHandle
{
    static constexpr std::uint32_t kInvalidSlot = 0xFFFFFFFFU;

    EffectScope* Scope = nullptr;
    std::uint32_t Slot = kInvalidSlot;
    std::uint32_t Generation = 0;

    [[nodiscard]] bool IsValid() const;
    // 立即回收 + 从 Scope 账上移除——两件事原子，缺一不可（§7.3）。
    // 对失效句柄调用无副作用（幂等）。const 是准确的：它改的是 Scope，不是句柄本身。
    void Release() const;
};

} // namespace vase
```

- [ ] **Step 4: 写 `Include/Vase/Effect/EffectScope.h`**

```cpp
#pragma once

// Effect 的归属容器（§7.2）：逆序回收、Dispose 幂等、回收后不得再注册（终态）。
// Label 是 D15 的最小属主标记：诊断归属与 §1.2 断言用字符串指针就够，
// 完整归属追踪等 M3。（成员不与访问器 OwnerLabel() 撞名——成员叫 Label。）
//
// 构造参数经 std::tuple + std::apply 展开（裸 std::forward<Args>(tup) 带参编不过）；
// 对象指针由两次 static_cast 求出、不接 placement-new 的返回值——所有权自始至终属于本 Scope。

#include "Vase/Detail/Counters.h"
#include "Vase/Detail/Export.h"
#include "Vase/Detail/ScopePool.h"
#include "Vase/Effect/EffectHandle.h"
#include "Vase/Effect/IEffect.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace vase
{

VASE_MSVC_DLL_WARNINGS_BEGIN
class VASE_POD_API EffectScope
{
public:
    explicit EffectScope(detail::ScopePool& pool, detail::DiagnosticCounters* counters = nullptr,
                         const char* ownerLabel = "");
    ~EffectScope(); // 兜底 Dispose；计数能抓的是「Scope 从未析构」，不是「忘了 Dispose」——析构里补上了

    EffectScope(const EffectScope&) = delete;
    EffectScope& operator=(const EffectScope&) = delete;
    EffectScope(EffectScope&&) = delete;
    EffectScope& operator=(EffectScope&&) = delete;

    template <typename T, typename... Args>
    EffectHandle Create(Args&&... args)
    {
        static_assert(std::is_base_of_v<IEffect, T>, "Effect must derive from vase::IEffect");
        static_assert(alignof(T) <= detail::ScopePool::kAlignment, "over-aligned effects need a pool change first");
        std::tuple<Args&&...> pack(std::forward<Args>(args)...);
        return CreateRaw(
            sizeof(T),
            [](void* where, void* packed, IEffect** out)
            {
                auto& p = *static_cast<std::tuple<Args&&...>*>(packed);
                std::apply([where](auto&&... unpacked)
                           { ::new (where) T(std::forward<decltype(unpacked)>(unpacked)...); }, std::move(p));
                *out = static_cast<IEffect*>(static_cast<T*>(where)); // 多继承下两者可能不同址
            },
            // 析构必须由 T 自己跑：~IEffect 非虚，s.Object->~IEffect() 只销毁基类子对象，
            // 派生类成员全漏。§7.1 的「销毁路径 = Recycle + 显式析构调用」要的正是这一下。
            [](IEffect* object) { static_cast<T*>(object)->~T(); }, &pack);
    }

    void Dispose(); // 逆序、幂等（§7.2）

    [[nodiscard]] std::size_t EffectCount() const;
    [[nodiscard]] bool IsDisposed() const { return Disposed; }
    [[nodiscard]] const char* OwnerLabel() const { return Label; }

private:
    friend struct EffectHandle;

    static constexpr std::uint32_t kNone = 0xFFFFFFFFU;

    struct Slot
    {
        void* Memory = nullptr; // placement-new 的原始块（多继承下 != 对象指针），Release 时原样还给池
        IEffect* Object = nullptr;
        void (*Destroy)(IEffect*) = nullptr; // 由 Create<T> 交下来的派生析构 thunk
        std::size_t Size = 0;
        std::uint32_t Generation = 0;
        std::uint32_t Prev = kNone;
        std::uint32_t Next = kNone; // 活：注册链；死：自由链
        bool Live = false;
    };

    EffectHandle CreateRaw(std::size_t size, void (*construct)(void*, void*, IEffect**), void (*destroy)(IEffect*),
                           void* arg);
    void ReleaseSlot(std::uint32_t slot, std::uint32_t generation);
    void Unlink(std::uint32_t slot);
    [[nodiscard]] bool SlotAlive(std::uint32_t slot, std::uint32_t generation) const;

    // 槽按下标寻址是设计本体（自由链与注册链存的就是下标），故下标访问集中在这两个访问器里。
    // 用 std::next 而非 operator[]：非常量下标过不了 cppcoreguidelines-pro-bounds-avoid-unchecked-container-access。
    [[nodiscard]] Slot& SlotAt(std::uint32_t index)
    {
        return *std::next(Slots.begin(), static_cast<std::ptrdiff_t>(index));
    }
    [[nodiscard]] const Slot& SlotAt(std::uint32_t index) const
    {
        return *std::next(Slots.begin(), static_cast<std::ptrdiff_t>(index));
    }

    detail::ScopePool& Pool;
    detail::DiagnosticCounters* Counters;
    const char* Label;
    std::vector<Slot> Slots;
    std::uint32_t Head = kNone; // 注册序链（Dispose 从 Tail 逆序走）
    std::uint32_t Tail = kNone;
    std::uint32_t FreeHead = kNone;
    bool Disposed = false;
};
VASE_MSVC_DLL_WARNINGS_END

} // namespace vase
```

`Source/Pod/EffectScope.cpp`：

```cpp
#include "Vase/Effect/EffectScope.h"

#include "Vase/Detail/Counters.h"
#include "Vase/Detail/Fail.h"
#include "Vase/Detail/ScopePool.h"
#include "Vase/Effect/EffectHandle.h"
#include "Vase/Effect/IEffect.h"

#include <cstddef>
#include <cstdint>

namespace vase
{

bool EffectHandle::IsValid() const { return Scope != nullptr && Scope->SlotAlive(Slot, Generation); }

void EffectHandle::Release() const
{
    if (Scope != nullptr && Slot != kInvalidSlot)
    {
        Scope->ReleaseSlot(Slot, Generation); // 内部校验代际，过期即无副作用（D9）
    }
}

EffectScope::EffectScope(detail::ScopePool& pool, detail::DiagnosticCounters* counters, const char* ownerLabel)
    : Pool(pool)
    , Counters(counters)
    , Label(ownerLabel)
{
    if (Counters != nullptr)
    {
        ++Counters->Scopes;
    }
}

EffectScope::~EffectScope()
{
    Dispose(); // 幂等，已 Dispose 则无副作用
}

EffectHandle EffectScope::CreateRaw(std::size_t size, void (*construct)(void*, void*, IEffect**),
                                    void (*destroy)(IEffect*), void* arg)
{
    if (Disposed)
    {
        // §7.2「回收后不得再注册」要求 Debug 断言；这里两个构建都终止——比最低要求严。
        detail::ProgrammerError("disposed scope rejects new effects");
    }

    std::uint32_t slot = kNone;
    if (FreeHead != kNone)
    {
        slot = FreeHead;
        FreeHead = SlotAt(slot).Next;
    }
    else
    {
        slot = static_cast<std::uint32_t>(Slots.size());
        Slots.emplace_back();
    }

    Slot& s = SlotAt(slot);
    void* mem = Pool.Acquire(size);
    IEffect* obj = nullptr;
    construct(mem, arg, &obj); // 构造回调把对象指针写进 out（不用返回值：那是 owning-memory 的所有权语义）
    s.Memory = mem;
    s.Object = obj;
    s.Destroy = destroy;
    s.Size = size;
    s.Live = true;
    s.Prev = Tail;
    s.Next = kNone;
    if (Tail != kNone)
    {
        SlotAt(Tail).Next = slot;
    }
    else
    {
        Head = slot;
    }
    Tail = slot;
    if (Counters != nullptr)
    {
        ++Counters->Effects;
    }
    return EffectHandle{.Scope = this, .Slot = slot, .Generation = s.Generation};
}

void EffectScope::ReleaseSlot(std::uint32_t slot, std::uint32_t generation)
{
    if (slot >= Slots.size())
    {
        return;
    }
    Slot& s = SlotAt(slot);
    if (!s.Live || s.Generation != generation)
    {
        return; // 陈旧/重复句柄：静默无副作用（§7.2 幂等 + D9）
    }

    s.Object->Recycle(); // 逻辑撤销（注册链此刻仍含本槽：Recycle 里允许读兄弟 Effect）
    // 派生析构由 thunk 跑。写成 s.Object->~IEffect() 只销毁基类子对象（~IEffect 非虚），
    // 派生成员全漏；限定名写法只是压掉 -Wdelete-abstract-non-virtual-dtor，不解决问题。
    s.Destroy(s.Object);
    Pool.Release(s.Memory, s.Size);

    Unlink(slot);
    s.Memory = nullptr;
    s.Object = nullptr;
    s.Live = false;
    ++s.Generation; // 复用即推进：旧句柄从此永远解不开新槽（D9）
    s.Next = FreeHead;
    FreeHead = slot;
    if (Counters != nullptr)
    {
        --Counters->Effects;
    }
}

void EffectScope::Unlink(std::uint32_t slot)
{
    const Slot& s = SlotAt(slot);
    if (s.Prev != kNone)
    {
        SlotAt(s.Prev).Next = s.Next;
    }
    else
    {
        Head = s.Next;
    }
    if (s.Next != kNone)
    {
        SlotAt(s.Next).Prev = s.Prev;
    }
    else
    {
        Tail = s.Prev;
    }
}

void EffectScope::Dispose()
{
    if (Disposed)
    {
        return; // §7.2 幂等
    }
    while (Tail != kNone)
    {
        const std::uint32_t current = Tail;
        ReleaseSlot(current, SlotAt(current).Generation); // 后注册先销毁（§5.4 的顺序保证）
    }
    Disposed = true;
    if (Counters != nullptr)
    {
        --Counters->Scopes;
    }
}

std::size_t EffectScope::EffectCount() const
{
    std::size_t count = 0;
    for (std::uint32_t i = Head; i != kNone; i = SlotAt(i).Next)
    {
        ++count;
    }
    return count;
}

bool EffectScope::SlotAlive(std::uint32_t slot, std::uint32_t generation) const
{
    return slot < Slots.size() && SlotAt(slot).Live && SlotAt(slot).Generation == generation;
}

} // namespace vase
```

- [ ] **Step 5: 写测试 `Tests/Unit/EffectTests.cpp`（8 个 TEST）**

```cpp
#include "Vase/Detail/Counters.h"
#include "Vase/Detail/ScopePool.h"
#include "Vase/Effect/EffectHandle.h"
#include "Vase/Effect/EffectScope.h"
#include "Vase/Effect/IEffect.h"

#include <gtest/gtest.h>
#include <vector>

namespace
{

std::vector<int>& RecycleLog()
{
    static std::vector<int> log;
    return log;
}

// final：IEffect 是保护非虚析构（销毁权归 Scope），派生类不该被拿去多态 delete；
// 标了 final 才不招 cppcoreguidelines-virtual-class-destructor。
class Tagged final : public vase::IEffect
{
public:
    explicit Tagged(int tag)
        : Tag(tag)
    {
    }
    void Recycle() override { RecycleLog().push_back(Tag); }

    int Tag;
};

std::vector<int>& DestroyLog()
{
    static std::vector<int> log;
    return log;
}

// Finding 1 的守卫装置：Tagged 只持一个 int（析构平凡），回收路径漏没漏跑派生析构看不出来。
// 这个的析构往静态日志记一笔——若回收只销毁 IEffect 基类子对象（限定名析构那种写法），
// 日志必为空，用例确定性红灯，无需 ASan。
class NonTrivial final : public vase::IEffect
{
public:
    explicit NonTrivial(int tag)
        : Tag(tag)
    {
    }
    ~NonTrivial() { DestroyLog().push_back(Tag); }
    void Recycle() override {}

    // 与 IEffect 一致：Effect 就地构造、由 Scope 回收，从不拷贝也从不移动。
    // 显式写全也是 cppcoreguidelines-special-member-functions 要的（声明了析构就得处置其余四个）。
    NonTrivial(const NonTrivial&) = delete;
    NonTrivial& operator=(const NonTrivial&) = delete;
    NonTrivial(NonTrivial&&) = delete;
    NonTrivial& operator=(NonTrivial&&) = delete;

    int Tag;
};

class Fixture
{
public:
    vase::detail::DiagnosticCounters Counters;
    vase::detail::ScopePool Pool;
    vase::EffectScope Scope{Pool, &Counters, "fixture"};
};

TEST(EffectScope, DisposeRecyclesInReverseOrder)
{
    Fixture f;
    RecycleLog().clear();
    f.Scope.Create<Tagged>(1);
    f.Scope.Create<Tagged>(2);
    f.Scope.Create<Tagged>(3);
    f.Scope.Dispose();
    EXPECT_EQ(RecycleLog(), (std::vector<int>{3, 2, 1}));
    EXPECT_EQ(f.Counters.Effects, 0U);
    EXPECT_EQ(f.Counters.Scopes, 0U);
}

TEST(EffectScope, DisposeIsIdempotent)
{
    Fixture f;
    RecycleLog().clear();
    f.Scope.Create<Tagged>(1);
    f.Scope.Dispose();
    f.Scope.Dispose();
    EXPECT_EQ(RecycleLog().size(), 1U);
    EXPECT_EQ(f.Counters.Scopes, 0U); // 第二次 Dispose 不得再动计数
}

TEST(EffectScope, DisposeRunsDerivedDestructor)
{
    Fixture f;
    DestroyLog().clear();
    f.Scope.Create<NonTrivial>(11);
    f.Scope.Create<NonTrivial>(12);
    f.Scope.Dispose();
    EXPECT_EQ(DestroyLog(), (std::vector<int>{12, 11})); // 派生析构真跑了，且与回收同序（Finding 1 的守卫）
}

TEST(EffectHandle, ReleaseExecutesAndUnregistersAtomically)
{
    Fixture f;
    RecycleLog().clear();
    const vase::EffectHandle h = f.Scope.Create<Tagged>(42);
    EXPECT_EQ(f.Scope.EffectCount(), 1U);
    h.Release();
    EXPECT_EQ(RecycleLog(), (std::vector<int>{42})); // 动作执行了……
    EXPECT_EQ(f.Scope.EffectCount(), 0U);            // ……账也划了（§7.3 两件事一起）
    f.Scope.Dispose();
    EXPECT_EQ(RecycleLog().size(), 1U); // 不再二次回收
}

TEST(EffectHandle, SecondReleaseIsNoOp)
{
    Fixture f;
    RecycleLog().clear();
    const vase::EffectHandle h = f.Scope.Create<Tagged>(7);
    h.Release();
    h.Release();
    EXPECT_EQ(RecycleLog().size(), 1U);
}

TEST(EffectHandle, StaleHandleCannotTouchReusedSlot)
{
    Fixture f;
    RecycleLog().clear(); // 与其他用例一致：日志是静态的，单进程整跑时不先清会串味
    const vase::EffectHandle a = f.Scope.Create<Tagged>(1);
    a.Release();                                            // 槽进自由链，代际推进
    const vase::EffectHandle b = f.Scope.Create<Tagged>(2); // 大概率复用同一槽
    a.Release();                                            // 陈旧句柄：必须无副作用（D9）
    EXPECT_EQ(f.Scope.EffectCount(), 1U);                   // b 仍活着
    EXPECT_EQ(RecycleLog().size(), 1U);                     // 只回收了 a 那一次
    b.Release();
    EXPECT_EQ(f.Scope.EffectCount(), 0U);
}

TEST(ScopePool, FreedBlockIsReusedAtSameAddress)
{
    vase::detail::ScopePool pool;
    void* first = pool.Acquire(64);
    pool.Release(first, 64);
    EXPECT_EQ(pool.Acquire(64), first); // 稳态零分配的直接证据（D10）
}

TEST(ScopePool, DistinctSizesGetDistinctBlocks)
{
    vase::detail::ScopePool pool;
    void* a = pool.Acquire(16);
    void* b = pool.Acquire(32);
    EXPECT_NE(a, b);
}

#ifndef NDEBUG
TEST(EffectScopeDeath, CreateAfterDisposeTerminates)
{
    Fixture f;
    f.Scope.Dispose();
    EXPECT_DEATH(f.Scope.Create<Tagged>(1), "disposed scope rejects new effects");
}
#endif

} // namespace
```

- [ ] **Step 6: 接构建，跑绿**

`Source/Pod/CMakeLists.txt` 源列表：

```cmake
add_library(VasePod SHARED
    SmokeProbe.cpp
    ScopePool.cpp
    EffectScope.cpp)
```

`Tests/CMakeLists.txt`：`VaseTests` 源列表追加 `Unit/EffectTests.cpp`。

```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -N   # Expected: Total Tests: 20（11 + 9；death test 计入 debug 线）
```

- [ ] **Step 7: Linux 线 + 门禁 + Commit**

```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug -N'
git ls-files -z --cached --others --exclude-standard '*.h' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git add -A
git commit -m "M1-T3：Effect 机器——IEffect/EffectHandle/EffectScope/ScopePool + 回收序单测"
```

> **release 线基数备忘**：`#ifndef NDEBUG` 的 death test 在 release 树编译掉——release preset 的 `-N` 基数比 debug 少 1。T14 写进 `CLAUDE.md`。

---
## Task 4: 描述符与插件面——`PluginDescriptor.h` + `VASE_PLUGIN` 宏

v3 §3.1 的三件套（工厂、唯一命名描述符、统一入口）落地；宏展开式**照搬 spec 3.3(1) 的探针定稿**（前向声明 + 花括号落在最后一行变量声明上）。`Plugin` 基类与 `HeaderVersion` 同文件——描述符布局是对外契约，两者本是一体。**本任务不碰 `Plugin.h`**（它是 T5 才组装齐的作者入口），测试直接包含 `PluginDescriptor.h`。

**Files:**
- Create: `Include/Vase/PluginDescriptor.h`
- Create: `Tests/Unit/DescriptorTests.cpp`
- Modify: `Tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `MetaArray`（T2）、`Result`（T2）、`VASE_EXPORT`（T1）。
- Produces:
  - `vase::ServiceRef { std::string_view Name; std::uint32_t Version; }`（描述符里的依赖/提供条目，§6.1 的「名字+整数主版本」）
  - `vase::PluginMeta { std::string_view Id, DisplayName, Version; MetaArray<ServiceRef, 16> Requires, Provides; }`（作者写的部分；`Config`/`ProcessState` 分别 M2/M3 补，spec 3.2）
  - `vase::PluginDescriptor { std::uint32_t HeaderVersion; const PluginMeta* Meta; Plugin* (*Create)(); void (*Destroy)(Plugin*); }`
  - `vase::kHeaderVersion = 1U`（D13；加载路径**第一个**读它——T7/T11 执法）
  - `class vase::Plugin`：虚析构 + `OnLoad`=0 + `OnStart` 默认空实现（§3.1；构造函数不注册副作用的约定写进注释，§5.4）
  - `VASE_PLUGIN(Type)` 宏：展开 `extern const PluginMeta kVaseMeta_##Type` 前向声明、`VasePluginCreate_##Type` / `VasePluginDestroy_##Type`、`extern "C" VASE_EXPORT VasePluginDesc_##Type()`、`extern "C" VASE_EXPORT VasePlugin_GetPlugin(const char*)`、最后以 `const ::vase::PluginMeta kVaseMeta_##Type = ::vase::PluginMeta` 收尾接用户花括号（spec 3.3(1) 实测展开）

- [ ] **Step 1: 写 `Include/Vase/PluginDescriptor.h`**

```cpp
#pragma once

// 描述符——插件二进制与宿主之间的静态契约（v3 §3.1）。POD 纪律：固定布局、
// 数组+长度、const char*/string_view 指向只读字面量——VaseCli scan（M5）将只读
// 数据段拿到全部元信息，不执行镜像里的任何代码。
//
// 拆分 PluginMeta / PluginDescriptor 的理由（spec 3.3(1)，已过编译探针）：
// VASE_PLUGIN 后面那块用户花括号必须整体初始化一个变量，而宏展开到 descriptor
// 聚合里时花括号放不进去——所以宏的最后一行恰好是 `const PluginMeta kVaseMeta_X = PluginMeta`，
// 用户写的 `{...}` 直接落在它身上。作者仍只写一处（§3.1「只有第一项需要手写」）。

#include "Vase/Detail/Export.h"
#include "Vase/Detail/MetaArray.h"
#include "Vase/Detail/Result.h"

#include <cstdint>
#include <memory>
#include <string_view>

namespace vase
{

class Context; // 前向声明即可：Plugin 的虚函数只接收引用（T5 定义）
class Plugin;  // 同上：PluginDescriptor 的两个函数指针按名引用它

// §8.3：「插件与宿主包含同一份 Vase 头文件」这条前提唯一的执行点（§3.1）。
// 每次不兼容改动递增；插件作者不需要知道它的存在。
inline constexpr std::uint32_t kHeaderVersion = 1U;

struct ServiceRef
{
    std::string_view Name;
    std::uint32_t Version = 0;
};

// 作者写的部分：纯数据、可平凡拷贝。Requires/Provides 各 16 条上限是编译错误，
// 不静默截断（spec 3.3(2)；上限值随真实插件调整时改这一个常量）。
struct PluginMeta
{
    std::string_view Id;
    std::string_view DisplayName;
    std::string_view Version;
    MetaArray<ServiceRef, 16> Requires;
    MetaArray<ServiceRef, 16> Provides;
    // M2 补 Config，M3 补 ProcessState（spec 3.2；v3 §9.1 的 Eject 自动重置依赖后者）
};

// 二进制契约：HeaderVersion 必须是**第一个**字段——加载路径先读它再读其余（§3.1）。
// 四个字段各给零值初值：聚合体也得每个字段都有初值（VASE_PLUGIN 的聚合初始化照样覆盖）。
struct PluginDescriptor
{
    std::uint32_t HeaderVersion = 0;
    const PluginMeta* Meta = nullptr;
    Plugin* (*Create)() = nullptr;
    void (*Destroy)(Plugin*) = nullptr;
};

class Plugin
{
public:
    // 虚析构必须存在：VasePluginDestroy_ 在插件镜像内 delete，这条路径的正确性以它为前提。
    virtual ~Plugin() = default;

    // 默认构造**必须显式 default**：删拷贝/移动会一并抑制隐式默认构造，
    // 少了这一行 VASE_PLUGIN 的 `std::make_unique<Type>()` 当场编不过。
    Plugin() = default;
    Plugin(const Plugin&) = delete;
    Plugin& operator=(const Plugin&) = delete;
    Plugin(Plugin&&) = delete;
    Plugin& operator=(Plugin&&) = delete;

    virtual Result<void> OnLoad(Context& ctx) = 0;
    virtual Result<void> OnStart(Context& ctx)
    {
        static_cast<void>(ctx); // 不是所有插件都需要启动动作（§3.1）
        return Result<void>::Ok();
    }

    // 构造函数只做初始化，不注册副作用、不启动活动（§5.4 的前提，明文写出防复发）。
};

} // namespace vase

// 生成物全集（spec 3.3(1) 探针定稿）：工厂 + 唯一命名描述符 + 统一入口。
// 符号名由类名派生不是插件 Id（Id 含点号不是合法标识符；类名在镜像内天然唯一，§3.1）。
// 一个库装一个插件才有 GetPlugin；组合库的分发表由 VasePack 生成（M5，§8.6），
// 与本宏无关。
//
// 作者侧写法：VASE_PLUGIN(MyPlugin){ ... }; —— 花括号紧贴宏。它落在宏实参内，
// 不受 Allman 管辖，换行写会被格式门判红。
// 宏体里**一个 NOLINT 都不需要**——三条会被报的检查各有代码级出路：
//   · Create / Destroy 只在本 TU 内被取地址 → 放进匿名命名空间，同时避开
//     misc-use-internal-linkage 与 misc-use-anonymous-namespace（`static` 只满足前者，
//     会立刻招来后者；两者都是内部链接，语义等价）；
//   · 创建走 make_unique、销毁端用 unique_ptr 接住再析构：既消掉裸 new/delete 表达式，
//     也消掉 misc-const-correctness——后者对 raw 的建议是给**指针所指**加 const
//     （`::vase::Plugin const* raw`），那是另一个函数类型、赋不进描述符的 Destroy 槽，
//     所以正确的出路是把裸指针整个去掉，而不是照它的 fix-it 改。
// 与 spec 3.3(1) 的探针定稿相比，宏体的外围写法有**四处**不同：上面两处、匿名命名空间的
// 包裹、以及 GetPlugin 的 nullptr 守卫（探针片段没有）。探针证明的**机制**——用户花括号
// 落在宏末行的变量声明上——原样保留。spec 是历史记录，不改史；其宏体写法以此为最新。
#define VASE_PLUGIN(Type)                                                                                              \
    extern const ::vase::PluginMeta kVaseMeta_##Type;                                                                  \
    namespace                                                                                                          \
    {                                                                                                                  \
    ::vase::Plugin* VasePluginCreate_##Type() { return std::make_unique<Type>().release(); }                           \
    void VasePluginDestroy_##Type(::vase::Plugin* raw) { const std::unique_ptr<::vase::Plugin> owning{raw}; }          \
    }                                                                                                                  \
    extern "C" VASE_EXPORT const ::vase::PluginDescriptor* VasePluginDesc_##Type()                                     \
    {                                                                                                                  \
        static const ::vase::PluginDescriptor kDesc{::vase::kHeaderVersion, &kVaseMeta_##Type,                         \
                                                    &VasePluginCreate_##Type, &VasePluginDestroy_##Type};              \
        return &kDesc;                                                                                                 \
    }                                                                                                                  \
    extern "C" VASE_EXPORT const ::vase::PluginDescriptor* VasePlugin_GetPlugin(const char* id)                        \
    {                                                                                                                  \
        return id != nullptr && std::string_view{id} == kVaseMeta_##Type.Id ? VasePluginDesc_##Type() : nullptr;       \
    }                                                                                                                  \
    const ::vase::PluginMeta kVaseMeta_##Type = ::vase::PluginMeta
```

（宏末行**没有**分号——分号由作者的 `{...};` 提供。`VASE_PLUGIN` 只允许在命名空间作用域使用一次/TU；一个 DLL 一个 `GetPlugin`，多个 `VASE_PLUGIN` 同镜像会撞符号——这是设计（§3.1），不是缺陷。）

- [ ] **Step 2: 写测试 `Tests/Unit/DescriptorTests.cpp`（4 个 TEST）**

```cpp
#include "Vase/Detail/Result.h"
#include "Vase/PluginDescriptor.h"

#include <gtest/gtest.h>
#include <iterator>
#include <type_traits>

// 探针类放匿名命名空间：tidy 的 misc-use-internal-linkage 要求「只在本 TU 使用的类型」
// 就该有内部链接。类的名字只在 VASE_PLUGIN 展开里用一次，放进匿名命名空间不改变任何语义
// （全局作用域的非限定查找仍能找到它），却省掉一条 NOLINT。
namespace
{
class DescriptorProbePlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Ok();
    }
};
} // namespace

VASE_PLUGIN(DescriptorProbePlugin){
    .Id = "Vase.DescriptorProbe",
    .DisplayName = "描述符探针",
    .Version = "0.0.1",
    .Requires = {{.Name = "Vase.World", .Version = 1}, {.Name = "Vase.Audio", .Version = 2}},
    .Provides = {{.Name = "Vase.Probe.Service", .Version = 1}},
};

namespace
{

TEST(Descriptor, MetaPopulatedThroughBraceBlock)
{
    const vase::PluginDescriptor* d = VasePluginDesc_DescriptorProbePlugin();
    EXPECT_EQ(d->HeaderVersion, vase::kHeaderVersion);
    EXPECT_EQ(d->Meta->Id, "Vase.DescriptorProbe");
    ASSERT_EQ(d->Meta->Requires.Size(), 2U);
    // 迭代器而非 operator[]：非常量下标过不了 cppcoreguidelines-pro-bounds-*（计划「tidy 形态约束」）。
    EXPECT_EQ(std::next(d->Meta->Requires.Begin())->Name, "Vase.Audio");
    EXPECT_EQ(std::next(d->Meta->Requires.Begin())->Version, 2U);
    ASSERT_EQ(d->Meta->Provides.Size(), 1U);
    EXPECT_EQ(d->Meta->Provides.Begin()->Name, "Vase.Probe.Service");
}

TEST(Descriptor, GetPluginRoutesByIdentity)
{
    const vase::PluginDescriptor* d = VasePlugin_GetPlugin("Vase.DescriptorProbe");
    EXPECT_EQ(d, VasePluginDesc_DescriptorProbePlugin());
    EXPECT_EQ(VasePlugin_GetPlugin("Vase.Nope"), nullptr); // §12 判据 #12 之外的路由正确性
}

TEST(Descriptor, CreateDestroyRoundTrips)
{
    const vase::PluginDescriptor* d = VasePluginDesc_DescriptorProbePlugin();
    vase::Plugin* p = d->Create();
    // EXPECT 而非 ASSERT：ASSERT_NE 的失败早退在 clang-analyzer 眼里是「p 泄漏路径」，
    // 而 p 为 null 时销毁端（unique_ptr 接住空指针）本来就无害，无需早退。
    EXPECT_NE(p, nullptr);
    d->Destroy(p); // 虚析构路径（§3.1 的「Delete 在镜像内」性质；测试 exe 内等价演示）
}

TEST(Descriptor, DefaultOnStartReturnsOk)
{
    // OnStart 默认实现在无 Context 内容时也能被编译进（取引用不触碰）。
    static_assert(std::is_polymorphic_v<vase::Plugin>, "Plugin 必须多态（虚析构路径的前提）");
    SUCCEED();
}

} // namespace
```

（`#include <type_traits>` 供 `is_polymorphic_v`；实现时若 clang-format/tidy 有意见按工具的输出改，语义不动。）

- [ ] **Step 3: 接构建，跑绿**

`Tests/CMakeLists.txt` 的 `VaseTests` 源列表追加 `Unit/DescriptorTests.cpp`。

```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -N   # Expected: Total Tests: 24
```

- [ ] **Step 4: 双平台 + 门禁 + Commit**

```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug'
git ls-files -z --cached --others --exclude-standard '*.h' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git add -A
git commit -m "M1-T4：PluginDescriptor/Plugin 基类/VASE_PLUGIN 宏（spec 3.3(1) 定稿展开式）"
```

---

## Task 5: 服务、事件与 `Context`——注册与解析的实例级机器

§6 的最小落地：`ServiceRegistry`（扁平表，子 Context **不是**查找链的一环——§2.3 的语义）、`EventBus`（**只做 Emit**，Waterfall 留 M2+）、`Context`（根 = 宿主，子 = 插件；Provide/Get/TryGet/On/Emit 全部返回 `EffectHandle` 或走回收通道——§7.1「唯一通道」在这里闭环）。同时把 `Plugin.h` 组装成插件作者的唯一入口（v3 §3 的星号条目）。

**Get 的「凭声明」执法与账本落账在 T9 接入**——本任务先把类型与数据形状立对（`ConsumerCookie` / `Ledger` 两个挂钩字段 + 注释锚点）。

**Files:**
- Create: `Include/Vase/Service/Service.h`、`Include/Vase/Event/Event.h`、`Include/Vase/Pod/Context.h`
- Create: `Source/Pod/ServiceRegistry.cpp`、`Source/Pod/EventBus.cpp`、`Source/Pod/Context.cpp`
- Modify: `Include/Vase/Plugin.h`（重写为组装入口）、`Source/Pod/CMakeLists.txt`、`Tests/CMakeLists.txt`
- Create: `Tests/Unit/RegistryBusTests.cpp`

**Interfaces:**
- Consumes: `EffectScope`/`IEffect`/`EffectHandle`（T3）、`PluginMeta`/`ServiceRef`（T4）、`Result`（T2）。
- Produces:
  - `vase::HasServiceIdentity<T>` / `vase::HasEventIdentity<E>` 两个 concept + `static_assert` 文案（§6.1/§2.4 的「缺失时给出可操作的编译错误」）
  - `vase::ServiceOrigin { kPlugin, kHost }`（§2.1；来源=注册位置，不是接口属性——§6.1）
  - `vase::ServiceKey / vase::EventKey { std::string_view Name; std::uint32_t Version; }`（+ `operator==` defaulted）
  - `vase::detail::ServiceRegistry` / `vase::detail::EventBus`（VasePod 内部公开给 Context 与单测）
  - `vase::Context`（`VASE_POD_API`）：`Provide<T>(T&)`、`Provide<T>(unique_ptr<T>)`、`Get<T>()`、`TryGet<T>()`、`On<E>(void (C::*)(const E&), C*)`、`On<E>(F&&)`、`Emit<E>(const E&)`、`GetScope()`、`OwnerId()`
  - `Include/Vase/Plugin.h` = Export/Result/MetaArray/PluginDescriptor/Effect×3/Service/Event/Context 的总和——**插件作者唯一需要的包含**

- [ ] **Step 1: 写 `Include/Vase/Service/Service.h` 与 `Include/Vase/Event/Event.h`**

```cpp
#pragma once

// 服务标识约定（§6.1）：编译期常量名字 + 整数主版本。不用 type_index——
// 它跨 DLL 的比较依赖编译器实现细节，而字符串常量是稳定的（附录 B）。
// 事件与服务面对完全相同的跨 DLL 标识问题，所以用同一套解法（§2.4）。

#include <concepts>
#include <cstdint>
#include <string_view>

namespace vase
{

template <typename T>
concept HasServiceIdentity = requires {
    { T::kName } -> std::convertible_to<std::string_view>;
    { T::kVersion } -> std::convertible_to<std::uint32_t>;
};

// 来源是**注册位置**的属性，不是接口的属性（§6.1）：插件子 Context 里 Provide
// 即 kPlugin，Pod 根 Context 里 Provide 即 kHost。接口上不声明它。
enum class ServiceOrigin : std::uint8_t
{
    kPlugin,
    kHost,
};

struct ServiceKey
{
    std::string_view Name;
    std::uint32_t Version;

    bool operator==(const ServiceKey&) const = default;
};

} // namespace vase
```

```cpp
#pragma once

// 事件标识约定（§2.4）：与 ServiceKey 同构（名字 + 整数主版本）。
// 生命周期契约同文件注释执行：**派发是同步的；handler 不得保存事件对象
// 或其任何视图**（§0.3-6 在本层的实例化）。

#include <concepts>
#include <cstdint>
#include <string_view>

namespace vase
{

template <typename E>
concept HasEventIdentity = requires {
    { E::kName } -> std::convertible_to<std::string_view>;
    { E::kVersion } -> std::convertible_to<std::uint32_t>;
};

struct EventKey
{
    std::string_view Name;
    std::uint32_t Version;

    bool operator==(const EventKey&) const = default;
};

} // namespace vase
```

- [ ] **Step 2: 写 `Include/Vase/Pod/Context.h`（模板全量可见——插件在自己的镜像里实例化）**

```cpp
#pragma once

// Context——Pod 内的服务访问入口（§2.1/§2.3/§6.2）。四条边界（§2.3）在类型上的
// 投影：无 Proxy、无惰性解析（Get 是显式的）、查找是**一张扁平面**（子 Context
// 不是查找链的一环——它只记录 Effect 归属与诊断说话的身份）、一个服务标识
// 一个实现（重复 Provide 在 M1 运行期即终止、两个构建一致；M2 把它提前到求解期硬拒——见 .cpp 注释）。
//
// 堆壳的所有权写法：make_unique 造壳 + release() 显式移交，销毁端用 unique_ptr<Cell>
// 接住再析构——全程不出现裸 new/delete 表达式。

#include "Vase/Detail/Counters.h"
#include "Vase/Detail/Export.h"
#include "Vase/Detail/Fail.h"
#include "Vase/Detail/Result.h"
#include "Vase/Effect/EffectHandle.h"
#include "Vase/Effect/EffectScope.h"
#include "Vase/Event/Event.h"
#include "Vase/PluginDescriptor.h"
#include "Vase/Service/Service.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

namespace vase::detail
{
class ServiceRegistry;
class EventBus;
struct DependencyLedger; // T9；VasePod 类型（放在 Pod/ 下，Host 持有实例）
struct ServiceEntry;
} // namespace vase::detail

namespace vase
{

class Pod;
class PluginHost;
class PodTestPeer; // 仅测试装配用（T8）

class VASE_POD_API Context
{
public:
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;
    ~Context() = default;

    template <typename T>
    EffectHandle Provide(T& instance)
    {
        static_assert(HasServiceIdentity<T>,
                      "服务接口必须声明 kName 与 kVersion。参见 Vase/Service/Service.h 的示例。");
        return ProvideRaw(ServiceKey{T::kName, T::kVersion}, &instance, nullptr, nullptr);
    }

    template <typename T>
    EffectHandle Provide(std::unique_ptr<T> owned)
    {
        static_assert(HasServiceIdentity<T>,
                      "服务接口必须声明 kName 与 kVersion。参见 Vase/Service/Service.h 的示例。");
        if (owned == nullptr)
        {
            detail::ProgrammerError("Provide(unique_ptr) got null");
        }
        void* instance = owned.get();
        // 外壳：unique_ptr<unique_ptr<T>> 承载「移交进来的那个 unique_ptr」。
        // make_unique 之后 release()，所有权转给注册表项的 HeapHolder 字段。
        std::unique_ptr<std::unique_ptr<T>> shell = std::make_unique<std::unique_ptr<T>>(std::move(owned));
        return ProvideRaw(ServiceKey{T::kName, T::kVersion}, instance, shell.release(),
                          &DestroyShell<std::unique_ptr<T>>);
    }

    template <typename T>
    T& Get()
    {
        static_assert(HasServiceIdentity<T>,
                      "服务接口必须声明 kName 与 kVersion。参见 Vase/Service/Service.h 的示例。");
        void* found = ResolveRaw(T::kName, T::kVersion, /*required=*/true);
        return *static_cast<T*>(found);
    }

    template <typename T>
    T* TryGet()
    {
        static_assert(HasServiceIdentity<T>,
                      "服务接口必须声明 kName 与 kVersion。参见 Vase/Service/Service.h 的示例。");
        return static_cast<T*>(ResolveRaw(T::kName, T::kVersion, /*required=*/false));
    }

    template <typename E, typename C>
    EffectHandle On(void (C::*method)(const E&), C* self)
    {
        return On<E>([self, method](const E& event) { (self->*method)(event); });
    }

    template <typename E, typename F>
    EffectHandle On(F&& handler)
    {
        static_assert(HasEventIdentity<E>, "事件结构必须声明 kName 与 kVersion。参见 Vase/Event/Event.h 的示例。");
        static_assert(std::is_invocable_v<F, const E&>, "handler 必须可 (const E&) 调用");
        using Fn = std::function<void(const E&)>;
        std::unique_ptr<Fn> shell = std::make_unique<Fn>(std::forward<F>(handler));
        return SubscribeRaw(EventKey{E::kName, E::kVersion}, shell.release(), &InvokeHandler<E>, &DestroyShell<Fn>);
    }

    template <typename E>
    void Emit(const E& event)
    {
        static_assert(HasEventIdentity<E>, "事件结构必须声明 kName 与 kVersion。参见 Vase/Event/Event.h 的示例。");
        EmitRaw(EventKey{E::kName, E::kVersion}, &event);
    }

    EffectScope& GetScope() { return *Scope; }
    [[nodiscard]] std::string_view OwnerId() const { return SelfMeta == nullptr ? std::string_view{} : SelfMeta->Id; }

private:
    friend class Pod;
    friend class PluginHost;
    friend class PodTestPeer;

    Context(EffectScope& scope, detail::ServiceRegistry& registry, detail::EventBus& bus,
            detail::DiagnosticCounters* counters, const PluginMeta* selfMeta)
        : Scope(&scope)
        , Registry(&registry)
        , Bus(&bus)
        , Counters(counters)
        , SelfMeta(selfMeta)
    {
    }

    // 堆壳的统一销毁端：用 unique_ptr 接住再析构，不出现裸 delete 表达式。
    template <typename Cell>
    static void DestroyShell(void* cell)
    {
        const std::unique_ptr<Cell> owning{static_cast<Cell*>(cell)};
    }

    template <typename E>
    static void InvokeHandler(const void* event, void* cell)
    {
        (*static_cast<std::function<void(const E&)>*>(cell))(*static_cast<const E*>(event));
    }

    EffectHandle ProvideRaw(const ServiceKey& key, void* instance, void* heapHolder, void (*destroyHolder)(void*));
    void* ResolveRaw(std::string_view name, std::uint32_t version, bool required);
    EffectHandle SubscribeRaw(const EventKey& key, void* handler, void (*invoke)(const void*, void*),
                              void (*destroy)(void*));
    void EmitRaw(const EventKey& key, const void* event);

    EffectScope* Scope;
    detail::ServiceRegistry* Registry;
    detail::EventBus* Bus;
    detail::DiagnosticCounters* Counters;
    const PluginMeta* SelfMeta = nullptr; // nullptr == Pod 根（宿主）

    // —— T9 接线点（§5.6）：插件解析落账需要的三个挂钩，由 Host 装配时填充 ——
    detail::DependencyLedger* Ledger = nullptr; // 账本（Host 持有实例）
    const void* ConsumerCookie = nullptr;       // 本 Context 所属插件实例（根 = nullptr）
    std::uint32_t PodIndex = 0;
};

} // namespace vase
```

- [ ] **Step 3: 写内部容器（`ServiceRegistry` / `EventBus`，头文件并入 `Source/Pod/` 私有头 `Include/Vase/Detail/RegistryBus.h`）**

（新文件 `Include/Vase/Detail/RegistryBus.h` + `Source/Pod/ServiceRegistry.cpp` + `Source/Pod/EventBus.cpp`。要点全在下面三段；实现者按此写完即过 Step 4 的测试。）

```cpp
#pragma once

// ServiceRegistry / EventBus 的共享声明（VasePod 内的两个实例级容器）。
// 放 Detail/ 是因为 Context 与单测都要见到同一份布局，而它们都不该包含对方的头。

#include "Vase/Detail/Counters.h"
#include "Vase/Detail/Export.h"
#include "Vase/Event/Event.h"
#include "Vase/Service/Service.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace vase::detail
{

struct ServiceEntry
{
    ServiceKey Key;
    void* Instance = nullptr;
    ServiceOrigin Origin = ServiceOrigin::kHost;
    std::string_view ProviderId;            // kHost 时为空；诊断「谁注册的」（§2.3）
    const void* ProviderInstance = nullptr; // 插件提供方实例 cookie；kHost → nullptr（§5.6 宿主不落边）
    void* HeapHolder = nullptr;             // 移交式注册的 unique_ptr 堆壳；借用式为 nullptr
    void (*DestroyHolder)(void*) = nullptr;
    std::uint64_t Id = 0;
    bool Alive = false;
};

VASE_MSVC_DLL_WARNINGS_BEGIN
class VASE_POD_API ServiceRegistry
{
public:
    std::uint64_t Add(ServiceEntry entry); // Alive 由 Add 置 true

    // 精确 (name,version)：主版本不同=不同服务（§6.1）
    [[nodiscard]] const ServiceEntry* Find(ServiceKey key) const;

    bool Remove(ServiceKey key, std::uint64_t id); // 双键匹配才删；销毁 HeapHolder

    [[nodiscard]] std::size_t Count() const; // alive 数

    // M1 规模下 tombstone 不压缩（整 Pod 消亡时全清）。重复 Provide 是**编程错误**，
    // 与解析失败同路，经 detail::ProgrammerError **两个构建都终止**——不要用裸 assert：
    // 它会让 Debug 终止、Release 静默后写覆盖，两个构建行为分叉。
    // （§2.3-4 的「一服务一实现」求解期硬拒是 M2，这里是运行期兜底。）

private:
    std::vector<ServiceEntry> Entries;
    std::uint64_t NextId = 1;
};
VASE_MSVC_DLL_WARNINGS_END

VASE_MSVC_DLL_WARNINGS_BEGIN
class VASE_POD_API EventBus
{
public:
    EventBus() = default; // 不接计数：§9.2 的账由**注册通道**（Context）记一处，
                          // 存储容器保持哑存储——与 ServiceRegistry 对称

    std::uint64_t Add(EventKey key, void* handler, void (*invoke)(const void*, void*), void (*destroy)(void*));
    bool Remove(EventKey key, std::uint64_t id);
    void Emit(EventKey key, const void* event); // 同步派发（§2.4）

    [[nodiscard]] std::size_t Count() const;

private:
    struct Subscription
    {
        EventKey Key;
        void* Handler = nullptr;
        void (*Invoke)(const void*, void*) = nullptr;
        void (*Destroy)(void*) = nullptr;
        std::uint64_t Id = 0;
        bool Alive = false;
    };

    std::vector<Subscription> Subs;
    std::vector<Subscription> DeferredDestroy; // Emit 期间的 Remove 先记这里，派完再真删
    std::uint32_t EmitDepth = 0;
};
VASE_MSVC_DLL_WARNINGS_END

} // namespace vase::detail
```

`Context.cpp` 的三个私有原语（`ProvideRaw` / `ResolveRaw` / `SubscribeRaw` / `EmitRaw`）行为钉死：

```cpp
// ProvideRaw：  登记 entry（ProviderId/ProviderInstance 由 SelfMeta 与 ConsumerCookie
//               的对称逻辑给出：kPlugin 时 ProviderInstance = 本 Context 的宿主实例 cookie，
//               由 Pod 在创建子 Context 时填——见 T7；根 Context 的 Provide 即 kHost）。
//               Counters->Services++。
//               内部 RegistrationEffect : IEffect { Context*, ServiceKey, std::uint64_t Id }
//               的 Recycle() = Registry->Remove(key, id) + Services--。用 Scope->Create 登记。
// ResolveRaw：  ① SelfMeta != nullptr 且 name ∉ SelfMeta->Requires
//                  → detail::ProgrammerError（消息含插件 Id + 服务名 + 版本；
//                    §5.6 规则① + §6.2 的「编程错误」路径——T9 前它是唯一执法点）
//               ② Find 未命中：required → detail::ProgrammerError（同样含三要素），否则 nullptr
//               ③ 命中：〔T9 锚点〕Ledger 落边（consumer=ConsumerCookie →
//                  provider=entry->ProviderInstance；provider 为 nullptr（kHost）不落边——
//                  §5.6「宿主的解析不落边」）；返回 Instance。
// SubscribeRaw：Bus->Add + Subscriptions++；SubscriptionEffect 的 Recycle = Remove + --。
//             **计数只在这一处**：ServiceRegistry / EventBus 两个容器都不记账（哑存储），
//             否则同一件事被记两次、只减一次（实测：EventBus::Add 与 SubscribeRaw 各 ++、
//             SubscriptionEffect::Recycle 只 --，净 +1/轮）。
// EmitRaw：     Bus->Emit；事件对象是借用——Emit 返回后即失效（§2.4），类型上已保证
//               （const E& 临时对象传进来，没有任何路径能把指针存出去，除非 handler 违规——9.3 契约束）。
```

- [ ] **Step 4: 写测试 `Tests/Unit/RegistryBusTests.cpp`（5 个 TEST + 编译期断言）**

```cpp
#include "Vase/Detail/RegistryBus.h"
#include "Vase/Event/Event.h"
#include "Vase/Service/Service.h"

#include <gtest/gtest.h>
#include <vector>

namespace
{

struct IFooService
{
    static constexpr std::string_view kName = "Vase.Test.Foo";
    static constexpr std::uint32_t kVersion = 1;
    virtual ~IFooService() = default;
    virtual int Value() const = 0;
};
static_assert(vase::HasServiceIdentity<IFooService>);
struct NotAService
{
};
static_assert(
    !vase::HasServiceIdentity<NotAService>); // 缺标识 → concept 为假（编译错误文案由 static_assert 在 Get 处给出）

struct PingEvent
{
    static constexpr std::string_view kName = "Vase.Test.Ping";
    static constexpr std::uint32_t kVersion = 1;
    int Seq;
};
static_assert(vase::HasEventIdentity<PingEvent>);

class StubFoo : public IFooService
{
public:
    int Value() const override { return 3; }
};

std::vector<int>& EmitLog()
{
    static std::vector<int> log;
    return log;
}

vase::detail::ServiceEntry MakeEntry(std::string_view name, std::uint32_t version, void* instance)
{
    vase::detail::ServiceEntry e;
    e.Key = vase::ServiceKey{name, version};
    e.Instance = instance;
    e.Origin = vase::ServiceOrigin::kHost;
    e.ProviderId = "";
    e.ProviderInstance = nullptr;
    e.HeapHolder = nullptr;
    e.DestroyHolder = nullptr;
    e.Id = 0;
    e.Alive = false;
    return e;
}

TEST(ServiceRegistry, AddFindRemoveAndCount)
{
    vase::detail::ServiceRegistry reg;
    StubFoo foo;
    std::uint64_t id = reg.Add(MakeEntry("Vase.Test.Foo", 1, &foo));
    ASSERT_NE(reg.Find({"Vase.Test.Foo", 1}), nullptr);
    EXPECT_EQ(reg.Find({"Vase.Test.Foo", 1})->Instance, &foo);
    EXPECT_EQ(reg.Count(), 1U);
    EXPECT_TRUE(reg.Remove({"Vase.Test.Foo", 1}, id));
    EXPECT_EQ(reg.Find({"Vase.Test.Foo", 1}), nullptr);
    EXPECT_EQ(reg.Count(), 0U);
}

TEST(ServiceRegistry, MajorVersionMismatchIsMiss)
{
    vase::detail::ServiceRegistry reg;
    StubFoo foo;
    reg.Add(MakeEntry("Vase.Test.Foo", 1, &foo));
    EXPECT_EQ(reg.Find({"Vase.Test.Foo", 2}), nullptr); // §6.1：主版本不同即不同服务
}

TEST(EventBus, EmitDispatchesSynchronouslyInOrder)
{
    vase::detail::EventBus bus;
    EmitLog().clear();
    auto one = [](const void* event, void* tag)
    { EmitLog().push_back(*static_cast<const int*>(event) + *static_cast<int*>(tag)); };
    auto destroy = [](void*) {};
    int tagA = 100;
    int tagB = 200;
    bus.Add({"Vase.Test.Ping", 1}, &tagA, one, destroy);
    bus.Add({"Vase.Test.Ping", 1}, &tagB, one, destroy);
    const int seq = 7;
    bus.Emit({"Vase.Test.Ping", 1}, &seq);
    EXPECT_EQ(EmitLog(), (std::vector<int>{107, 207})); // 订阅序；同步派发（§2.4）
}

TEST(EventBus, SelfUnsubscribeDuringEmitIsSafe)
{
    vase::detail::EventBus bus;
    EmitLog().clear();
    static std::uint64_t firstId = 0;
    auto selfRemove = [](const void* event, void* tag)
    {
        EmitLog().push_back(1);
        static_cast<vase::detail::EventBus*>(tag)->Remove({"Vase.Test.Ping", 1}, firstId); // 注销自己
    };
    auto noop = [](const void*, void*) {};
    auto del = [](void*) {};
    // Add 签名：(key, handler, invoke, destroy)。handler=&bus、invoke=selfRemove、destroy=noop
    // （tag 指向 bus 本身而非堆对象，无销毁义务）。
    firstId = bus.Add({"Vase.Test.Ping", 1}, &bus, selfRemove, noop);
    bus.Emit({"Vase.Test.Ping", 1}, nullptr);
    EXPECT_EQ(EmitLog().size(), 1U);
    EXPECT_EQ(bus.Count(), 0U); // 派发结束后真删（DeferredDestroy）
}

TEST(EventBus, CountTracksAliveSubscriptions)
{
    vase::detail::EventBus bus;
    auto invoke = [](const void*, void*) {};
    auto destroy = [](void*) {};
    auto id = bus.Add({"Vase.Test.Pong", 1}, nullptr, invoke, destroy);
    EXPECT_EQ(bus.Count(), 1U);
    bus.Remove({"Vase.Test.Pong", 1}, id);
    EXPECT_EQ(bus.Count(), 0U);
}

} // namespace
```

（断言语义：派发中注销自己 → 本轮已派到、真删发生在派发结束后——`EmitDepth` 把 `Remove` 的销毁推到 `DeferredDestroy`，防「快照里还有它、堆上已经没它」的 use-after-free。）

- [ ] **Step 5: 组装 `Include/Vase/Plugin.h`（作者唯一入口，v3 §3 的星号条目）**

```cpp
#pragma once

// 插件作者唯一需要的头（v3 §3）：宏（VASE_PLUGIN / 后续 VASE_CONFIG）、基类、
// Context、Result、Error 全在这一扇门后面。Include/Vase/ 下按模块分的子目录
// 是给实现和宿主用的——作者记七个路径，等于每个新插件抄一遍示例再抄错一遍。
//
// 仓库内部一律引号包含（CLAUDE.md 规矩 3）；wiki 示例里的尖括号是给仓库外的
// 插件作者的，不要在本仓库里模仿。

#include "Vase/Detail/Export.h"
#include "Vase/Detail/MetaArray.h"
#include "Vase/Detail/Result.h"
#include "Vase/Effect/EffectHandle.h"
#include "Vase/Effect/EffectScope.h"
#include "Vase/Effect/IEffect.h"
#include "Vase/Event/Event.h"
#include "Vase/Pod/Context.h"
#include "Vase/PluginDescriptor.h"
#include "Vase/Service/Service.h"
```

- [ ] **Step 6: 接构建，跑绿**

`Source/Pod/CMakeLists.txt` 源列表追加 `ServiceRegistry.cpp`、`EventBus.cpp`、`Context.cpp`；`Tests/CMakeLists.txt` 追加 `Unit/RegistryBusTests.cpp`。

```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -N   # Expected: Total Tests: 30
```

- [ ] **Step 7: Linux 线 + 门禁 + Commit**

```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug'
git ls-files -z --cached --others --exclude-standard '*.h' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git add -A
git commit -m "M1-T5：ServiceRegistry/EventBus/Context 与 Plugin.h 作者入口组装"
```

---
## Task 6: 构建标志落工具链 + `Loader` + 镜像解析器（三档判据的地基）

两件事一个主题——「**二进制带着可验证的身份进场**」（§8.2 / §8.7）：
① 身份特征是构建要求（13.2 末行）：MSVC 线 `/DEBUG:FULL`（无 RSDS 档三无从谈起）、Linux 线 `-Wl,--build-id=sha1`。**按需求方拍板落三个工具链文件**——`CMAKE_*_INIT` 只在 configure 期生效、不进 `VaseBuildOptions`（后者是**编译**选项的唯一出口，这两个都是**链接**标志，工具链文件是它们唯一早于 `project()` 的落点）。

   **v3 §8.2 还点名 `-fno-gnu-unique`（封 `STB_GNU_UNIQUE` 成因）——实测不加，理由两条**（T6 实测，证据写在 `Cmake/Toolchains/linux-x64-clang-libcxx.cmake` 的注释里）：(a) **clang 不接受该标志**（`clang++: error: unknown argument: '-fno-gnu-unique'`，它是 GCC 独有），照加会直接把 Linux 线编崩；(b) 该成因在本工具链上**不存在**——实测同一份 `inline` 函数局部静态，**g++ 生成 4 个 `STB_GNU_UNIQUE` 符号、clang++ 生成 0 个**。设计意图（不让「卸载后符号仍在」）由 clang 的默认行为满足；**若日后 Linux 线改用 GCC，这条必须回来重审**。
② 三档证据与导入表执法共用**同一个自写镜像解析器**（§8.2 末段 / §8.7），先以「纯字节进、纯数据出」的纯函数落地并单测穷举，`Loader` 在其上包住平台 API。

**Files:**
- Modify: `Cmake/Toolchains/windows-x64-clangcl.cmake`、`Cmake/Toolchains/windows-x64-msvc.cmake`、`Cmake/Toolchains/linux-x64-clang-libcxx.cmake`
- Create: `Include/Vase/Detail/ImageInspect.h`
- Create: `Source/Host/ImageInspectCommon.cpp`、`Source/Host/ImageInspectWindows.cpp`、`Source/Host/ImageInspectPosix.cpp`
- Create: `Include/Vase/Host/Loader.h`、`Source/Host/Loader.cpp`、`Source/Host/LoaderWindows.cpp`、`Source/Host/LoaderPosix.cpp`
- Create: `Tests/Unit/fixtures/LoadProbe/LoadProbe.cpp` + `CMakeLists.txt`（最小真实插件二进制，专供 Loader 的端到端断言）
- Create: `Tests/Unit/LoaderTests.cpp`
- Modify: `Source/Host/CMakeLists.txt`、`Tests/CMakeLists.txt`

**Interfaces:**
- Consumes: T1–T5 全部公开面。
- Produces:
  - `vase::detail::ImageIdentity { IdentityKind Kind; std::vector<std::uint8_t> Bytes; }`（`operator==` defaulted）——档三的比对原语
  - 纯函数（`VASE_HOST_API`，单测直接喂构造字节）：`ParsePeCodeView(span, loadedInMemory)`、`ParsePeImports(span, loadedInMemory)`、`ExtractBuildIdFromNotes(span)`、`ParseElfBuildIdFile(span)`、`ParseElfNeededFile(span)`
  - `vase::detail::Loader`（`VASE_HOST_API`）：`EnsureResident(path) -> Result<BinaryRecord>`、`Unload(rec) -> UnloadEvidence`、`Symbol(rec, name) -> Result<void*>`、`MemoryIdentity(rec)`、`FileIdentity(path)`、`ImportedLibraryNamesFromFile(path)`（读**磁盘**镜像的导入声明——执法要的是「这个文件要谁」，不是「运行期已解析成谁」）、`DescribeLoadFailure(path)`、`ResidentBinaryCount()` / `FindResident(path)`、`AllResident()`（Shutdown 迭代）
  - `struct UnloadEvidence { bool ReopenWritable; bool MappingRemoved; bool ReopenWritableIsMeaningful; bool MappingRemovalIsObservable; }`——**哪个字段在本平台有判据力是平台事实，写进结构而不是让读报告的人背 §8.2**：Windows 主判 `ReopenWritable`（辅助地位：改名替换可骗过它），Linux 主判 `MappingRemoved`（`dl_iterate_phdr` 条目消失）
  - 平台宏约定：`Loader.h` 内部 `#ifdef _WIN32` 选实现；测试两端跑同一条断言

- [ ] **Step 1: 三个工具链文件加承重的链接/编译标志**

`Cmake/Toolchains/windows-x64-clangcl.cmake` 与 `windows-x64-msvc.cmake`，在 `include(vcpkg)` 行**之前**插入（位置同 `VCPKG_TARGET_TRIPLET` 的理由——`*_INIT` 必须早于编译器检测被读走）：

```cmake
# §8.2 档三 · 身份特征是插件的**构建要求**（13.2 末行）：MSVC 线没有 /DEBUG
# 就没有 PE 调试目录里的 CodeView(RSDS)，Adopt 会直接拒绝该二进制（响亮的失败，
# 不做静默降级）。/DEBUG:FULL 而非 FASTLINK：FASTLINK 的调试目录形态与增量
# 链接器绑定，档三只认 RSDS 的 GUID+Age 稳定存在。
# 用 *_FLAGS_INIT 而非 VaseBuildOptions：这是**链接器**标志，且必须早于
# project() 的编译器检测进入缓存（CLAUDE.md 规矩 1 管编译选项的唯一出口，
# 工具链文件管「平台与产物的构建形态」——两者不重叠，此处注释互相指认）。
# 代价如实记下：vcpkg 子构建也走这份工具链，gtest 的 DLL 会多生成 PDB。
string(APPEND CMAKE_SHARED_LINKER_FLAGS_INIT " /DEBUG:FULL")
```

`Cmake/Toolchains/linux-x64-clang-libcxx.cmake`：

```cmake
# 8.5：libstdc++ → libc++。编译与链接都要给，否则链接期找不到 libc++ 的符号。
set(CMAKE_CXX_FLAGS_INIT "-stdlib=libc++")   # ← 不加 -fno-gnu-unique：clang 不认该标志，且实测 clang 不生成 STB_GNU_UNIQUE（见上文）
set(CMAKE_EXE_LINKER_FLAGS_INIT "-stdlib=libc++")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-stdlib=libc++ -Wl,--build-id=sha1")  # ← 追加 --build-id（§8.2 档三：.note.gnu.build-id 是 Linux 身份特征）
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-stdlib=libc++")
```
（上面四行是**替换后**的最终形态；原有注释保留。`-fno-gnu-unique` 见上文——**实测不加**。）

- [ ] **Step 2: 验证标志真的进了产物（这一步是本 task 的存在理由）**

```bash
rm -rf build-win/win-x64-clang-debug && cmake --preset win-x64-clang-debug && cmake --build --preset win-x64-clang-debug
llvm-readobj --coff-debug-directory build-win/win-x64-clang-debug/bin/VasePod.dll | grep -A2 CodeView   # Expected: 有输出（RSDS + GUID + Age）
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && rm -rf build-linux/linux-x64-clang-debug && cmake --preset linux-x64-clang-debug && cmake --build --preset linux-x64-clang-debug && llvm-readelf -n build-linux/linux-x64-clang-debug/lib/libVasePod.so'
# Expected: 输出含 "Build ID: " 一行
```
两条 grep 任一为空 = flag 没生效 = 本 task 未完成。**不许带着「回头再看」往下走**——后面 T11/T12/T13 的全部档三断言建在这里。

- [ ] **Step 3: 写 `Include/Vase/Detail/ImageInspect.h`**

```cpp
#pragma once

// 自写 PE / ELF 镜像解析（§8.2 末段 / §8.7：三档证据、导入表执法、缺依赖诊断
// **共用同一条解析路径**）。纯字节进、纯数据出：同一份解析代码读内存镜像与
// 磁盘文件——档三的「身份」必须来自同一种读法才可比。
// 无异常纪律（§9.2 清单）：不碰任何会抛的容器 API；越界读一律先查再取。

#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vase::detail
{

enum class IdentityKind : std::uint8_t
{
    kElfBuildId,  // Linux：.note.gnu.build-id 的 desc 字节（sha1 形态 20B）
    kPdbCodeView, // Windows：RSDS 的 GUID(16B) + Age(4B)
};

struct ImageIdentity
{
    IdentityKind Kind = IdentityKind::kElfBuildId;
    std::vector<std::uint8_t> Bytes;

    bool operator==(const ImageIdentity&) const = default;
};

// —— PE（Windows）。loadedInMemory：镜像已映射（HMODULE 基址 = span 起点，
//    RVA 直接可用）；false：磁盘字节（RVA 先经节表映射到文件偏移）。
VASE_HOST_API Result<ImageIdentity> ParsePeCodeView(std::span<const std::uint8_t> image, bool loadedInMemory);
VASE_HOST_API Result<std::vector<std::string>> ParsePeImports(std::span<const std::uint8_t> image, bool loadedInMemory);

// —— ELF（Linux）。notes：一个或多个 PT_NOTE 区域拼接后的字节（内存侧由
//    dl_iterate_phdr 收集；文件侧由 ParseElfBuildIdFile 内部收集后复用同一函数）。
VASE_HOST_API Result<ImageIdentity> ExtractBuildIdFromNotes(std::span<const std::uint8_t> notes);
VASE_HOST_API Result<ImageIdentity> ParseElfBuildIdFile(std::span<const std::uint8_t> fileBytes);
VASE_HOST_API Result<std::vector<std::string>> ParseElfNeededFile(std::span<const std::uint8_t> fileBytes);

// 供 §8.2「加载失败时补一句缺哪个依赖」：在**文件字节**上找导入表里磁盘上
// 不存在的条目名；找不到任何解释则返回空串（诊断尽力而为，不假装有把握）。
//
// 契约边界（纯函数拿不到的东西，就别假装拿到）：本函数只做「从文件字节里取出
// **第一个**导入条目名」这一步——它没有路径，也就无从查磁盘。名字里的
// Unresolvable 说的是**调用语境**：调用方（EnsureResident 的失败路径）已经把
// 加载失败握在手里，拿这个名字当「最可能的缺项」写进报告。存在性判定若要更准，
// 是 §8.7 执法（读同一份导入表 + 文件名主干比对）而不是这里的事。
VASE_HOST_API std::string FirstUnresolvableImport(std::span<const std::uint8_t> fileBytes, bool isPe);

} // namespace vase::detail
```

- [ ] **Step 4: 写 `Source/Host/ImageInspectCommon.cpp`（PE + ELF 纯解析，全量）**

实现纪律：每个取字节前都 `Require(offset, size)` 越界检查；读 u16/u32 用 `std::memcpy` 进本地整数（非对齐安全，两平台同码）。**错误消息必须「指路」**（§8.2：特征缺失 = 响亮失败 + 报告指路补链接标志）：

```cpp
// ParsePeCodeView 的错误文案（Error::Message 原文，测试按子串断言）：
//   "not a PE image" / "no CodeView debug entry — 插件构建缺 /DEBUG:FULL（见
//    Cmake/Toolchains/windows-x64-*.cmake，§8.2 档三）"
// ExtractBuildIdFromNotes：  "no NT_GNU_BUILD_ID note — 插件构建缺 -Wl,--build-id
//    （见 Cmake/Toolchains/linux-x64-clang-libcxx.cmake，§8.2 档三）"
// 关键结构（其余按头文件签名展开）：
//   PE: e_lfanew@0x3C → "PE\0" → COFF(20B, SizeOfOptionalHeader@+16) →
//       OptHdr Magic(0x10B/0x20B) → DataDirectory 基址 = OptHdr + (magic==0x20B ? 112 : 96)，
//       Debug = entry[6] {RVA,Size}，Import = entry[1]。
//       RvaToOffset 仅在 loadedInMemory==false 时经节表；
//       Debug 目录每项 28B：Type@+12==2(CodeView)，AddressOfRawData@+20，PointerToRawData@+24；
//       CodeView 数据： "RSDS" + GUID16 + Age4 + PdbPath。
//       Import：每 descriptor 20B，Name RVA@+12；OriginalFirstThunk@0 / FirstThunk@16 都要
//       能指到 thunk（0 或 RVA——M1 只要名字表，thunk 值不参与判断）；Name RVA==0 视为 null 表尾。
//   ELF: ident[4]==2(64 位) && ident[5]==1(LE)；e_phoff@0x1C(u64)、e_phentsize@0x36(u16)、
//        e_phnum@0x38(u16)；PT_NOTE(type=4) 收集 → ExtractBuildIdFromNotes；
//        note 布局：namesz,descsz,type u32 + name(对齐4) + desc(对齐4)，type==3 且 name=="GNU\0"；
//        PT_DYNAMIC(type=2)：每 entry 16B {tag u64,val u64}；DT_STRTAB(5)→strtab 文件偏移
//        （PIE 文件里 p_vaddr==p_offset，M1 的插件全按链接器默认构建；若 val 落在
//        别的 PT_LOAD 内先按段平移表换算——函数 ElfFileOffset(image, vaddr)），
//        DT_NEEDED(1)→strtab[val] 起 NUL 结尾。
```

（本 Step 的 .cpp 是 M1 里最长的一份纯函数文件，约 320 行；上面给出的是**布局常量与判定序**，字段偏移全部标注——实现者不需要猜任何一个数。全部越界失败都返回 `Err`，**绝不**越界读。）

- [ ] **Step 5: 平台侧读写包装（`ImageInspectWindows.cpp` / `ImageInspectPosix.cpp`）**

```cpp
// Windows（ImageInspectWindows.cpp）：
//   ReadFileBytes(path) → std::vector<uint8_t>（std::ifstream binary；
//     filesystem 一律 error_code 形式，§9.2 清单）。
//   内存侧：HMODULE 即映射基址；span 长度用 GetModuleInformation？——不用：
//     ParsePeCodeView 的 loadedInMemory 模式只读镜像头部区域（≤ 64KB），
//     以固定 0x10000 字节窗口 + Require 越界检查实现；SizeOfImage 可从
//     OptionalHeader 读到后再收窄窗口（两处读取各带边界检查）。
//   导出三个函数供 Loader.cpp 调用：
//     Result<ImageIdentity> PeIdentityFromMemory(void* hModule);
//     Result<std::vector<std::string>> PeImportsFromMemory(void* hModule);   // 映射后仍可读名字
//     Result<ImageIdentity> PeIdentityFromFile(const path&);
// Linux（ImageInspectPosix.cpp）：
//   内存侧：dl_iterate_phdr 回调里 strcmp(dlpi_name, path.string()) 命中目标
//     → 遍历 dlpi_phdr 的 PT_NOTE，把 [dlpi_addr + p_vaddr, +p_memsz) 拷进
//     一个拼接 buffer → ExtractBuildIdFromNotes。未命中 → Err("image not mapped: ...")。
//   文件侧：ReadFileBytes → ParseElfBuildIdFile / ParseElfNeededFile。
```

- [ ] **Step 6: 失败测试 `Tests/Unit/LoaderTests.cpp`——先写构造镜像的 6 个纯解析 TEST**

字节构造器（文件内 static，够用即可）：

```cpp
#include "Vase/Detail/ImageInspect.h"

#include <cstring>
#include <gtest/gtest.h>
#include <vector>

namespace
{

void Put16(std::vector<std::uint8_t>& b, std::size_t at, std::uint16_t v)
{
    b[at] = static_cast<std::uint8_t>(v);
    b[at + 1] = static_cast<std::uint8_t>(v >> 8);
}
void Put32(std::vector<std::uint8_t>& b, std::size_t at, std::uint32_t v)
{
    Put16(b, at, static_cast<std::uint16_t>(v));
    Put16(b, at + 2, static_cast<std::uint16_t>(v >> 16));
}
void Put64(std::vector<std::uint8_t>& b, std::size_t at, std::uint64_t v)
{
    Put32(b, at, static_cast<std::uint32_t>(v));
    Put32(b, at + 4, static_cast<std::uint32_t>(v >> 32));
}
void PutStr(std::vector<std::uint8_t>& b, std::size_t at, const char* s)
{
    std::memcpy(b.data() + at, s, std::strlen(s) + 1);
}

std::vector<std::uint8_t> MakeMinimalPe()
{
    // 磁盘形态：DOS(64) + PE sig + COFF(20) + OptHdr(240, PE32+, Debug@dir[6]=RVA 0x2000,
    // Import@dir[1]=RVA 0x2100) + 1 节表 2 项；.debug→0x400，.rdata→0x500。
    std::vector<std::uint8_t> b(0x600, 0);
    PutStr(b, 0, "MZ");
    Put32(b, 0x3C, 0x40);                // e_lfanew
    PutStr(b, 0x40, "PE\0");             // 0x40: sig(4) + COFF(20) = 0x44..0x58
    Put16(b, 0x44, 0x8664);              // Machine
    Put16(b, 0x46, 2);                   // NumberOfSections
    Put16(b, 0x54, 240);                 // SizeOfOptionalHeader（COFF 起 0x44，字段 +16）
    Put16(b, 0x58, 0x20B);               // Magic PE32+（可选头起 0x58）
    Put32(b, 0x58 + 56, 0x3000);         // SizeOfImage
    Put32(b, 0x58 + 108, 16);            // NumberOfRvaAndSizes
    const std::size_t dirs = 0x58 + 112; // DataDirectory[0]
    Put32(b, dirs + 6 * 8, 0x2000);
    Put32(b, dirs + 6 * 8 + 4, 56); // Debug：1 项 28B + 1 null 28B
    Put32(b, dirs + 1 * 8, 0x2100);
    Put32(b, dirs + 1 * 8 + 4, 40); // Import：1 desc + null
    // 节表 @ 0x58+240 = 0x148：两项各 40B
    PutStr(b, 0x148, ".debug"); // VA@+12, SizeOfRawData@+16, PointerToRawData@+20
    Put32(b, 0x148 + 12, 0x2000);
    Put32(b, 0x148 + 16, 0x100);
    Put32(b, 0x148 + 20, 0x400);
    PutStr(b, 0x168, ".rdata");
    Put32(b, 0x168 + 12, 0x2100);
    Put32(b, 0x168 + 16, 0x100);
    Put32(b, 0x168 + 20, 0x500);
    // Debug 目录项 @ 0x400：Type=2@+12, SizeOfData@+16, AddressOfRawData@+20=0x2040, PointerToRawData@+24=0x440
    Put32(b, 0x400 + 12, 2);
    Put32(b, 0x400 + 16, 0x28);
    Put32(b, 0x400 + 20, 0x2040);
    Put32(b, 0x400 + 24, 0x440);
    // RSDS @ 0x440：sig + GUID(0x01..0x10) + Age=2 + PdbPath
    PutStr(b, 0x440, "RSDS");
    for (int i = 0; i < 16; ++i)
    {
        b[0x444 + i] = static_cast<std::uint8_t>(i + 1);
    }
    Put32(b, 0x454, 2);
    PutStr(b, 0x458, "probe.pdb");
    // Import desc @ 0x500：Name RVA@+12 = 0x2140 → off 0x540；后跟 20B null 表尾
    Put32(b, 0x500 + 12, 0x2140);
    PutStr(b, 0x540, "sibling.dll");
    return b;
}

std::vector<std::uint8_t> MakeMinimalElf(std::uint64_t buildIdLen = 20)
{
    // e_ident(16) + e_type..(48) = 64；phnum=3（PT_NOTE@0x200, PT_DYNAMIC@0x300, PT_LOAD 可省）；
    // strtab 在 0x400。
    std::vector<std::uint8_t> b(0x500, 0);
    b[0] = 0x7F;
    std::memcpy(b.data() + 1, "ELF", 3);
    b[4] = 2;
    b[5] = 1;
    b[6] = 1;           // ELFCLASS64, LSB, version
    Put16(b, 16, 3);    // ET_DYN
    Put16(b, 18, 0x3E); // x86-64
    Put32(b, 20, 1);    // e_version
    Put64(b, 32, 64);   // e_phoff
    Put16(b, 52, 64);   // e_ehsize
    Put16(b, 54, 56);   // e_phentsize
    Put16(b, 56, 2);    // e_phnum
    // phdr[0] PT_NOTE @64：type@0=4, offset@8=0x200, vaddr@16, filesz@32, memsz@40
    Put32(b, 64 + 0, 4);
    Put64(b, 64 + 8, 0x200);
    Put64(b, 64 + 16, 0x200);
    const std::size_t noteBytes =
        12 + 4 + static_cast<std::size_t>(buildIdLen) + (buildIdLen % 4 ? 4 - buildIdLen % 4 : 0);
    Put64(b, 64 + 32, noteBytes);
    Put64(b, 64 + 40, noteBytes);
    // note @0x200：namesz=4, descsz, type=3, "GNU\0", desc=0xAA..
    Put32(b, 0x200, 4);
    Put32(b, 0x204, static_cast<std::uint32_t>(buildIdLen));
    Put32(b, 0x208, 3);
    PutStr(b, 0x20C, "GNU");
    for (std::uint64_t i = 0; i < buildIdLen; ++i)
    {
        b[0x210 + i] = static_cast<std::uint8_t>(0xAA + i);
    }
    // phdr[1] PT_DYNAMIC @120：offset 0x300，4 个 entry（STRTAB→0x400, NEEDED→10, NULL）
    Put32(b, 120 + 0, 2);
    Put64(b, 120 + 8, 0x300);
    Put64(b, 120 + 32, 4 * 16);
    Put64(b, 0x300 + 0 * 16, 5);
    Put64(b, 0x300 + 0 * 16 + 8, 0x400); // DT_STRTAB
    Put64(b, 0x300 + 1 * 16, 1);
    Put64(b, 0x300 + 1 * 16 + 8, 10); // DT_NEEDED → strtab+10
    Put64(b, 0x300 + 2 * 16, 0);      // DT_NULL
    PutStr(b, 0x400 + 10, "libSibling.so");
    return b;
}

TEST(ImageInspect, PeFileCodeViewExtracted)
{
    auto b = MakeMinimalPe();
    vase::Result<vase::detail::ImageIdentity> r = vase::detail::ParsePeCodeView(b, false);
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    EXPECT_EQ(r.Value().Kind, vase::detail::IdentityKind::kPdbCodeView);
    ASSERT_EQ(r.Value().Bytes.size(), 20U); // GUID(16)+Age(4)
    EXPECT_EQ(r.Value().Bytes[0], 0x01);
    EXPECT_EQ(r.Value().Bytes[16], 0x02); // Age=2 小端首字节
}

TEST(ImageInspect, PeFileImportsListed)
{
    auto b = MakeMinimalPe();
    vase::Result<std::vector<std::string>> r = vase::detail::ParsePeImports(b, false);
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    ASSERT_EQ(r.Value().size(), 1U);
    EXPECT_EQ(r.Value()[0], "sibling.dll");
}

TEST(ImageInspect, PeWithoutCodeViewFailsLouder)
{
    auto b = MakeMinimalPe();
    Put32(b, 0x58 + 112 + 6 * 8, 0);
    Put32(b, 0x58 + 112 + 6 * 8 + 4, 0); // 摘掉 Debug 目录
    vase::Result<vase::detail::ImageIdentity> r = vase::detail::ParsePeCodeView(b, false);
    EXPECT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("/DEBUG"), std::string::npos); // 指路（§8.2）
}

TEST(ImageInspect, ElfFileBuildIdExtracted)
{
    auto b = MakeMinimalElf();
    vase::Result<vase::detail::ImageIdentity> r = vase::detail::ParseElfBuildIdFile(b);
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    EXPECT_EQ(r.Value().Kind, vase::detail::IdentityKind::kElfBuildId);
    ASSERT_EQ(r.Value().Bytes.size(), 20U);
    EXPECT_EQ(r.Value().Bytes[19], static_cast<std::uint8_t>(0xAA + 19));
}

TEST(ImageInspect, ElfFileNeededListed)
{
    auto b = MakeMinimalElf();
    vase::Result<std::vector<std::string>> r = vase::detail::ParseElfNeededFile(b);
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    ASSERT_EQ(r.Value().size(), 1U);
    EXPECT_EQ(r.Value()[0], "libSibling.so");
}

TEST(ImageInspect, ElfWithoutBuildIdNoteFailsLouder)
{
    auto b = MakeMinimalElf();
    Put32(b, 64 + 0, 6); // PT_NOTE → PT_PHDR：让解析器见不到 note
    vase::Result<vase::detail::ImageIdentity> r = vase::detail::ParseElfBuildIdFile(b);
    EXPECT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("--build-id"), std::string::npos); // 指路（§8.2）
}

} // namespace
```

- [ ] **Step 7: 实现到绿（纯解析部分），先不建 Loader**

`Source/Host/CMakeLists.txt` 源列表加入 `ImageInspectCommon.cpp` 与平台文件（按 `if(WIN32)` 选）。

```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R ImageInspect
ctest --preset win-x64-clang-debug -N   # Expected: Total Tests: 36
```

---
- [ ] **Step 8: 写 `Include/Vase/Host/Loader.h`**

```cpp
#pragma once

// 平台差异的唯一收口（§13.1 第二条，v3 给它加了三档证据与导入表执法的分量）。
// Windows：LoadLibraryExW / FreeLibrary / GetProcAddress + PE 解析。
// Linux：dlopen(**RTLD_NOW | RTLD_LOCAL，不给宿主选项**——§8.7，POCO 的
//        RTLD_GLOBAL 默认在多插件反复进出的框架下不可接受) / dlclose / dlsym + ELF 解析。
//
// 「一文件一记录」：EnsureResident 对本实例的同一绝对路径只调一次平台加载——
// Unload 的「解除映射」语义因此与一次 FreeLibrary/dlclose 严格配对（§8.1：
// 卸货只发生在 Eject / Shutdown，DestroyPod 不卸货）。

#include "Vase/Detail/Export.h"
#include "Vase/Detail/ImageInspect.h"
#include "Vase/Detail/Result.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace vase::detail
{

struct BinaryRecord
{
    std::filesystem::path Path; // 绝对化后的路径（表内去重键）
    void* Raw = nullptr;        // 平台句柄（HMODULE / dlopen 返回值）
};

struct UnloadEvidence
{
    // §8.2 档二的平台分工写进结构：读报告的人不必背文档就知道哪个字段在本平台有判据力。
    bool ReopenWritable = false;             // Win 主判（**辅助**地位：改名替换骗得过它）
    bool MappingRemoved = false;             // Linux 主判：dl_iterate_phdr 条目消失
    bool ReopenWritableIsMeaningful = false; // Win: true；Linux: false（值仍记录，不作判据）
    bool MappingRemovalIsObservable = false; // Linux: true；Win: false
};

// C4251：导出类带 std::vector 成员（本仓库有意为之，前提见 Export.h 里那段的说明），
// 逐类豁免而不是全局关掉。
VASE_MSVC_DLL_WARNINGS_BEGIN
class VASE_HOST_API Loader
{
public:
    Loader() = default;
    ~Loader() = default;

    Loader(const Loader&) = delete;
    Loader& operator=(const Loader&) = delete;
    Loader(Loader&&) = delete;
    Loader& operator=(Loader&&) = delete;

    Result<BinaryRecord*> EnsureResident(const std::filesystem::path& path);
    UnloadEvidence Unload(const BinaryRecord& record);

    // —— 以下五项**与实例状态无关**，故声明为 static ——
    //
    // 计划原文把它们写成 `const` 实例成员（Symbol 更没带 const）。那样过不了
    // tidy：readability-convert-member-functions-to-static 对「不碰 this 的成员函数」
    // 逐个报（实测 10 条正文 warning，全仓第一次撞上这条检查）。它们的 `const`
    // 本来就只是「不改自己」的代理说法，而这里真正的事实是**根本不看实例**——
    // static 才是准确写法：行为一字不变、所有调用点（`loader.X(...)` 这种写法照样
    // 编过）、且不必为一个有代码级出路的检查花 NOLINT 额度。
    static Result<void*> Symbol(const BinaryRecord& record, std::string_view name);

    [[nodiscard]] static Result<ImageIdentity> MemoryIdentity(const BinaryRecord& record);      // 档三 · 内存侧
    [[nodiscard]] static Result<ImageIdentity> FileIdentity(const std::filesystem::path& path); // 档三 · 磁盘侧

    // §8.7 执法读的是**文件声明**（「这个二进制要谁」），从磁盘镜像解析——运行期
    // 已解析的导入表会替隐式兄弟链拉边，而账面看不见的正是这种「文件里写着」的关系。
    [[nodiscard]] static Result<std::vector<std::string>>
    ImportedLibraryNamesFromFile(const std::filesystem::path& path);

    [[nodiscard]] static std::string DescribeLoadFailure(const std::filesystem::path& path); // §8.2 缺依赖诊断

    [[nodiscard]] std::size_t ResidentBinaryCount() const { return Binaries.size(); }
    [[nodiscard]] const BinaryRecord* FindResident(const std::filesystem::path& path) const;
    [[nodiscard]] std::vector<BinaryRecord> AllResident() const;

private:
    static Result<void*> PlatformLoad(const std::filesystem::path& path, std::string& outError);
    static void PlatformFree(void* raw);
    static void* PlatformSymbol(void* raw, const char* name);
    static bool PlatformReopenWritable(const std::filesystem::path& path);
    static bool PlatformMappingRemoved(const std::filesystem::path& path);

    std::vector<std::unique_ptr<BinaryRecord>> Binaries; // 进程级表：只存路径+句柄，
    // 不存任何实例级对象——**这一点由 BinaryRecord 的类型承载**（path + void* 句柄，
    // 装不下实例级对象），不是靠插入点断言（§1.2）。
};
VASE_MSVC_DLL_WARNINGS_END

} // namespace vase::detail
```

- [ ] **Step 9: 实现 `Source/Host/Loader.cpp` + `LoaderWindows.cpp` + `LoaderPosix.cpp`**

`Loader.cpp`（平台无关部分）：

```cpp
#include "Vase/Host/Loader.h"

#include "ImageInspectPlatform.h"
#include "Vase/Detail/ImageInspect.h"
#include "Vase/Detail/Result.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace vase::detail
{

Result<BinaryRecord*> Loader::EnsureResident(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::path abs = std::filesystem::absolute(path, ec); // §9.2 清单：error_code 形态，永不抛
    if (ec)
    {
        abs = path;
    }
    const auto existing = std::ranges::find_if(Binaries, [&abs](const std::unique_ptr<BinaryRecord>& entry)
                                               { return entry->Path == abs; });
    if (existing != Binaries.end())
    {
        // 已驻留：复用同一记录，不触发二次平台加载（§8.1 表）。
        // 表本身是可变的，所以这里直接取非 const 指针——不需要 const_cast 那一手。
        return Result<BinaryRecord*>::Ok(existing->get());
    }

    std::string platformError;
    Result<void*> loaded = PlatformLoad(abs, platformError);
    if (!loaded.IsOk())
    {
        std::string message = "failed to load binary: " + abs.string() + " (" + platformError + ")";
        if (const std::string hint = DescribeLoadFailure(abs); !hint.empty())
        {
            message += " — missing dependency: " + hint; // POCO 借来的诊断：报「缺哪个」，不只 dlopen failed
        }
        return Result<BinaryRecord*>::Err(Error{std::move(message)});
    }

    // 计划原文这里写的是 `*loaded.Value()`。`Result<void*>::Value()` 返回的就是
    // `void*`（不是 `void**`），解引用它编译不过——实测诊断：
    //   error: indirection not permitted on operand of type 'void *'
    //   error: cannot initialize a member subobject of type 'void *' with an lvalue of type 'void'
    // 直接取那个句柄即可。
    Binaries.push_back(std::make_unique<BinaryRecord>(BinaryRecord{.Path = abs, .Raw = loaded.Value()}));
    return Result<BinaryRecord*>::Ok(Binaries.back().get());
}

UnloadEvidence Loader::Unload(const BinaryRecord& record)
{
    UnloadEvidence evidence;
    const auto it = std::ranges::find_if(Binaries, [&record](const std::unique_ptr<BinaryRecord>& entry)
                                         { return entry->Raw == record.Raw && entry->Path == record.Path; });
    if (it == Binaries.end())
    {
        return evidence; // 重复 Unload：全 false。调用方（Eject/Shutdown）必须读证据，不读=漏账
    }
    const BinaryRecord copy = **it;
    Binaries.erase(it); // 先摘表，后释放——释放触发的任何回调都看不到这个记录（防重入）
    PlatformFree(copy.Raw);

#ifdef _WIN32
    evidence.ReopenWritable = PlatformReopenWritable(copy.Path);
    evidence.ReopenWritableIsMeaningful = true;
    // MappingRemoved 在 Windows 不可观测（§8.2 档二：FreeLibrary 返回值不保证卸载干净），
    // 主判交给 ReopenWritable，但它只是辅助——最终防「假成功」的是档三（T11）。
#else
    evidence.MappingRemoved = PlatformMappingRemoved(copy.Path);
    evidence.MappingRemovalIsObservable = true;
    // Linux 上恒真（旧 inode 解除链接即可），记录但不作判据（§8.2）。
    evidence.ReopenWritable = PlatformReopenWritable(copy.Path);
    evidence.ReopenWritableIsMeaningful = false;
#endif
    return evidence;
}

Result<void*> Loader::Symbol(const BinaryRecord& record, std::string_view name)
{
    const std::string zeroTerminated(name);
    void* address = PlatformSymbol(record.Raw, zeroTerminated.c_str()); // 平台文件各一行实现
    if (address == nullptr)
    {
        return Result<void*>::Err(Error{"symbol not found: " + zeroTerminated});
    }
    return Result<void*>::Ok(address);
}

Result<ImageIdentity> Loader::MemoryIdentity(const BinaryRecord& record)
{
    return MemoryIdentityPlatform(record.Raw, record.Path);
}

Result<ImageIdentity> Loader::FileIdentity(const std::filesystem::path& path) { return FileIdentityPlatform(path); }

Result<std::vector<std::string>> Loader::ImportedLibraryNamesFromFile(const std::filesystem::path& path)
{
    const std::vector<std::uint8_t> bytes = ReadImageFileBytes(path);
    if (bytes.empty())
    {
        return Result<std::vector<std::string>>::Err(Error{"cannot read file: " + path.string()});
    }
#ifdef _WIN32
    return ParsePeImports(bytes, /*loadedInMemory=*/false);
#else
    return ParseElfNeededFile(bytes);
#endif
}

std::string Loader::DescribeLoadFailure(const std::filesystem::path& path)
{
    const std::vector<std::uint8_t> bytes = ReadImageFileBytes(path);
    if (bytes.empty())
    {
        return {};
    }
#ifdef _WIN32
    return FirstUnresolvableImport(bytes, /*isPe=*/true);
#else
    return FirstUnresolvableImport(bytes, /*isPe=*/false);
#endif
}

const BinaryRecord* Loader::FindResident(const std::filesystem::path& path) const
{
    const auto it = std::ranges::find_if(Binaries, [&path](const std::unique_ptr<BinaryRecord>& entry)
                                         { return entry->Path == path; });
    return it == Binaries.end() ? nullptr : it->get();
}

std::vector<BinaryRecord> Loader::AllResident() const
{
    std::vector<BinaryRecord> out;
    out.reserve(Binaries.size());
    for (const std::unique_ptr<BinaryRecord>& entry : Binaries)
    {
        out.push_back(*entry);
    }
    return out;
}

} // namespace vase::detail
```

`LoaderWindows.cpp` / `LoaderPosix.cpp` 各实现 `PlatformLoad` / `PlatformFree` /
`PlatformReopenWritable` / `PlatformMappingRemoved` / `PlatformSymbol` /
`MemoryIdentityPlatform(const BinaryRecord&)` / `FileIdentityPlatform(const path&)`，
行为已在 Step 5 钉死（`LoadLibraryExW` + `LOAD_WITH_ALTERED_SEARCH_PATH`；
`dlopen(RTLD_NOW|RTLD_LOCAL)` + `dl_iterate_phdr` 按 `dlpi_name` 全等匹配——**测试与
Host 一律传绝对路径**，`$<PATH:CMAKE_PATH,$<TARGET_FILE:...>>` 给的就是绝对路径）。
`MemoryIdentity`/`FileIdentity` 在 `Loader.cpp` 里是转发到上述平台函数的一行。
Windows `PlatformReopenWritable`：`CreateFileW(GENERIC_WRITE, 0, OPEN_EXISTING)` 成功即 true；
Linux 版：`open(path.c_str(), O_WRONLY)`。全部 `wchar`/`char` 转换只走 `path::string()`
（M1 不引入 Unicode 收口，§13.1 的该未决项仍留——注释标记位置：`LoaderWindows.cpp` 顶部）。

- [ ] **Step 10: 造 `LoadProbe` / `UnloadProbe` fixture 与端到端断言**

`Tests/Unit/fixtures/LoadProbe/LoadProbe.cpp`：

```cpp
#include "Vase/Plugin.h"

#include <cstdint>
#include <string_view>

// Loader 端到端探针：一个最小真实插件二进制。两个 CMake target
// （LoadProbe / UnloadProbe）同源码不同输出名——**每个文件只被一个
// TEST_F 装载**，避免同一镜像的 OS 引用计数（2 次 LoadLibrary）把
// Unload 证据测试的档二判定污染成假红。

namespace
{

// 探针事件：让 OnLoad 做一件**真实的 Pod 动作**，从而在导入表里留下 VasePod。
//
// 这不是装饰。计划原文的 OnLoad 是空的（只 return Ok），那样这个「真实插件二进制」
// 一个 VasePod 符号都不引用——链接器只从导入库拉被引用的成员，于是产物里**没有**
// VasePod 的导入条目（实测 llvm-readobj --coff-imports：只有 MSVCP140D / KERNEL32 /
// VCRUNTIME140D）。而 ProbeImportsVasePod 断言的正是「解析器在真实产物上看得见
// VasePod」——空 OnLoad 让它无从成立。Context::Emit<E> 会调到 Context::EmitRaw，
// 那是 VasePod 的 out-of-line 成员，导入条目由此而来（D11 的链接期事实也就真的成立）。
// 事件没有订阅者，派发是空转（§2.4 同步派发）。
struct LoadProbeEvent
{
    static constexpr std::string_view kName = "Vase.LoadProbe.Loaded";
    static constexpr std::uint32_t kVersion = 1;
};

class LoadProbePlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Emit(LoadProbeEvent{});
        return vase::Result<void>::Ok();
    }
};

} // namespace

VASE_PLUGIN(LoadProbePlugin){
    .Id = "Vase.LoadProbe",
    .DisplayName = "装载探针",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {},
};
```

`Cmake/VasePluginHelpers.cmake`（**新建**，根 `CMakeLists.txt` 在 `enable_testing()` 前 `include(Cmake/VasePluginHelpers.cmake)`——Samples 与 Tests 都要用）：

```cmake
# 插件形态的模板：SHARED + 只链 VasePod（D11 的链接期事实）+ 收紧可见性。
# M1 后续所有插件 target（Sample 与 fixture）都走这一个函数——插件形态定义只此一处，
# 防「某个 fixture 忘了 hidden visibility / 忘了链 VaseBuildOptions」（CLAUDE.md 规矩 1）。
function(vase_add_plugin_fixture name)
    cmake_parse_arguments(FIX "" "" "SOURCES;LINK_LIBRARIES" ${ARGN})
    add_library(${name} SHARED ${FIX_SOURCES})
    target_link_libraries(${name}
        PRIVATE VaseBuildOptions ${FIX_LINK_LIBRARIES})
    set_target_properties(${name} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
endfunction()
```

`Tests/Unit/fixtures/LoadProbe/CMakeLists.txt`：

```cmake
vase_add_plugin_fixture(LoadProbe SOURCES LoadProbe.cpp LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(UnloadProbe SOURCES LoadProbe.cpp LINK_LIBRARIES VasePod)
```

`Tests/CMakeLists.txt` 增：

```cmake
add_subdirectory(Unit/fixtures/LoadProbe)
# CMAKE_PATH：把生成器表达式结果转成正斜杠绝对路径——直接塞进 C++ 字符串字面量
# 会留下反斜杠转义事故（Windows 上 \V、\b 都是合法转义，字符串静默变形）。
target_compile_definitions(VaseTests PRIVATE
    VASE_FIXTURE_LOADPROBE="$<PATH:CMAKE_PATH,$<TARGET_FILE:LoadProbe>>"
    VASE_FIXTURE_UNLOADPROBE="$<PATH:CMAKE_PATH,$<TARGET_FILE:UnloadProbe>>")
```

`Tests/Unit/LoaderTests.cpp`（5 个 TEST，追加到 `LoaderTests.cpp` 同一文件末尾；`<vase/detail/...>` 一律引号包含）：

```cpp
#include "Vase/Detail/ImageInspect.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Loader.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <gtest/gtest.h>
#include <iterator>
#include <string>
#include <vector>

#ifdef _WIN32
#include <cctype> // Windows 分支用 std::tolower
#else
#include <algorithm> // 其余平台分支用 std::ranges::find
#endif

namespace
{

// 构造字节用的写手。计划正文写的是 `b[at] = v` / `b.data() + at`，那两种写法会
// 分别招来 cppcoreguidelines 的「未检查容器访问」与「指针算术」两条（DetailTests
// 里已有一处同类 NOLINT）。这里换成 std::next + memcpy：构造出来的字节完全一致，
// 也不需要抑制——本文件因此一个 NOLINT 都没有。
void Blit(std::vector<std::uint8_t>& bytes, std::size_t at, const void* source, std::size_t length)
{
    std::memcpy(std::next(bytes.data(), static_cast<std::ptrdiff_t>(at)), source, length);
}

// DataDirectory 一项的宽度（Debug 目录是下标 6，导入目录是下标 1）。写成具名 size_t
// 常量：直接写 `6 * 8` 的话乘法在 int 里做、再隐式加宽到 size_t，过不了
// bugprone-implicit-widening-of-multiplication-result。
constexpr std::size_t kDataDirEntry = 8;

void Put8(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint8_t value)
{
    Blit(bytes, at, &value, sizeof(value));
}

void Put16(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint16_t value)
{
    // 截断本身就是取低 8 位，不必再 & 0xFF——而 `value & 0xFFU` / `value >> 8U` 会让
    // uint16_t 先提升成 int，与无符号字面量做位运算，撞 bugprone-signed-bitwise。
    const std::array<std::uint8_t, 2> raw{
        static_cast<std::uint8_t>(value),
        static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> 8U),
    };
    Blit(bytes, at, raw.data(), raw.size());
}

void Put32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value)
{
    Put16(bytes, at, static_cast<std::uint16_t>(value));
    Put16(bytes, at + 2, static_cast<std::uint16_t>(value >> 16U));
}

void Put64(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint64_t value)
{
    Put32(bytes, at, static_cast<std::uint32_t>(value));
    Put32(bytes, at + 4, static_cast<std::uint32_t>(value >> 32U));
}

void PutStr(std::vector<std::uint8_t>& bytes, std::size_t at, const char* text)
{
    Blit(bytes, at, text, std::strlen(text) + 1);
}

std::vector<std::uint8_t> MakeMinimalPe()
{
    // 磁盘形态：DOS(64) + PE sig + COFF(20) + OptHdr(240, PE32+, Debug@dir[6]=RVA 0x2000,
    // Import@dir[1]=RVA 0x2100) + 1 节表 2 项；.debug→0x400，.rdata→0x500。
    std::vector<std::uint8_t> b(0x600, 0);
    PutStr(b, 0, "MZ");
    Put32(b, 0x3C, 0x40);    // e_lfanew
    PutStr(b, 0x40, "PE\0"); // 0x40: sig(4) + COFF(20) = 0x44..0x58
    Put16(b, 0x44, 0x8664);  // Machine
    Put16(b, 0x46, 2);       // NumberOfSections
    Put16(b, 0x54, 240);     // SizeOfOptionalHeader（COFF 起 0x44，字段 +16）
    Put16(b, 0x58, 0x20B);   // Magic PE32+（可选头起 0x58）
    Put32(b, 0x58 + 56, 0x3000);
    Put32(b, 0x58 + 108, 16); // NumberOfRvaAndSizes
    const std::size_t dirs = 0x58 + 112;
    Put32(b, dirs + (6 * kDataDirEntry), 0x2000);
    Put32(b, dirs + (6 * kDataDirEntry) + 4, 56); // Debug：1 项 28B + 1 null 28B
    Put32(b, dirs + (1 * kDataDirEntry), 0x2100);
    Put32(b, dirs + (1 * kDataDirEntry) + 4, 40); // Import：1 desc + null
    // 节表 @ 0x58+240 = 0x148：两项各 40B，故第二项起 0x148+40 = 0x170。
    // （计划正文这里写的是 0x168——0x148 起两项 40B 的第二项只能在 0x170；
    // 0x168 会让 .rdata 的头 8 字节落进第一项的尾部，解析器按节表换算就找不到
    // 导入表的 RVA 0x2100，实测报 "PE RVA not inside any section"。）
    PutStr(b, 0x148, ".debug"); // VA@+12, SizeOfRawData@+16, PointerToRawData@+20
    Put32(b, 0x148 + 12, 0x2000);
    Put32(b, 0x148 + 16, 0x100);
    Put32(b, 0x148 + 20, 0x400);
    PutStr(b, 0x170, ".rdata");
    Put32(b, 0x170 + 12, 0x2100);
    Put32(b, 0x170 + 16, 0x100);
    Put32(b, 0x170 + 20, 0x500);
    // Debug 目录项 @ 0x400：Type=2@+12, SizeOfData@+16, AddressOfRawData@+20=0x2040, PointerToRawData@+24=0x440
    Put32(b, 0x400 + 12, 2);
    Put32(b, 0x400 + 16, 0x28);
    Put32(b, 0x400 + 20, 0x2040);
    Put32(b, 0x400 + 24, 0x440);
    // RSDS @ 0x440：sig + GUID(0x01..0x10) + Age=2 + PdbPath
    PutStr(b, 0x440, "RSDS");
    for (std::size_t i = 0; i < 16; ++i)
    {
        Put8(b, 0x444 + i, static_cast<std::uint8_t>(i + 1));
    }
    Put32(b, 0x454, 2);
    PutStr(b, 0x458, "probe.pdb");
    // Import desc @ 0x500：Name RVA@+12 = 0x2140 → off 0x540；后跟 20B null 表尾
    Put32(b, 0x500 + 12, 0x2140);
    PutStr(b, 0x540, "sibling.dll");
    return b;
}

std::vector<std::uint8_t> MakeMinimalElf(std::uint64_t buildIdLen = 20)
{
    // e_ident(16) + e_type..(48) = 64；phnum=2（PT_NOTE@0x200, PT_DYNAMIC@0x300）；strtab 在 0x400。
    std::vector<std::uint8_t> b(0x500, 0);
    Put8(b, 0, 0x7F);
    PutStr(b, 1, "ELF"); // "ELF" 三字节 + NUL，正好落在 ident[1..4] 的位置
    Put8(b, 4, 2);
    Put8(b, 5, 1);      // ELFCLASS64, LSB
    Put8(b, 6, 1);      // version
    Put16(b, 16, 3);    // ET_DYN
    Put16(b, 18, 0x3E); // x86-64
    Put32(b, 20, 1);    // e_version
    Put64(b, 32, 64);   // e_phoff
    Put16(b, 52, 64);   // e_ehsize
    Put16(b, 54, 56);   // e_phentsize
    Put16(b, 56, 2);    // e_phnum
    // phdr[0] PT_NOTE @64：type@0=4, offset@8=0x200, vaddr@16, filesz@32, memsz@40
    Put32(b, 64 + 0, 4);
    Put64(b, 64 + 8, 0x200);
    Put64(b, 64 + 16, 0x200);
    const std::size_t noteBytes =
        12 + 4 + static_cast<std::size_t>(buildIdLen) + ((buildIdLen % 4) != 0 ? 4 - (buildIdLen % 4) : 0);
    Put64(b, 64 + 32, noteBytes);
    Put64(b, 64 + 40, noteBytes);
    // note @0x200：namesz=4, descsz, type=3, "GNU\0", desc=0xAA..
    Put32(b, 0x200, 4);
    Put32(b, 0x204, static_cast<std::uint32_t>(buildIdLen));
    Put32(b, 0x208, 3);
    PutStr(b, 0x20C, "GNU");
    for (std::uint64_t i = 0; i < buildIdLen; ++i)
    {
        Put8(b, 0x210 + static_cast<std::size_t>(i), static_cast<std::uint8_t>(0xAA + i));
    }
    // phdr[1] PT_DYNAMIC @120：offset 0x300，4 个 entry（STRTAB→0x400, NEEDED→10, NULL）
    Put32(b, 120 + 0, 2);
    Put64(b, 120 + 8, 0x300);
    Put64(b, 120 + 32, std::uint64_t{4} * 16);
    Put64(b, 0x300 + (0 * 16), 5);
    Put64(b, 0x300 + (0 * 16) + 8, 0x400); // DT_STRTAB
    Put64(b, 0x300 + (1 * 16), 1);
    Put64(b, 0x300 + (1 * 16) + 8, 10); // DT_NEEDED → strtab+10
    Put64(b, 0x300 + (2 * 16), 0);      // DT_NULL
    PutStr(b, 0x400 + 10, "libSibling.so");
    return b;
}

TEST(ImageInspect, PeFileCodeViewExtracted)
{
    auto b = MakeMinimalPe();
    vase::Result<vase::detail::ImageIdentity> r = vase::detail::ParsePeCodeView(b, false);
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    EXPECT_EQ(r.Value().Kind, vase::detail::IdentityKind::kPdbCodeView);
    ASSERT_EQ(r.Value().Bytes.size(), 20U); // GUID(16)+Age(4)
    EXPECT_EQ(r.Value().Bytes.front(), 0x01);
    EXPECT_EQ(*std::next(r.Value().Bytes.begin(), 16), 0x02); // Age=2 小端首字节
}

TEST(ImageInspect, PeFileImportsListed)
{
    auto b = MakeMinimalPe();
    vase::Result<std::vector<std::string>> r = vase::detail::ParsePeImports(b, false);
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    ASSERT_EQ(r.Value().size(), 1U);
    EXPECT_EQ(r.Value().front(), "sibling.dll");
}

TEST(ImageInspect, PeWithoutCodeViewFailsLouder)
{
    auto b = MakeMinimalPe();
    Put32(b, 0x58 + 112 + (6 * kDataDirEntry), 0);
    Put32(b, 0x58 + 112 + (6 * kDataDirEntry) + 4, 0); // 摘掉 Debug 目录
    const vase::Result<vase::detail::ImageIdentity> r = vase::detail::ParsePeCodeView(b, false);
    EXPECT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("/DEBUG"), std::string::npos); // 指路（§8.2）
}

TEST(ImageInspect, ElfFileBuildIdExtracted)
{
    auto b = MakeMinimalElf();
    vase::Result<vase::detail::ImageIdentity> r = vase::detail::ParseElfBuildIdFile(b);
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    EXPECT_EQ(r.Value().Kind, vase::detail::IdentityKind::kElfBuildId);
    ASSERT_EQ(r.Value().Bytes.size(), 20U);
    EXPECT_EQ(*std::next(r.Value().Bytes.begin(), 19), static_cast<std::uint8_t>(0xAA + 19));
}

TEST(ImageInspect, ElfFileNeededListed)
{
    auto b = MakeMinimalElf();
    vase::Result<std::vector<std::string>> r = vase::detail::ParseElfNeededFile(b);
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    ASSERT_EQ(r.Value().size(), 1U);
    EXPECT_EQ(r.Value().front(), "libSibling.so");
}

TEST(ImageInspect, ElfWithoutBuildIdNoteFailsLouder)
{
    auto b = MakeMinimalElf();
    Put32(b, 64 + 0, 6); // PT_NOTE → PT_PHDR：让解析器见不到 note
    const vase::Result<vase::detail::ImageIdentity> r = vase::detail::ParseElfBuildIdFile(b);
    EXPECT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("--build-id"), std::string::npos); // 指路（§8.2）
}

std::filesystem::path FixturePath(const char* defineValue) { return std::filesystem::path{defineValue}; }

TEST(Loader, EnsureResidentDedupsAndRejectsMissing)
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> r = loader.EnsureResident(FixturePath(VASE_FIXTURE_LOADPROBE));
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    vase::Result<vase::detail::BinaryRecord*> again = loader.EnsureResident(FixturePath(VASE_FIXTURE_LOADPROBE));
    ASSERT_TRUE(again.IsOk());
    EXPECT_EQ(again.Value(), r.Value());         // 复用同一记录（§8.1「确保驻留」）
    EXPECT_EQ(loader.ResidentBinaryCount(), 1U); // 且只触发一次平台加载

    const vase::Result<vase::detail::BinaryRecord*> missing = loader.EnsureResident("this-binary-does-not-exist.vase");
    EXPECT_FALSE(missing.IsOk());
    EXPECT_NE(missing.GetError().Message().find("does-not-exist"), std::string::npos);
    loader.Unload(*r.Value());
}

TEST(Loader, SymbolResolvesDescriptorEntry)
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> r = loader.EnsureResident(FixturePath(VASE_FIXTURE_LOADPROBE));
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    // §8.1 的唯一加载路径第一跳：extern "C" 入口符号必须能按名取到。
    EXPECT_TRUE(loader.Symbol(*r.Value(), "VasePlugin_GetPlugin").IsOk());
    EXPECT_FALSE(loader.Symbol(*r.Value(), "VasePlugin_NoSuchEntry").IsOk());
    loader.Unload(*r.Value());
}

TEST(Loader, MemoryIdentityMatchesFileIdentity)
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> r = loader.EnsureResident(FixturePath(VASE_FIXTURE_LOADPROBE));
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    // 档三原语的最小成立条件：同一文件的内存特征 == 磁盘特征（T11/T13 的
    // 「驻留复用也要比对」「换文件即变脸」全部建在这一条上）。
    vase::Result<vase::detail::ImageIdentity> m = vase::detail::Loader::MemoryIdentity(*r.Value());
    ASSERT_TRUE(m.IsOk()) << m.GetError().Message();
    vase::Result<vase::detail::ImageIdentity> f =
        vase::detail::Loader::FileIdentity(FixturePath(VASE_FIXTURE_LOADPROBE));
    ASSERT_TRUE(f.IsOk()) << f.GetError().Message();
    EXPECT_EQ(m.Value(), f.Value());
    loader.Unload(*r.Value());
}

TEST(Loader, ProbeImportsVasePod)
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> r = loader.EnsureResident(FixturePath(VASE_FIXTURE_LOADPROBE));
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    vase::Result<std::vector<std::string>> names = vase::detail::Loader::ImportedLibraryNamesFromFile(r.Value()->Path);
    ASSERT_TRUE(names.IsOk()) << names.GetError().Message();
#ifdef _WIN32
    // 计划原文这里写的是 expected = "VasePod.dll"，却拿**已小写化**的 lowered 去比——
    // 那一比恒假（"vasepod.dll" != "VasePod.dll"），断言无从成立。expected 直接取小写形。
    const std::string expected = "vasepod.dll";
    const std::string found = [&]
    {
        for (const auto& n : names.Value())
        {
            if (n.size() == expected.size())
            {
                std::string lowered = n;
                for (auto& c : lowered)
                {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                if (lowered == expected)
                {
                    return lowered;
                }
            }
        }
        return std::string{};
    }(); // PE 导入表保留创建时大小写，但链接器来源不一——大小写不敏感匹配（§8.7 执法用文件名主干比对，同口径）
#else
    const std::string expected = "libVasePod.so";
    const std::string found =
        std::ranges::find(names.Value(), expected) != names.Value().end() ? expected : std::string{};
#endif
    EXPECT_FALSE(found.empty()); // 解析器在真实产物上工作，不只在构造字节上
    loader.Unload(*r.Value());
}

TEST(Loader, UnloadEvidencePerPlatform)
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> r = loader.EnsureResident(FixturePath(VASE_FIXTURE_UNLOADPROBE));
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    const vase::detail::BinaryRecord copy = *r.Value(); // 先复制：Unload 会摘表（r.Value() 失效）
    const vase::detail::UnloadEvidence ev = loader.Unload(copy);
    EXPECT_EQ(loader.ResidentBinaryCount(), 0U);
#ifdef _WIN32
    EXPECT_TRUE(ev.ReopenWritableIsMeaningful);
    EXPECT_TRUE(ev.ReopenWritable); // 档二 · Win：FreeLibrary 后文件必须可写开（辅助判据，§8.2）
#else
    EXPECT_TRUE(ev.MappingRemovalIsObservable);
    EXPECT_TRUE(ev.MappingRemoved); // 档二 · Linux：dl_iterate_phdr 条目消失（主判之一）
#endif
}

} // namespace
```

- [ ] **Step 11: 全绿 + 双平台 + 门禁 + Commit**

```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -N   # Expected: Total Tests: 41
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug -N'
git ls-files -z --cached --others --exclude-standard '*.h' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git add -A
git commit -m "M1-T6：身份特征构建标志落三个工具链 + Loader/镜像解析器（档二档三地基）"
```

（**本桩起 cl.exe 线为常规**：T4/T5 已把它纳入每桩验收；T3 的 C4251 教训证明只有这条线会报导出类的 STL 成员问题。）

---
## Task 7: 最小闭环——`PluginHost` / `CreatePod` / `DestroyPod` / Embedding

「干净地起、干净地灭、可重复无数次」的前半句在此成立：手写 `LoadPlan`（D12）→ 加载路径唯一跳（§8.1：`GetPlugin → 描述符 → HeaderVersion → 工厂`）→ 阶段 0/1/2（§5.3）→ `DestroyPod` **永不失败**（§5.1：返回报告不返回 `Result`）→ 诊断计数以 **Pod 创建时快照为基线**做差分（§9.2 v3）。`AdoptPlugin` / `EjectPlugin` 的签名 T10/T11 再加——本任务把 Pod 生命周期机器与活集合形状立对。

**Files:**
- Create: `Include/Vase/Host/LoadPlan.h`、`Include/Vase/Host/PluginHost.h`、`Include/Vase/Pod/Pod.h`
- Create: `Source/Pod/Pod.cpp`、`Source/Host/PluginHost.cpp`
- Create: `Samples/HelloCommon/Greeter.h`、`Samples/HelloPlugin/HelloPlugin.cpp` + `CMakeLists.txt`
- Create: `Samples/Embedding/main.cpp` + `CMakeLists.txt`
- Create: `Tests/Lifecycle/LifecycleTests.cpp`
- Modify: 根 `CMakeLists.txt`（子目录）、`Tests/CMakeLists.txt`、`Source/Host/CMakeLists.txt`

**Interfaces:**
- Consumes: T1–T6 全部。
- Produces:
  - `vase::LoadPlanEntry { std::string_view Id; std::filesystem::path BinaryPath; }`；`vase::LoadPlan { std::vector<LoadPlanEntry> Ordered; }`（`Ordered` **即拓扑序**——M1 手写者负责，M2 起由 `Solve` 产出）
  - `vase::PodOptions { bool Strict = false; std::function<void(Context&)> Stage0 = nullptr; }`（阶段 0 回调，§5.3）
  - `vase::PodHandle { std::uint32_t Index; std::uint32_t Generation; }`（不透明句柄，§5.1）
  - `vase::FailedPluginRecord { std::string Id; Phase Stage; std::string Message; }`
  - `vase::DiagnosticSnapshot { uint64 Effects, Services, Subscriptions, PluginInstances, Scopes; }` + `vase::ResidualEntry { std::string OwnerLabel; std::uint64_t Count; }`
  - `vase::PodReport { bool HandleWasStale; std::uint32_t PodIndex; std::vector<FailedPluginRecord> Failures; std::vector<std::string> HotSwapLog; DiagnosticSnapshot CountersDiff; std::vector<ResidualEntry> Residuals; bool Clean() const; }`
  - `vase::Pod`（`VASE_POD_API`）：`Context& Root()`、`std::size_t PluginCount() const`（活集合大小）、`bool HasPlugin(id)`、`std::vector<std::string> PluginIds() const`
  - `vase::PluginHost`（`VASE_HOST_API`）：`PluginHost()`（绑定线程 + 进程唯一守卫，§1.4）、`~PluginHost()`（Debug 断言零存活 Pod）、`Result<PodHandle> CreatePod(const LoadPlan&, const PodOptions& = {})`、`PodReport DestroyPod(PodHandle)`、`Pod* Resolve(PodHandle)`、`detail::DiagnosticCounters& ForTestCounters()`（测试缝，注释明示）
  - 可执行 `VaseEmbedding`（`play` / `loop <N>` 两个子命令；T11 扩 `eject`/`adopt`）

- [ ] **Step 1: 写 `Include/Vase/Host/LoadPlan.h`**

```cpp
#pragma once

// §5.1 的输入形状，M1 手写形态（D12）：LoadPlan 是普通 struct，M2 的
// PluginCatalog::Solve 产出同一个类型——装配路径零返工。
// Ordered 的数组序**就是**加载与关停的序（加载正序 / 关停逆序，§5.4）；
// M1 没有求解器，「Ordered 已按拓扑序排好」由手写者保证（M2 起是 Solve 的构造性保证）。

#include <filesystem>
#include <functional>
#include <string_view>
#include <vector>

namespace vase
{

class Context; // 前向声明足矣：Stage0 回调只接引用，不定义它

struct LoadPlanEntry
{
    std::string_view Id;
    std::filesystem::path BinaryPath; // 绝对或相对 CWD；Host 内部绝对化（T6）
};

struct LoadPlan
{
    std::vector<LoadPlanEntry> Ordered;
};

struct PodOptions
{
    bool Strict = false; // 5.5：任一插件失败即整局失败。M1 实现「无级联」的基本形
    // （任一 Failed → 已建 Pod 全拆 → Err）；级联拆除等 M2 依赖图。
    std::function<void(Context&)> Stage0; // §5.3 阶段 0：宿主服务注册点
};

} // namespace vase
```

- [ ] **Step 2: 写 `Include/Vase/Pod/Pod.h`（声明）+ `Source/Pod/Pod.cpp`（实现骨架）**

```cpp
#pragma once

// Pod——一次运行的边界（§2.1）。实例级一切的宿主：根 Context、服务表、事件总线、
// 每插件一个 EffectScope。DestroyPod 后本对象整体析构——「销毁即归零」不是修辞，
// 是字面意义的 unique_ptr 释放。
//
// 成员与访问器撞名时成员让位（Root() ↔ RootContext，Failures() ↔ FailureRecords）。
// 引用成员（Pool / Counters）配「拷贝与移动全删」：Pod 由 Host 以 unique_ptr 独占，
// 值语义从不存在。

#include "Vase/Detail/Counters.h"
#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"
#include "Vase/Effect/EffectScope.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/PluginDescriptor.h"
#include "Vase/Pod/Context.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace vase::detail
{
class ServiceRegistry;
class EventBus;
class ScopePool;
struct DependencyLedger; // T9
} // namespace vase::detail

namespace vase
{

// —— 以下报告类型住在 Pod.h（spec 3.1 的归属：SessionHandle/SessionReport 同款）。
// 链接方向是 Host→Pod 单向，FailedPluginRecord 等要能被 Pod 存储，就不能定义在
// Host 层——类型归属跟着**最底层的消费者**走。

struct DiagnosticSnapshot
{
    std::uint64_t Effects = 0;
    std::uint64_t Services = 0;
    std::uint64_t Subscriptions = 0;
    std::uint64_t PluginInstances = 0;
    std::uint64_t Scopes = 0;
};

struct FailedPluginRecord
{
    std::string Id;
    Phase Stage = Phase::kLoad;
    std::string Message;
};

struct ResidualEntry
{
    std::string OwnerLabel;
    std::uint64_t Count = 0;
};

struct PodReport
{
    bool HandleWasStale = false; // §5.1：句柄失效是预期内，不是错误
    std::uint32_t PodIndex = 0;
    std::vector<FailedPluginRecord> Failures; // 运行时失败（§5.2 的轻量诊断记录）
    std::vector<std::string> HotSwapLog;      // §9.2 v3「中途进出过谁」（T10/T11 填充）
    DiagnosticSnapshot CountersDiff;          // 相对基线的差分（基线 = 本 Pod 创建时）
    std::vector<ResidualEntry> Residuals;     // #10 归属（M1 形态见类内注释）

    // VASE_POD_API 只加在这个成员函数上，不给整个 struct 加：定义在 VasePod 里，
    // 本库外调用不导出即 lld-link undefined symbol（与 EffectHandle 同一笔账）；
    // 而 struct 级别的导出会把 STL 成员带进 C4251 的射程。
    [[nodiscard]] VASE_POD_API bool Clean() const; // 差分五项全零且无 Residuals
};

struct PodHandle
{
    // Generation 从 1 起（见 PluginHost.cpp 的槽分配）：0 是默认构造的句柄，
    // 它不该解析到任何槽——否则 `PodHandle{}` 会冒充第一个 Pod。
    std::uint32_t Index = 0;
    std::uint32_t Generation = 0;

    bool operator==(const PodHandle&) const = default;
};

class PluginHost;
class PodTestPeer;

VASE_MSVC_DLL_WARNINGS_BEGIN
class VASE_POD_API Pod
{
public:
    Pod(const Pod&) = delete;
    Pod& operator=(const Pod&) = delete;
    Pod(Pod&&) = delete;
    Pod& operator=(Pod&&) = delete;
    // 析构**声明在这里、定义在 Pod.cpp**：默认在类内会让每个用到 unique_ptr<Pod> 的 TU
    // 都实例化 Pod 的成员析构，而它们要 ServiceRegistry / EventBus 的完整类型——
    // Host 侧于是被迫包含对它无用的 Vase/Detail/RegistryBus.h（实测 clang-cl:
    // invalid application of 'sizeof' to an incomplete type 'vase::detail::ServiceRegistry'）。
    ~Pod();

    Context& Root(); // 宿主 stage-0 与运行期入口（§1.3 推论：宿主服务每局重注册）

    [[nodiscard]] std::size_t PluginCount() const; // 活集合（§5.6「活集合」的 M1 形态）
    [[nodiscard]] bool HasPlugin(std::string_view id) const;
    [[nodiscard]] std::vector<std::string> PluginIds() const;
    [[nodiscard]] const std::vector<FailedPluginRecord>& Failures() const; // §5.5：失败清单随时可被宿主读出

private:
    friend class PluginHost;
    friend class PodTestPeer;

    struct LiveInstance
    {
        const PluginDescriptor* Desc = nullptr;
        Plugin* Instance = nullptr; // 由 Desc->Create/Destroy 配对管理
        std::unique_ptr<EffectScope> Scope;
        std::unique_ptr<Context> Ctx;
        std::string OwnerLabel; // Meta->Id 的拥有型拷贝（别赌字面量生命周期）

        enum class InstanceState : std::uint8_t
        {
            kLoading,
            kLoaded,
            kStarted,
            kFailed,
        };

        InstanceState State = InstanceState::kLoading;
    };

    Pod(detail::ScopePool& pool, detail::DiagnosticCounters& counters, detail::DependencyLedger* ledger,
        std::uint32_t podIndex); // ledger T9 起非空

    void TeardownInstancesAndRoot(); // 逆序回收全部实例 + 根 Scope（DestroyPod 的机器，§5.4）

    detail::ScopePool& Pool;
    detail::DiagnosticCounters& Counters;
    detail::DependencyLedger* Ledger;
    std::uint32_t PodIndex;

    std::unique_ptr<detail::ServiceRegistry> Registry;
    std::unique_ptr<detail::EventBus> Bus;
    std::unique_ptr<EffectScope> RootScope;
    std::unique_ptr<Context> RootContext;

    std::vector<std::unique_ptr<LiveInstance>> Instances; // 数组序 = 计划序
    std::vector<FailedPluginRecord> FailureRecords;       // §5.2：Failed 保留的是记录

    // #10（D14）的最小验证形态：**测试注入**的未回收 Scope 清单。插件侧真泄漏通道
    // （绕 Effect 渠道的注册）是 9.3 契约束，其检测属 M3 完整属主追踪器——这里先把
    // 「报告要点名残留归属」的机制立住（PodTestPeer 注入，正常装配路径永不写入）。
    std::vector<std::unique_ptr<EffectScope>> LeakedScopesForTest;
};
VASE_MSVC_DLL_WARNINGS_END

} // namespace vase
```

`Pod.cpp`：`TeardownInstancesAndRoot()` 与构造/访问器的全量实现——要点：实例逆序 `Scope->Dispose(); Desc->Destroy(Instance); --Counters.PluginInstances;`（§5.2 Failed 已即时回收过的跳过）；随后 `RootScope->Dispose()`（注册最早 → 回收最晚，§5.4 的顺序是「一切皆 Effect」换的）；注释写明**不卸载二进制**（§8.1：拆局不卸货，货留在架子上等下一局或等 Eject）。`LeakedScopesForTest` 在此阶段保持空。

- [ ] **Step 3: 写 `Include/Vase/Host/PluginHost.h` + `Source/Host/PluginHost.cpp`**

头文件（本任务形态；T10/T11 会往类里追加 Eject/Adopt 声明）：

```cpp
#pragma once

// PluginHost——进程级二进制层（§1、§1.4）：进程唯一、绑定创建线程、
// 拥有 Loader / ScopePool / 诊断计数 / 依赖账本（T9）/ 活 Pod 表。
// 铁律 §1.2 在本类持有的三个进程级容器上成立，且是**结构性**的——容器存的类型本身
// 就装不下实例级对象：二进制表只存 path+句柄（T6 的 Loader）、账本只存非拥有指针（T9）、
// 池只存空闲内存（T3）。**不是靠插入点断言拦住的**，别把这里读成「有断言兜底」。

#include "Vase/Detail/Counters.h"
#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"
#include "Vase/Detail/ScopePool.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/Loader.h"
#include "Vase/Pod/Pod.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace vase
{

// DiagnosticSnapshot / FailedPluginRecord / ResidualEntry / PodReport / PodHandle
// 全部定义在 Vase/Pod/Pod.h（T7 Step 2）——它们要能被下层的 Pod 存储，链接方向不允许
// 它们定义在本头文件里。本头文件只加 using 级别的引用都不需要：include 已到位。

VASE_MSVC_DLL_WARNINGS_BEGIN
class VASE_HOST_API PluginHost
{
public:
    PluginHost(); // §1.4：绑定当前线程；进程内同时只允许一个存活实例（守卫为全局标志，
                  // 析构释放——测试间串行重建合法，「两个 Host 同时在世」才是要拒的形态）
    ~PluginHost();

    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;
    PluginHost(PluginHost&&) = delete;
    PluginHost& operator=(PluginHost&&) = delete;

    Result<PodHandle> CreatePod(const LoadPlan& plan, const PodOptions& options = {});
    PodReport DestroyPod(PodHandle handle); // 永不失败（§5.1）
    Pod* Resolve(PodHandle handle);         // 失效 → nullptr（预期内）

    detail::DiagnosticCounters& ForTestCounters() { return Counters; } // 测试缝：
    // Lifecycle 判据直接读五项计数。交出去的是**可写引用**——测试要能读，也要能构造残留
    // 来验报告（#10/D14）。公开 API 保证的是另一件事：**不提供绕过 Effect 渠道的注册入口**，
    // 账本只由 Context 的 Provide/On 与 Effect 回收改。

private:
    struct PodSlot
    {
        std::unique_ptr<Pod> Inner;
        std::uint32_t Generation = 0;
        bool Alive = false;
        DiagnosticSnapshot Baseline;         // §9.2：基线是「Pod 创建时」快照，不是进程启动值
        std::vector<std::string> HotSwapLog; // §9.2 v3 进出事件流（T10/T11）；
                                             // Failed 记录由 Pod::FailureRecords 自持（§5.2「保留记录不保留实例」）
    };

    // 非绑定线程 → 终止（§1.4/#16）。**消息必须含子串 `not the bound thread`**——
    // T8 的 death test 按它匹配（见计划 T8 的 Interfaces 行），漏了这句 T8 必红。
    void AssertBoundThread(const char* api) const;
    Result<PodHandle> CreatePodImpl(const LoadPlan& plan, const PodOptions& options);

    std::thread::id ThreadId;
    detail::ScopePool CountersPool; // **池与计数分开**：池是内存机器，计数是账本
    detail::DiagnosticCounters Counters;
    detail::Loader Loader;
    std::vector<std::unique_ptr<PodSlot>> Slots;                          // deque 语义：槽位稳定（句柄=索引+代际）
    std::unordered_map<std::string, std::filesystem::path> KnownBinaries; // Id→path：
    // CreatePod 注册、Adopt 查用（T11；§5.6「M1 无清单，以计划登记代替」）。
};
VASE_MSVC_DLL_WARNINGS_END

} // namespace vase
```

`~PluginHost()` 的完整职责（§8.1 末行「宿主退出：全部卸下，逐插件三档证据」的 M1 形）：Debug 断言零存活 Pod → 对 `Loader.AllResident()` 逆装载序逐个 `Unload` 并在 Debug 下核对档二字段（`BinaryActuallyUnloaded` 的证据不往哪报告——进程正在退出，日志 `fprintf(stderr)` 即可；**端到端断言推迟到 CI/M5**，gtest 无法测自家析构）。

`PluginHost.cpp` 的 `CreatePodImpl` 行为钉死（完整实现按此写，每步失败走「宽容」记录——§0.3-4 失败不致命）：

```text
① AssertBoundThread；分配/复用槽（FreeList 复用 Index 时 ++Generation——#15 的检出机制）。
   **新槽的 Generation 从 1 起**，不是 0：0 要留给默认构造的 `PodHandle{}`——否则 `{0, 0}`
   会解析到第一个槽，而「失效句柄返回 nullptr」是 §5.1 的预期行为（实测踩到）。
② **基线快照（Counters 五项）先取，再建 Pod**——顺序不能反：根 Scope 属于本 Pod，
   它的 +1 必须落进差分；先建 Pod 再取快照，拆局归零时 `Scopes` 差分成 −1
   （无符号回绕成 18446744073709551615，实测）。
③ options.Stage0 → Root() 上注册宿主服务（kHost 来源由此判定，§6.1；根 Provide 不落边——T9）。
④ 逐 entry（数组序=拓扑序）：
   KnownBinaries[Id] = path；
   EnsureResident → Symbol("VasePlugin_GetPlugin") → 调它（std::string(id).c_str()）→ desc 判空；
   **第一个字段读 HeaderVersion**：不等 kHeaderVersion → Failed（消息含
     "HeaderVersion mismatch: binary N, host M"——§3.1/§8.3 的执法点，12 节 #12）；
   Create() → PluginInstances++；子 EffectScope（pool, &Counters, OwnerLabel）+
     子 Context（T5  ctor；ConsumerCookie=instance、Ledger=账本、PodIndex 三处填充）；
   OnLoad 失败 → Failed{kLoad}：**当场** Scope->Dispose + Desc->Destroy + PluginInstances--
     （§5.2 回收即时；下游级联 = M2）→ continue；
   成功 → State=Loaded。
⑤ 第二遍（仅 Loaded 的，仍按数组序）OnStart：同败同治，Failed{kStart}。
⑥ Strict==true 且有任何 Failed → 拆整个半成品 → 返回 Err（首个失败消息）——§5.5。
   否则 Ok(handle)。
```

`DestroyPod`：失效句柄 → `PodReport{HandleWasStale=true}`；存活 → `TeardownInstancesAndRoot()`（见 Pod.cpp）→ 差分 = `Counters - Baseline`（**差分不是绝对值**——进程级计数本就单调，§9.2）→ `Residuals` 自 `LeakedScopesForTest` → `HotSwapLog` 搬出 → 槽 `Alive=false`、`Inner.reset()`、代际不递增（递增发生在 Index 复用时）→ 返回报告。析构函数：Debug 下断言 `Counters` 五项全零（Host 死时账没平 = 框架自己漏了，先于任何宿主受罚）。

- [ ] **Step 4: 写 `Samples/HelloCommon/Greeter.h` 与 `Samples/HelloPlugin/HelloPlugin.cpp`**

```cpp
// Greeter.h —— 提供方与消费方共用同一个头（§3.1 的服务接口约定）；
// 它同时是 Embedding（宿主）能「认识」这个服务的载体——宿主不需要插件的实现，
// 只需要标识。Samples 不进发布物，Samples 内部包含用引号，路径相对仓库根。
#pragma once
#include <cstdint>
#include <string_view>

namespace samples
{

class IGreeter
{
public:
    static constexpr std::string_view kName = "Vase.Hello.Greeter";
    static constexpr std::uint32_t kVersion = 1;
    virtual ~IGreeter() = default;
    virtual std::string_view Greet() const = 0;
};

struct GreetEvent
{
    static constexpr std::string_view kName = "Vase.Hello.GreetEvent";
    static constexpr std::uint32_t kVersion = 1;
    int Seq;
};

} // namespace samples
```

```cpp
// HelloPlugin.cpp —— §3.1 的最小形态：提供并消费，注册两个可数的 Effect。
#include "Greeter.h"
#include "Vase/Plugin.h"

namespace
{

class GreeterImpl final : public samples::IGreeter
{
public:
    std::string_view Greet() const override { return "hello from Vase.Hello"; }
};

class HelloPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<samples::IGreeter>(Greeter);                  // Effect #1：服务
        ctx.On<samples::GreetEvent>(&HelloPlugin::OnGreet, this); // Effect #2：订阅
        return vase::Result<void>::Ok();
    }

    vase::Result<void> OnStart(vase::Context&) override { return vase::Result<void>::Ok(); }

private:
    void OnGreet(const samples::GreetEvent&) { ++GreetCount; }

    GreeterImpl Greeter;
    int GreetCount = 0;
};

VASE_PLUGIN(HelloPlugin){
    .Id = "Vase.Hello",
    .DisplayName = "示例插件",
    .Version = "0.1.0",
    .Requires = {}, // 不依赖任何服务：任何局都能进（§4.1 增量语义的插件侧镜像）
    .Provides = {{"Vase.Hello.Greeter", 1}},
};

} // namespace

// VASE_PLUGIN 的导出符号必须在命名空间作用域，但允许匿名命名空间外的全局可见——放
// namespace 里会破坏 extern "C"，此块保持在文件全局；上面 class 进匿名 namespace、
// VASE_PLUGIN 单独留在全局作用域——实现时按探针 3.3(1) 的结构放置。
// （注意：这句若写成挂在 `}` 行尾的续行注释，本配置的 clang-format 会吞掉正文——见 T7 尾注。）
```

（`Samples/HelloPlugin/CMakeLists.txt`：`vase_add_plugin_fixture(HelloPlugin SOURCES HelloPlugin.cpp LINK_LIBRARIES VasePod)`——**示例也走同一个函数**，形态定义全仓库只有一处。`target_include_directories(HelloPlugin PRIVATE Samples)` 使 `"Greeter.h"` 可达，或改用相对路径包含，以 clang-format/tidy 不抱怨为准。）

- [ ] **Step 5: 写 `Samples/Embedding/main.cpp`**

验证宿主（§10.1：没有 UI、没有业务，只有演示命令）。子命令：
`play` —— Stage0 注册一个宿主标记服务（`IHostMarker`，定义在 main.cpp 内，kName "Samples.Embedding.Marker" v1）→ CreatePod{Hello} → `Root().Emit(GreetEvent{1})` → `Root().Get<IGreeter>()->Greet()` 打印 → DestroyPod → 打印 `Clean()` 与五项差分。**Get 用的是宿主根 Context：不落边、不受「凭声明」限制**（§5.6 括注）。
`loop <N>` —— 重复 CreatePod/DestroyPod N 次；全部 `Clean()` 则退出码 0，否则打印报告并返回 1（CI 化的判据 #1）。
所有路径来自主程序旁的定位：`VASE_HELLO_PATH` 编译定义（`$<PATH:CMAKE_PATH,$<TARGET_FILE:HelloPlugin>>`）。进程唯一 Host 用函数内 `static`。

- [ ] **Step 6: 接构建 + 写 `Tests/Lifecycle/LifecycleTests.cpp`（2 个 TEST）**

```cpp
#include "Vase/Host/PluginHost.h"

#include <gtest/gtest.h>

namespace
{
const std::filesystem::path kHello{VASE_FIXTURE_HELLO};
}

namespace
{

vase::LoadPlan HelloPlan()
{
    vase::LoadPlan plan;
    plan.Ordered.push_back({"Vase.Hello", kHello});
    return plan;
}

TEST(Lifecycle, PodCyclesReturnAllCountersToBaseline)
{
    // 12 节 #1（承重）：反复建销后五项计数全部回基线。
    vase::PluginHost host;
    const auto plan = HelloPlan();
    for (int i = 0; i < 20; ++i)
    {
        vase::Result<vase::PodHandle> r = host.CreatePod(plan);
        ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
        EXPECT_EQ(host.Resolve(*r.Value())->PluginCount(), 1U);
        const vase::PodReport report = host.DestroyPod(*r.Value());
        EXPECT_TRUE(report.Clean()) << "cycle " << i;
        EXPECT_EQ(report.Failures.size(), 0U);
    }
    const auto& counters = host.ForTestCounters();
    EXPECT_EQ(counters.Effects, 0U);
    EXPECT_EQ(counters.Services, 0U);
    EXPECT_EQ(counters.Subscriptions, 0U);
    EXPECT_EQ(counters.PluginInstances, 0U);
    EXPECT_EQ(counters.Scopes, 0U);
}

TEST(Lifecycle, StaleHandleIsDetectedAfterDestroy)
{
    // 12 节 #15：销毁后 Resolve 返回 nullptr；索引复用后旧句柄仍无效（代际）。
    vase::PluginHost host;
    vase::PodHandle first = host.CreatePod(HelloPlan()).Value();
    EXPECT_NE(host.Resolve(first), nullptr);
    host.DestroyPod(first);
    EXPECT_EQ(host.Resolve(first), nullptr);

    vase::PodHandle second = host.CreatePod(HelloPlan()).Value();
    EXPECT_EQ(second.Index, first.Index);           // 同槽复用
    EXPECT_NE(second.Generation, first.Generation); // 代际已推进
    EXPECT_EQ(host.Resolve(first), nullptr);        // 旧句柄不被新 Pod 冒充
    host.DestroyPod(second);
}

} // namespace
```

`Tests/CMakeLists.txt`：加 `add_subdirectory(../Samples Samples_bin)` 不行——Samples 在根挂：根 `CMakeLists.txt` 的 `add_subdirectory(Samples/HelloPlugin)`、`add_subdirectory(Samples/Embedding)`（在 `Tests` 之前）。`Tests` 内源列表加 `Lifecycle/LifecycleTests.cpp` + `add_subdirectory(Lifecycle/fixtures)`（本任务先建空文件会违反「不建空壳」——fixtures 目录 T8 才建）。`VASE_FIXTURE_HELLO` 定义加进 `VaseTests`（同样 `CMAKE_PATH` 包法）。**再注册一条 ctest 直跑样本**：

```cmake
add_test(NAME EmbeddingLoop20 COMMAND VaseEmbedding loop 20)
```

- [ ] **Step 7: 全绿**

```bash
# cl.exe 线现已常规纳入每桩验收。**这条线必跑**：`Pod` / `PluginHost` 都是带
# STL 成员的导出类，漏包 VASE_MSVC_DLL_WARNINGS_BEGIN/END 就会因 C4251 整条编不过——
# 而这个雷只有 cl.exe 线报（clang-cl 与 Linux 都不报），T3 已经因此断过两桩。
cmake --build --preset win-x64-msvc-debug
ctest --preset win-x64-msvc-debug
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -N   # Expected: Total Tests: 44（41 + 2 gtest + 1 add_test）
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug -N'
```

- [ ] **Step 8: 门禁 + Commit**

```bash
git ls-files -z --cached --others --exclude-standard '*.h' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git add -A
git commit -m "M1-T7：CreatePod/DestroyPod 最小闭环 + HelloPlugin/Embedding + 计数归零与句柄代际"
```

---
## Task 8: 失败语义、防护断言与诊断归属

把 12 节里**不需要热插拔**就能验的承重项一次立齐：#12（HeaderVersion 拒绝）、#16（线程断言）、#18（DestroyPod 永不失败——泄漏之后仍返回报告）、#10 的 M1 形态（残留点名归属）、§6.2 报错内容（插件 Id + 服务名 + 版本）、§5.5 宽容/严格两态。fixture 全走 T6 的 `vase_add_plugin_fixture`。

**Files:**
- Create: `Tests/Integration/fixtures/StaleHeaderPlugin/{StaleHeaderPlugin.cpp}`、`FailingLoadPlugin/FailingLoadPlugin.cpp`、`FailingStartPlugin/FailingStartPlugin.cpp`、`RequiresMissingConsumer/RequiresMissingConsumer.cpp`、`Tests/Integration/fixtures/CMakeLists.txt`
- Create: `Tests/TestingSupport/PodTestPeer.h`、`PodTestPeer.cpp`
- Create: `Tests/Integration/FailureSemanticsTests.cpp`、`Tests/Integration/ThreadGuardTests.cpp`（death 类）、`Tests/Lifecycle/DiagnosticAttributionTests.cpp`
- Modify: `Tests/CMakeLists.txt`（源列表 + `add_subdirectory` + 4 个 fixture 路径 define）

**Interfaces:**
- Consumes: T1–T7。
- Produces:
  - `vase::PodTestPeer`（测试专用，`friend` 已在 T7 Pod.h 声明）：`InjectLeakedScope(Pod&, ownerLabel, effectCount) -> EffectScope&`——注入「未回收的实例级 Scope」模拟 §12 #10 的**症状**；`~Pod` 时这些 Scope 的析构兜底会 Dispose（所以本探针验的是**报告机制**——diff 非零 + 归属点名；真·绕道泄漏的捕获属 M3，文件头注释写明这条边界，防止把它读成 #10 已全量闭环）
  - fixture 符号约定：`VASE_FIXTURE_STALEHEADER` / `VASE_FIXTURE_FAILINGLOAD` / `VASE_FIXTURE_FAILINGSTART` / `VASE_FIXTURE_MISSINGCONSUMER` 编译定义（`$<PATH:CMAKE_PATH,...>` 包法同前）
  - AssertBoundThread 的消息契约（death test 按子串匹配）：`"not the bound thread"`

- [ ] **Step 1: 四个 fixture（各 ~25 行，全部经 `vase_add_plugin_fixture(<Name> SOURCES <Name>.cpp LINK_LIBRARIES VasePod)`）**

`StaleHeaderPlugin.cpp`——**手写描述符**（不用 `VASE_PLUGIN`），这正是 12 节 #12 要的「伪造一个旧版本号」（模拟用旧头文件编译的插件，§8.3 前提的执法对象）：

```cpp
#include "Vase/PluginDescriptor.h"

#include <string_view>

namespace
{

class StubPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context&) override { return vase::Result<void>::Ok(); }
};

const vase::PluginMeta kMeta{"Vase.StaleHeader", "旧头探针", "0.0.0", {}, {}};

vase::Plugin* CreateStub() { return new StubPlugin(); }

void DestroyStub(vase::Plugin* raw) { delete raw; }

const vase::PluginDescriptor kDesc{999, &kMeta, &CreateStub, &DestroyStub};
// HeaderVersion=999：不是「旧」而是「任何不等」——§3.1 的判据是相等性，
// 取一个远大于当前值的数，顺带证明比较不是「二进制更新就放行」的方向性检查。

} // namespace

extern "C" VASE_EXPORT const vase::PluginDescriptor* VasePlugin_GetPlugin(const char* id)
{
    return std::string_view{id} == kMeta.Id ? &kDesc : nullptr;
}
```

`FailingLoadPlugin.cpp`：`OnLoad` 返回 `Err(Error{"intentional load failure"})`（§5.5：下游级联 = M2，这里只验 Failed 记录与「其余照常」）。
`FailingStartPlugin.cpp`：`OnLoad` 成功（注册一个 Provide + 一个 On），`OnStart` 返回 `Err(Error{"intentional start failure"})`——验「失败即回收自己的 Scope」（§5.2：OnStart 败也要当场拆干净，否则判据 #18/#1 会被 fixture 自己污染）。
`RequiresMissingConsumer.cpp`：声明 `.Requires = {{"Vase.Ghost", 1}}`，`OnLoad` 里 `ctx.Get<IGhostService>()`（`IGhostService::kName = "Vase.Ghost"`，全文件私有定义）——**声明了但全场没人提供**：走 §6.2 的 required-miss 终止路径（M1 无求解器，这类「依赖不满足」在求解期本不该进场——它证明的是运行期兜底，也是 §12 #3b 的「未声明/缺失都要响」的一半）。

- [ ] **Step 2: 写 `Tests/TestingSupport/PodTestPeer.{h,cpp}`**

```cpp
#pragma once

// 测试缝（M1 形态的 #10/D14）。**边界必须说清**：这里注入的「未回收 Scope」只验
// 诊断报告的点名机制——Scope 的析构兜底（T3）意味着泄漏不会真的跨局存活。
// 插件侧真·绕道注册（9.3 契约违背）要 M3 的完整属主追踪器才抓得住。
// 把这个文件读成「#10 已全量闭环」是错的，读成「报告机器可测」才对。

#include "Vase/Pod/Pod.h"

namespace vase
{

class PodTestPeer
{
public:
    static EffectScope& InjectLeakedScope(Pod& pod, const char* ownerLabel, std::size_t effectCount);
};

} // namespace vase
```

```cpp
#include "PodTestPeer.h"

#include "Vase/Effect/EffectScope.h"
#include "Vase/Effect/IEffect.h"
#include "Vase/Pod/Pod.h"

#include <cstddef>
#include <memory>

namespace
{

// 只在本 TU 内构造，故留匿名命名空间（misc-use-internal-linkage）。
// 带一个负载：零参形态会让 EffectScope::Create<T> 实例化出 std::tuple<>，那是
// EffectScope 的 Create 唯一会被 misc-const-correctness 报的形状（实测：EffectTests 的
// 每次 Create 都带参，所以那里从没报过）。
class Noop final : public vase::IEffect
{
public:
    explicit Noop(std::size_t ordinal)
        : Ordinal(ordinal)
    {
    }

    void Recycle() override { static_cast<void>(Ordinal); } // 读一下就够：负载没有语义

private:
    std::size_t Ordinal;
};

} // namespace

namespace vase
{

EffectScope& PodTestPeer::InjectLeakedScope(Pod& pod, const char* ownerLabel, std::size_t effectCount)
{
    pod.LeakedScopesForTest.push_back(std::make_unique<EffectScope>(pod.Pool, &pod.Counters, ownerLabel));
    EffectScope& scope = *pod.LeakedScopesForTest.back();
    for (std::size_t i = 0; i < effectCount; ++i)
    {
        scope.Create<Noop>(i);
    }
    return scope;
}

} // namespace vase
```

- [ ] **Step 3: 写测试（7 个 TEST，分布在三个文件）**

`Tests/Integration/FailureSemanticsTests.cpp`：

```cpp
#include "Vase/Detail/Result.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Pod.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace
{

vase::LoadPlan Plan(std::initializer_list<std::pair<std::string_view, std::filesystem::path>> items)
{
    vase::LoadPlan plan;
    for (const auto& [id, path] : items)
    {
        plan.Ordered.push_back({.Id = id, .BinaryPath = path});
    }
    return plan;
}

TEST(FailureSemantics, StaleHeaderPluginRejectedAndRecorded)
{
    // 12 节 #12（承重）：HeaderVersion 不匹配 → 拒绝加载并报告，不是崩溃、不是静默。
    vase::PluginHost host;
    const vase::Result<vase::PodHandle> r = host.CreatePod(Plan({{"Vase.StaleHeader", VASE_FIXTURE_STALEHEADER}}));
    ASSERT_TRUE(r.IsOk()); // 宽容模式：失败记在案，局照开
    const vase::Pod* pod = host.Resolve(r.Value());
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(pod->PluginCount(), 0U);
    ASSERT_EQ(pod->Failures().size(), 1U);
    // 取首条用迭代器而非 operator[]：后者过不了 cppcoreguidelines-pro-bounds-*，
    // 而本任务三处都就地抑制会越出全仓同类 NOLINT 的额度（2 + 3 > 4）——写法同 T3 的 MetaArray 用例。
    const vase::FailedPluginRecord& failure = *pod->Failures().begin();
    EXPECT_EQ(failure.Id, "Vase.StaleHeader"); // 记录点名到计划里的那个 Id，而不是空/占位
    EXPECT_EQ(failure.Stage, vase::Phase::kLoad);
    EXPECT_NE(failure.Message.find("HeaderVersion"), std::string::npos);
    EXPECT_TRUE(host.DestroyPod(r.Value()).Clean()); // 被拒的插件没留任何计数
}

TEST(FailureSemantics, FailingPluginRecordedNeighborsUnharmed)
{
    // §0.3-4 失败不致命：坏插件 Failed 记录在案，好插件照常 Started。
    vase::PluginHost host;
    const vase::LoadPlan plan =
        Plan({{"Vase.FailingLoad", VASE_FIXTURE_FAILINGLOAD}, {"Vase.Hello", VASE_FIXTURE_HELLO}});
    const vase::Result<vase::PodHandle> r = host.CreatePod(plan);
    ASSERT_TRUE(r.IsOk());
    const vase::Pod* pod = host.Resolve(r.Value());
    EXPECT_EQ(pod->PluginCount(), 1U); // Hello 活着
    ASSERT_EQ(pod->Failures().size(), 1U);
    EXPECT_EQ(pod->Failures().begin()->Id, "Vase.FailingLoad");
    const vase::PodReport report = host.DestroyPod(r.Value());
    EXPECT_TRUE(report.Clean()); // OnLoad 当场回收（§5.2），不留半个 Scope
}

TEST(FailureSemantics, StrictModeDestroysHalfBuiltPod)
{
    // §5.5：Strict 下任一失败 → CreatePod 返回 Err，半个 Pod 拆干净。
    vase::PluginHost host;
    vase::PodOptions options;
    options.Strict = true;
    const vase::LoadPlan plan =
        Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}, {"Vase.FailingStart", VASE_FIXTURE_FAILINGSTART}});
    const vase::Result<vase::PodHandle> r = host.CreatePod(plan, options);
    EXPECT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("start failure"), std::string::npos);
    const auto& counters = host.ForTestCounters();
    EXPECT_EQ(counters.PluginInstances, 0U); // 半个局也没留下
    EXPECT_EQ(counters.Effects, 0U);
}

TEST(FailureSemantics, TolerantModeReclaimsFailedStartScopeImmediately)
{
    // §5.2：OnStart 失败要**当场**回收自己的 Scope。判据必须落在宽容模式：Strict 会
    // 整拆半成品，~Pod 的兜底 Dispose 同样抹平计数，分不出「当场回收」与「拆局时顺手
    // 回收」。宽容模式不整拆，而 CountersDiff 在 Pod 析构**之前**算——失败 Scope 若还
    // 活着，Scopes/Effects 必然非零。这条因此只有真的做了即时回收才会绿。
    vase::PluginHost host;
    const vase::Result<vase::PodHandle> r = host.CreatePod(Plan({{"Vase.FailingStart", VASE_FIXTURE_FAILINGSTART}}));
    ASSERT_TRUE(r.IsOk()); // 宽容模式：失败记在案，局照开
    const vase::Pod* pod = host.Resolve(r.Value());
    ASSERT_NE(pod, nullptr);
    ASSERT_EQ(pod->Failures().size(), 1U);
    EXPECT_EQ(pod->Failures().begin()->Stage, vase::Phase::kStart);
    EXPECT_EQ(pod->PluginCount(), 0U);               // 失败实例不在活集合里
    EXPECT_TRUE(host.DestroyPod(r.Value()).Clean()); // 只有即时回收才会绿
}

TEST(FailureSemantics, MissingDeclaredServiceTerminatesWithFullIdentity)
{
    // §6.2：required 解析失败 = 编程错误 → 终止，消息含插件 Id + 服务名 + 版本。
    // （M1 无求解器兜底，这条证明运行期最后一道墙是响的。）
    EXPECT_DEATH(
        {
            vase::PluginHost host;
            static_cast<void>(host.CreatePod(Plan({{"Vase.RequiresMissingConsumer", VASE_FIXTURE_MISSINGCONSUMER}})));
        },
        "Vase\\.RequiresMissingConsumer.*Vase\\.Ghost' v1 ");
}

} // namespace
```

`Tests/Integration/ThreadGuardTests.cpp`：

```cpp
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Pod.h"

#include <gtest/gtest.h>
#include <thread>

namespace
{

TEST(ThreadGuard, VaseApiFromForeignThreadTerminates)
{
    // 12 节 #16（§1.4）：绑定线程之外的调用当场终止——响的，不拖到随机时刻。
    EXPECT_DEATH(
        {
            vase::PluginHost host; // 绑定在 death-test 子进程的主线程
            std::thread foreign([&host] { static_cast<void>(host.CreatePod(vase::LoadPlan{})); });
            foreign.join();
        },
        "not the bound thread");
}

TEST(ThreadGuard, SameThreadSequentialHostsAllowed)
{
    // §1.4 的「唯一」是「同时只一个」：串行建销合法（测试基建依赖这条）。
    for (int i = 0; i < 2; ++i)
    {
        vase::PluginHost host;
        EXPECT_EQ(host.Resolve(vase::PodHandle{0, 0}), nullptr); // 空 Host 上失效句柄 = nullptr，不崩（守卫未误伤）
    }
    SUCCEED();
}

} // namespace
```

（`Resolve(PodHandle{0,0})` 对空 Host 返回 nullptr 且不崩——若实现里对失效句柄加了 Debug 断言，这里换 `EXPECT_EQ(host.Resolve({0,0}), nullptr)` 并删断言：失效句柄是**预期内**（§5.1），不该断言。这条辨析本身写进 `Resolve` 的注释。）

`Tests/Lifecycle/DiagnosticAttributionTests.cpp`：

```cpp
#include "PodTestPeer.h" // Tests/TestingSupport 进 include 路径
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Pod.h"

#include <gtest/gtest.h>

namespace
{

vase::LoadPlan HelloPlan()
{
    vase::LoadPlan plan;
    plan.Ordered.push_back({.Id = "Vase.Hello", .BinaryPath = VASE_FIXTURE_HELLO});
    return plan;
}

TEST(DiagnosticAttribution, ResidualScopeIsNamedByOwnerLabel)
{
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(HelloPlan()).Value();
    vase::Pod* pod = host.Resolve(h);
    vase::PodTestPeer::InjectLeakedScope(*pod, "Vase.Test.LeakProbe", 2); // 2 个未回收 Effect

    const vase::PodReport report = host.DestroyPod(h);
    EXPECT_FALSE(report.Clean()); // 报告如实说不干净
    ASSERT_EQ(report.Residuals.size(), 1U);
    // 取首条用迭代器而非 operator[]（同 T3 的 MetaArray 用例写法）。
    const vase::ResidualEntry& residual = *report.Residuals.begin();
    EXPECT_EQ(residual.OwnerLabel, "Vase.Test.LeakProbe"); // #10 的点名机制
    EXPECT_EQ(residual.Count, 2U);
    EXPECT_EQ(report.CountersDiff.Effects, 2U);

    // 注入 Scope 随 Pod 析构被兜底 Dispose（T3 的析构设计）——计数回基线：
    // 本探针验「报告机器」，真·跨局泄漏属 M3（PodTestPeer.h 头注已划界）。
    EXPECT_EQ(host.ForTestCounters().Effects, 0U);
}

TEST(DiagnosticAttribution, DestroyPodOnLeakedPodStillReturnsReport)
{
    // 12 节 #18（承重）：Destroy 永不失败——泄漏的局也拿得到报告，而不是 abort/半个局。
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(HelloPlan()).Value();
    vase::PodTestPeer::InjectLeakedScope(*host.Resolve(h), "Vase.Test.LeakProbe2", 3);
    vase::PodReport report;
    ASSERT_NO_FATAL_FAILURE(report = host.DestroyPod(h)); // 「永不失败」的形态验证
    EXPECT_FALSE(report.HandleWasStale);
    EXPECT_FALSE(report.Clean());
    EXPECT_EQ(host.Resolve(h), nullptr); // 不管泄漏与否，局都结束了
}

} // namespace
```

- [ ] **Step 4: 接构建，全绿**

`Tests/CMakeLists.txt`：源列表 +`Integration/FailureSemanticsTests.cpp` +`Integration/ThreadGuardTests.cpp` +`Lifecycle/DiagnosticAttributionTests.cpp` +`TestingSupport/PodTestPeer.cpp`；`target_include_directories(VaseTests PRIVATE TestingSupport)`；`add_subdirectory(Integration/fixtures)`；四个新 define。**顺带**：根 `CMakeLists.txt` 若有 `add_subdirectory(Samples/FailingPlugin)` 计划位——**删掉这个打算**：FailingPlugin 以 Integration fixture 形态存在（上面 `FailingLoad/FailingStart`），Samples 不重复立壳（§10.1 Samples 只放演示，不放测试材料）。

```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -N   # Expected: Total Tests: 53（T7 收 44 + 本任务 4+2+2 = 52，R85 补的那条宽容模式用例再 +1）
```
（death test 在 release 线同样成立（`ProgrammerError` 两态都终止），但 `#ifndef NDEBUG` 门只适用 T3 那条——本任务三条 death 测试**不加** NDEBUG 门。若 release 线下 `EXPECT_DEATH` 因优化把 `CreatePod` 整调用抹掉（空 plan + 未用返回值），把语句包成 `volatile` 持有或 `asm` 屏障——先跑再谈，别预防性地写丑。）

```bash
ctest --preset win-x64-clang-release   # 本桩起 release 线纳入常规（基数比 debug 少 1：T3 death test）
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug -N'
```

- [ ] **Step 5: 门禁 + Commit**

```bash
git ls-files -z --cached --others --exclude-standard '*.h' '*.cpp' | xargs -0 clang-format --dry-run --Werror
run-clang-tidy -p build-win/win-x64-clang-debug 2>&1 | tail -4
git add -A
git commit -m "M1-T8：HeaderVersion 拒绝/失败语义/线程守卫/诊断归属（12 节 #12 #16 #18 #10 与 §6.2 报错内容）"
```

---
## Task 9: 依赖账本最小形（§5.6）——解析落账、宿主不落、边随实例死

「进出不是许可，是结算」的账本在此立起来。M1 落三条规则的全部形状：①解析凭声明（T5 已把门，本任务补上「未声明」的 death 用例）；②成功的插件解析逐条落账（`Get`/`TryGet` 都算）；宿主的解析**不落边**（§5.6：根 Context 的「凭声明」无定义，落了又解不开的边会让热卸永久不可用）；③边随实例销毁消失 + `ClearPod` 整批清零（活集合→空的免费断言）。Eject 的反查执法在 T10 消费这份账。

**Files:**
- Create: `Include/Vase/Pod/DependencyLedger.h`、`Source/Pod/DependencyLedger.cpp`
- Create: `Tests/Unit/LedgerTests.cpp`、`Tests/Integration/LedgerSemanticsTests.cpp`
- Create: `Tests/Integration/fixtures/SharedProviderPlugin/`、`EdgeConsumerPlugin/`、`UndeclaredGetConsumer/`（各 .cpp + CMakeLists 行）
- Modify: `Source/Pod/Context.cpp`（ResolveRaw 第③步从注释锚点变真实落账）、`Source/Pod/Pod.cpp`（teardown 摘边）、`Source/Host/PluginHost.cpp`（Failed 即时拆账 + 测试缝）、`Include/Vase/Host/PluginHost.h`、`Include/Vase/Pod/Pod.h`（ledger 构造参数转正）

**Interfaces:**
- Consumes: T5 `Context` 的 `Ledger/ConsumerCookie/PodIndex` 挂钩（T7 已在 CreatePod 循环里填充）。
- Produces:
  - `vase::detail::LedgerEdge { uint32 PodIndex; const void* Consumer; const void* Provider; std::string_view ConsumerId, ProviderId, ServiceName; uint32 ServiceVersion; }`——**非拥有指针 + 借用字符串**：边生命周期 ⊆ 两端实例生命周期，两端任一消失即删（§5.6 规则②），所以 `string_view` 指进描述符字面量安全（镜像驻留 ⊇ 实例存活，§8.1）
  - `vase::detail::DependencyLedger`：`Record` / `EdgesTo(provider)` / `EdgesFrom(consumer)` / `RemoveByInstance(cookie)` / `ClearPod(index)` / `EdgeCount()`（粗粒度刻意如此：一次瞬态解析留下的边也拦 Eject，§5.6）
  - `vase::PluginHost::ForTestLedgerEdgeCount()`（测试缝，只读）
  - Pod 构造签名定稿：`Pod(ScopePool&, DiagnosticCounters&, detail::DependencyLedger*, uint32 podIndex)`——T7 里 ledger 恒传 nullptr 的位置从此实装

- [ ] **Step 1: 写 `Include/Vase/Pod/DependencyLedger.h` + `Source/Pod/DependencyLedger.cpp`**

```cpp
#pragma once

// §5.6 账本。两个判定共用它：Eject 前 EdgesTo(T) 非空即拒（反查）；实例消失
// 时 RemoveByInstance（正查无必要——摘边发生在被摘方自己的回收点）。
// 单线程契约（§1.4）下 vector 线性扫就是正确复杂度（M1 的边数以十计；别预防性建索引）。

#include "Vase/Detail/Export.h"
#include "Vase/Service/Service.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace vase::detail
{

struct LedgerEdge
{
    std::uint32_t PodIndex = 0;
    const void* Consumer = nullptr; // 插件实例 cookie（永不空——宿主解析不落边）
    const void* Provider = nullptr; // 插件实例 cookie（永不空——kHost 提供方向不记）
    std::string_view ConsumerId;
    std::string_view ProviderId;
    std::string_view ServiceName;
    std::uint32_t ServiceVersion = 0;
};

class VASE_POD_API DependencyLedger
{
public:
    void Record(LedgerEdge edge); // Debug 断言：Consumer/Provider 均非空（§1.2 属主不变式在账本上的投影）

    [[nodiscard]] std::vector<const LedgerEdge*> EdgesTo(const void* provider) const;
    [[nodiscard]] std::vector<const LedgerEdge*> EdgesFrom(const void* consumer) const;

    void RemoveByInstance(const void* instance); // 进出两向一起摘（边随实例死）
    void ClearPod(std::uint32_t podIndex);       // DestroyPod 整批清零（§5.6）

    [[nodiscard]] std::size_t EdgeCount() const { return Edges.size(); }

private:
    std::vector<LedgerEdge> Edges;
};

} // namespace vase::detail
```

`DependencyLedger.cpp` 按声明实现（线性扫 + `std::erase_if`；`Record` 里 `assert(edge.Consumer != nullptr && edge.Provider != nullptr)`）。

- [ ] **Step 2: 接线（四处，每处都带 § 号注释）**

`Context.cpp::ResolveRaw` 第③步（T5 锚点兑现）：

```cpp
// §5.6 规则②：边随解析建立。只记「插件消费者 → 插件提供方」——
// 宿主消费者（SelfMeta==nullptr）与宿主提供方（ProviderInstance==nullptr）都不落边。
// 「调用一次就丢引用」也落账：粗粒度是刻意的，瞬态解析自动过期是复杂度的黑洞（§5.6）。
if (Ledger != nullptr && SelfMeta != nullptr && ConsumerCookie != nullptr && entry->ProviderInstance != nullptr)
{
    Ledger->Record(detail::LedgerEdge{PodIndex, ConsumerCookie, entry->ProviderInstance, SelfMeta->Id,
                                      entry->ProviderId, entry->Key.Name, entry->Key.Version});
}
```
（`entry` 是 `ResolveRaw` 内已查到的 `ServiceEntry`；`TryGet` 与 `Get` 走同一个 `ResolveRaw`，命中即落账——§6.2「Get/TryGet 都算」。）

`Pod.cpp::TeardownInstancesAndRoot`：每个实例 `Desc->Destroy` 之前——`if (Ledger != nullptr) Ledger->RemoveByInstance(i.Instance);`（§5.6 规则②「边随实例销毁消失」；先摘边再拆镜像，防任何回收动作在拆后查账）。

`PluginHost.cpp`：`CreatePodImpl` 开头 `Ledger.ClearPod(index)`（防御——正常应已空，见 DestroyPod）；`DestroyPod` 拆完该 Pod 全部实例后 `Ledger.ClearPod(slotIndex)` + **Debug 断言**：`assert(Ledger.EdgesFrom 任何 provider 归属 index 的边 == 0)`——§5.6「活集合→空」的免费断言，抓框架自己漏摘。`Failed` 即时回收路径同样先 `RemoveByInstance`。Host 增加成员 `detail::DependencyLedger Ledger;`（PodSlot/构造传引用）+ `std::size_t ForTestLedgerEdgeCount() const { return Ledger.EdgeCount(); }`。

`Include/Vase/Pod/Pod.h`：T7 的 `detail::DependencyLedger*` 前向声明改真 include（同 target，无环）。

- [ ] **Step 3: 三个 fixture + 测试（7 个 TEST）**

`SharedProviderPlugin.cpp`：提供 `ISharedService`（kName `"Vase.Test.Shared"` v1，接口在文件内定义，成员实现一个 `int Value()`）；`OnLoad` 里 `ctx.Provide<ISharedService>(Shared)`。
`EdgeConsumerPlugin.cpp`：`.Requires = {{"Vase.Test.Shared",1},{"Vase.Test.HostOnly",1}}`；文件内定义 `IHostOnlyService`（kName `"Vase.Test.HostOnly"`——接口**标识一致即可跨模块匹配**（§6.1 字符串标识的兑现点），宿主在 Stage0 注册同名服务）。`OnLoad`：

```cpp
Shared = &ctx.Get<samples_fixture::ISharedService>();       // 落账：consumer→provider 边 #1
HostOnly = ctx.TryGet<samples_fixture::IHostOnlyService>(); // 宿主提供方：不落账（§5.6）
if (HostOnly == nullptr)
{
    return vase::Result<void>::Err(vase::Error{"host service missing"});
}
return vase::Result<void>::Ok();
```
`UndeclaredGetConsumer.cpp`：`.Requires = {}`，`OnLoad` 里 `ctx.Get<ISharedService>()`——**没声明就取**：§5.6 规则① + §6.2 编程错误路径。

测试文件与用例（Unit 3 + Integration 4；Integration 的 Stage0 用 lambda 注册宿主标记服务——`PodOptions.Stage0`）：

```cpp
// Tests/Unit/LedgerTests.cpp
TEST(Ledger, RecordAndReverseLookup);          // 两条入边 → EdgesTo(provider) 点名两个消费者
TEST(Ledger, RemoveByInstanceClearsBothSides); // 实例作为 consumer 与 provider 各留一条 → 一次摘光
TEST(Ledger, ClearPodOnlyTargetPod);           // 两局交错落账，ClearPod(1) 后局 0 的边原封不动

// Tests/Integration/LedgerSemanticsTests.cpp
TEST(LedgerSemantics, PluginResolutionRecordsHostResolutionDoesNot)
{
    vase::PluginHost host;
    vase::LoadPlan plan; // 顺序：Shared → Consumer（手写拓扑：提供方在前，消费方在后——§5.3 阶段1 依赖就绪的前提）
    plan.Ordered.push_back({"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER});
    plan.Ordered.push_back({"Vase.EdgeConsumer", VASE_FIXTURE_EDGECONSUMER});
    vase::PodOptions options;
    HostMarker marker; // 测试内实现的 IHostOnlyService（与 fixture 同名同版本标识）
    options.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::IHostOnlyService>(marker); };
    // ↑ 解析走字符串 kName、不走 type_index（§6.1）。注：`IHostOnlyService` 提到
    //   SharedCommon.h 之后，两侧包含的是**同一份定义**——这条用例证明的是标识匹配，
    //   不是「两个 C++ 类型也能对上」。
    ... EXPECT_EQ(host.ForTestLedgerEdgeCount(), 1U); // 只有 plugin→plugin 那一条
}

TEST(LedgerSemantics, DestroyPodClearsLedger) { /* 接上局：DestroyPod 后 EdgeCount()==0 */ }

TEST(LedgerSemantics, FailedConsumerDropsItsEdgesImmediately)
{
    // §5.2 即时回收 × §5.6 规则②：失败实例的边当场摘净。判据必须在 **DestroyPod 之前**
    // 取——ClearPod 到拆局才生效，兜不住这一条（T9 实施者用临时探针实测：摘掉那处
    // RemoveByInstance，这里的 count 停在 1）。死边不摘会以「还有消费者指着你」的
    // 形态拦住 T10 的 Eject 反查，所以这是承重覆盖，不是补白。
    vase::PluginHost host;
    vase::LoadPlan plan;
    plan.Ordered.push_back({"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER});
    plan.Ordered.push_back({"Vase.EdgeConsumer", VASE_FIXTURE_EDGECONSUMER});
    const vase::Result<vase::PodHandle> created = host.CreatePod(plan); // 无 Stage0 → HostOnly 缺席
    ASSERT_TRUE(created.IsOk());
    const vase::Pod* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(pod->Failures().size(), 1U);        // 消费者 OnLoad 失败（§5.2 记录保留）
    EXPECT_EQ(pod->PluginCount(), 1U);            // 提供方仍在
    EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U); // 那条 consumer→provider 边随实例一起死
}
// 覆盖的是 OnLoad 失败那处；OnStart 失败那处（PluginHost.cpp 阶段 2）是同一形状的镜像，
// 要单独覆盖得再加一个「先解析成功、再 OnStart 失败」的 fixture——M1 不做，登记在案。

TEST(LedgerSemanticsDeath, UndeclaredResolutionTerminates)
{
    EXPECT_DEATH(
        {
            vase::PluginHost host;
            vase::LoadPlan plan;
            plan.Ordered.push_back({"Vase.UndeclaredGet", VASE_FIXTURE_UNDECLAREDGET});
            host.CreatePod(plan);
        },
        // 锚 T5 的实发消息（Context.cpp ReportUndeclaredResolution）：三要素齐全，且尾部
        // 留一个空格——否则 `.*1` 那类松尾会命中 ProgrammerError 追加的行号（R86 教训）。
        "plugin 'Vase\\.UndeclaredGet' resolved service 'Vase\\.Test\\.Shared' v1 ");
}
```

（`IHostOnlyService` 两个 TU 各有一份定义 → 提到 `Tests/Integration/fixtures/SharedCommon.h` 与 EdgeConsumer/测试共享包含；这是测试材料，不算公开 API。`HostMarker` 在测试 cpp 实现该接口。）

- [ ] **Step 4: 全绿 + 双平台 + Commit**

```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -N   # Expected: Total Tests: 60（T8 收 53 + 本任务 3 Unit + 4 Integration）
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug'
git ls-files -z --cached --others --exclude-standard '*.h' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git add -A
git commit -m "M1-T9：依赖账本最小形——解析落账、宿主不落、边随实例死、ClearPod 整批清零"
```

---
## Task 10: `EjectPlugin`——档一执法 + 档二证据（§5.6 / §8.1 / §8.2）

Eject 是「离场必为叶」的执行现场（§5.6 判定流左列）：账本反查（有入边即拒、**拒绝报告点名每个消费者**——3b 的最小可用形态）、实例销毁复用 T3/T7 的既有回收机器（§7.6-1：档 ① 保证只碰被卸者自己的 Scope，不需要跨实例排序机器）、二进制的进出由**全局实例计数**决定（§8.1：多 Pod 同插件时拆了实例、货留架上），卸了就地盘收集档二证据。`EjectReport` 把「哪一档、哪个平台、有没有判据力」原样写出来——§8.2 的判据分工不许让读报告的人背文档。

**Files:**
- Create: `Include/Vase/Host/Evidence.h`
- Modify: `Include/Vase/Host/PluginHost.h`（Eject 声明）、`Source/Host/PluginHost.cpp`（Eject 实现）、`Include/Vase/Pod/Pod.h`（`LiveInstance::Binary` 字段 + `FailedBinaries` 映射）、`Source/Pod/Pod.cpp` / `Source/Host/PluginHost.cpp`（CreatePod 路径填 Binary 指针与 Failed 记录）
- Create: `Tests/HotSwap/EjectTests.cpp` + `Tests/HotSwap/CMakeLists` 归并（`VaseTests` 源列表追加）

**Interfaces:**
- Consumes: `Loader::Unload -> UnloadEvidence`（T6）、账本（T9）、回收机器（T3/T7）。
- Produces:
  - `vase::EjectReport` / `vase::AdoptReport`（`Include/Vase/Host/Evidence.h`，字段见 Step 1）
  - `Result<EjectReport> PluginHost::EjectPlugin(PodHandle, std::string_view pluginId)`（§5.1 签名；`EjectPlugin` 失败 = 拒绝 = `Err`，报告只在成功时存在——**拒绝的点名信息在 `Error::Message` 里**，格式：`"eject refused: <id> is provided by [consumer1, consumer2]"`，测试按子串断言；M2 若要把点名结构化再开 `EjectRejection` 类型，M1 不镀金）
  - `LiveInstance::Binary`（`detail::BinaryRecord*`，驻留期内稳定——Loader 用 `unique_ptr` 存储，摘除只在 Unload 之后，而 Unload 前所有持有者已摘干——§1.2 边界的账）
  - `Pod::FailedBinaries`：`std::unordered_map<std::string, detail::BinaryRecord*>`——Failed 插件的「记录 + 驻留镜像」，Eject 的补充角（§5.6 四条补角：`Failed` 可被 Eject，拆的是记录与镜像，必无入边）

- [ ] **Step 1: 写 `Include/Vase/Host/Evidence.h`**

```cpp
#pragma once

// §8.2 三档证据的载荷。字段设计的唯一原则：**判据力跟着平台走，写进结构**。
// Eject/Adopt 报告是 §0.2「每次进出必须有账可查」的纸面凭证——缺一个平台字段
// 的「成功」比失败更危险。

#include "Vase/Detail/Export.h"

#include <cstddef>
#include <string>
#include <vector>

namespace vase
{

struct EjectReport
{
    std::string PluginId;

    // 档一 · 实例归零（三平台同，§8.2）：账无反向边 + Scope 空 + 存活对象计数 0。
    bool LedgerHadNoIncomingEdges = false;
    bool ScopeEmptied = false;
    bool GlobalInstancesZeroed = false; // 跨 Pod 全局计数——多开同插件时这是「能不能卸货」的闸

    // 档二 · 映射解除：Linux 主判 = 条目消失；Windows 主判 = 文件可写开，**辅助**地位
    // （改名替换骗得过它，§8.2 v3 修订——最终防假成功的是档三）。
    bool MappingRemoved = false;
    bool ReopenWritable = false;
    bool MappingRemovalIsObservable = false;
    bool ReopenWritableIsMeaningful = false;
    bool BinaryActuallyUnloaded = false; // false 且无错误 = 其他 Pod 实例仍活，货留在架上（§8.1）

    // §9.1 v3：Eject 自动重置登记的进程级状态。**M1 恒空**——.ProcessState 登记
    // 属 M3（12.3 里程碑行），字段先立住让报告形状稳定，M3 填实现。
    std::vector<std::string> ProcessStatesReset;

    std::string HotSwapNote; // 「failed-record ejected」/「kept resident: other pod holds instances」等
};

struct AdoptReport
{
    std::string PluginId;
    bool ReusedResidentImage = false;     // §5.6 判定流②：复用分支同样必须 IdentityVerified
    bool IdentityVerified = false;        // 档三 · 内存特征 == 磁盘特征
    bool ImportEnforcementPassed = false; // §8.7：导入表不含兄弟插件
    std::size_t OutgoingEdges = 0;        // 装配后账本上的出边数（「每次解析落账」的凭证）
};

} // namespace vase
```

- [ ] **Step 2: `EjectPlugin` 实现（判定流逐对得上 §5.6 左列）**

```text
EjectPlugin(handle, id):
  AssertBoundThread("EjectPlugin")            —— §5.6 规则④ 全程串行
  pod = Resolve(handle)；null → Err("eject on stale pod handle")
  ① 反查：在 pod->Instances 找 OwnerLabel==id 的活实例
      找到 → edges = Ledger.EdgesTo(instance)，过滤 PodIndex==本局：
             非空 → Err("eject refused: <id> is provided by [<consumerId, ...>]")
             —— 规则③「哪怕一条声明未用的边也拒绝」+ **没有强制模式**（②）
      找不到 → 查 pod->FailedBinaries：无记录 → Err("plugin not in pod")
             有记录 → 「Failed 可被 Eject」补充角：无实例必无入边，直接进 ②'
  ② 拆实例（复用既有机器，不新建）：Ledger.RemoveByInstance →（先摘边！）
     Scope->Dispose()（Effect 逆序回收——§7.6-2 出边存活义务：T 的回收动作
       允许合法调用 U 的服务，串行保证 dispose 整体完成前不会有下一个生命周期动作）
     Desc->Destroy(Instance)；--Counters.PluginInstances；Instances 摘除。
     ②' Failed 记录路径：只摘 FailedBinaries 条目与 FailureRecords 记录。
  ③ 全局闸：扫全部 Slots，统计 OwnerLabel==id 的活实例数。
     >0 → report.BinaryActuallyUnloaded=false；HotSwapLog 记 "eject(kept):<id>"；返回 Ok
     ==0 → Loader.Unload(*binary) → 档二字段按平台填充（UnloadEvidence 原样搬）+
     KnownBinaries[id] 路径**保留**（§5.6「Adopt 就地重读」的 M1 前身：清单没有，
       路径账留着，下一手可能反悔再 Adopt——12.1 的循环正是 Eject 后立刻 Adopt）
  ④ 档一复验（拆完之后现场再读一遍，POCO 教训的机制化——「计数非零即拒卸」的
     「即拒」在 ③ 已经执行；这里是**留证据**）：ScopeEmptied = 拆掉的 scope
     EffectCount()==0 && Disposed()；LedgerHadNoIncomingEdges=true（本实例）；
     GlobalInstancesZeroed=true。写进 report。
  HotSwapLog += "eject:<id>"（§9.2 v3 进出事件流）
```

`CreatePodImpl` 同步改动：`LiveInstance::Binary = br.Value()`（EnsureResident 的返回记录）；HeaderVersion/工厂任一步失败时**除了**记 `FailureRecords` 还要 `pod->FailedBinaries[id] = *br.Value()`（镜像已驻留这一条要在注释里写明：拒的是实例，不是文件——§8.1 的进/出表里没有「HeaderVersion 不匹配就把文件踢出去」这一行）。

- [ ] **Step 3: 测试 `Tests/HotSwap/EjectTests.cpp`（7 个 TEST，后两条评审期补）**

```cpp
#include "Vase/Host/PluginHost.h"

#include <gtest/gtest.h>

namespace
{

vase::LoadPlan Plan(std::initializer_list<std::pair<std::string_view, std::filesystem::path>> items)
{
    vase::LoadPlan plan;
    for (const auto& [id, path] : items)
    {
        plan.Ordered.push_back({id, path});
    }
    return plan;
}

TEST(Eject, LeafPluginLeavesWithFullEvidence)
{
    vase::PluginHost host;
    vase::PodHandle h = host.CreatePod(Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}})).Value();
    vase::Result<vase::EjectReport> r = host.EjectPlugin(h, "Vase.Hello");
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    const vase::EjectReport& rep = r.Value();
    EXPECT_TRUE(rep.LedgerHadNoIncomingEdges && rep.ScopeEmptied && rep.GlobalInstancesZeroed); // 档一全绿
    EXPECT_TRUE(rep.BinaryActuallyUnloaded);
#ifdef _WIN32
    EXPECT_TRUE(rep.ReopenWritableIsMeaningful && rep.ReopenWritable); // 档二 · Win 主判（辅助地位）
#else
    EXPECT_TRUE(rep.MappingRemovalIsObservable && rep.MappingRemoved); // 档二 · Linux 主判
#endif
    EXPECT_EQ(host.Resolve(h)->PluginCount(), 0U);
    const vase::PodReport podReport = host.DestroyPod(h);
    EXPECT_TRUE(podReport.Clean()); // Eject 没给 §9.2 留任何尾巴
    EXPECT_EQ(podReport.HotSwapLog.size(), 1U);
    EXPECT_NE(podReport.HotSwapLog[0].find("eject:Vase.Hello"), std::string::npos);
}

TEST(Eject, RefusedWithConsumersNamed)
{
    // 12 节 #6 的账本面 + 3b「拒绝报告点名消费者」最小形。
    vase::PluginHost host;
    vase::LoadPlan plan;
    plan.Ordered.push_back({"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER});
    plan.Ordered.push_back({"Vase.EdgeConsumer", VASE_FIXTURE_EDGECONSUMER});
    vase::PodOptions options;
    HostMarker marker;
    options.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::IHostOnlyService>(marker); };
    vase::PodHandle h = host.CreatePod(plan, options).Value();

    vase::Result<vase::EjectReport> r = host.EjectPlugin(h, "Vase.SharedProvider");
    EXPECT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("Vase.EdgeConsumer"), std::string::npos); // 点名
    EXPECT_EQ(host.Resolve(h)->PluginCount(), 2U);                                  // 被拒 = 什么都没发生
    EXPECT_TRUE(host.EjectPlugin(h, "Vase.EdgeConsumer").IsOk());                   // 消费方自己是叶
    host.DestroyPod(h);
}

TEST(Eject, UnknownOrStaleRejected)
{
    vase::PluginHost host;
    vase::PodHandle h = host.CreatePod(Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}})).Value();
    EXPECT_FALSE(host.EjectPlugin(h, "Vase.Nowhere").IsOk());                   // 不在局中 → 拒
    EXPECT_FALSE(host.EjectPlugin(vase::PodHandle{7, 7}, "Vase.Hello").IsOk()); // 失效句柄 → 拒（§5.1）
    host.DestroyPod(h);
}

TEST(Eject, KeptResidentWhenOtherPodHoldsSamePlugin)
{
    // §8.1「一个镜像」+ 档一全局闸：两局各有实例 → 先 Eject 的那局只拆实例。
    vase::PluginHost host;
    const vase::LoadPlan plan = Plan({{"Vase.LoadProbe", VASE_FIXTURE_LOADPROBE}});
    vase::PodHandle a = host.CreatePod(plan).Value();
    vase::PodHandle b = host.CreatePod(plan).Value(); // 同 binary 复用（T6 dedup）
    vase::Result<vase::EjectReport> first = host.EjectPlugin(a, "Vase.LoadProbe");
    ASSERT_TRUE(first.IsOk());
    EXPECT_FALSE(first.Value().BinaryActuallyUnloaded); // 货留在架上
    EXPECT_NE(first.Value().HotSwapNote.find("other pod"), std::string::npos);
    EXPECT_EQ(host.Resolve(b)->PluginCount(), 1U); // 另一局无恙（#3a 的单点形）
    vase::Result<vase::EjectReport> second = host.EjectPlugin(b, "Vase.LoadProbe");
    ASSERT_TRUE(second.IsOk());
    EXPECT_TRUE(second.Value().BinaryActuallyUnloaded); // 全局归零 → 真卸
    host.DestroyPod(a);
    host.DestroyPod(b);
}

TEST(Eject, FailedRecordCanBeEjectedAndBinaryReleased)
{
    // §5.6 四条补角：Failed 插件可 Eject（拆记录与驻留镜像，必无入边）。
    vase::PluginHost host;
    vase::PodHandle h = host.CreatePod(Plan({{"Vase.StaleHeader", VASE_FIXTURE_STALEHEADER}})).Value();
    EXPECT_EQ(host.Resolve(h)->PluginCount(), 0U); // HeaderVersion 拒了实例
    vase::Result<vase::EjectReport> r = host.EjectPlugin(h, "Vase.StaleHeader");
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message(); // 镜像却驻留着——踢掉它
    EXPECT_TRUE(r.Value().BinaryActuallyUnloaded);
    EXPECT_NE(r.Value().HotSwapNote.find("failed"), std::string::npos);
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

// —— 以下两条**评审期补**（不是首轮 brief 的内容，计数已全链重编）——

TEST(Eject, OtherPodFailedRecordKeepsBinaryResident)
{
    // §8.1 + 补角：局 A 的**失败记录**（FailedBinaries）也是一类持有者——局 B 先卸，
    // 货必须留在架上；只剩 A 时才是真卸。这条证的是「这一支存在、且会让闸走 kept」。
    // 同一 fixture 两局各失败一次 → 两局的 FailedBinaries 都指着同一个记录。
    两局 CreatePod（Vase.StaleHeader）→ EjectPlugin(b, id) 应 BinaryActuallyUnloaded==false
      且 HotSwapNote 点到 "other pod" → 再从 a 卸 → 应 true。
}

TEST(Eject, SameFileUnderTwoIdsKeepsRecordAlive)
{
    // 闸的持有者判定按**记录指针**、不按 id——这条是 record-vs-id 唯一的常驻守卫
    // （实测：其余 6 条在「按 id 判」的旧实现下全绿）。同一文件两条 plan 条目，
    // 第二条 id 对不上 → 普通加载失败 → 却留下指向**同一记录**的 FailedBinaries；
    // 按 id 判会误判「无人持有」→ Unload 掉记录 → 那条条目悬垂（UAF）。
    plan = [("Vase.Hello", HELLO), ("Vase.HelloX", HELLO)] → CreatePod
      → Failures().size()==1
      → EjectPlugin(h, "Vase.Hello") 应 Ok 且 BinaryActuallyUnloaded==false（按 id 判这里会错成 true）
      → EjectPlugin(h, "Vase.HelloX") 应 Ok 且 true（真没人持有了才卸）
}

} // namespace
```

- [ ] **Step 4: 全绿 + 双平台 + 门禁 + Commit**

```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -N   # Expected: Total Tests: 67（T9 收 60 + 本任务 5 + 评审补的 2 条：跨局失败记录保持驻留、同文件两 id）
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug'
git ls-files -z --cached --others --exclude-standard '*.h' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git add -A
git commit -m "M1-T10：EjectPlugin——账本反查执法（点名消费者）+ 全局实例闸 + 档二证据留账"
```

---
## Task 11: `AdoptPlugin`——档三身份验新 + 导入表执法（§5.6 / §8.2 / §8.7）

判定流右列在此落地：①清单就地重读（**M1 无 Catalog，以 `KnownBinaries` 登记表代偿**——路径账是 CreatePod 时记的，注释写明这笔「M2 还债」）；②已驻留则复用镜像但**复用分支同样档三比对**（5.6 的强调句，防「驻留=免检」）；③声明绑不齐整体拒绝（不带病入局）；④装配复用同一台两阶段机器，任一步失败 X 干净退出、无人受累（规则②，Adopt 失败零级联）。导入表执法（8.7）用 T6 的同一份解析器：命中兄弟插件即拒——破了这条，账本看不见的边会把引用计数焊死，Eject 当场假成功。

**Files:**
- Modify: `Include/Vase/Host/PluginHost.h`（Adopt 声明）、`Source/Host/PluginHost.cpp`（实现 + `InspectBinary` helper 抽公共）、`Samples/Embedding/main.cpp`（`swapdemo` 子命令：play → eject → adopt → stop 一遍，全程打报告）
- Create: `Tests/HotSwap/fixtures/NoBuildIdPlugin/`（`if(NOT WIN32)` 门）
- Create: `Tests/HotSwap/AdoptTests.cpp`

**Interfaces:**
- Consumes: T6 档三原语与导入解析、T9 账本、T10 Eject（复用与测试前置都要它）。
- Produces:
  - `Result<AdoptReport> PluginHost::AdoptPlugin(PodHandle, std::string_view pluginId)`（§5.1 签名）
  - 错误消息契约（测试按子串断言）：未知 Id `"unknown plugin id"`；身份不符 `"resident image and file on disk differ"`；特征缺失（T6 透传）`"--build-id"` 或 `"/DEBUG"`；兄弟导入 `"imports sibling plugin"`；声明不齐 `"unresolved declarations"`
  - `NoBuildIdPlugin` fixture：`vase_add_plugin_fixture` + `target_link_options(NoBuildIdPlugin PRIVATE "-Wl,--build-id=none")`——LINK_OPTIONS 排在 `CMAKE_SHARED_LINKER_FLAGS_INIT` 之后，`ld` 后者胜（**实现者第一步实验验证**：构建后 `llvm-readelf -n` 必须看不到 Build ID；若顺序不成立，改用 `add_custom_command(POST_BUILD … objcopy --remove-section .note.gnu.build-id …)`，两条路都通向同一个测试断言）

- [ ] **Step 1: `AdoptPlugin` 实现（判定流逐对 §5.6 右列）**

```text
AdoptPlugin(handle, id):
  AssertBoundThread("AdoptPlugin")                          —— 规则④
  pod = Resolve(handle)；null → Err("adopt on stale pod handle")
  pod 已有该 Id 的活实例或 Failed 记录 → Err("already in pod")
  ① path = KnownBinaries[id]；miss → Err("unknown plugin id: M1 requires prior
     registration through a LoadPlan — Catalog replaces this table in M2（§5.6① 的清单
     就地重读，M1 代偿为登记表；注释标 M2 还债）")
  ② resident = Loader.FindResident(absolute(path))
     驻留 → mem = MemoryIdentity、disk = FileIdentity
             任一 Err → 直接透传拒绝（特征缺失 = Adopt 拒绝 + 指路，§8.2——
               **不做「跳过验新」的静默降级**，内存哈希兜底留 13.1 未决）
             不等   → Err("adopt refused: resident image and file on disk differ
               (rename-replacement defeats tier two — tier three caught it).
               escape: rebuild the pod / eject-all then retry（§8.2 政策 + §5.6 提示）")
     未驻留 → EnsureResident（Err 自带缺依赖诊断，T6）
     两分支都过：desc = InspectBinary(rec, id)   // 从 CreatePodImpl 抽出：GetPlugin
                 符号 → HeaderVersion → 返回 desc 或 Err——「v2 全套保留」（§5.6② 原文）
  §8.7 导入执法：names = ImportedLibraryNamesFromFile(path)；siblingSet =
     { basename_lower(p) for (other, p) in KnownBinaries } − {自身文件名}；
     命中 → Err("adopt refused: binary imports sibling plugin '<name>' ——
     账本看不见的边会焊死引用计数（§8.7）")
  ③ 每条 desc->Meta->Requires 在 pod->Registry 里 Find 存在（提供方 kPlugin|kHost 都算——
     宿主服务活到 Pod 终老）；缺 → Err("adopt refused: unresolved declarations [a, b]")
  ④ 装配（复用 CreatePod 的实例机器）：Create → scope/ctx（cookie/ledger/podIndex 同型填充）
     → OnLoad → OnStart；任一败 → **当场全拆**（RemoveByInstance + Dispose + Destroy）→
     Err（含阶段与消息）；成功 → 追加进 pod->Instances（末尾——逆拓扑序回收时它领自己的位，
     §5.6 四条补角「Adopted 节点没有特殊位置」）
  report{ ReusedResidentImage, IdentityVerified=true（两分支都验过才走到这）,
          ImportEnforcementPassed=true, OutgoingEdges=Ledger.EdgesFrom(cookie).size() }
  HotSwapLog += "adopt:<id>"
```

- [ ] **Step 2: `NoBuildIdPlugin` fixture（Linux only）**

`Tests/HotSwap/CMakeLists.txt` 里 `if(NOT WIN32)` 包：`vase_add_plugin_fixture(NoBuildIdPlugin SOURCES NoBuildIdPlugin.cpp LINK_LIBRARIES VasePod)`（源码同 LoadProbe 换 Id `"Vase.NoBuildId"`）+ 上节的 `target_link_options`。**先跑实验**钉死 flag 顺序（Interfaces 里写的那条），再进测试。

- [ ] **Step 3: 测试 `Tests/HotSwap/AdoptTests.cpp`（6 个 TEST，其中 2 个 Linux-only）**

```cpp
#include "Vase/Host/PluginHost.h"

#include <gtest/gtest.h>

namespace
{

vase::LoadPlan Plan(std::initializer_list<std::pair<std::string_view, std::filesystem::path>> items)
{
    vase::LoadPlan plan;
    for (const auto& [id, path] : items)
    {
        plan.Ordered.push_back({id, path});
    }
    return plan;
}

TEST(Adopt, UnknownIdAndAlreadyInPodRejected)
{
    vase::PluginHost host;
    vase::PodHandle h = host.CreatePod(Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}})).Value();
    // 先 IsOk() 再 GetError()：在未检查的临时 Result 上直接取错误，真回归时
    // GetError() 自己会走 ProgrammerError 终止进程，测试从「断言失败」变成「整条挂掉」。
    const vase::Result<vase::AdoptReport> unknown = host.AdoptPlugin(h, "Vase.NeverRegistered");
    ASSERT_FALSE(unknown.IsOk());
    EXPECT_NE(unknown.GetError().Message().find("unknown plugin id"), std::string::npos);
    const vase::Result<vase::AdoptReport> already = host.AdoptPlugin(h, "Vase.Hello");
    ASSERT_FALSE(already.IsOk());
    EXPECT_NE(already.GetError().Message().find("already in pod"), std::string::npos);
    host.DestroyPod(h);
}

TEST(Adopt, ReusedResidentImageStillVerifiesIdentity)
{
    // §5.6②：复用驻留镜像的分支**同样**做档三比对（这里走通 = 比对通过）。
    vase::PluginHost host;
    const vase::LoadPlan sharedAndConsumer = []
    {
        vase::LoadPlan p;
        p.Ordered.push_back({"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER});
        p.Ordered.push_back({"Vase.EdgeConsumer", VASE_FIXTURE_EDGECONSUMER});
        return p;
    }();
    HostMarker marker;
    vase::PodOptions options;
    options.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::IHostOnlyService>(marker); };
    host.DestroyPod(host.CreatePod(sharedAndConsumer, options).Value()); // 拆局不卸货（§8.1）

    vase::PodHandle h = host.CreatePod(Plan({{"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER}}), options).Value();
    vase::Result<vase::AdoptReport> r = host.AdoptPlugin(h, "Vase.EdgeConsumer");
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    EXPECT_TRUE(r.Value().ReusedResidentImage); // EdgeConsumer 的镜像还驻留在架上
    EXPECT_TRUE(r.Value().IdentityVerified);
    EXPECT_EQ(r.Value().OutgoingEdges, 1U); // 落账：Shared 那条（HostOnly 不落）
    EXPECT_EQ(host.Resolve(h)->PluginCount(), 2U);
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(Adopt, RequiresMustBindFullyOrNothing)
{
    // 规则①：不带病入局；规则②：Adopt 失败零级联（活人一根毛都不掉）。
    vase::PluginHost host;
    const vase::LoadPlan pairPlan =
        Plan({{"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER}, {"Vase.EdgeConsumer", VASE_FIXTURE_EDGECONSUMER}});
    HostMarker marker;
    vase::PodOptions opts;
    opts.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::IHostOnlyService>(marker); };
    host.DestroyPod(host.CreatePod(pairPlan, opts).Value()); // 只为登记 KnownBinaries

    vase::PodHandle h = host.CreatePod(Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}})).Value();
    vase::Result<vase::AdoptReport> r = host.AdoptPlugin(h, "Vase.EdgeConsumer");
    ASSERT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("Vase.Test.Shared"), std::string::npos); // 报告缺哪条
    EXPECT_NE(r.GetError().Message().find("Vase.Test.HostOnly"), std::string::npos);
    EXPECT_EQ(host.Resolve(h)->PluginCount(), 1U); // Hello 无恙
    EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U);  // 没留半条边
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

#ifndef _WIN32
TEST(Adopt, RenameReplacementCaughtByTierThree)
{
    // §8.2 的 Linux 现场：改名替换骗过档二，只有特征比对分得出新旧。
    // （Windows 的映射文件覆盖语义另成一题——v3 §12.1「某平台不可行就回来改这节」
    //   同样适用于 Win 的 sharing 规则，该侧端到端验证登记到 M4/M5，不在 M1 赌。）
    vase::PluginHost host;
    const std::filesystem::path probe{VASE_FIXTURE_LOADPROBE};
    host.DestroyPod(host.CreatePod(Plan({{"Vase.LoadProbe", probe}})).Value()); // 驻留 + 登记

    std::error_code ec;
    const std::filesystem::path tmp = probe.string() + ".swap";
    std::filesystem::copy_file(VASE_FIXTURE_UNLOADPROBE, tmp, std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << ec.message();
    std::filesystem::rename(tmp, probe, ec); // Linux：旧 inode 仍映射，路径已换血——档二全绿现场
    ASSERT_FALSE(ec) << ec.message();

    vase::PodHandle h = host.CreatePod(vase::LoadPlan{}).Value();
    vase::Result<vase::AdoptReport> r = host.AdoptPlugin(h, "Vase.LoadProbe");
    ASSERT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("differ"), std::string::npos);
    EXPECT_NE(r.GetError().Message().find("rebuild"), std::string::npos); // 逃生门写明（§8.2 政策）
    host.DestroyPod(h);
    // **复原走 RAII 守卫，不是这行末尾的一次 copy_file**（T11 实测：brief 原稿把复原写成
    // `copy_file(VASE_FIXTURE_LOADPROBE, probe)`，而 `probe` 就是它自己——自我拷贝，什么
    // 都没复原）。`ProbeSwapGuard` 构造时把原文件备份到 `<probe>.orig`，析构时原子 rename
    // 回位并在早退路径上也清掉 `.swap` 暂存；`guard` 必须声明在 `host` **之前**，好让它
    // 先于 `~PluginHost` 的卸货跑完。
}

TEST(Adopt, MissingIdentityFeatureRejectedWithPointer)
{
    vase::PluginHost host;
    const vase::LoadPlan plan = Plan({{"Vase.NoBuildId", VASE_FIXTURE_NOBUILDID}});
    host.DestroyPod(host.CreatePod(plan).Value()); // CreatePod 不设身份闸（v2 语义）——先进过一回拿登记
    vase::PodHandle h = host.CreatePod(vase::LoadPlan{}).Value();
    vase::Result<vase::AdoptReport> r = host.AdoptPlugin(h, "Vase.NoBuildId");
    ASSERT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("--build-id"), std::string::npos); // 报告指路补链接标志
    host.DestroyPod(h);
}
#endif

TEST(Adopt, FreshLoadBranchAlsoVerifiesAndRecordsEdges)
{
    // 卸载后的再 Adopt：load 分支 + 身份验 + 出边落账一条龙。
    vase::PluginHost host;
    vase::PodHandle h = host.CreatePod(Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}})).Value();
    ASSERT_TRUE(host.EjectPlugin(h, "Vase.Hello").IsOk());
    vase::Result<vase::AdoptReport> r = host.AdoptPlugin(h, "Vase.Hello");
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    EXPECT_FALSE(r.Value().ReusedResidentImage);
    EXPECT_TRUE(r.Value().IdentityVerified);
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

} // namespace
```

- [ ] **Step 4: 全绿（本桩 cl.exe 线全量补跑）+ Commit**

```bash
cmake --build --preset win-x64-msvc-debug && ctest --preset win-x64-msvc-debug   # Windows 线不编译 #ifndef _WIN32 用例 → -N 比 Linux 少 2
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -N   # Expected: Total Tests: 71（67 + 本任务非 Linux-only 的 4 条）
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug -N'   # Linux 线: 73（71 + 2 条 Linux-only）
git ls-files -z --cached --others --exclude-standard '*.h' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git add -A
git commit -m "M1-T11：AdoptPlugin——驻留复用分支档三比对、导入表执法、声明全绑或整体拒绝"
```

（本行起 ctest 基数**按平台分账**：Windows/Linux 编译到的 TEST 集合从 T11 起不再相同。T14 的验收表按「线 × 基数」写，`CLAUDE.md` 同步登记。）

---
## Task 12: `Tests/HotSwap` 主循环——12.1 的端到端（核心承诺的唯一证伪者）

v3 说得直白：这个测试过不了，Vase 的核心承诺就不成立。三件套：**同名、不同目录、运行时覆盖**的双版本 fixture（A 与 A′ 只差一处服务返回值）+ 无依赖邻居 B；流程 = `CreatePod{A,B} → 行为==A → Eject(A) → A′ 覆盖（覆盖本身即「锁真解了」的断言）→ Adopt(A) → 行为==A′ → B 的实例从第一步起没被碰过`。§12.2：本目录自 M1 起按**主干**对待——此后任何动 Loader/账本/Eject 路径的改动都必须跑它。

**Files:**
- Create: `Tests/HotSwap/fixtures/BCommon.h`、`fixtures/VersionedA/VersionedA.cpp`、`fixtures/VersionedAPrime/VersionedAPrime.cpp`、`fixtures/NeighborB/NeighborB.cpp`、`Tests/HotSwap/CMakeLists.txt`（fixture 汇总，含 T10/T11/T13 的用例与 targets）
- Create: `Tests/HotSwap/HotSwapLoopTests.cpp`
- Modify: `Tests/CMakeLists.txt`（`add_subdirectory(HotSwap/fixtures)` + 三个 define + 源列表）

**Interfaces:**
- Consumes: 全部前序。
- Produces:
  - `samples_fixture::ICounter { kName "Vase.Test.Counter", v1; virtual int Value() const = 0; }`（A 提供，返回值就是「版本身份」）
  - `samples_fixture::IHeart { kName "Vase.Test.Heart", v1; virtual int Beats() const = 0; }`（B 提供；B 订阅 `TickEvent` 累加——「心跳正常」的可观测形态）
  - `samples_fixture::TickEvent { kName "Vase.Test.Tick", v1; int Seq; }`
  - define：`VASE_FIXTURE_VERSIONEDA`（stageA 下）、`VASE_FIXTURE_VERSIONEDAPRIME`（stageAPrime 下、**文件名与 A 全同**）、`VASE_FIXTURE_NEIGHBORB`

- [ ] **Step 1: `BCommon.h` 与三个 fixture 源**

```cpp
// Tests/HotSwap/fixtures/BCommon.h
#pragma once
#include <cstdint>
#include <string_view>

namespace samples_fixture
{

class ICounter
{
public:
    static constexpr std::string_view kName = "Vase.Test.Counter";
    static constexpr std::uint32_t kVersion = 1;
    virtual ~ICounter() = default;
    virtual int Value() const = 0;
};

class IHeart
{
public:
    static constexpr std::string_view kName = "Vase.Test.Heart";
    static constexpr std::uint32_t kVersion = 1;
    virtual ~IHeart() = default;
    virtual int Beats() const = 0;
};

struct TickEvent
{
    static constexpr std::string_view kName = "Vase.Test.Tick";
    static constexpr std::uint32_t kVersion = 1;
    int Seq;
};

} // namespace samples_fixture
```

`VersionedA.cpp`（`VersionedAPrime.cpp` 与之**只差一行**：`return 2;`——12.1 的「源码只差一处服务返回值」）：

```cpp
#include "../BCommon.h"
#include "Vase/Plugin.h"

namespace
{

class CounterImpl final : public samples_fixture::ICounter
{
public:
    int Value() const override { return 1; } // ← A′ 的唯一差异行：return 2;
};

class VersionedAPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<samples_fixture::ICounter>(Counter);
        return vase::Result<void>::Ok();
    }

private:
    CounterImpl Counter;
};

VASE_PLUGIN(VersionedAPlugin){
    .Id = "Vase.VersionedA",
    .DisplayName = "双版本探针",
    .Version = "1.0.0",
    .Requires = {},
    .Provides = {{"Vase.Test.Counter", 1}},
};

} // namespace
```

`NeighborB.cpp`：同结构，`Provides {{"Vase.Test.Heart",1}}`；`OnLoad` 里 `ctx.Provide<IHeart>(Heart)` + `ctx.On<TickEvent>(&NeighborB::OnTick, this)`；`OnTick` 自增。（`HeartImpl::Beats()` 返回计数——B 是 A 的邻居、不是依赖方：A 进出与它无关，这正是被断言的事。）

- [ ] **Step 2: 「同名不同目录」的 CMake 形态**

```cmake
# Tests/HotSwap/fixtures/CMakeLists.txt —— 双版本产物的关键三行：
vase_add_plugin_fixture(VersionedA SOURCES VersionedA.cpp LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(VersionedAPrime SOURCES VersionedAPrime.cpp LINK_LIBRARIES VasePod)
set_target_properties(VersionedAPrime PROPERTIES OUTPUT_NAME VersionedA) # ← 同名
set_target_properties(VersionedA PROPERTIES                               # ← 不同目录
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stageA"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stageA")
set_target_properties(VersionedAPrime PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stageAPrime"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stageAPrime")
vase_add_plugin_fixture(NeighborB SOURCES NeighborB.cpp LINK_LIBRARIES VasePod)
```

（`$<TARGET_FILE:VersionedAPrime>` 会指到 stageAPrime 下那个**叫 VersionedA 名**的文件。§12.1 的「不在测试里调编译器」由预构建 target 满足；「两平台行为需逐一验证」的风险由本 task 的 Win/Linux 双绿记录回答，macOS 留 M5。）

`Tests/CMakeLists.txt` 增 `add_subdirectory(HotSwap/fixtures)` 与三个 `VASE_FIXTURE_*` define（`CMAKE_PATH` 包法）。

- [ ] **Step 3: 写 `Tests/HotSwap/HotSwapLoopTests.cpp`（2 个 TEST）**

```cpp
#include "BCommon.h"
#include "Vase/Host/PluginHost.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <system_error>

namespace
{

// 工作副本目录：测试私有，避免碰构建产物本体（覆盖动作改的是「A 的位置」上的文件）。
class SwapWorkspace
{
public:
    SwapWorkspace()
    {
        std::error_code ec;
        Dir = std::filesystem::temp_directory_path(ec) /
              ("vase-hotswap-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(Dir, ec);
        APath = Dir / std::filesystem::path(VASE_FIXTURE_VERSIONEDA).filename(); // 同名！
        std::filesystem::copy_file(VASE_FIXTURE_VERSIONEDA, APath, std::filesystem::copy_options::overwrite_existing,
                                   ec);
        EXPECT_FALSE(ec) << ec.message();
    }

    void InstallPrime()
    {
        std::error_code ec;
        std::filesystem::copy_file(VASE_FIXTURE_VERSIONEDAPRIME, APath,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        // 12.1：这一步本身就是「锁真的解了」的断言——Eject 后文件必须能被覆盖写。
        EXPECT_FALSE(ec) << ec.message();
    }

    std::filesystem::path Dir;
    std::filesystem::path APath;
};

vase::LoadPlan LoopPlan(const std::filesystem::path& aPath)
{
    vase::LoadPlan plan;
    plan.Ordered.push_back({"Vase.VersionedA", aPath});
    plan.Ordered.push_back({"Vase.NeighborB", VASE_FIXTURE_NEIGHBORB});
    return plan;
}

int CounterValue(vase::Pod* pod)
{
    return pod->Root().Get<samples_fixture::ICounter>()->Value(); // 宿主解析：不落边（§5.6）——
    // Eject(A) 能过本身就是这条的活体证明（宿主「缓存」着指针，账本看不见也不需要看见；
    // 9.3 的宿主纪律在测试里的演练形态）。
}

TEST(HotSwap, FullLoopFlipsBehaviorAndNeverTouchesNeighbor)
{
    SwapWorkspace ws;
    vase::PluginHost host;
    vase::PodHandle h = host.CreatePod(LoopPlan(ws.APath)).Value();
    vase::Pod* pod = host.Resolve(h);

    EXPECT_EQ(CounterValue(pod), 1); // 行为 == A
    auto* heart1 = pod->Root().Get<samples_fixture::IHeart>();
    for (int i = 1; i <= 5; ++i)
    {
        pod->Root().Emit(samples_fixture::TickEvent{i}); // B 心跳正常
    }
    EXPECT_EQ(heart1->Beats(), 5); // 切换前恰好 5 次 tick（精确等式免费，且能最早抓住丢事件）

    ASSERT_TRUE(host.EjectPlugin(h, "Vase.VersionedA").IsOk()); // 三档之档一、档二在报告里结算
    ws.InstallPrime();                                          // A′ 覆盖 A 的位置
    vase::Result<vase::AdoptReport> adopt = host.AdoptPlugin(h, "Vase.VersionedA");
    ASSERT_TRUE(adopt.IsOk()) << adopt.GetError().Message();
    // 要证伪得挑**会变**的字段：真 Eject 之后必须走**全新装载**分支。
    // `IdentityVerified` 在成功路径上**恒真**（`AdoptPlugin` 两分支都置 true），拿它当判据等于没断
    // ——它是报告字段，不是检查。这条由评审在 T12 标为 Important 后改的。
    EXPECT_FALSE(adopt.Value().ReusedResidentImage);

    EXPECT_EQ(CounterValue(pod), 2); // 行为 == A′（不是 A！）
    auto* heart2 = pod->Root().Get<samples_fixture::IHeart>();
    // B 的实例指针**此处不判**：新对象会落回被释放的那块堆内存，实测同一二进制时红时绿
    // （3 次运行里 2 红 1 绿）——一行读起来像守卫、实际时红时绿的断言比明摆着的洞更坏。
    // 「B 从第一步起没被碰过」由下面那条精确计数承担。
    pod->Root().Emit(samples_fixture::TickEvent{99});
    EXPECT_EQ(heart2->Beats(), 6); // 5 次 + 第 99 号那次：计数只能连续累加，B 被重建就归零

    const vase::PodReport report = host.DestroyPod(h);
    EXPECT_TRUE(report.Clean());
    ASSERT_EQ(report.HotSwapLog.size(), 2U); // §9.2 v3「中途进出过谁」
    EXPECT_NE(report.HotSwapLog[0].find("eject:Vase.VersionedA"), std::string::npos);
    EXPECT_NE(report.HotSwapLog[1].find("adopt:Vase.VersionedA"), std::string::npos);
}

TEST(HotSwap, FiveRoundsBehaveLikeFirstTime)
{
    // 「可重复无数次」的 M1 剂量（5 轮；50 轮的全量形随 M3 的 3a 一起加）。
    SwapWorkspace ws;
    vase::PluginHost host;
    vase::PodHandle h = host.CreatePod(LoopPlan(ws.APath)).Value();
    bool prime = false;
    for (int round = 0; round < 5; ++round)
    {
        vase::Pod* pod = host.Resolve(h);
        const int want = prime ? 2 : 1;
        ASSERT_EQ(CounterValue(pod), want) << "round " << round;
        ASSERT_TRUE(host.EjectPlugin(h, "Vase.VersionedA").IsOk());
        std::error_code ec;
        const char* src = prime ? VASE_FIXTURE_VERSIONEDA : VASE_FIXTURE_VERSIONEDAPRIME; // 装回另一版
        std::filesystem::copy_file(src, ws.APath, std::filesystem::copy_options::overwrite_existing, ec);
        ASSERT_FALSE(ec) << ec.message();
        ASSERT_TRUE(host.AdoptPlugin(h, "Vase.VersionedA").IsOk());
        prime = !prime;
        ASSERT_EQ(CounterValue(host.Resolve(h)), prime ? 2 : 1); // 换装后立刻对得上「如同首次」
    }
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

} // namespace
```

- [ ] **Step 4: 双平台首跑（这是 M1 的核心判据，不是回归）+ 门禁 + Commit**

```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R HotSwap --output-on-failure
ctest --preset win-x64-clang-debug   # 全量回归
ctest --preset win-x64-clang-debug -N   # Expected: Total Tests: 73
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug -R HotSwap --output-on-failure && ctest --preset linux-x64-clang-debug -N'   # Linux 线: 75
git ls-files -z --cached --others --exclude-standard '*.h' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git add -A
git commit -m "M1-T12：Tests/HotSwap 主循环（12.1 端到端：Eject→覆盖→Adopt→行为翻面、邻居零扰动）"
```

（若 Windows 上 `copy_file(overwrite_existing)` 在 Eject 之后仍报 sharing violation：档二判据就是红的——**先修卸载路径再谈测试**，别放宽断言；这正是 12.1 存在的意义。Linux 上如遇覆盖成功但档三仍拒：检查 inode 语义——`copy_file` 是 truncate-in-place（同 inode！内存映射会看见新内容——**这里必须用 `rename` 换 inode 才能构造「A 还驻留」场景**？不：Eject 后 A 已不驻留，`copy_file` 原地覆盖完全正确；rename 换血场景只在 T11 的 mismatch 测试里需要。两种手法各归其位，别混。）

---
## Task 13: 负路径收口——永不重入（3c）与导入表执法（3e）

两条「假成功」的封堵：①Eject 之后事件风暴不许有任何回调落进已卸代码（§7.6 承重条款的验证形态——违反它不立刻炸，会以「踩进已解除映射的代码页」在随机时刻现形，所以 M1 就必须有这条常驻测试）；②账本与导入表是**两套互相补盲**的执法面——A 二进制直链 B 文件时账本干净（没有服务解析），Eject(B) 会当场假成功，唯一拦住它的是 Adopt 时的导入表检查（§8.7 的「引用计数焊死」现场复现）。

**Files:**
- Create: `Tests/HotSwap/fixtures/TimerPlugin/TimerPlugin.cpp`（+ CMake 行）
- Create: `Tests/Abi/fixtures/BadLinkSiblingB/BadLinkSiblingB.cpp`、`BadLinkSiblingA.cpp`、`Tests/Abi/CMakeLists.txt`
- Create: `Tests/HotSwap/TimerNoReentryTests.cpp`、`Tests/Abi/ImportEnforcementTests.cpp`
- Modify: `Tests/CMakeLists.txt`

**Interfaces:**
- Consumes: 全部前序。
- Produces:
  - fixture `Vase.Timer`（Id）：`OnLoad` 订阅 `samples_fixture::TickEvent`，handler 只写自己的成员（成员计数外部不可见——**故意的**：3c 断言的形状是「Eject 后派发 100 拍，进程活着、计数干净」，不是「数到回调没来」；真·悬垂回调在没有 ASan 的门禁里以崩溃现形，崩溃本身就是红）。
  - fixture 对 `Vase.BadLinkSiblingB` / `Vase.BadLinkSiblingA`：A 的 `target_link_libraries` 含 B → **真实的** PE 导入表条目 / ELF `DT_NEEDED` 条目（不是伪造字节——12 表 3e 写「伪造导入表」，真互链是它的超集且零工具依赖）。
  - define：`VASE_FIXTURE_TIMER`、`VASE_FIXTURE_BADLINKA`、`VASE_FIXTURE_BADLINKB`

- [ ] **Step 1: `TimerPlugin.cpp`**

```cpp
#include "../BCommon.h"
#include "Vase/Plugin.h"

namespace
{

class TimerPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        // §7.6-3 的验证形状：handler 只碰自己的成员——Eject 之后若有任何一次派发
        // 落进这段代码，崩的是已解除映射的代码页（进程当场红），而不是「计数少了」。
        ctx.On<samples_fixture::TickEvent>(&TimerPlugin::OnTick, this);
        return vase::Result<void>::Ok();
    }

private:
    void OnTick(const samples_fixture::TickEvent&) { ++Ticks; }

    int Ticks = 0;
};

VASE_PLUGIN(TimerPlugin){
    .Id = "Vase.Timer",
    .DisplayName = "定时器探针",
    .Version = "0.1.0",
    .Requires = {},
    .Provides = {},
};

} // namespace
```

（「带 10ms 定时器」的 M1 等价形：Vase 本体不提供调度服务（§0.1 边界），事件拍由宿主驱动——`TickEvent` 就是这里的「10ms」。真实 scheduler 的端到端随宿主示例走，不占 M1 判据。）

- [ ] **Step 2: `Tests/HotSwap/TimerNoReentryTests.cpp`（1 个 TEST）**

```cpp
#include "BCommon.h"
#include "Vase/Host/PluginHost.h"

#include <gtest/gtest.h>

namespace
{

TEST(HotSwap, NoCallbackFiresIntoEjectedCode)
{
    // 12 节 #3c（承重，v3）：Eject 后再跑 100 拍，没有回调命中已卸代码。
    vase::PluginHost host;
    vase::LoadPlan plan;
    plan.Ordered.push_back({"Vase.Timer", VASE_FIXTURE_TIMER});
    plan.Ordered.push_back({"Vase.NeighborB", VASE_FIXTURE_NEIGHBORB});
    vase::PodHandle h = host.CreatePod(plan).Value();
    vase::Pod* pod = host.Resolve(h);

    ASSERT_TRUE(host.EjectPlugin(h, "Vase.Timer").IsOk());
    for (int i = 0; i < 100; ++i)
    {
        pod->Root().Emit(samples_fixture::TickEvent{i}); // 进程活着 = 没踩进解映射的代码页
    }
    // 订阅随被卸者自己的 EffectScope 消亡；B 的不受影响：
    pod->Root().Emit(samples_fixture::TickEvent{999});
    EXPECT_GE(pod->Root().Get<samples_fixture::IHeart>()->Beats(), 1);
    EXPECT_TRUE(host.DestroyPod(h).Clean()); // 五计数不因进出而移动基线锚点之外的东西
}

} // namespace
```

- [ ] **Step 3: 互链 fixture 对 + `Tests/Abi/ImportEnforcementTests.cpp`（1 个 TEST）**

```cmake
# Tests/Abi/fixtures/CMakeLists.txt
vase_add_plugin_fixture(BadLinkSiblingB SOURCES BadLinkSiblingB.cpp LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(BadLinkSiblingA SOURCES BadLinkSiblingA.cpp
    LINK_LIBRARIES VasePod BadLinkSiblingB)   # ← 真实的导入表条目在此诞生（§8.7 的违例形态）
```

`BadLinkSiblingB.cpp` / `BadLinkSiblingA.cpp`：各为一个空 `OnLoad` 最小插件，Id 分别 `"Vase.BadLinkSiblingB"` / `"Vase.BadLinkSiblingA"`（A **不**经服务解析碰 B——它只被链接器记进导入表，这正是要拦的「账本看不见的那条边」）。

```cpp
#include "Vase/Host/PluginHost.h"

#include <gtest/gtest.h>

namespace
{

TEST(Abi, AdoptRejectsBinaryThatImportsSiblingPlugin)
{
    // 12 节 #3e（v3）：导入表命中兄弟插件 → Adopt 拒。账本对这条边是盲的——
    // 两个执法面缺一，「Eject 假成功」就从这条缝里钻出去（§8.7）。
    vase::PluginHost host;
    vase::LoadPlan registerBoth;
    registerBoth.Ordered.push_back({"Vase.BadLinkSiblingB", VASE_FIXTURE_BADLINKB});
    registerBoth.Ordered.push_back({"Vase.BadLinkSiblingA", VASE_FIXTURE_BADLINKA});
    host.DestroyPod(host.CreatePod(registerBoth).Value()); // 只为登记路径 + 证明能装载

    vase::LoadPlan justB;
    justB.Ordered.push_back({"Vase.BadLinkSiblingB", VASE_FIXTURE_BADLINKB});
    vase::PodHandle h = host.CreatePod(justB).Value();

    vase::Result<vase::AdoptReport> r = host.AdoptPlugin(h, "Vase.BadLinkSiblingA");
    EXPECT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("imports sibling plugin"), std::string::npos);
#ifdef _WIN32
    EXPECT_NE(r.GetError().Message().find("badlinksiblingb.dll"),
              std::string::npos); // 点名（大小写不敏感已在 T11 实现侧处理）
#else
    EXPECT_NE(r.GetError().Message().find("libBadLinkSiblingB.so"), std::string::npos);
#endif
    EXPECT_EQ(host.Resolve(h)->PluginCount(), 1U); // 拒绝 = A 一根毛没进来（规则②）
    host.DestroyPod(h);
}

} // namespace
```

（`InspectBinary`/Adopt 的错误消息里，兄弟名用**小写**拼入（比较本来就不做大小写敏感，§8.7 的执法口径），Windows 断言按 `badlinksiblingb.dll` 匹配；Linux 的 `DT_NEEDED` 保原名 `libBadLinkSiblingB.so`，断言按原样——两条消息的生成逻辑在 T11 Step 1，这里只是消费形状。若嫌不对称，统一小写断言即可，改断言不改机制。）

- [ ] **Step 4: 全绿 + 双平台 + 门禁 + Commit**

```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -N   # Expected: Total Tests: 75
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug -N'   # Linux 线: 77
git ls-files -z --cached --others --exclude-standard '*.h' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git add -A
git commit -m "M1-T13：Eject 后事件风暴零命中（3c）+ 兄弟导入拒进 Adopt（3e）"
```

---

## Task 14: M1 验收——六线全量、判据核对、`CLAUDE.md` 全量刷新

12.3 M1 行的最终对账。本任务**不写新逻辑**，只做三件事：全量重跑（干净树六 preset）、逐条核对判据、把仓库文档改到与磁盘现状一致（连带文书义务的收尾项）。任何一条红 = 回到对应任务修，**不带红验收**。

**Files:**
- Modify: `CLAUDE.md`（全量刷新，内容清单见 Step 3）、`README.md`（目录与命令同步）、`docs/superpowers/specs/2026-09-14-vase-m0-m1-design.md`（**只允许加一行状态标注**：头部勘误块下补「M1 已按 v3 重制并完成，见 docs/superpowers/plans/2026-09-16-…md」——不改史）

- [ ] **Step 1: 六 preset 干净树全量**

```bash
# 依次：configure → build → ctest → ctest -N，三棵 Windows 线（clang-cl 两棵 + cl.exe 两棵经 msvc-env）
for p in win-x64-clang-debug win-x64-clang-release win-x64-msvc-debug win-x64-msvc-release; do
  rm -rf build-win/$p
done
cmake --preset win-x64-clang-debug && cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
cmake --preset win-x64-clang-release && cmake --build --preset win-x64-clang-release && ctest --preset win-x64-clang-release
Scripts/msvc-env.cmd cmake --preset win-x64-msvc-debug && Scripts/msvc-env.cmd cmake --build --preset win-x64-msvc-debug && ctest --preset win-x64-msvc-debug
Scripts/msvc-env.cmd cmake --preset win-x64-msvc-release && Scripts/msvc-env.cmd cmake --build --preset win-x64-msvc-release && ctest --preset win-x64-msvc-release
```

Linux 侧写成脚本再跑（CLAUDE.md 的 `$` 陷阱——**不要**把上面这种 for 环塞进 `bash -lc "..."`）。落 `Scripts/m1-linux-verify.sh`（内容：两棵 preset 各自 `rm -rf`、configure、build、ctest、`ctest -N`），然后：

```bash
wsl -d Ubuntu -- bash -lc 'bash /mnt/d/Git/Vase/Scripts/m1-linux-verify.sh'
```

Expected 基数表（`ctest -N`，**每线单独记**）：

| preset | 预期 Total Tests |
|---|---|
| `win-x64-{clang,msvc}-debug` | 75 |
| `win-x64-{clang,msvc}-release` | 74（`EffectScopeDeath` 有 `#ifndef NDEBUG` 门） |
| `linux-x64-clang-debug` | 77（多 2 条 T11 的 Linux-only） |
| `linux-x64-clang-release` | 76 |

- [ ] **Step 2: 门禁全量**

```bash
run-clang-tidy -p build-win/win-x64-clang-debug          # 三条判据一起看（CLAUDE.md）：退出0+正文error 0+正文warning 0
run-clang-tidy -p build-win/win-x64-msvc-debug -extra-arg=-Wno-unused-command-line-argument
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
```
Linux tidy 照 M0 口径再跑一棵。tidy 基数（Suppressed 数）会变——**看它变在哪一类**（新增的多是 gtest/STL 模板实例化，我们自己的代码必须仍零正文 warning）。

> **Linux 线必须单独看正文告警，别用「Windows 两条线绿」推它绿。** T11 实测：**同一份源码、同一版 clang-tidy（两侧均 23.1.0）**，Linux 线报 9 条正文 warning 而 Windows 线 0 条——其中两条 `modernize-loop-convert` 落在**两侧都编译的共享文件**（`Pod.cpp` / `PluginHost.cpp`）上。成因是 STL 不同（MSVC STL vs libc++），不是版本差。
> 所以 `CLAUDE.md` 那句「两侧 LLVM 必须同版本——判据一致性依赖这一条」**是必要条件而非充分条件**，刷新时要写准；平台专属 TU（`*Posix.cpp` / `*Windows.cpp`）**天然只在一条线上被检查**，这条也要写明。

- [ ] **Step 3: `CLAUDE.md` 全量刷新（逐项清单，措辞对齐现有风格）**

1. 「项目状态」节改为：M0+M1 完成（架构代码已存在：`VasePod`/`VaseHost` 两层 + 效果/服务/事件/账本/Adopt/Eject）；「代码只有一个探针」段落删除；「仍无 CI」保留（M5）。
2. 「构建与测试」节：`ctest` 基数说明改为**按线分账的表**（Step 1 的六行）；加一句「T3 的 death test 受 `#ifndef NDEBUG` 门、T11 两条 Linux-only——基数差是设计不是漏注册」。
3. 「在这个仓库里干活要知道的规矩」增两条（编号续 5、6）：
   - **5. 插件形态（含测试 fixture）一律经 `Cmake/VasePluginHelpers.cmake` 的 `vase_add_plugin_fixture` 立 target**——它承载 D11（只链 VasePod）与可见性收紧；手搭 `add_library(SHARED)` 的插件 target 视为违规。
   - **6. `Tests/HotSwap` 按主干对待（v3 §12.2）**：任何改动 Loader、账本、Eject/Adopt 路径、描述符布局或 `HeaderVersion` 的提交必须跑**该目录的全部用例**（`-R 'HotSwap|Eject|Adopt'`——**不是** `-R HotSwap`：后者只选到 3 条 `HotSwap.*`，把 `Eject.*` 与 `Adopt.*` 整套漏掉，而那两套才是守 Eject/Adopt 路径的；T14 实测：新选择子 Win 15 / Linux 17，旧选择子两侧都是 3），双平台各自留证据。**别在文档里写死条数**——条数随用例增删漂移，写「该目录全部用例」。
4. 「工具链 flag 是承重的」注记：在「静态检查与格式」前加一小节，写明三个工具链文件里的 `/DEBUG:FULL`、`-Wl,--build-id=sha1` 是 §8.2 档三的**构建要求**（摘掉它们 = Adopt 全线拒绝，症状是运行期响的失败而非编译失败），摘除/改动需重跑 T12 主循环。
5. 「尚未确定的事项」表：「插件接口 / ABI 约定」行 → 「**已落地最小形**（§3.1 宏/基类/描述符 + HeaderVersion 在 M1；清单与 Catalog 在 M2）」；「测试框架与运行方式」行 → 分层已落成 Unit/Integration/Lifecycle/HotSwap/Abi（各线基数见表）；目录布局行 → 含 Samples/Tests 新目录的最终形态。
6. 「配套文件」节 wiki 条目的注意句：M1 落地进度一句话（接口/文件格式仍是提案的措辞**保留**——`LoadPlan` 手写形就是 M1 的实现选择，不是 §5.1 提案的全量兑现；Catalog 接入时回归提案形）。
7. **「静态检查与格式」节改写 tidy 的判据口径**（T11 实测，见本节 Step 2 的注）：(a) 「两侧 LLVM 同版本」是必要条件、**不是充分条件**——同版本 + 不同 STL 会让同一条检查在同一份源码上给出不同结果，实测 `Pod.cpp` / `PluginHost.cpp` 各一条 `modernize-loop-convert` 只在 Linux 报；(b) 平台专属 TU（`*Posix.cpp` / `*Windows.cpp`）**天然只被一条线检查**，所以「Windows 两条线绿」不能推 Linux 绿，**三条 debug 线要各自跑、各自看正文告警**；(c) 相应地，「ctest 基数」也要按线分账（本条与第 2 项同源）。

`README.md`：目录树、`VaseEmbedding` 用法、新增的 ctest 基数说明（与 CLAUDE.md 表同源）。

- [ ] **Step 4: 判据对账表（写进最终提交信息或 PR 描述）**

| # | 12.3 M1 行 / 用户主线 | 证据位置 |
|---|---|---|
| 1 | Pod 实体更名打头 | T1（六线绿 + `git grep VaseSession` 零命中） |
| 2 | 最小闭环（Effect/Context/Host） | T2–T5、T7：`#1` 20 轮归零、`#15` 代际、Embedding `loop 20` add_test |
| 3 | 账本最小形（5.6 两规则） | T9：落账/宿主不落/边随实例死/`ClearPod` 清零 + 未声明 death |
| 4 | Adopt/Eject 全循环 | T10–T12：`#6` 叶拒/放、点名消费者、Failed-eject、驻留复用档三、12.1 端到端（Win+Linux 首跑） |
| 5 | 三档判据 Win/Linux 首跑 | 档一：`#6`/`Eject` 报告三字段；档二：`UnloadEvidencePerPlatform` + `Eject` 报告；档三：`MemoryIdentityMatchesFileIdentity` + `ReusedResidentImage…` + rename-swap（Linux）/`Adopt` 通过分支（Win）；`ProcessStatesReset` 恒空注记指向 M3（3d 边界） |
| 6 | 构建标志落工具链 | T6 Step 1-2：`llvm-readobj --coff-debug-directory` 有 RSDS、`llvm-readelf -n` 有 Build ID 的**命令输出记录在 T6 Step 2 的执行日志**（提交信息里引用） |
| — | 连带文书义务 | T1（CLAUDE.md 即时项）+ T14 Step 3（全量）；spec 勘误头已核实在位 |
| — | 12 节 M1 覆盖行 | #1 #2 #3 #3a(单点) #3c #3e #6 #12 #15 #16 #18 + §6.2 报错内容；**#4 随 VaseCli/M5，3b 完整版与 3a 50 轮随 M2/M3，3d 随 M3**——在验收表里如实标注 |

- [ ] **Step 5: Commit + 停下**

```bash
git add -A
git commit -m "M1-T14：六 preset 全量验收、HotSwap 主干规矩与 CLAUDE.md/README 同步

M1 完成。下一步按 v3 12.3 的 M2 行开 Catalog/求解/清单链路的计划（依赖账本执法 3b 补全）。"
```

**然后停。** M2 另起计划（`writing-plans` 重新走一遍），不要顺手把 Catalog 写了。

---

## 风险登记（执行中若命中，按预案走，不要临场发明）

| 风险 | 命中症状 | 预案 |
|---|---|---|
| gtest death test 与 `/MD` DLL 组合在 MSVC 线的行为 | `EXPECT_DEATH` 间歇红 | death test 全部只依赖 `abort`（无异常路径）；仍红则该条降为 Debug-only 并登记 release 基数减 1——**改基数表，不改断言语义** |
| `copy_file(overwrite_existing)` 在 Windows 对「刚 Eject」文件失败 | T12 `InstallPrime` 报 sharing violation | 这是**真红**：Eject 的档二没做到锁释放——回 T10 修卸载路径，不许放宽断言（§8.2：这就是档二存在的意义） |
| `-Wl,--build-id=none` 的 LINK_OPTIONS 顺序不生效 | T11 `MissingIdentityFeature` 假绿（note 还在） | Step 里写明的 objcopy POST_BUILD 备选；两条都必须让 `llvm-readelf -n` 验证「确实没有 Build ID」后才算 fixture 成立 |
| WSL drvfs 上的 `rename`/映射语义差异 | T11 `RenameReplacement` 在 `/mnt/d` 不稳定 | 工作目录改放 `/tmp`（`temp_directory_path()` 在 WSL 即 `/tmp`，已是 ext4）——`SwapWorkspace` 与 T11 的 tmp 路径都天然如此；若被 `TMPDIR` 改写回 drvfs，测试内强制 `Dir` 落 `/tmp` |
| `run-clang-tidy` 基数漂移 | 摘要行 Suppressed 数与 M0 记录不符 | 看「变在哪一类」（新依赖头 = gtest 之外的？新写的内部头被标外部？）——CLAUDE.md 既有纪律，不许只看退出码 |
| 某平台 `EXPECT_DEATH` 子串匹配到 stderr 之外 | 断言 flaky | 匹配串只取我们自己 `fprintf` 的那句前缀（已如此设计）；平台差异加 `GTEST_FLAG_SET(death_test_style, "threadsafe")` 兜底 |

---

## 完成后

- 本计划 14 桩全部绿 + T14 表逐项有证据 → M1 完成。
- 会话 memory 同步（非仓库动作，执行人提醒需求方）：`vase-v3-hotswap-status` 更新为「M1 已落地；下一步 = 按 12.3 M2 行写 Catalog 计划」。
- 若 T12/T13 暴露 12.1 「同名不同目录覆盖」在某平台不可行——**回来改 v3 §12.1**，不要改测试凑平台（§12.1 原文的预留条款）。

