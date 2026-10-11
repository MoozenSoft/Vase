# VasePack 静态链入：进程内来源（spec）

**日期**：2026-10-09 · **状态**：brainstorming 六问六裁 → **grilling 十二问十二裁（一轮事实核查掀出一处既有缺陷、两处推翻）** → 本文 · **实施计划**：spec 复审通过后由 writing-plans 另立
· **分支**：`vasepack-static-link`（自 `main` `57ce732` 起——VasePack 波已 squash 合入 main，本波吃的 `vase_pack_plugins` 已在主干上）

**上游权威**：`wiki/vase-architecture.md` v3 的 §8.1（唯一的加载路径与进/出表）、§8.6（组合库与符号保活，含末条 iOS 例外）、§8.2（三档证据链）、§8.7（符号可见性与链接纪律）、§9.1（进程级状态）、§12.3（M5 行「Android / iOS 仅冷装配」）、§13.3 / §13.4；`CLAUDE.md` 的「项目状态」「构建与测试」「静态检查与格式」与规矩 1 / 5 / 6 / 7；VasePack spec 的 D201（组合库平铺）、D203（本波范围）、D204（证人经 `Loader` 不链进 `VaseTests`）、D205（生成 TU 进 tidy 面）、D208（欠账收窄）、D212（pack 归档不进产物根）、D215（平铺规则的守卫）。

历史决定 D1–D215 不改史，本文决定从 **D216** 续号。

> **[术语（本波先定，因为既有代码里有个同形异义词）]** 本波引入的概念叫「**进程内来源**（in-process source）」——**入口已经在本进程的镜像里**，宿主不必（也不能）从磁盘装它。它的现实用例是**静态链入**（iOS 的 `libVasePlugins.a`）。
>
> **为什么不用 `Resident`**：既有代码里 `Loader::EnsureResident` / `FindResident` / `AllResident` / `ResidentBinaryCount` 的「resident」是**「已被本 Loader 装载、尚未卸载」**（`EjectReport` 的 `kept resident` 也是这个意思）。新概念若也叫 `Resident`，`~PluginHost` 那句「跳过 resident 记录」会当场歧义——**两个含义在同一条语句里同时成立**。故新概念一律用 `InProcess`，`resident` 一词维持既有含义不动。

> **[定位说明 · 本波不是桌面演练]** 2026-10-09 的裁定（VasePack spec §1）说：桌面形态的组合库**不是发布形态的答案**，理由之一是桌面插件本就按名动态加载。**本波不在那个裁定之下**——静态链入是一个**可真切的动作**，被验证的机制（静态链接 → 无 `dlopen` → 入口即链接期引用）与 iOS 上**逐字相同**；差别只在产物装载方式的外壳（`.a` 链进 App vs 静态段链进测试可执行），而那正是「机制与外壳分离」的地方。**真 iOS / Android 腿（toolchain / triplet / preset / `LibraryFileName` 的 `.a` 支）与组合库的 N:1 清单链不在本波**，见 §6.1。

---

## 1. 背景与定位

§8.1 把宿主侧的加载定死成**唯一一条路**：

```text
加载二进制 → VasePlugin_GetPlugin(id) → 描述符 → 用描述符里的工厂实例化
```

这条统一是有意的：它让开发期的「每插件一个 DLL」与发布期的「合并成一个库」在 Vase 侧**没有任何差别**。但**它预设了「加载二进制」这一步存在**——而那一步在 iOS 上不存在：`libVasePlugins.a` 由宿主 App **链接期链入**，进程起来时它已经在内存里了。

于是有两条互相独立的缺口：

1. **入口从哪来**：宿主无法表达「这个插件的入口已经在我进程里了」，只能给一个磁盘路径。
2. **§8.1 那句「等价」从未被验过**：VasePack 波那六条证人**只到 `Loader`**（`Tests/Integration/VasePackTests.cpp:32,150` 的 `loader.EnsureResident(PackedLibrary())`；全文 `PluginHost` 只出现在 `:43`、`:164` 两句注释里），**没有任何一处 `CreatePod`**。被证过的只是「`Loader` 装得下、取得到符号」。

本波补这两条。第 2 条不是旁枝：**它与第 1 条是同一条装配循环**，差异只在入口来源。没有第 2 条作对照，第 1 条绿了也说明不了任何事。

> **[grilling 的判决材料（2026-10-10，全仓核查）]** 为回答「第 2 条真能过吗」而做的核查，掀出了一处**既有缺陷**，也把两处原稿推翻。四条已核事实，后有专节：
> - **① 一处既有缺陷**：`PluginHost::MakeInstance` 把**每个插件的**进程级状态表写进**共享的** `BinaryRecord`（`PluginHost.cpp:596-597`），一二进制 N 插件时后写覆盖先写 ⇒ Eject 只重置最后装配的那一个插件。全仓**唯一**一处把「一 binary ⇔ 一插件」当真。见 §2.5。
> - **② 它会伪装成绿**：#1 的三员 Sample **都没有 `ProcessStates`**（带状态的是 `Tests/HotSwap/fixtures/StatefulPlugin` 与 `Tests/Integration/fixtures/EdgeConsumerPlugin`），表是空的 ⇒ 覆盖了也看不出来。**#1 会绿，而绿是假的。** 见 §5.1 的证人 #7。
> - **③ 描述符是常量初始化**（`PluginDescriptor.h:153-167`）：`kDesc` 是函数内 `static const`，初始化式全是常量表达式（`FieldsOf` 是 `constexpr`、`MetaArray` 两个构造器是 `constexpr`、其余是取址常量）⇒ 落只读数据段、无动态初始化器、不需 `__cxa_guard`。**§5.5 原第 4 点就此销掉。**（一处分开看：`VasePlugin_Descriptors` 里 `kAll[]` 那一格**是**动态初始化——它调函数；不影响宿主填入口地址。）
> - **④ `FindResident` 的洞是可达的**：唯一调用方 `AdoptImpl:1014`，用它分流 `ReusedResidentImage`（`:1015`）。空路径会命中进程内记录 ⇒ §3.3 的边界与 §6.2 地雷 1 **不是理论**。

---

## 2. 机制：进程内来源

### 2.1 问题的形状

`PluginHost.cpp:402` 的装配循环里，入口是这样拿到的：

