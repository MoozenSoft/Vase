# ABI 与跨 DLL 边界

**这是本 skill 里第二重要的文件。** 核心规则只有一句：

> **Vase 内部的 C++ 写法 ≠ 插件边界上的 C++ 写法。**

内部改错了，重新编译整个仓库就修好了；边界上改错了，是**已经发出去的插件**加载不进新宿主、或者更糟——加载进来了但 vtable 布局已经不同。所以从"内部实现"往"跨 DLL 可见面"搬代码时，**要重新过一遍这份清单**，不能因为"仓库里别处就是这么写的"就照抄。

---

## 1. 主路径是纯 C++ 接口，前提有三条

`§8.3`：主路径是纯 C++ 接口（非 C ABI），前提是插件与宿主：

1. 用**同一工具链**构建（同编译器、同版本）；
2. 用**同一 CRT 配置**（MSVC 下 `/MD` 共享 CRT，保证跨模块 `new` / `delete` 落到同一个堆）；
3. 包含**同一份 Vase 头文件**。

> **前两条靠构建系统约束，第三条靠机制。只写前提不写检查，前提就是一句祈使句。**

第三条的执行点就是 `HeaderVersion`：

```cpp
inline constexpr std::uint32_t kHeaderVersion = 1U;

struct PluginDescriptor {
    std::uint32_t HeaderVersion = 0;   // 必须是第一个字段
    const PluginMeta* Meta = nullptr;
    Plugin* (*Create)() = nullptr;
    void (*Destroy)(Plugin*) = nullptr;
};

// 布局由断言承重：加载路径就按这个偏移先读它，改动字段顺序会编译期失败，
// 而不是运行期读到垃圾。
static_assert(offsetof(PluginDescriptor, HeaderVersion) == 0, "...");
```

**每次不兼容改动递增它。** 什么算不兼容：`Plugin` 虚函数表变了、`PluginMeta` / `PluginDescriptor` 的字段或顺序变了、`ServiceRef` 变了、任何跨边界结构体的布局变了。递增时在提交信息里写清改了什么——插件作者不需要知道它存在，但**宿主需要一句能读懂的解释**。

---

## 2. 描述符纪律：POD

`PluginMeta` / `PluginDescriptor` / `MetaArray` / `ServiceRef` 全部是**纯数据、可平凡拷贝**：

- 固定布局、数组 + 长度；
- `const char*` / `string_view` 指向**只读字面量**；
- **没有堆、没有构造函数副作用、没有虚函数**。

理由不是洁癖：`VaseCli scan`（M5）要**只读数据段拿到全部元信息，不执行镜像里的任何代码**。这条一旦破掉，静态扫描就得跑代码——那是另一个量级的安全与复杂度问题。

`MetaArray` 而不是 `std::span` 的原因（`spec 3.3(2)`，已过编译探针）：`std::span` **没有 `initializer_list` 构造函数**，文档写法直接编不过；也不押注 `initializer_list` **成员**的存储（探针未复现悬垂，正确性依赖实现细节）。自持存储、`constexpr` 可构造、POD 可平凡拷贝——**跨 Win/Linux 三套 STL 不赌行为**。

`Requires` / `Provides` 各 16 条上限是**编译错误，不是静默截断**。

---

## 3. 符号可见性

| 宏 | 用途 |
|---|---|
| `VASE_EXPORT` / `VASE_IMPORT` | `__declspec(dllexport/dllimport)` 或 `__attribute__((visibility("default")))` |
| `VASE_HIDDEN` | Linux/macOS 的 `visibility("hidden")`（Windows 上是空的） |
| `VASE_POD_API` / `VASE_HOST_API` | 各动态库自己的公开 API：构建该库时导出（由 `VASE_<LIB>_BUILD` 宏切换），其它地方导入 |

两个 DLL 都设了 `CXX_VISIBILITY_PRESET hidden` + `VISIBILITY_INLINES_HIDDEN ON`。**Linux/macOS 上默认可见性是全开，必须收紧到"只露 `VASE_EXPORT` 的"——这也是插件宿主能干净卸载的前提之一。**

**新增导出面的两条经验**（既有代码里的原话）：

