# VasePack：发布期组合库（spec）

**日期**：2026-10-04 · **状态**：brainstorming 四问四裁 → **grilling 十四问十四裁（一轮事实核查掀出三处推翻与七条新增）** · **实施计划**：spec 复审通过后由 writing-plans 另立 · **分支**：`vasepack-desktop`（自 main `16279ff` 起）

**上游权威**：`wiki/vase-architecture.md` v3 的 §8.6（组合库与符号保活）、§11.3（`VasePack` 命令行与产物表）、§13.2（「插件形态」行：开发期每插件一个动态库，发布期由 VasePack 组合成一个库）、§13.4（对外分发不做、移动端热插拔不做）；`CLAUDE.md` 的「项目状态」（M5 记名欠账余 VasePack 一件）、规矩 1（`VaseBuildOptions` 是编译选项唯一出口）、规矩 5（`vase_add_plugin_fixture` 是插件形态唯一出口）、规矩 6（描述符布局类改动跑族）；插件产物布局波 spec 的 D185（产物根跟随平台惯例）与 D189（`ArtifactLayoutTests` 的立项理由）。

历史决定 D1–D195 不改史，本文决定从 **D196** 续号。

**本文的地位**：本波把 §8.6 的组合库从提案落成机制。**C++ 公开面增量只有一处**——`VASE_PLUGIN` 宏新增一个编译期开关 `VASE_PLUGIN_PACK_OBJECT`（**不开则逐字不变**）；构建面另有增量（`PACK` 关键字、`vase_pack_plugins` 函数、`VASE_PLUGIN_CLASS` 属性、`HelloCommon` INTERFACE 库），与 ABI 无关。**`kHeaderVersion` 不动**、描述符布局不动、开发形的导出符号集合不动、`Result<T>` / `Error` 不动。§11.3 的 `Tools/VasePack` **工具形态被改口**为 CMake 函数（D196）。

凡本文与磁盘代码冲突处以磁盘为准（仓库惯例）；本文与架构文档冲突处以本文为准并回头挂勘误（§9 清单）。

> **[定位说明]** 本波组合库的**唯一消费者是 §5 那几条证人**——§13.4 已裁定对外分发不做，本仓库里也没有「产品」。本波交付的是**机制**，不是某个产品的发布形。这条如实写在这里，不当隐含前提。

> **[裁定（2026-10-09，需求方）· 桌面形态记为演练]** 本波交付的**桌面形态不是发布形态的答案**，自此记为**机制演练**。理由三条：
> ① §8.6 的保活论证解决的是**静态链接**的「无人引用即丢弃」，而桌面插件本就按名动态加载——一个库里装三个 vs 三个库各装一个，差别只有「少三个文件」；
> ② 组合库**平铺在产物根**，而 `PluginCatalog::Refresh` **只扫子目录** ⇒ 它对 Catalog 轴不可见，`scan` / `validate` / `plan` / `doctor` 四命令与 `VaseCliValidateProductRoot` **一个都看不见它**；宿主若要用，只能**手写 `LoadPlan` 硬编码 Id**——那恰好放弃了组合库本该带来的「发现」；
> ③ 因此桌面形态的**可用性存疑**，且这条**已知、不再是未察的缺口**。
>
> **正式目标是移动端发布形态**（`iOS .a` / `Android .so`，§11.3 那两行）——**那才是保活问题真实存在的地方**（静态库由宿主链入、未引用的成员会被整个丢掉，§8.6 末那条 iOS 例外说的就是它）。桌面先行的理由只剩「便宜」：不需要 NDK/Xcode，八线已经在跑，**而机制是平台共享的**——但**桌面验通 ≠ 移动端已解**。
> **连带**：D208 的欠账形状由「余项 = 移动产物」升格为「**余项 = 目标本身**」。

---