```cpp
const Result<detail::BinaryRecord*> resident = Loader.EnsureResident(entry.BinaryPath);
// … InspectBinary(*resident.Value(), entry.Id) 内部走 Loader::Symbol(record, "VasePlugin_GetPlugin")
```

`EnsureResident` 的输入是**路径**，`Symbol` 的输入是**平台句柄**——两者都预设了「这个二进制是从文件装进来的」。静态链入的插件两样都没有。

### 2.2 入口类型与计划来源（D216 / D217 / D235）

`Include/Vase/Host/LoadPlan.h`：

```cpp
// §8.1 那条唯一一跳的签名。VASE_PLUGIN 生成的 VasePlugin_GetPlugin 逐字是这个类型。
using PluginEntryPoint = const PluginDescriptor* (*)(const char*);

struct LoadPlanEntry
{
    std::string_view Id;
    std::filesystem::path BinaryPath;
    // 非空 = 进程内来源：入口已在本进程镜像里，BinaryPath 被忽略。
    PluginEntryPoint InProcessEntry = nullptr;
    // …其余字段原样
};
```

**类型名取 `PluginEntryPoint`（D235）**：`PluginHost.cpp:57` 已有一个私有的 `using PluginEntryPoint = …`，与它**逐字同型**——那是同一类型的第二份声明（规矩 7）。原稿的 `PluginEntry` 被推翻：它与 `LoadPlanEntry` 只差一个词，在 `LoadPlanEntry::…` 这种位置上极易读混；且 `PluginEntryPoint` 已在代码里、含义准确。归并后**删私有 alias**。

**为什么是「入口指针」而不是另两条路**（D216）：入口指针保住 §8.1 的唯一路径不变形——变的只是**入口从哪来**。更要紧的是：**宿主写下 `&VasePlugin_GetPlugin` 这个动作，本身就是一条链接期硬引用**。§8.6 末条那句「宿主必须真的引用到包导出的符号，否则静态库会被整个丢掉」，因此从**文档告诫**升格成**类型约束**：不写引用就拿不到入口、编不出计划。

**误用闸（D217）**：`BinaryPath` 与 `InProcessEntry` **同时非空** ⇒ `Err` 带名点名两者。口径照 `manifest expectation required` 那一族（D88 的契约子串清单），是误用不是环境错。

**为什么不引入 tagged 值形（D234）**：非法状态确实**可表示**，但既有 `LoadPlanEntry::Decision` / `Reason`（`Reason` 仅 `kSkip` 有意义）已经立了「可表示但非法、由闸兜」的先例；`std::variant` 会把这个**宿主面结构**变成不可平凡拷贝/指定初始化的形，与「全成员 NSDMI + 指定初始化」的既有写法冲突。代价是误用闸**必须有证人**（已在 §5.1 证人 #4）。

**ABI（D227）**：`LoadPlanEntry` 是宿主面结构，加字段 = ABI 增量；**`kHeaderVersion` 不动**、描述符布局不动。先例：M2b 波 2 给同一个 struct 加过 `std::optional<ManifestExpectation> Expected`。

### 2.3 `Loader` 的进程内记录（D218 / D230）

`Include/Vase/Host/Loader.h`：

```cpp
struct BinaryRecord
{
    std::filesystem::path Path;
    void* Raw = nullptr;
    bool InProcess = false;  // 进程内来源：镜像即宿主，Raw 无意义、不可卸
    // …进程级状态见 §2.5（形状由 D237 改）
};
```

- **登记入口**：新增一个与 `EnsureResident` 并列的方法 `EnsureInProcess(PluginEntryPoint)`，**去重键 = 入口指针**。
- **`Symbol`**：对进程内记录直接从入口取值，**不走平台缝**。派上用场的是同一签名下的既有调用点，`PluginHost` 一行不改。
- **`Unload`**：对进程内记录**不卸**（`PlatformFree` 根本不必调），证据如实标。
- **零平台文件改动**：`Source/Host/Loader.cpp` 是三条平台线共用的那一层（`LoaderWindows.cpp` / `LoaderPosix.cpp` 只提供 `PlatformLoad` / `PlatformFree` / `PlatformSymbol` / `PlatformReopenWritable` / `PlatformMappingRemoved` 五个叶子）。进程内分支**整支住共享层**，`Cmake/Toolchains/` 与平台文件**一个字都不动**。这是本设计的一个承重性质——它说明「进程内来源」是**宿主侧概念**，不是平台概念。
- **`EnsureInProcess` 保留 `Result` 返回（D232）**：它**永不失败**（没有可失败的平台动作），但装配循环靠 `Result` 统一处理才不会有失败分支被漏掉，且 `EnsureResident` / `Symbol` 也都返回 `Result`。**注释必须写明「永不失败」**，否则日后会有人给它编一条不存在的失败路径（§6.2 地雷 3）。

> **[D230 · 一处链接期事实]** 分发表 TU 定义的是 `extern "C" VASE_EXPORT const PluginDescriptor* VasePlugin_GetPlugin(const char*)`（`Cmake/VasePackDispatch.cpp.in:25`）——**每份组合库各定义一份、非 inline 非 weak**。所以**一个宿主至多链入一个静态组合库**：链第二个当场重定义、链接失败（响亮）。动态面无此限制（`Loader` 按路径装，可任意多个）。
> **两个连带后果**：① 「去重键 = 入口指针」因此**退化成常量**——写它是为形状一致，不是真去重；② 这是桌面波 D214 地雷①（「一个描述符符号不得由两个 `.Pack` 定义」）的**宿主侧对偶**，已记入 §6.2。

### 2.4 装配循环的分流

`PluginHost.cpp` 的装配循环里只有取记录那一句分流：

```cpp
const Result<detail::BinaryRecord*> resident =
    entry.InProcessEntry != nullptr ? Loader.EnsureInProcess(entry.InProcessEntry)
                                    : Loader.EnsureResident(entry.BinaryPath);
```

其余（`InspectBinary` → 期望比对 → Provides 碰撞 → 两阶段装配）**逐字不动**。

### 2.5 一二进制 N 插件的进程级状态（D236 / D237）——本波修的既有缺陷

**缺陷。** `PluginHost::MakeInstance` 里：

```
596    record.ProcessStates = desc->Meta->ProcessStates.Begin();
597    record.ProcessStateCount = desc->Meta->ProcessStates.Size();
```