- 只给**需要的成员函数**加 `VASE_POD_API`，不给整个 `struct` 加——`struct` 级别的导出会把 STL 成员带进 `C4251` 的射程（`PodReport::Clean()` 就是这么处理的）；
- 定义在 `VasePod` 里的成员函数不导出即**链接期失败**（实测 lld-link: `undefined symbol`），不会拖到运行期。

### `RTLD_LOCAL` 与链接纪律（§8.7）

热插拔把"符号污染"从卫生问题升格为**安全问题**——插件会反复进出，任何一条隐式耦合都是日后 Eject 假成功的伏笔。三条纪律：

| 纪律 | 内容 | 执法点 |
|---|---|---|
| **`RTLD_LOCAL`** | Linux/macOS 的 `dlopen` 一律 LOCAL。不学 POCO 的 `RTLD_GLOBAL` 默认 | `Loader` 层封装，**不给宿主选项** |
| **禁二进制级互链** | 插件只准链 Vase 本体一个库；插件 A 的导入表不得引用插件 B 的文件名 | Adopt 时读 PE/ELF 导入表，命中兄弟插件即拒 |
| **跨插件只走服务边** | 插件间一切调用经服务解析（账本可见），不走裸符号引用 | 上条执法 + `RTLD_LOCAL` 使符号边根本不可表达 |

破第二条的后果不是"不规范"：加载器会替 B→A 拉一条**账本看不见的边**，还把引用计数焊死 → **Eject 当场假成功**。

**`VaseHost` 的交付形态随之定死：Linux/macOS 上必须编为共享库**（`RTLD_LOCAL` 下插件对 Vase 符号的导入需要可见的导出表，而可执行文件不导出符号，除非 `-rdynamic`——那又回到全局污染）。

---

## 4. 边界上不做什么

| 规则 | 理由 |
|---|---|
| 不在边界上依赖 `type_index` 的运行时比较 | 跨 DLL 的比较依赖编译器实现细节。**服务与事件标识一律用 `kName`（编译期常量字符串）+ `kVersion`（整数主版本）** |
| **禁跨 DSO 使用 `dynamic_cast` / `typeid`**（v3 明文化） | RTTI 跨模块比较不可靠；`RTLD_LOCAL` 更会让它**静默**失效 |
| 边界上不传所有权 | 不传 `std::unique_ptr` 跨边界；或明确配对约定 |
| 异常不跨边界 | 见 `error-handling.md` |
| 优先 `std::string_view` / `std::span` | 少一次分配，也少一个 ABI 面 |
| 不让插件回传宿主侧类型 | 插件只认识 Vase 头里的东西 |

### 标识约定的**唯一解法**（服务与事件同构）

```cpp
struct DamageEvent {
    static constexpr std::string_view kName    = "Vase.Combat.DamageEvent";
    static constexpr uint32_t         kVersion = 1;
    EntityId Target;
    float    Amount;
};
```

`ctx.On<DamageEvent>(...)` 通过 `E::kName` / `E::kVersion` 查表，并用概念 `HasEventIdentity` / `HasServiceIdentity`（`static_assert`）校验声明完整。

**如果按 C++ 类型标识，跨 DLL 的坑要踩两遍。这里没有第二条路可走。** 改 `kName` = 换了一个服务/事件；改 `kVersion` = 不兼容变更。

---

## 5. 身份特征：档三验证的构建要求

热插拔的核心承诺是"换了新代码"能生效。**"重载拿到旧代码"的机制是 `dlopen` 命中缓存镜像、根本不重读磁盘**（POSIX 明文：同路径命中 ⇒ 同一句柄，引用计数 ++）。真实触发条件是：引用计数未归零、被其他已加载对象 `DT_NEEDED`、`RTLD_NODELETE`、`STB_GNU_UNIQUE`。

档一（实例归零）与档二（映射解除）**可以全绿，只有身份特征比对分得出新旧**。所以：

| 平台 | 身份特征 | 构建要求 |
|---|---|---|
| Windows | PE 调试目录的 CodeView **RSDS** GUID + Age | **`/DEBUG:FULL`**（没有它就没有 RSDS） |
| Linux | `.note.gnu.build-id` 的 desc | **`-Wl,--build-id=sha1`** |
| macOS | `LC_UUID` | 链接器必写，天然满足 |