> **[事实取证注 A · 探针实测（2026-10-04，Windows 11 / clang-cl 23.1.0 + cl.exe）]** 本波的形状是先跑探针再裁的。探针住仓库外临时目录（`%TEMP%\vasepack-probe\`），两个最小插件（类 `ProbeA` / `ProbeB`，Id `Probe.A` / `Probe.B`）、一份按 §8.6 形态写的手工分发表 TU、一个按字面查符号的最小宿主；**跑完即弃**（已删），故下面的输出原文是这些结论的唯一记录。**两台 Windows 编译器逐条复现**。
>
> ① **重定义是真的，且是链接期硬失败**。两个插件对象链进同一个库：
> ```
> lld-link: error: duplicate symbol: VasePlugin_GetPlugin
> >>> defined at pa.obj
> >>> defined at pb.obj
> lld-link: error: duplicate symbol: VasePlugin_Descriptors
> >>> defined at pa.obj
> >>> defined at pb.obj
> ```
> cl.exe 线同形：`pb_cl.obj : error LNK2005: VasePlugin_GetPlugin already defined in pa_cl.obj`（两枚名字各一条）+ `fatal error LNK1169: one or more multiply defined symbols found`（rc=2）。**成因在 `PluginDescriptor.h:143,149`**：这两枚入口是 `extern "C" VASE_EXPORT`、每个插件库各定义一份；只有 `VasePluginDesc_<Type>()` 是唯一命名的。
>
> ② **选项「`-D` 重命名」可行，但产物挂死符号**。`-DVasePlugin_GetPlugin=VasePlugin_GetPlugin_ProbeA` 这类文本替换在**两台编译器上照常命中 `extern "C" VASE_EXPORT` 的定义**，唯一命名的 `VasePluginDesc_ProbeA` 不受影响，链接 rc=0。但组合库导出表（实测）：
> ```
> Name: VasePluginDesc_ProbeA
> Name: VasePluginDesc_ProbeB
> Name: VasePlugin_Descriptors
> Name: VasePlugin_Descriptors_ProbeA     <- 死符号
> Name: VasePlugin_Descriptors_ProbeB     <- 死符号
> Name: VasePlugin_GetPlugin
> Name: VasePlugin_GetPlugin_ProbeA       <- 死符号
> Name: VasePlugin_GetPlugin_ProbeB       <- 死符号
> ```
> 多出的 4 枚是**被 `VASE_EXPORT` 标成 dllexport 却无人调用**的死符号；按 40 个插件算是 **80 枚**。
>
> ③ **选项「宏体加 pack 分支」可行，产物导出表恰好 4 枚**。把两枚入口抽成 `VASE_PLUGIN_ENTRIES(Type)`、由 `VASE_PLUGIN_PACK_OBJECT` 决定其空否（探针里这份变体头是仓库头的一份**副本**，仓库那份未动）：pack 形对象**只剩** `VasePluginDesc_ProbeA`；组合库导出表：
> ```
> Name: VasePluginDesc_ProbeA
> Name: VasePluginDesc_ProbeB
> Name: VasePlugin_Descriptors
> Name: VasePlugin_GetPlugin
> ```
> 对照实验：**同一份变体头、不发那个开关**，对象即退回开发形（三枚）——失配路径因此是**重定义、链接失败、响亮**。两台编译器均编得过。
>
> ④ **宿主零改动成立（两个选项都一样）**。最小宿主按 §8.1 的字面契约走 `LoadLibraryW` + `GetProcAddress("VasePlugin_GetPlugin")`：
> ```
> host: 按字面查 VasePlugin_GetPlugin / VasePlugin_Descriptors
>   Combined_pack.dll: A=Probe.A B=Probe.B miss=null header=4
>      descriptors=2
>        [0] Probe.A
>        [1] Probe.B
>   Combined_ren.dll: A=Probe.A B=Probe.B miss=null header=4
>      descriptors=2
> ```
> cl.exe 产出的两个组合库跑同一枚宿主，输出逐字相同。**`PluginHost.cpp:64`（查 `VasePlugin_GetPlugin`）与 `Inspect.cpp:44`（查 `VasePlugin_Descriptors`）都不必动。**
>
> ⑤ **「target 名 == 类名」这条隐含约定今天已不成立**（全仓清点，41 个 `VASE_PLUGIN` 站点 / 44 个 `vase_add_plugin_fixture` 目标）：**14 个 target 名没有同名类名**（`BadLinkSiblingA/B`、`LoadProbe`、`NeighborB`、`NeighborC`、`StatefulPlugin`、`UnloadProbe`、`VersionedA` 四兄弟、`NoEnumeration`、`StaleEnum`、`StaleHeaderPlugin`）、**11 个类名没有同名 target**（各带 `Plugin` / `Impl` 后缀者）、**一份源被多个 target 复用**（`Tests/Unit/fixtures/LoadProbe/LoadProbe.cpp` → `LoadProbe` + `UnloadProbe`——同一份源 ⇒ 同 `VasePluginDesc_LoadProbePlugin` 符号，是 §6.2 地雷①的真身）、另有**同名但不同源**的一对（`Samples/HelloPlugin/HelloPlugin.cpp` 与 `Samples/HelloPluginPrime/HelloPlugin.cpp`——类名与符号都不同，**勿按 basename 当同一份**）、另有 3 个 fixture（`NoEnumeration` / `StaleEnum` / `StaleHeaderPlugin`）**根本没有 `VASE_PLUGIN`**。故「拿 target 名拼 `VasePluginDesc_<target>`」当场断；**但会被打包的只有 `Samples/` 那三个**，三者恰好类名与 target 名一致。

> **[事实取证注 B · grilling 期的全仓核查（2026-10-04）]** grilling 十四问里，四问的答案取决于环境事实，逐条查过原文：
>
> ⑥ **「函数调用之后又对同一 target 下命令」全仓只有 5 处**，且分属三个不同维度：
> - **include 维度（3 处，正是 PACK 的三员）**：`Samples/HelloPlugin/CMakeLists.txt:9-10`、`Samples/HelloPluginPrime/CMakeLists.txt:8-9`、`Samples/DependentPlugin/CMakeLists.txt:8-9`，三者都是 `target_include_directories(... PRIVATE "${PROJECT_SOURCE_DIR}/Samples/HelloCommon")`；
> - **输出落位维度（1 处）**：`Tests/HotSwap/fixtures/CMakeLists.txt:30-55`，`Versioned*` 四兄弟的 `OUTPUT_NAME` 与三个输出目录（D194 的「后设者胜」）；
> - **链接选项维度（1 处）**：`Tests/CMakeLists.txt:135-143`，`NoIdentityPlugin` 的 `target_link_options`（`/DEBUG:NONE` 等）。
>
> ⇒ 纪律的准星只能是「**编译期输入**」，不是笼统的「不得再 `target_*`」——后两处碰的是落位与链接期维度，对 pack 形的静态库**本无影响**（静态库不链接、不产出交付物）。
>
> ⑦ **`Samples/HelloCommon` 的消费者恰好 5 处**（`grep` 全仓，`Tests/` 下无第六个）：`Samples/HelloPlugin/HelloPlugin.cpp:2,3`、`Samples/HelloPluginPrime/HelloPlugin.cpp:4,5`、`Samples/DependentPlugin/DependentPlugin.cpp:4`、`Samples/Embedding/main.cpp:7`、`Tools/VaseConsole/Console.cpp:3`。**五处都靠 `target_include_directories` 指目录**（`Samples/Embedding/CMakeLists.txt:13`、`Tools/VaseConsole/CMakeLists.txt:14`、另三处见⑥）。
>
> ⑧ **`VasePod` / `VaseHost` / `VaseCatalog` 对 `Include/` 全是 `PUBLIC`**（`Source/Pod/CMakeLists.txt:11-12`、`Source/Host/CMakeLists.txt:31-32`、`Source/Catalog/CMakeLists.txt:9-10`）⇒ 组合库那句 `target_link_libraries(VasePlugins PRIVATE VasePod)` 就把 `Vase/PluginDescriptor.h` 带给生成的 TU，**生成物的 include 来源不成问题**。
>
> ⑨ **全仓唯一的 STATIC 库是 `VaseCliCore`**（`Tools/VaseCli/CMakeLists.txt:4`），且它**只被可执行文件静态链接**；**全仓零处 `POSITION_INDEPENDENT_CODE`**（根 `CMakeLists.txt`、`Cmake/VaseBuildOptions.cmake` 都没有）。⇒「静态库链进 SHARED 库」在本仓库**从未出现过**，是本波引入的新构建面（D199/D207）。
>
> ⑩ **`VaseThirdPartyCli` 的形状可照抄**（`Cmake/VaseThirdParty.cmake:31,34-35`）：`add_library(... INTERFACE)` + `target_include_directories(... INTERFACE "裸路径")`，**不带** `$<BUILD_INTERFACE:...>`（仓库无 `install()`，裸路径即既有写法）。
>
> ⑪ **平铺的组合库对既有验证面是隐形的**（grilling 期否掉了两处误判，如实记）：`PluginCatalog::Refresh` 只**遍历子目录**（`PluginCatalog.cpp:154` 的 `directory_iterator` + `:175` 的 `dir / "plugin.json"` + `:189` 的缺清单 warning），故产物根里一个平铺文件既不被扫、也不产生 warning；`Tests/Unit/ArtifactLayoutTests.cpp` 是**逐目标显式断言**（`ExpectOwnsDirectory`），不是「扫根断言别无他物」。⇒ **`VaseCliValidateProductRoot` 与 `ArtifactLayoutTests` 都不会因本波而红**——但代价是**布局规则本身也没有守卫**（D215 处理）。

> **[裁定注 · brainstorming（2026-10-04）]** 四问四裁：
> **Q1 · 形态（D196）**：**CMake 函数**。§8.6/§11.3 的 `Tools/VasePack` 工具形态改口。
> **Q2 · 去重机制（D197）**：**宏体加 pack 分支**。取证注②③是判决材料：两选项都可行，差别全部在产物导出表（4 vs 8 枚）。
> **Q3 · 符号名真值（D198）**：**target 属性 `VASE_PLUGIN_CLASS`**（缺省 = target 名）。取证注⑤判定「纯约定」不可行。
> **Q4 · 本波范围（D203）**：**机制 + 装载/枚举证人**。清单链与 Pod 装配明记不做。

> **[裁定注 · grilling（2026-10-04）]** 十四问十四裁（记 **G1–G14**——与上方 brainstorming 的 Q1–Q4 分属两轮、各自编号，勿混）。**三处推翻原稿**（G6 / G2 / G1+G9）、**七条新增**（D209–D215）：
> **G1（D209）** `Samples/HelloCommon` 立成 **INTERFACE 库**——`PACK` 的目标拿不到「调用之后」加的 include（取证注⑥），照原样实现 pack 形**编不过**。
> **G2（D198 落地）** **`CLASS` 关键字不加**——三个 Sample 恰好类名 == target 名，本波它一次都不会被写到；属性保留（拼名只读一处）。
> **G3（D201/§9）** 组合库**平铺**落产物根 + 补 `README.md` 两处落位描述。
> **G4（D211）** 主动闸**主动 `FATAL_ERROR` 带用户写的名字**，不让 CMake 报推导出来的 `${name}.Pack`。
> **G5（D202）** 示范**保留三员**，补写 `FailingPlugin` 的正当理由（空 Provides 那员撑起证人 #3）。
> **G6（D199）** pack 形从**对象库**改为 **STATIC 库**——对象库把全部对象无条件塞进组合库，§8.6 的「保活」问题**根本不出现**；静态库才复现原始场景且可证伪（D213）。
> **G7（D210）** 纪律升格进 `CLAUDE.md` 规矩 5，**窄到「编译期输入」**（取证注⑥：宽措辞误伤两处无关维度）。
> **G8（D205/D214）** 生成 TU 落**构建树**；「组合库与独立插件同 Id 共存」作为「清单链不做」的**具体未来代价**记名。
> **G9（D209）** 收编范围 = **五处全收编**（取证注⑦）——只改插件 fixture 会让「HelloCommon 在哪」有两个答案。
> **G10（D213）** 保活的兑现 = **一次性变异取证**（常驻证人区分不了对象库与静态库）。
> **G11（D212）** 静态库归档落**独立输出目录**，不进产物根——构建中间物不是交付物。
> **G12（D211）** 闸的覆盖面**扩到三项**：未 `PACK` / 重复 / 空（后两项都是**静默**）。
> **G13（D214）** 三条地雷记名：同描述符符号的两 `.Pack` 不得同库 / 手写入口不受开关管辖 / 枚举顺序无契约。**（终审另补一枚：两个 target 共享同一 Id 者不得同列 `PLUGINS`——枚举面重复 Id，静默。）**
> **G14（D215）** `ArtifactLayoutTests` 增一条断言钉「组合库平铺」（取证注⑪：该规则今天无守卫，违反是静默的）。
>
> 另：**§5.4 的 Linux 首验证点**（D207）经 G6 扩到含 PIC 与「静态库成员里的 `dllexport` 经归档仍导出」——取证注⑨：静态库链进 SHARED 是本仓库从未有过的构建面。

---

## 1. 背景与定位

§13.2 的「插件形态」一行是两半：**开发期每插件一个动态库**（M0–M6 已落，插件产物布局波刚把它的落位定死）、**发布期由 VasePack 组合成一个库**（一直空着）。本波落后半。

组合库要解决的问题只有两个，§8.6 都已定：

1. **一张统一入口的分发表**——一个库装 N 个插件，`VasePlugin_GetPlugin(id)` 得路由到对的那个；
2. **保活**——静态库按需提取，而插件之间不互相引用，不加干预会被整个丢掉。§8.6 明确选了**硬引用链**（分发表显式调用每个插件的描述符符号）而不是链接器开关（`--whole-archive` / `-force_load` / `/include`），理由是**跨平台一致、使用者不必配置、不会因漏一个标志而静默失效**。

本波不重新论证这两条，只把它们落成机制。**两条都要真承重**（这是 D199 选静态库的理由）——只落第一条的话，第二条会被实现形态悄悄消解。

## 2. 机制：pack 形与分发表

### 2.1 问题的形状

`VASE_PLUGIN(Type)` 今天的展开物是**四枚符号**（`PluginDescriptor.h:110`）：工厂 `VasePluginCreate_<Type>`（匿名命名空间，内部链接）、唯一命名描述符 `VasePluginDesc_<Type>()`、**统一入口 `VasePlugin_GetPlugin(const char*)`**、**枚举面 `VasePlugin_Descriptors(uint32_t*)`**。后两枚是 `extern "C"`、**每库各一份**——一个库装一个插件的前提就写在这里。取证注①把它变成链接期硬失败。

### 2.2 pack 形：宏体加一支开关（D197）

把它抽成一个内部宏，由编译期开关决定其空否：

```cpp
#ifdef VASE_PLUGIN_PACK_OBJECT
#define VASE_PLUGIN_ENTRIES(Type)   // pack 形：不发——分发表 TU 代发
#else
#define VASE_PLUGIN_ENTRIES(Type)   // 开发形：逐字照旧，与今天完全一致
    ...两枚 extern "C" VASE_EXPORT...