写的是**当前这个插件**的表，而 `record` 是**按文件共享**的（「一文件一记录」）。组合库（一库 N 插件）下每次 `MakeInstance` 覆盖前值 ⇒ 记录里只剩**最后装配的那个插件**的表；`EjectPlugin` 的 Reset 循环（`:914-919`）只遍历这一份 ⇒ **其余 N−1 个插件的进程级状态永远不被重置**，甚至可能重置错插件。

**它为什么至今没人发现**：全仓**从没有**测试把「一个库装 N 个插件」交给 `PluginHost`——桌面波那六条证人只到 `Loader`（§1）。这是本波证人 #1 正好要补的那个缺口。

**修法（D237：二进制级，不降粒度）。** `BinaryRecord` 改存**该二进制全部插件的进程级状态**（按插件 Id 记一份，插入去重），`EjectPlugin` 逐条 Reset。

粒度取**二进制级**的三条理由：① D91 的判据本就二进制级——「这份 binary 在任何 Pod 无活实例」→ 重置，而 Eject 那道全局闸（`:861-906`）三类持有者**全按记录指针**统计；改成插件级会让「Eject A 只重置 A」与「闸仍看整份 binary」两个粒度打架。② 物理事实站在二进制级：静态量活在**镜像**里、按镜像共享，`Loader.h:35-37` 的注释逐字写着「进程级状态跟着二进制走（§9.1），而记录的生命周期正是镜像驻留期」。③ 反例检验：A、B 同库，卸 A 时 B 仍活 ⇒ 闸挡下、不重置；最后卸 B ⇒ 整份 binary 离架，A 与 B 的静态量一起重置——与「binary 出场」自洽。

**边界（决定可行性）**：`ProcessStates` / `ProcessStateCount` 全仓**只有一个写点**（`:596-597`）与**一个读点**（`:914-919`），`BinaryRecord` 的构造点只有 `Loader.cpp:54`。修法因此是**封闭**的——不需要扫别处。

**连带**：`report.SemanticDependencyPossible`（`:925`）依赖 `ProcessStatesReset` 非空，修后它在组合库上按**整份并集**判定，与 D110 的口径一致。

---

## 3. 行为语义

### 3.1 三档证据在进程内条目上不适用（D221）

| 档 | 正常条目 | 进程内条目 |
|---|---|---|
| 档一（实例归零） | 有判据力 | **有判据力**——与实例无关 |
| 档二（卸货） | `MappingRemoved` / `ReopenWritable` | **不适用**——没有可卸的镜像（镜像就是宿主自己），没有可重开的文件 |
| 档三（身份验新） | 内存 ↔ 磁盘比对 | **不适用**——没有独立的磁盘侧 |
| §8.7 兄弟执法 | 读文件导入表 | **不适用**——没有文件可读 |

**如实记，不假装查过**——这是 §9.1 与 M4/D110 语义依赖知情位的同一条纪律。

### 3.2 `EjectPlugin` 拒绝（D219）

进程内条目上的 `EjectPlugin` 在**任何拆除动作之前**返回 `Err`，契约子串点名原因：`linked-in binary cannot be ejected`。

理由有二：**其一**，§8.2 档二在进程内条目上结构性不可达，放行就是「Eject 假成功」——§8.7 明写「任何一条隐式耦合都是日后 Eject 假成功的伏笔」，这是框架自己不能犯的同一类错。**其二**，§12.3 那句「Android / iOS 仅冷装配」与 §13.4 的「移动端不做任何热插拔」，自此**由机制承载**，不再是契约束。

`DestroyPod` 不受影响——拆局本就不卸货（§8.1），冷装配的正当用例照走。

> **[D229 · 一处死角，接受并记名]** 进程级状态（§9.1）的**唯一**重置触发点是 `EjectPlugin`（M3/D91；`DestroyPod` 不重置）。Eject 在进程内条目上被一律拒 ⇒ **静态链入插件的 `ProcessStates` 在进程生命周期内永远等不到重置**。§9.1 的承诺是「可被登记、**可重置**」，后半句在进程内来源上落空。
> **接受**的理由：Reset 的语义是「**这份 binary** 在任何 Pod 无活实例时重置」，而进程内来源的 binary 就是宿主自己——它的「实例」概念更接近进程本身。另两条路都被否：给 `DestroyPod` 加 Reset 会改一条已定的 §8.1 语义（拆局不卸货、不重置）；新开 `ResetProcessStates(id)` 公开 API 是为一件**在目标平台（iOS）上不存在**的问题买单（iOS 进程每启动一次都是新的）。
> **但必须记名**——入 §6.2 地雷 5 与 wiki §13.3「已知的静默失败点」。

### 3.3 `AdoptPlugin` 结构上不可表达（D220 / D239）

`AdoptRequest` 只有 `BinaryPath`，**加不了进程内来源**——语义上不加代码。诚实地说清边界：宿主若把**自己的镜像路径**传给 `Adopt`，不会命中任何进程内分支，而是走普通路径——`dlopen` 宿主自身（或 `LoadLibraryExW` 同一路径）通常拿得到句柄，随后 `dlsym` / `GetProcAddress` 取 `VasePlugin_GetPlugin` **取不到**（入口不在导出面），以「symbol not found」响亮失败。**这是接受的边界，不是缺口。**

**两道防护（D239）**：主防在 `Loader::FindResident`——它的契约是「按路径查、只认逐字相等」，一条空路径的记录本就不该被查到（这是 `Loader` 自己的不变式，且已核**可达**：`AdoptImpl:1014` 用它分流 `ReusedResidentImage`）。第二层在 `AdoptImpl` 加**空路径早闸** ⇒ `Err` 点名「adopt 需要文件路径」——它的价值是把「路径面表达不了进程内来源」从**文档句**变成**运行期断言**，但**别拿它当唯一防线**。

### 3.4 宿主关机

`~PluginHost`（`PluginHost.cpp:226-239`）遍历 `AllResident()` 逐个 `Unload`。进程内记录**必须跳过**——否则会拿空句柄走卸货序列，并打出「binary still resident at host shutdown」的假警报。跳过后，进程内记录随 `Loader` 析构自然消失，正确。

---

## 4. 构建面

### 4.1 `vase_pack_plugins` 增 `STATIC` 关键字（D222）

```cmake
vase_pack_plugins(VasePluginsStatic PLUGINS HelloPlugin DependentPlugin FailingPlugin STATIC)
```