**这两条 flag 不是可选的优化，是身份特征的构建要求**，写在三个工具链文件的 `CMAKE_SHARED_LINKER_FLAGS_INIT` 里——**flag 的字面值与"改动必须重跑 HotSwap 主循环"这条要求，以根 `CLAUDE.md`「工具链 flag 是承重的」那节为准**（本表留着是因为它按平台对位身份特征，那份没有 macOS 一行）。**摘掉它们的症状是运行期的响亮失败，不是编译失败**：构建照常全绿，直到 Adopt 全线拒绝才暴露。

**特征缺失 = Adopt 直接拒绝**，报告指路补链接标志——**响亮的失败，不做"跳过验新"的静默降级**。内存哈希兜底记在 §13.1，暂不设计。

另一个 configure 期的坑：`CMAKE_*_FLAGS_INIT` **只在工具链首次 configure 时进入缓存**。工具链后来才加上 `/DEBUG:FULL` 而某棵树在那之前就配过，那棵树的 `CMAKE_SHARED_LINKER_FLAGS` 就是空的。**修法是删掉那棵树重新 configure**，不是用 `-D` 钉一个永久的手工 override——后者会把后续所有工具链改动一起遮住。

---

## 6. `C4251` 与 `/wd4251`：已知的债

导出面的类**带着 STL 成员**（`ScopePool` 的 `Chunks` / `FreeLists`、`EffectScope` 的 `Slots`、`DependencyLedger` 的 `Edges`…），`cl.exe` 会对每一个包含这些头的 TU 报 **C4251**，在 `/WX` 下即 error。Vase 在构建侧用全局 `/wd4251` 一次压掉（§8.3）。

**flag 本身、全局豁免的两条代价、以及 §8.5 前提破掉时的退路，以根 `CLAUDE.md` 规矩 1 末段为准；站点数与 M5 的两条还债路径以 `wiki/vase-architecture.md` 13.3 为准**（那里 2026-09-18 已记账，本文件不重抄实测数）。这一节只留 `CLAUDE.md` 与 wiki 都不讲的那一层：**为什么会错配**。

1. **它是整 TU 免检**——新增"导出类带 STL 成员"**不再有逐处把关**；
2. **它出不了这棵树**——关键错配在于：**那条 flag 顺着 CMake 的 link 边传播，而 C4251 顺着 include 边触发**。仓库外的插件作者按 `§3.1` 只写 `#include <Vase/Plugin.h>`、在自己的工程里构建，C4251 会报在 **Vase 的头文件那一行**、由**他们自己的**编译选项管，Vase 无法替他们加。

**前提是 §8.5 的工具链与 STL 矩阵钉死、D13 的 `HeaderVersion` 兜着。若那条前提破掉**（允许插件用不同版本的 MSVC STL 构建），全局豁免即失去依据，届时改 pimpl 或把 STL 成员移出导出面。

**这笔债在 M5 分发时必须还**，二选一：① 把豁免随导出目标一起分发（`install(EXPORT)` 的 INTERFACE target 带 `/wd4251`）；② 把 STL 成员移出插件面（pimpl）。**②同时兑现"Vase 需要作为二进制分发给第三方插件作者"这条启用条件。**

---

## 7. 搬运代码时的检查流程

从内部实现往跨 DLL 面搬东西，或新增一个跨边界类型时，逐条过：

1. 它有虚函数吗？虚表布局变了要**递增 `kHeaderVersion`**。
2. 它带 STL 成员吗？带就不是 POD——**它绝不能进描述符**。
3. 它的字段顺序会变吗？改顺序 = 改 ABI，即使类型名没变。
4. 它在边界上被 `dynamic_cast` / `typeid` / `type_index` 识别吗？**一律改成 `kName` + `kVersion`。**
5. 它跨边界传所有权吗？`unique_ptr` 不跨；要跨就写清配对约定。
6. 它是字符串吗？`string_view` 借用 + 明确"谁的镜像在它背后"（见 `ownership-lifetime.md` 第 3 节）。
7. 它进描述符了吗？那就必须是纯数据、可平凡拷贝、指向只读字面量。

**改了这一节里任何一条，验证要求就是 `verification.md` 表里的"Loader / 描述符布局 / `HeaderVersion`"那一行**——`Tests/HotSwap/` 全部用例，双平台各自留证据。