#endif

#define VASE_PLUGIN(Type) ... VASE_PLUGIN_ENTRIES(Type) ...   // 宏体其余一字不动
```

**不开开关 = 今天的展开物，逐字不变。** 这是本波敢说「C++ 面零 ABI 增量」的依据：`kHeaderVersion` 不动、描述符布局不动、开发形的导出集合不动。pack 形下，一个插件对象只剩 `VasePluginDesc_<Type>()`（+ 内部链接的工厂/销毁端）。

**失配是响亮的**（取证注③的对照实验）：pack 形漏发开关 ⇒ 退回开发形 ⇒ 多个插件链进一个库时重定义 ⇒ 链接失败。不静默。

### 2.3 分发表不写 Id 字面量（D200）

`PluginDescriptor.h:112-113` 今天写着「组合库的分发表由 VasePack 生成（M5，§8.6），**与本宏无关**」。这句**半对半错**：分发表确实由 VasePack 生成（对），但**统一入口的发射范围本来就是宏的职责**（错）——组合库之所以需要一张生成的分发表，正因为宏在每库都发一份统一入口。本波把它改准成：

> 开发形每库一份统一入口与枚举面；组合形的分发表由 VasePack 生成，故 pack 形本宏不发这两枚（`VASE_PLUGIN_PACK_OBJECT`）。

生成的 TU **只含符号声明与路由逻辑，不含任何 Id 字面量**：

```cpp
extern "C" const vase::PluginDescriptor* VasePluginDesc_<A>();
extern "C" const vase::PluginDescriptor* VasePluginDesc_<B>();
...
static const vase::PluginDescriptor* const kAll[] = {VasePluginDesc_<A>(), VasePluginDesc_<B>(), ...};