- **缺省 `SHARED`**：既有调用点（`Samples/VasePlugins/CMakeLists.txt`）**逐字不变**，四个桌面 preset 的产物零变化。
- `STATIC` 支与 `SHARED` 支**共用同一套分发表生成器**（`configure_file` + `Cmake/VasePackDispatch.cpp.in`）。**这是把产物形态接在这里、而不是在测试里另立一套的理由**：另立一套就是分发表生成器的**第二份真值**，撞规矩 7（`Cmake/VasePluginHelpers.cmake:92` 是它唯一的家）。
- 本关键字是**甲/乙波次会复用的那一块**（真移动腿只差 naming 与 toolchain），故本波吃下它不是越界，是还账。

### 4.2 见证目标（D223）

**独立 gtest 可执行**（`Tests/StaticLink/`，新目录），把静态组合库**真链进去**。

- **不链进 `VaseTests`**：静态链入的插件从启动起就在镜像里，把三个 Sample 塞进那个 348 条用例的大二进制会让「`Vase.Hello` 既是进程内来源、又从 `Samples/HelloPlugin` 动态装」在别处相撞；独立可执行同时与 iOS「宿主是一个独立二进制」同形。
- 代价：验证矩阵多一个二进制（`ctest -N` 的 gtest 计数 +N，tidy 的 TU 数 +N），按落账实测改基数表。

### 4.3 统一的中间产物目录（D231）

**新立一条仓库级约定**：`Cmake/VasePluginHelpers.cmake` 的文件作用域定义一次

```cmake
set(VASE_PACK_INTERMEDIATE_DIR "${CMAKE_BINARY_DIR}/pack-objects")
```

由两个函数共用（规矩 7：同一件事只准有一处字面真值）。

- **位置**：`${CMAKE_BINARY_DIR}/pack-objects/`——产物根（`bin/` / `lib/`）之外的**兄弟目录**，与 `CMakeFiles/` 同级。「会不会被发布」因此是**结构事实**，不靠命名约定去记。
- **名字**：沿用 `pack-objects`（D212 已落地、文档与注释已引用，改名是纯 churn）；它读作「**pack 的产物**」，新的 STATIC 组合库确实就是 pack 的产物。
- **收编范围 = 「打包链路的中转」**：`.Pack` 归档 + STATIC 组合库。
  - **不收**生成的 `${target}.dispatch.cpp`：它是**生成的源**、不是中转，且 D205 专门讲过它进 tidy 面（`compile_commands` 含它），挪它会牵到那段论证、收益为零。
  - **不收** `VaseCliCore`：它有真实消费者（`VaseCli` 链它），是**正常静态库目标**、不是打包中间物。混进来会让「中间目录」失去判据力——**任何静态库都符合，那就等于没有规则**。
- **连带一处改准（不是推翻）**：`.Pack` 归档现在按 `${CMAKE_CURRENT_BINARY_DIR}/pack-objects` **散在各生成目录**（每插件一个）。统一后改指 `VASE_PACK_INTERMEDIATE_DIR`。**D212 的决定原文（「静态库归档落独立输出目录，不进产物根」）原样成立**，改的只是**哪个**目录。
- **SHARED 组合库不动**：它平铺在产物根（D201/D215 原样）。落位规则于是恰好切成两类——**桌面发布形（SHARED）平铺在产物根，链接输入（STATIC）落中间目录**。这条不对称**有判据**，不是意外：桌面三条腿的发布形就是共享库（§8.5），而静态组合库**在本波**只作链接输入。（iOS 的 `.a` 确实是发布形——那归乙，走下一段的拷贝路径。）
- **乙的路径**：iOS 的 `.a` 将来是**交付物**，届时若需要它出现在发布位置，本仓库已有现成先例——`MANIFEST` 的 POST_BUILD **拷贝**（D186 / D125 结案形态）：中间目录产出、显式拷贝到交付位。**本波不改这条**，只保证「本波的 STATIC 组合库是链接输入」。
- **不加断言**：§5 的证人已覆盖产物形态；而「中间目录在产物根之外」由两个不同的根目录**结构性**保证，守卫的边际价值不高。（D215 的教训针对的是**平铺**这类无结构保证的规则。）

---

## 5. 验证要求

### 5.1 证人清单

| # | 证人 | 走的路径 | 承重 |
|---|---|---|---|
| 1 | 宿主装配**共享**组合库：三个条目共享一个 `BinaryPath`（`VasePlugins.dll`），`CreatePod` 后三个插件都在、服务解析通 | `PluginHost::CreatePod` | **是**（D225：补 §8.1「等价」的零证据；它是 #2 的对照） |
| 2 | 宿主装配**静态**链入组合库：三个条目共享一个 `InProcessEntry` | `PluginHost::CreatePod` | **是**（本波目标） |
| 3 | 进程内条目上 `EjectPlugin` ⇒ `Err` + 契约子串 | `PluginHost::EjectPlugin` | 是 |
| 4 | 两来源同填 ⇒ `Err` 带名 | `PluginHost::CreatePod` | 否 |
| 5 | 保活常驻证人（见 §5.2） | 链接期 + 运行期 | **是** |
| 6 | 进程内条目上的 `DestroyPod` 照常（拆局不卸货） | `PluginHost::DestroyPod` | 否 |
| 7 | **组合库上的进程级状态 Reset 成立**（D238）：一个**动态**组合库，成员含**带 `ProcessStates`** 的 fixture；装配 → Eject → 断言**全部**成员的 `ProcessStatesReset` 齐、且 `Reset()` 真跑 | `PluginHost::{CreatePod,EjectPlugin}` | **是**（#1/#2 的假绿补丁，见下） |

**#1 与 #2 必须共用同一份断言体**（同一组 Sample、同一套读数），否则「等价」这句话没有比较面。

**#7 为什么必须另立、且不能并进 #1/#2**：#1/#2 的三员 Sample **都没有 `ProcessStates`**（已核），因此它们**结构上无法**覆盖 §2.5 那个缺陷——表是空的，覆盖了也看不出来。而那个缺陷是 §8.1「等价」的**直接反例**，本波既然把它修了（D236），就必须有一条会因它而红的证人。用**动态**面（`Loader` 按路径装，不受 D230「一宿主至多一个静态组合库」的限制）另起一库，一条证人只钉一件事。

### 5.2 保活常驻证人：弱符号探针（D224 / D233）