extern "C" VASE_EXPORT const vase::PluginDescriptor* VasePlugin_GetPlugin(const char* id)
{
    if (id == nullptr) { return nullptr; }
    for (const auto* d : kAll) { if (d->Meta->Id == std::string_view{id}) { return d; } }
    return nullptr;
}
```

路由靠**遍历描述符比对 Id**，与 `VASE_PLUGIN` 单插件入口的行为逐字同律。好处：**Id 不进生成物**，于是没有「清单改了、生成物没跟上」这一类漂移面；生成物全部内容都是机械可导的。

**`kAll` 里的调用是真调用**（不是取地址），所以 `/OPT:REF` 与 Release 优化不会把它削掉；两枚入口是 `dllexport`，即导出根，同样不会被削。

## 3. CMake 形态

### 3.1 为什么是 CMake 函数，不是独立工具（D196）

§8.6/§11.3 把 VasePack 写成 `Tools/VasePack` 命令行工具（`VasePack pack <插件列表> --output VasePlugins`）。本波**改口为 CMake 函数**，理由三条：

1. **规矩 1**：`VaseBuildOptions` 是编译选项的唯一出口。一个自己驱动编译器的工具，要么**重建一套编译选项**（直接冲突），要么要求调用方把全部 flag 递进来（那它只剩一层薄包装）。
2. **规矩 5**：`vase_add_plugin_fixture` 是插件形态的唯一出口。组合库要复用同样的源、include、链接与可见性收紧——这些知识今天全在那个函数里，另立一处必腐。
3. 本仓库只有一个构建系统（CMake）。§8.6 说 VasePack 是「CocoaPods 式的构建期工具」——CocoaPods 做的正是**与构建系统集成**，而这里集成点天然是 CMake。

「工具」这个名分随之作废：**没有 `Tools/VasePack/` 目录、没有可执行文件**。产物是构建出来的组合库本身，机制载体是 `Cmake/VasePluginHelpers.cmake` 里新增的函数。

### 3.2 插件形态的唯一出口要补上 include 那一维（D209 / D210）

`Samples/HelloCommon` 今天是个**裸头目录**，五个消费者各自用 `target_include_directories` 指它（取证注⑦）。而 `PACK` 的目标拿不到「调用之后」加的 include（取证注⑥）——照原样实现，pack 形**编不过**。

**解法**：把 `Samples/HelloCommon` 立成 **INTERFACE 库**（形状照抄 `VaseThirdPartyCli`，取证注⑩），五个消费者一律改为 `target_link_libraries(... PRIVATE HelloCommon)`：

```cmake
# Samples/HelloCommon/CMakeLists.txt（新）
add_library(HelloCommon INTERFACE)
target_include_directories(HelloCommon INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
```

于是**插件要什么全部经 `LINK_LIBRARIES` 表达**，`vase_add_plugin_fixture` 的调用之后不再有编译期输入的追加。这条纪律随之升格进规矩 5（D210）：

> 插件的**编译期输入**（源、include、编译定义）一律经 `vase_add_plugin_fixture` 的调用点参数表达；调用之后不得再对同一 target 追加编译期输入。（**不辖**输出落位与链接选项——取证注⑥后两处维度不受影响。）

**五处全收编**（D209）的理由是**一处字面真值**：`HelloCommon` 一旦成为 INTERFACE 库，「它的头在哪」就必须只剩一个答案；只改三个插件 fixture 会让仓库里同时存在两种找法。`Samples/Embedding` 另挂的 `Tests/TestingSupport`（它是可执行、不是插件 fixture）不在收编范围，留着不动。全收编后，本条纪律在全仓**零违反**。

### 3.3 `PACK` 关键字与静态库（D199）

`vase_add_plugin_fixture` 增加一个裸关键字 `PACK`：

```cmake
vase_add_plugin_fixture(HelloPlugin
    SOURCES HelloPlugin.cpp
    LINK_LIBRARIES VasePod HelloCommon
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    PACK)
```

- `PACK` **额外**建一个**静态库** `${name}.Pack`：同样的源、同样的 include、同样的链接库、同样的 `hidden` 可见性，外加 `target_compile_definitions(... PRIVATE VASE_PLUGIN_PACK_OBJECT)` 与 `set_target_properties(... POSITION_INDEPENDENT_CODE ON)`（取证注⑨：全仓零处 PIC，而它要被链进 SHARED 库）。**开发形那个 SHARED 一字不变。**
- 类名记成 target 属性 `VASE_PLUGIN_CLASS`（缺省 = target 名）——符号名的**真值只有这一处**（D198）。本波该属性恒等于 target 名（三个 Sample 恰好如此），它今天的角色是「将来要覆盖时只改一处」的锚；`CLASS` 关键字**不加**（没有需求就没有证人，YAGNI）。
- 静态库的归档产物落**独立输出目录**（D212），不进产物根：它是**构建中间物**，不是交付物（`Versioned` 四个 stage 目录是「要交出去的测试材料」，性质不同，不构成先例）。

**为什么不建第二个独立函数**（β：`vase_add_packed_plugin(HelloPluginPack SOURCES HelloPlugin.cpp ...)`）：那要求**同一个源写两遍**——加个源文件忘一处就是漂移。
**为什么不从既有 target 重建**（γ：`get_target_property(SOURCES)`）：那要复制一整组目标属性，漏一个 include ⇒ 编译红（响亮），但漏一个可见性 ⇒ **静默**。

### 3.4 `vase_pack_plugins`

```cmake
vase_pack_plugins(VasePlugins PLUGINS HelloPlugin DependentPlugin FailingPlugin)
```

按 `<name>.Pack` 取静态库、按 `VASE_PLUGIN_CLASS` 拼符号名，`configure_file` 出一份分发表 TU（落 `${CMAKE_CURRENT_BINARY_DIR}`，**构建树**，绝不落源码树），链成一个 SHARED 库：

- 源：生成的 TU；对每个成员 `target_link_libraries(VasePlugins PRIVATE ${name}.Pack)`（**不用 `$<TARGET_OBJECTS:>`**——它对静态库无效，且正是「按需提取」让保活承重）；
- 链接：`VaseBuildOptions` + `VasePod`（**只引用、不包含**——§8.6）；include 面由 `VasePod` 的 PUBLIC 传递（取证注⑧）；
- 可见性收紧：`CXX_VISIBILITY_PRESET hidden` + `VISIBILITY_INLINES_HIDDEN ON`（**静态库与组合库两处都设**，与插件同律；两枚入口靠 `VASE_EXPORT` 放出）。

**主动闸（D211）**，三项都在配置期、都带名字响亮报错：
1. `${name}.Pack` 不存在 ⇒ `HelloPlugin 未声明 PACK`（不要让 CMake 报推导出来的 `${name}.Pack` 找不到——用户写的是 `HelloPlugin`）；
2. `PLUGINS` 里有**重复**成员 ⇒ 否则生成的 `kAll` 会有两条同一描述符，**静默**造出重复 Id 的枚举面；
3. `PLUGINS` **为空** ⇒ 装零个插件的组合库无意义，与 D135「零个插件的树都算用法/环境错」同一律。

### 3.5 产物落位（D201）

组合库**不是插件 fixture**，不设 `RUNTIME/LIBRARY_OUTPUT_DIRECTORY`，走 CMake 默认——`bin/VasePlugins.dll`(Windows) / `lib/libVasePlugins.so`(Linux) / `lib/libVasePlugins.dylib`(macOS)，**平铺**，与 D185「产物根跟随平台惯例」一致。输出名取 §11.3 的 `VasePlugins`。

**这条落位规则在今天没有守卫**（取证注⑪：`Refresh` 只扫子目录故组合库隐形、`ArtifactLayoutTests` 是逐目标断言），所以 D215 给它补一条断言。

## 4. 示范组合库（D202）

新建 `Samples/VasePlugins/CMakeLists.txt`（只有 CMake、无源），一行：

```cmake
vase_pack_plugins(VasePlugins PLUGINS HelloPlugin DependentPlugin FailingPlugin)
```

装 `Samples/` 那三个，理由不是随手挑的：

- 三者的**类名恰好等于 target 名**，所以 `PACK` 只写一个词、`CLASS` 不写——**默认路径本身就是一个证据**；
- 三者构成一条**真依赖链**：`DependentPlugin` requires Greeter 并经 `IFarewell` 提供 `Vase.Dependent.Farewell`；`FailingPlugin` 的 `OnStart` 返回 `Err`；
- 三者**元数据恰好三种形态**：`HelloPlugin` 有 Provides（Greeter）无 requires；`DependentPlugin` 有 Provides（Farewell）**且**有 requires（Greeter）；`FailingPlugin` **两样都空**。证人 #3「元数据没被搅平」最硬的对照点正是那个**空 Provides** 的成员——少一个成员，这条证人的判别力就降一档。它同时是后续 Pod 装配波要用的失败注入材料。

`Samples/{HelloPlugin,DependentPlugin,FailingPlugin}/CMakeLists.txt` 各加 `PACK`，并把 include 改为 `LINK_LIBRARIES VasePod HelloCommon`（§3.2）。

## 5. 验证要求

### 5.1 证人：`Tests/Integration/VasePackTests.cpp`，六条（D204）

| # | 用例 | 钉住什么 |
|---|---|---|
| 1 | `GetPluginRoutesEveryPackedId` | 三个 Id 各领回非空，且 `Meta->Id` 与请求串**逐字**相等 |
| 2 | `DescriptorsEnumerateAllPackedPlugins` | 枚举面报 **3** 条、Id 集合恰是三者 |
| 3 | `PackedDescriptorsKeepTheirOwnMetadata` | `Vase.Dependent.Farewell` 只在 Dependent 的描述符里、Greeter 的 requires 也只在它那儿、`FailingPlugin` 两样皆空——**元数据没被搅平** |
| 4 | `UnknownIdAndNullReturnNull` | 未知 Id 与 `nullptr` 都得 null（宏体的守卫语义） |
| 5 | `HeaderVersionMatchesPackedBinary` | 三条描述符的 `HeaderVersion == vase::kHeaderVersion` |
| 6 | `LoaderFindsEntryByLiteralContractName` | **走真宿主路径**：`Loader::EnsureResident(组合库)` → `Loader::Symbol(record, "VasePlugin_GetPlugin")` → 调用 → 领回三个 Id |

**六条一律经 `Loader` 按路径装载后取符号——组合库不链进 `VaseTests`。** 这条是设计的硬要求：`VaseTests` 是全仓唯一的测试二进制（`Tests/CMakeLists.txt:176` 一次 `gtest_discover_tests`），把组合库链进去等于让**整个测试套件从进程启动起就驻留它**，且导入表直连绕过了 `Loader`——恰好把本波要证的「宿主零改动」那一跳挖空。路径照仓库既有形状经编译定义递入：`VASE_FIXTURE_VASEPLUGINS="$<PATH:CMAKE_PATH,$<TARGET_FILE:VasePlugins>>"`（`Tests/CMakeLists.txt:71-95` 那一族）。`$<TARGET_FILE:...>` 只建**构建次序**依赖，不建链接依赖，正是所需。

**第 6 条是全波的承重证人**：它把「宿主零改动」这句论断变成可证伪的——另钉两枚**字面名**（`"VasePlugin_GetPlugin"` / `"VasePlugin_Descriptors"` 都必须解析得到）**外加一条错名负例**（换个名字必须失败）。

**基数**：八线各 **+6**（debug `341 → 347`、release `340 → 346`），无 `#ifndef NDEBUG` 门、无平台门。

> **[落账勘误注（2026-10-04 收口波，只消歧、不改史）]** 上句的 `+6`（`341/340 → 347/346`）**只是本条六条证人的增量**；连同紧接的「另加一条布局断言」（`ArtifactLayout.PackedLibraryStaysFlatInTheProductRoot`，+1）合起来，八线共 **+7** → **debug 348 / release 347**——与 plan 的「预期基数变化」及收口波八线实测一致（见 `CLAUDE.md` 两张表与 plan 偏离登记第 11 条）。

**另加一条布局断言**（D215，住 `Tests/Unit/ArtifactLayoutTests.cpp`，不另立文件）：钉组合库**平铺在产物根**、不在自己的子目录里。该文件今天只断言「每个插件独占 `<产物根>/<name>/`」，而组合库是唯一一条反向规则（**不是**插件 fixture，**不进**子目录）——同一个波里两条相邻规则不该一条有守卫一条没有。**需做变异取证**：把 `VasePlugins` 临时挪进子目录，确认该断言转红。

### 5.2 保活承重性的一次性变异取证（D213）

**常驻证人区分不了对象库与静态库**：对象库把对象全量塞进组合库，静态库才按需提取——两种形态下枚举面都照样数出表里的条数（`outCount = kVasePluginTable.size()`，**计数由字面表长直接给出**，与被不被提取无关）。所以 D199 选静态库换来的那点收益，**只能靠一次性变异取证兑现**，且观察面必须是**导出表**（那个符号在不在库里），不是枚举计数：

临时抽掉生成物里对某一个插件的显式引用（表里少一行）⇒ **静态库下该插件没被任何东西引用、链接器不拉它进库** ⇒ 该描述符符号**从组合库导出表消失**；对象库下对象无条件进库、变异不动它。（枚举计数在两形态下都会因表长变短而变小，**区分不了**这两者——这正是本轮评审掀出的观察面选错，见 plan 偏离登记。）

照上个波 `ArtifactLayout` 那枚「临时给 prime 加 `MANIFEST` 即转红」的同款形态执行，**输出原文记进 plan 的偏离登记**。不记下来的话，D199 换来的东西没有证据。

### 5.3 tidy 与规矩 6

**生成的 TU 会进 tidy 面（D205）**：`configure_file` 出的分发表源住在 build 树里，`compile_commands.json` 含它 ⇒ `run-clang-tidy -p build-win/...` **会扫到它**。这是**仓库第一次有构建期生成的源进 tidy 面**。**不把它排除在 tidy 之外**——那正是「静默跳过」的形态（同 `git ls-files --cached --others` 那条的教训）。因此**生成物必须自己写到零正文 warning**。

> **勘误（落地期 Task 5 评审实测，2026-10-04，见 plan 偏离登记第 7 条）**：本段原接着写「直接包含所需头、Id 比较用 `std::string_view` 而非 `strcmp`、返回处显式 cast……**生成物零 NOLINT**」——**那句「零 NOLINT」是错的**，它建立在一个未经验证的假设上。实测生成物产生 **5 条 `readability-identifier-naming`**（两枚入口 + 三个 `VasePluginDesc_<类名>` 声明），根因是：**插件 TU 里同样的名字出自宏展开、clang-tidy 跳过宏展开的声明；真生成的文件不是宏**，于是这些检查**第一次咬上来**——仓库对这些 ABI 契约名的豁免一直是**偶然的**。契约名是 `extern "C"` 的约定名、**改不动** ⇒ 属「无代码级出路」⇒ 按仓库既有惯例（`Include/Vase/Config/ConfigMacros.h:24`、`Value.h:88`、`ManifestExpectation.h:25`、`PluginHost.h:42` 四处同款形态）**就地 NOLINT 区域 + 理由一句**。**改准后**的判据是「零正文 warning」，实现路径允许（且本例中必须）用带理由的 NOLINT 区；**但「有代码级出路的不许压」这条不变**——同批的 `misc-include-cleaner`（缺 `VASE_EXPORT` 的直接包含）与 `avoid-c-arrays` 两条因此**必须改代码**（补包含、改 `std::array`）。

**TU 四线各 +2**（117 → 119：本波新测试文件 + 生成的 TU），无平台专属新源、平台差维持 0。

**规矩 6 触发（D206）**：本波动了 `PluginDescriptor.h` 的宏——那是描述符契约的载体。**虽然描述符布局逐字未变**，仍按规矩 6 在 **三平台各跑 `-R 'HotSwap|Eject|Adopt'`** 并留证据。当前该族计 58 条（四线同值）。

### 5.4 plan 首验证点：Linux 先行（D207）

探针只跑了 Windows（取证注①–④），而本波引入了一个**全仓从未有过的构建面**（取证注⑨：静态库链进 SHARED 库）。故：

- **plan 首步 = 先在 Linux 线把组合库链出来并跑通第 6 条**，成了再铺八线；
- 三件事在这步一并取证：**`POSITION_INDEPENDENT_CODE` 是否足够**（不够的症状是响亮的重定位链接错误）、**静态库成员里的 `VASE_EXPORT` 经归档链进 `.so` 后是否照常导出**、**组合库在 `-fvisibility=hidden` 下能否被按字面名取到**；
- **失败即停下重裁**，不预置补丁（与 macOS 腿 D192 同款纪律）。

### 5.5 八线删树重配全量

configure → build → `ctest` → `ctest -N`，八线全绿、零警告。基数与 tidy 计数的现值见 `CLAUDE.md`「构建与测试」与「静态检查与格式」两张表（本波落账时按实测改）。

## 6. 本波不做，与已知的后果

### 6.1 不做（D203）

- **移动产物**——`libVasePlugins.a`(iOS) 与 Android `.so`。**这是正式目标，不是「顺延」**（见 §1 下方那则裁定）：§11.3 那张表本波只兑现桌面三行，而**桌面三行是演练**。移动波次必须一并裁定两件：**iOS 那条例外**（§8.6 末——宿主必须真的引用到包导出的符号，否则静态库会被整个丢掉，这正是 §8.6 选硬引用链而非链接器开关的理由所在），以及**组合库的 N:1 清单形态**（宿主仍需知道库里有谁；iOS 静态链入时宿主编译期就知道，这一层**可能比桌面简单**，待裁）。
- **清单链**——组合库的 N:1 清单问题（`plugin.json` 是 1:1 的形态）。`VaseCli scan` / `validate` / `plan` 面对组合库的行为**本波不验、不保证**；也**不加** ctest 级证人（唯一能加的 `VaseCli scan <组合库目录>` 会**破坏性回写清单**，且属本条范围）。
- **Pod 装配**——`CreatePod` 把组合库装进 Pod。由此牵出的语义（N 条 `LoadPlanEntry` 指向同一 `BinaryPath`、共享一个 `BinaryRecord`；以及档二/档三证据与 `SemanticDependencyPossible` 在「一份 binary 装 N 个插件」下的退化）**留给后续波次裁定**。本波只证到装载链的第一跳。
- **不给 44 个 fixture 补 `PACK`**——只有 `Samples/` 三个进组合库。
- **不改** `kHeaderVersion`、**不动**描述符布局、**不动**开发形的导出符号集合。
- **不加** `CLASS` 关键字（D198）。

### 6.2 已知后果与地雷（D214）

- **组合库与独立插件同 Id 共存于同一棵产物树**（`bin/HelloPlugin/` 里那个 `Vase.Hello` 与组合库里的 `Vase.Hello`）。今天**无害**——组合库对 Catalog 轴隐形成（取证注⑪）。但若将来组合库要进 Catalog/清单链，这就是**重复 Id**，`PluginCatalog.cpp:198-206` 会整棵树报错。这条是「清单链不做」的**具体未来代价**，不是抽象的范围声明。
- **同一个描述符符号不得由两个 `.Pack` 定义、进而同进一个组合库**。事实：`LoadProbe` 与 `UnloadProbe` 编**同一份** `Tests/Unit/fixtures/LoadProbe/LoadProbe.cpp`（类 `LoadProbePlugin`）；两者若都 `PACK` 又同时列进 `PLUGINS`，两个静态库都定义 `extern "C"` 的 `VasePluginDesc_LoadProbePlugin` ⇒ **重定义、链接失败（响亮）**。（`HelloPlugin` 与 `HelloPluginPrime` **不是**此形——它们编**两个不同文件**、类名不同，各定义自己的符号；它们的危险是下一枚。别按 basename 把不同目录的同名文件当同一份源。）本波只给 `Samples/` 三个加 `PACK`，**今天不触发**。不加机器闸（CMake 无法便宜地判定两个静态库是否含同名符号），文书记名。
- **两个 target 共享同一个 Id 者不得同列进 `PLUGINS`**（**终审补记的一枚，且是静默的**）。事实：`HelloPlugin` 与 `HelloPluginPrime` 的 `.Id` **都是 `Vase.Hello`**；若同时列进 `PLUGINS`，组合库的枚举面（`VasePlugin_Descriptors`）会带**重复 Id**，而 `VasePlugin_GetPlugin` 按 Id 线性查找只返**第一枚**。D211 的三项主动闸只挡**重复的 target 名**，**挡不住「两个 target 共用一个 Id」**——故这是**配置期不报、装进宿主才现形**的那一类。本波 `PLUGINS` 只列 `HelloPlugin`，**今天不触发**。
- **手写入口不受该开关管辖**。`Tests/Integration/fixtures/StaleHeaderPlugin/StaleHeaderPlugin.cpp:55` 手写了 `VasePlugin_GetPlugin`（模拟老二进制），它**不在宏里**、不受 `VASE_PLUGIN_PACK_OBJECT` 影响；若哪天有人把它 `PACK` 进组合库，同样重定义。记一笔以免日后当成「开关失灵」。
- **`VasePlugin_Descriptors` 的返回顺序无契约**。证人 #2 用集合断言正为此；生成物里的顺序 = `PLUGINS` 的书写顺序，但**不构成对外承诺**。

## 7. 决策表

| 编号 | 决定 | 理由 / 依据 |
|---|---|---|
| D196 | VasePack 采 **CMake 函数**形态，§8.6/§11.3 的 `Tools/VasePack` 工具形态**改口** | §3.1：规矩 1 与规矩 5 都是硬边界；本仓库只有一个构建系统 |
| D197 | 去重走**宏体加 pack 分支**（`VASE_PLUGIN_PACK_OBJECT` / `VASE_PLUGIN_ENTRIES`） | 取证注②③：两选项都可行，差别全在产物导出表（4 vs 8 枚，40 插件 = 80 枚死符号） |
| D198 | 符号名真值 = target 属性 `VASE_PLUGIN_CLASS`（缺省 = target 名）；**不加 `CLASS` 关键字** | 取证注⑤「纯约定」当场断；`CLASS` 本波零使用零证人，等首次有需求时再加 |
| D199 | pack 形 = **STATIC 库** `${name}.Pack` + `POSITION_INDEPENDENT_CODE ON` | §1：对象库会让 §8.6 的保活问题**不出现**；静态库才复现原始场景且可证伪（D213） |
| D200 | 分发表**不写 Id 字面量**，按描述符 Id 遍历比对 | §2.3：生成物全部机械可导，无「清单改了生成物没跟上」的漂移面 |
| D201 | 组合库走 CMake 默认输出目录、**平铺**，输出名 `VasePlugins` | §3.5：它不是插件 fixture；与 D185 一致 |
| D202 | 示范装 `Samples/` 三个插件，`CLASS` 不写 | §4：默认路径自证；三者三种元数据形态，空 Provides 那员撑起证人 #3 |
| D203 | 本波范围 = **机制 + 装载/枚举证人**；清单链与 Pod 装配明记不做 | §6.1 |
| D204 | 六条证人，**第 6 条走真宿主 `Loader` 为承重**；**组合库不链进 `VaseTests`** | §5.1：链进去会让整个测试二进制从启动起驻留它并绕过 `Loader` |
| D205 | 生成的 TU 落**构建树**、**进 tidy 面**、必须零正文 warning，**不排除** | §5.3：排除即「静默跳过」形态 |
| D206 | 规矩 6 触发：三平台各跑 `-R 'HotSwap\|Eject\|Adopt'` | §5.3：宏是描述符契约的载体 |
| D207 | plan 首验证点 = **Linux 线先行**（含 PIC 与「静态库里的 dllexport 经归档仍导出」），失败即停下重裁 | §5.4：取证注⑨——静态库链进 SHARED 是全仓首次 |
| D208 | **VasePack 欠账本波不销**，记名收窄 | 桌面机制本波兑现；余项 = 移动产物与发布形清单/装配链 |
| D209 | `Samples/HelloCommon` 立成 **INTERFACE 库**，**五处消费者全收编** | §3.2：`PACK` 拿不到函数后加的 include；只收编一半会让「HelloCommon 在哪」有两个答案 |
| D210 | 纪律升格进 `CLAUDE.md` 规矩 5，**窄到「编译期输入」** | 取证注⑥：宽措辞会误伤输出落位与链接选项两处无关维度 |
| D211 | `vase_pack_plugins` 主动闸挡**三项**：未 `PACK` / 重复 / 空 | §3.4：后两项都是**静默**，各造出重复 Id 的枚举面与零插件库 |
| D212 | 静态库归档落**独立输出目录**，不进产物根 | §3.3：构建中间物不是交付物 |
| D213 | 保活的兑现 = **一次性变异取证**（抽掉一处引用：符号从组合库**导出表消失**） | §5.2：常驻证人区分不了对象库与静态库；**观察面是导出表、不是枚举计数**（计数由表长直接给出，两形态下都变小） |
| D214 | 记名四条：同描述符符号的两 `.Pack` 不得同库 / 手写入口不受开关管辖 / 枚举顺序无契约 / 两个 target 共享同一 Id 者不得同列 `PLUGINS`（末条终审补）；外加「组合库与独立插件同 Id 共存」的未来代价 | §6.2 |
| D215 | `ArtifactLayoutTests` 增一条断言钉「组合库平铺」 | 取证注⑪：该规则今天无守卫，违反是静默的 |

## 8. 关键路径与改动清单

| 文件 | 改动 |
|---|---|
| `Include/Vase/PluginDescriptor.h` | 抽 `VASE_PLUGIN_ENTRIES(Type)` + `#ifdef VASE_PLUGIN_PACK_OBJECT` 两支；`VASE_PLUGIN` 宏体其余一字不动；112–113 行那句改准（D200）；宏体注释补 pack 形那支 |
| `CMakeLists.txt` | 新增 `add_subdirectory(Samples/HelloCommon)`——**须在 `Samples/HelloPlugin`（现 287 行）之前**；新增 `add_subdirectory(Samples/VasePlugins)`——**须在 `Samples/FailingPlugin`（295 行）之后、`add_subdirectory(Tests)`（300 行）之前**（后者要引用 `$<TARGET_FILE:VasePlugins>`） |
| `Cmake/VasePluginHelpers.cmake` | `vase_add_plugin_fixture` 增 `PACK`；新增 `vase_pack_plugins` 与其分发表 TU 模板；头注释补两者与 D210 纪律的存在理由 |
| `Samples/HelloCommon/CMakeLists.txt`（新） | INTERFACE 库（形状照 `VaseThirdPartyCli`） |
| `Samples/HelloPlugin/CMakeLists.txt` | 删 `target_include_directories`，改 `LINK_LIBRARIES VasePod HelloCommon`、加 `PACK` |
| `Samples/HelloPluginPrime/CMakeLists.txt` | 同上（**不加** `PACK`——它与 `HelloPlugin` 同 Id） |
| `Samples/DependentPlugin/CMakeLists.txt` | 同上（加 `PACK`） |
| `Samples/FailingPlugin/CMakeLists.txt` | 加 `PACK`（本就不需 `HelloCommon`） |
| `Samples/Embedding/CMakeLists.txt` | `Samples/HelloCommon` 那行改 `target_link_libraries`；`Tests/TestingSupport` 那行不动 |
| `Tools/VaseConsole/CMakeLists.txt` | 同上（只此一处 include 目录） |
| `Samples/VasePlugins/CMakeLists.txt`（新） | 一行 `vase_pack_plugins` |
| `Tests/Integration/VasePackTests.cpp`（新） | 六条证人 |
| `Tests/Unit/ArtifactLayoutTests.cpp` | 增一条断言钉组合库平铺（D215） |
| `Tests/CMakeLists.txt` | 源表加两个新测试文件；`target_compile_definitions` 加一条 `VASE_FIXTURE_VASEPLUGINS`（71–95 行那一族的形状）。**链接面零改动**——`VaseTests` 本就链 `VaseHost`（59–64 行）；**不新增对 `VasePlugins` 的链接**（D204） |

## 9. 文书义务

- `CLAUDE.md`：**项目状态**一笔（本波两段：机制 + 证人；D208 的欠账收窄措辞）；**目录布局行**补 `Samples/HelloCommon`（现为 INTERFACE 库）与 `Samples/VasePlugins/`；**规矩 5** 段补 D210 那条纪律；「构建与测试」基数与「静态检查与格式」TU 数按落账实测改。
- `README.md`：`115-117` 与 `314-316` 两处描述产物落位，补一句「组合库（`VasePlugins`）平铺在产物根」——它既不是可执行/框架库、也不进插件子目录，是现有两句之外的一类。
- `wiki/vase-architecture.md`：**§8.6** 挂现状注（形态改口为 CMake 函数、分发表不写 Id、静态库保活、桌面先行移动留桩；`Tools/VasePack` 目录不存在）；**§11.3** 挂现状注（同上，命令行形态作废）；**§13.2**「插件形态」行加指针；**§13.4** 补一句组合库与「对外分发不做」的关系（它随产品走，不是把 Vase 当 SDK 发）。
- `Cmake/VasePluginHelpers.cmake` 头部注释；`PluginDescriptor.h` 宏体注释（110–129 行那块）。
- 按改动面复核 `.claude/skills/vase-cpp-engineering/` 是否需要同步（至少 `references/abi-boundary.md` 与 `architecture.md` 涉及描述符生成物的段落）。
- 探针材料（`%TEMP%\vasepack-probe\`）**已删**，故取证注 A ①–⑤ 的输出原文是它们的唯一记录。

本文与磁盘冲突处以磁盘为准；本文定稿后若要偏离，按仓库惯例在 plan 的偏离登记里记名。