§8.6 的保活承诺是「静态库按需提取，插件对象没人引用就会被丢掉」。**正例证明不了这一半**——#1/#2 绿只说明引用链在，说明不了没有链时会丢。

**做法**：同一个见证可执行里链进一个**未被 `PLUGINS` 列入**的 `.Pack` 对象（fixture `Tests/StaticLink/fixtures/KeepAliveProbe.cpp`，类 `KeepAliveProbePlugin`，符号 `VasePluginDesc_KeepAliveProbePlugin`），对两枚描述符符号各做一次**弱引用**：

| 探针 | 期望 | 身份 |
|---|---|---|
| `VasePluginDesc_<已列入的 Sample>` | **非空** | **对照组 / 证伪线**——探针机制坏掉（恒空）时它当场红；它也覆盖「对象被提取了但弱解析失败」这一坏法 |
| `VasePluginDesc_KeepAliveProbePlugin` | **空** | 承重位——对象真没被提取 |

对照组是这条证人成立的前提：没有它，一个恒返回 null 的探针会让承重位「通过」而毫无意义。

**四处链接器实测（T10 调查，本仓库首次四平台全查；本节原稿把「弱引用不拉取归档成员」当成 ELF 与 Mach-O 共有的性质，其中 Mach-O 那一半被实测证伪）**：

| 链接器 | 未定义的**弱**引用会否拉取归档成员 | 本支的观测手段 |
|---|---|---|
| GNU ld（Linux） | **不会**（只记 `reference to`、不记 `definition of`） | 弱符号直判（`VasePluginDesc_… == nullptr`） |
| lld-link（win-clang-cl） | **不会**（读了 `.lib` 却无 `Loaded … KeepAliveProbe`） | 读产物导出表（`GetProcAddress`） |
| link.exe（win-cl.exe） | **不会**（最小复现；**T7 记的「为 dllexport 强制提取」是假读数**，见下） | 读产物导出表（`GetProcAddress`） |
| **Apple ld（macOS）** | **会**——弱未定义同样是提取压力，**与 ELF/COFF 分岔** | **`dlsym(RTLD_SELF, "VasePluginDesc_…")`**（收字符串、无链接期引用） |

- **Mach-O 上弱引用探针自败（决定性实验）**：把探针那处 `__attribute__((weak))` 引用**拿走**，成员随即从映像消失 ⇒ **那处引用本身就是元凶**——观测手段污染了被观测者。两份目标文件的符号绑定逐字相同（都记 `(undefined) weak external _VasePluginDesc_…`），**分岔纯在链接器**。故 macOS 支改用 `dlsym`：它不收链接期引用、原理上不污染（实测未提取返 `0x0`、已提取返地址）。三支**承重位与对照组仍走同一套机制**——本节那条硬要求按平台各自成立。
- **`/alternatename` 不成立**（Windows 原稿）：它**不阻止归档提取**（lld-link 与 link.exe 同；已做最小复现），且「比较两枚函数地址」会被编译器**常量折叠成 false、引用根本不发**。
- **clang-cl 的 COFF `__attribute__((weak))` 不安全**：符号若在别处被**调用**（分发表 TU 在静态初始化里真调描述符），lld-link 会把**那个调用**也解析成 null ⇒ **进程启动即崩**。故 Windows 侧弱引用既不能承载承重位、更不能承载对照组；实际形 = 读产物导出表（探针描述符带 `VASE_EXPORT`，对象**被提取才留下这个导出**）。
- **两条 Windows 线同结论（D241）**：clang-cl 与 cl.exe **都**不为 dllexport 强制提取归档成员 ⇒ 承重位在 Windows 两线上**都可证**，**D240 原记的「cl.exe 线不可证」缺口不存在**。
- **保活机制本身不受影响**：拉取**已列入**成员的是**分发表 TU 的硬引用**（`extern "C"` 声明 + 真调用），不是弱引用。Mach-O 更「贪」（弱引用也拉）只会让保留更保守，不破坏 §8.6 的保活方向。

**一条验证地雷（R12c，入 `CLAUDE.md`「四、怎么验」）**：Windows 上「某符号在不在映像里」的断言**必须从干净全量链接读**（先删 `bin/*.ilk`、或整树重配）。**MSVC 增量链接**会让 `bin/*.ilk` 保留「上一轮被提取、本轮已不再提取」成员的**幽灵导出**，把一个**已丢失**的成员读成**还在**——这是**假绿**，比假红危险（假红逼人重跑，假绿会被当结论）。T7 那次「`link.exe` 强制提取」的读数正是这么来的：它只验了带 `/INCREMENTAL` 的 msvc-debug 线，且其所谓「干净形态」没删 `bin/`（exe+ilk 幸存）。

**退路次序（D233）**：若 plan 期实测某条 Windows 线不通，**先换写法**（CMake 级 `target_link_options` 比源码里的 `#pragma comment(linker,…)` 更好核、更好查），两条 Windows 线都试不通才**降级**——而降级必须在 spec 里**留明缺口**（该线退化为「对照组非空」的弱化形 + 记名），**不用 `GTEST_SKIP`**：跳过一条承重证人等于没有它。**次序如实走完、未走到降级那一步**——T7 曾判 cl.exe 不通并降级，T10 复核证伪了该判据（D241），两条 Windows 线于是都走「换写法」这一档。

### 5.3 规矩 6 与八线

本波动了 `Loader`（`Loader.h` 的结构体与登记入口）、装配路径（`PluginHost.cpp` 的取记录分流）、**以及 `MakeInstance` / `EjectPlugin` 的进程级状态读写**（D236）——**规矩 6 触发**：三平台各跑 `-R 'HotSwap|Eject|Adopt'` 族并各留证据。该族本就覆盖 `Eject.ProcessStates*` 三条与 `Eject.ProcessStatesResetCoexistsWith*` 两枚并存证人，D236 的改动**必须让它们全绿**。八线删树重配全量按落账实测改两张表。

### 5.4 tidy

新 TU 进 tidy 面（`Tests/StaticLink/` 的源与 fixture）。`__attribute__((weak))` / `#pragma comment(linker,…)` 若触发 `misc-include-cleaner` 之类，按「有出路改代码、无出路就地说明」处置。四线各跑。

### 5.5 plan 首验证点（本波未跑构建探针的补偿）

**本波的设计基于磁盘代码通读与既有文书，未跑构建探针**——与 VasePack 波（取证注 A/B）不同，如实记。plan 必须把下面几件排在**动手之前**，任一失败即停下重裁：

1. **#1 共享组合库经 `PluginHost::CreatePod` 能否过**——预期能（多条目共享一个 `BinaryPath`，靠「一文件一记录」；已核：重复 Id 预检（`:285-323`）只查 Id 不查 binary，N 个不同 Id 指向同一文件**合法通过**）。它是 #2 的对照，它若不过，本波的范围结论要重写。
2. **静态组合库能否链接并保留描述符符号**——`vase_pack_plugins` 的 `STATIC` 支 + 宿主取址 `&VasePlugin_GetPlugin`。
3. **#7 先红后修**——先把 `Tests/StaticLink/` 的状态见证搭出来、确认它在**未修**的 `MakeInstance` 上**转红**（这是 D236 的判决材料，也是它的变异取证），再动 §2.5 的修法。
4. **弱符号探针在两条 Windows 线上是否真 armed**（§5.2 的变异取证 + D233 的退路次序）。**→ 已答（T7，T10 复核改准）**：clang-cl 线 armed（新形经变异取证）；**cl.exe 线 T7 曾判「不 armed」并按 D233 降级——该判据是假读数**（增量链接幽灵导出，见 §5.2 的地雷与 **D241**）。T10 干净全量链接下**两线都 armed**，降级**作废、缺口不存在**。

> ~~原第 4 点「`.Pack` 对象的静态初始化在进程内路径上是否照常」~~ **已销**——已核事实③：描述符是常量初始化，落只读数据段，不依赖 main 之前的动态初始化器。

---

## 6. 本波不做，与已知的后果

### 6.1 不做（D226）

- **真 iOS / Android 构建腿（甲）**：`Cmake/Toolchains/` 的移动工具链、`arm64-android` / `arm64-ios` triplet、两个 preset、vcpkg 在移动 triplet 上拉 gtest 的可行性。**这是正式目标的前置，归下一波**。
- **产物形态的移动侧（乙）**：`LibraryFileName` 的 `.a` 第四支、`libVasePlugins.a` 的命名与发布落位。**本波只做 `STATIC` 关键字与统一中间目录**（甲/乙会复用的那块）。
- **组合库的 N:1 清单 / 装配链（丁）**：组合库的清单形态、`Catalog` 与四条命令对它的可见性、`CreatePod` 从清单装组合库。**桌面波已入账的缺口原样留着**（平铺的组合库对 `Refresh` 不可见）。
- **Android 上的 `dlopen` 边界验证**：Android 是 `.so`（动态），走既有 `Loader` 路径，与进程内来源无关。

### 6.2 已知后果与地雷

1. **进程内记录与 `FindResident(path)`**：进程内记录的 `Path` 为空，而 `FindResident` 按路径逐字比较——查询空路径会命中它。**已核可达**（`AdoptImpl:1014-1015` 用它分流 `ReusedResidentImage`）。修法见 §3.3（D239）。
2. **`(Raw, Path)` 去重键在进程内记录上退化**：两者对进程内记录都无意义（`Raw` 空、`Path` 空），故 `Unload` 的既有查找式（`entry->Raw == record.Raw && entry->Path == record.Path`）**不可用于识别进程内记录**——识别只能靠 `InProcess` 标志本身，实现里别复用那一句。
3. **`EnsureInProcess` 永不失败却返回 `Result`**（D232）：签名保留是为了让装配循环的分流同形，**「它永不失败」必须写进注释**，否则日后会有人为它编一条不存在的失败路径。
4. **类型名的第二份声明**：`PluginHost.cpp:57` 的私有 `PluginEntryPoint` 与公开面新增的同名 alias 是同一类型的两份声明。归并后**删私有 alias**（D235，规矩 7）。
5. **进程内来源上的进程级状态永无重置路径**（D229）：见 §3.2 的裁定注。入 wiki §13.3。
6. **一个宿主至多链入一个静态组合库**（D230）：链接期事实（分发表入口一份一库）。已记 §2.3。
7. **`MakeInstance` 的覆盖缺陷（已修，记史）**：全仓**唯一**把「一 binary ⇔ 一插件」当真的地方，潜伏至本波——它没被发现，是因为「一库 N 插件」这条路径从没有测试走过。**这条记史本身是教训**：桌面波把「宿主零改动」写成了承重承诺（D204），却少了一条把组合库交给 `PluginHost` 的证人；本波 #1 补的正是它。

---

## 7. 决策表

| 编号 | 决定 | 理由 / 依据 |
|---|---|---|
| D216 | 接入形状 = **入口指针来源**（`LoadPlanEntry::InProcessEntry`）；**不**新开静态注册面、**不**走 `Loader` 的 self 后端 | §2.2：`&VasePlugin_GetPlugin` 本身即链接期硬引用，保活由类型承载；self 后端的 Windows 形态要求导出 exe 符号，等于把 §8.6 已否掉的「链接器开关」换马甲；新注册面会开出第二条加载路径，与 §8.1 冲突 |
| D217 | 两来源同填 = `Err` 带名 | §2.2：误用不是环境错，口径同 `manifest expectation required` 那一族 |
| D218 | `Loader` 增进程内记录（`BinaryRecord::InProcess`，去重键 = 入口指针），**分支整支住共享层** | §2.3：三条平台线共用 `Loader.cpp`，平台文件零改动 |
| D219 | `EjectPlugin` 对进程内条目在任何拆除之前 `Err` + 契约子串 `linked-in binary cannot be ejected` | §3.2：档二结构性不可达，放行即「Eject 假成功」；§12.3 / §13.4 的热插拔口径自此由机制承载 |
| D220 | `AdoptPlugin` **不加新通道**——`AdoptRequest` 结构上无法表达进程内来源 | §3.3：按路径误传宿主镜像走普通路径响亮失败，是接受的边界 |
| D221 | 档二 / 档三 / §8.7 兄弟执法对进程内条目**不适用，不假装查过** | §3.1：同 M4/D110 语义依赖知情位的纪律 |
| D222 | `vase_pack_plugins` 增 **`STATIC` 关键字**（缺省 `SHARED` 逐字不变） | §4.1：与既有调用点零冲突；分发表生成器只有一份真值（规矩 7） |
| D223 | 见证住**独立 gtest 可执行**，不链进 `VaseTests` | §4.2：隔离进程内镜像；与 iOS「宿主是一个独立二进制」同形 |
| D224 | 保活取证 = **常驻弱符号探针 + 列内对照组** | §5.2：对照组是证伪线，没有它承重位无意义 |
| D225 | 本波一并补「**宿主装配共享组合库**」正例 | §1 缺口 2：§8.1 的「等价」迄今零证据；且它是 #2 的对照 |
| D226 | 范围 = 进程内来源机制 + 宿主级两正例 + 拒绝面；**甲 / 乙 / 丁明记不做** | §6.1 |
| D227 | `kHeaderVersion` 与描述符布局**不动**；`LoadPlanEntry` 加字段 = 宿主面 ABI 增量 | §2.2；先例 M2b 波 2 的 `Expected` |
| D228 | **规矩 6 触发**：三平台各跑 `-R 'HotSwap\|Eject\|Adopt'` 族 | §5.3 |
| D229 | 进程内来源上的进程级状态**永无重置路径**——**接受并记名**（§6.2 地雷 5 + wiki §13.3） | §3.2 裁定注：Reset 是二进制级语义，而进程内来源的 binary 就是宿主；另两条路（`DestroyPod` 重置 / 新公开 API）分别改既有语义、为目标平台不存在的问题买单 |
| D230 | **一个宿主至多链入一个静态组合库**（链接期事实，记名）；「去重键 = 入口指针」据此退化为常量 | §2.3：分发表入口一份一库，非 inline 非 weak；是 D214 地雷①的宿主侧对偶 |
| D231 | **新立统一中间产物目录** `VASE_PACK_INTERMEDIATE_DIR = ${CMAKE_BINARY_DIR}/pack-objects/`（`VasePluginHelpers.cmake` 定义一次）；收编范围 = `.Pack` + STATIC 组合库；`.Pack` 落位由散置**改准**为统一；**不加断言** | §4.3：改准 D212 的落位而非其决定；dispatch TU 与 `VaseCliCore` 不在判据内（前者是源、后者是正常目标）；**推翻**本轮先前「平铺产物根 + 加断言」的推荐——那条会让「平铺」规则的去向浑浊 |
| D232 | `EnsureInProcess` 保留 `Result` 同形 + 注释写明「永不失败」 | §2.3 / §6.2 地雷 3：装配循环靠 `Result` 统一处理才不漏失败分支；另两个方法同形 |
| D233 | Windows 弱符号支的退路**次序**：先换写法（CMake 级 link option 优于 `#pragma comment(linker,…)`），两条线都不通才降级，且降级必须留明缺口；**不用 `GTEST_SKIP`** | §5.2：承重证人不许静默跳过 |
| D234 | `LoadPlanEntry` **保持两个来源字段**，不引 tagged 值形 | §2.2：既有 `Decision`/`Reason` 已立先例；`variant` 破坏指定初始化写法 |
| D235 | 入口类型名取 **`PluginEntryPoint`**（删 `PluginHost.cpp:57` 的私有 alias） | §2.2 / §6.2 地雷 4：`PluginEntryPoint` 已在代码里且含义准确；`PluginEntry` 与 `LoadPlanEntry` 易读混。**推翻** D216 原稿的类型名 |
| D236 | **本波修** `MakeInstance` 覆盖共享记录进程级状态的既有缺陷 | §2.5：它是 §8.1「等价」的直接反例，而 #1 的头号意义就是证「等价」；修法封闭（一写点、一读点、一构造点） |
| D237 | 修法粒度 = **二进制级并集**（记录按插件 Id 存该二进制全部插件的状态，Eject 逐条 Reset） | §2.5：D91 的判据与 Eject 全局闸都是二进制级；静态量按镜像共享；改成插件级会让两个粒度打架 |
| D238 | 另立**独立证人 #7**（动态组合库 + 带 `ProcessStates` 的 fixture），**先红后修** | §5.1：三员 Sample 无 `ProcessStates`，#1/#2 结构上覆盖不到该缺陷；动态面无 D230 的「至多一个」限制 |
| D239 | `FindResident` 层排除进程内记录（**主防**）+ `AdoptImpl` 空路径早闸（第二层） | §3.3：前者修通用形态（`Loader` 自己的不变式），后者把文档句变成运行期断言，但不当唯一防线 |
| D240 | **（历史，平台事实经 D241 复核部分作废）Windows 探测面按实测改准**：`/alternatename` 与 COFF 弱引用双双证伪，改走**读产物导出表**（`GetProcAddress(GetModuleHandleW(nullptr), "VasePluginDesc_…")`，判据 ⇔「对象被提取」）；**clang-cl 线成立、cl.exe 线降级**（`link.exe` 为 dllexport 强制提取归档成员 ⇒ 承重位在该线**不可证**，退化为弱化形并**在 §5.2 留明缺口**） | §5.2 + D233 的退路次序如实走完（先换写法、未用 `GTEST_SKIP`）；实测见 T7 报告与二进制级复核（`llvm-readobj --coff-exports`：clang-cl 映像无探针导出、msvc 映像有）。**代价记名**：本波「未列入 `PLUGINS` 的 `.Pack` 对象不被提取」在 **8 线中的 2 线（cl.exe）不可证**；ELF/Mach-O 支的承重位归 T10 八线实测 |
| D241 | **D240 的平台事实经 T10 四平台调查部分作废，探测面最后一次改准**：① **`link.exe` 不**为 dllexport 强制提取归档成员——D240 记的「cl.exe 线不可证」是**假读数**，根因是 **MSVC 增量链接（debug 线 `/INCREMENTAL`）+ 未删的 `bin/*.ilk` 保留幽灵导出**（T7 只验了 msvc-debug，且其「干净形态」没删 `bin/`）；⇒ **cl.exe 线改回与 clang-cl 同支**（断 `KeepAliveProbeAbsent()`），金丝雀支与 `KeepAliveProbePresent()` 删除，**D240 的记名缺口不存在**。② **Apple ld 会把弱未定义引用当提取压力**（与 GNU ld / lld-link 分岔；两份目标文件符号绑定逐字相同，分岔纯在链接器）⇒ **Mach-O 上弱引用探针自败**（决定性实验：拿走那处引用成员即消失），macOS 支的观测手段改 **`dlsym(RTLD_SELF, …)`**（收字符串、无链接期引用、实测端到端绿）。**否决**运行期读 `LC_SYMTAB`（为一条测试证人给 `ImageInspectMachO.cpp` 扩 symtab 解析，代价与收益不成比例）。③ **新增验证地雷入 `CLAUDE.md`**：Windows 上「符号在不在映像里」的断言必须从**干净全量链接**读（删 `bin/*.ilk`），否则幽灵导出会把**已丢失的成员读成还在**——**假绿**，比假红危险 | §5.2 + T10 调查报告（`task-10b-probe-investigation.md`，四平台实测：GNU ld / lld-link / link.exe 皆不为弱引用拉取归档成员，Apple ld 会）。**代价**：观测手段按平台分岔（Linux 弱符号 / macOS `dlsym` / Windows 读导出表），测试里多一个 `#if defined(__APPLE__)` 支；**对照组 `ListedDescriptorPresent()` 的存在即该分岔的证伪线**（`dlsym` 若找不到 EXE 自身符号即转红） |

---

## 8. 关键路径与改动清单

| 文件 | 改动 |
|---|---|
| `Include/Vase/Host/LoadPlan.h` | 新增 `PluginEntryPoint` 类型（`PluginDescriptor` 前向声明即可，不扩 include 面）；`LoadPlanEntry` 增 `InProcessEntry`（NSDMI + 省略豁免所需的 NOLINT）；头注释补两来源与误用闸 |
| `Include/Vase/Host/Loader.h` | `BinaryRecord` 增 `InProcess`；**进程级状态的表示按 D237 改**（按插件 Id 存该二进制的全部状态，注释写明二进制级语义与去重理由）；新增 `EnsureInProcess` 声明与「永不失败」注释；`FindResident` 的契约注释补「排除进程内记录」 |
| `Source/Host/Loader.cpp` | 进程内登记（按入口指针去重 + 建记录）；`Symbol` 的进程内分支；`Unload` 对进程内记录不卸；`FindResident` 排除进程内记录（D239 主防） |
| `Source/Host/PluginHost.cpp` | 装配循环的取记录分流（§2.4）；**进程级状态的写入点由 `MakeInstance` 改到 `InspectBinary` 成功后、并按 Id 去重累积**（D236/D237）；`EjectPlugin` 的 Reset 循环改遍历并集；`EjectPlugin` 的进程内早闸（D219）；`AdoptImpl` 的空路径早闸（D239 第二层）；`~PluginHost` 卸载循环跳过进程内记录（§3.4）；私有 `PluginEntryPoint` alias 删除（D235） |
| `Cmake/VasePluginHelpers.cmake` | 文件作用域 `set(VASE_PACK_INTERMEDIATE_DIR …)`；`.Pack` 的 `ARCHIVE_OUTPUT_DIRECTORY` 改指它（D231）；`vase_pack_plugins` 增 `STATIC` 关键字与同目录落位；头注释补两者 |
| `Tests/StaticLink/`（新） | 见证可执行 + 七条证人；保活探针与对照组（§5.2）——**观测手段按平台分岔**：Linux 弱符号 / macOS `dlsym(RTLD_SELF, …)` / Windows 读产物导出表（D241）；`fixtures/KeepAliveProbe.cpp`（未列入 `PLUGINS` 的 `.Pack` 成员） |
| `Tests/StaticLink/CMakeLists.txt`（新） | 第二个 gtest 可执行的接线；`vase_pack_plugins(... STATIC)` 与 `vase_pack_plugins(...)`（#7 的动态库）；`VASE_FIXTURE_*` 一族的 compile definition；`gtest_discover_tests` |
| `Tests/HotSwap/fixtures/CMakeLists.txt` 或 `Tests/Integration/fixtures/CMakeLists.txt` | 给 #7 用的带 `ProcessStates` 的 fixture 加 `PACK`（源选 `StatefulPlugin` 或 `EdgeConsumerPlugin`，plan 定） |
| 基数与 tidy 表 | 按落账实测改（`ctest -N` 的 gtest 计数、TU 数、NOLINT 命中） |

---

## 9. 文书义务

- `CLAUDE.md`：**项目状态**一笔（进程内来源机制 + 七条证人 + 拒绝面 + `MakeInstance` 缺陷的发现与修复；甲/乙/丁三笔明记余项）；**规矩 5** 段补 `STATIC` 关键字与 `VASE_PACK_INTERMEDIATE_DIR`（两者都是 `vase_pack_plugins` / `vase_add_plugin_fixture` 的构建面字面）；**规矩 6** 的族证据；「构建与测试」与「静态检查与格式」两张表按实测改；**「四、怎么验」补一条验证地雷（R12c）**——Windows 上「符号在不在映像里」的断言必须从干净全量链接读（删 `bin/*.ilk`），幽灵导出会把**已丢失的成员读成还在**（假绿）。
- `wiki/vase-architecture.md`：**§8.1** 的进/出表加一行「静态链入（进程内来源）」（触碰二进制的列全部写「否——镜像即宿主」），并给唯一加载路径那句补「入口从哪来」的分叉；**§8.6** 末条 iOS 例外挂现状注（它已由类型约束承载），**并在 §8.6 记 T10 实测的平台事实（D241）**（**Apple ld 会把弱未定义引用当提取压力**、与 ELF/COFF 分岔 ⇒ Mach-O 上「观测提取」的手段不许携链接期引用；`/alternatename` 与 COFF 弱引用双双不可用作归档提取判据；`link.exe` **不**为 dllexport 强制提取）；**§9.1** 补一句「进程内来源上 Reset 无触发路径」；**§13.2**「插件形态」行加指针；**§13.3** 补一条「接受的缺口」（D229）；**§12.3** 的 M5 行补现状指针。
- `README.md`：产物落位段补中间目录 `pack-objects/` 与 STATIC 组合库的落位（与 `VasePlugins` **不同处**：一个是链接输入、一个是交付形态）。
- `.claude/skills/vase-cpp-engineering/`：按改动面复核，至少 `references/abi-boundary.md`（`LoadPlanEntry` 的 ABI 增量）与 `references/plugin-lifecycle.md`（进程内条目的 Eject/Adopt 语义、以及「一库 N 插件」的记录级语义）。

本文与磁盘冲突处以磁盘为准；本文定稿后若要偏离，按仓库惯例在 plan 的偏离登记里记名。
