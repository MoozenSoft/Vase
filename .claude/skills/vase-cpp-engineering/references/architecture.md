# Vase 架构：分层、依赖、归属、可见性

这一份只讲架构事实：**谁依赖谁、谁拥有谁、谁可以访问谁**。C++ 写法不在这里（那是 `cpp-core-guidelines.md` 的事）。

所有内容以磁盘上的头文件为准。标了 **［提案］** 的是 `wiki/vase-architecture.md` 里有设计但**仓库里还没有**的部分——不要把提案写成既定事实。

---

## 1. 三层模型

```
┌─ 目录层 PluginCatalog ［提案］────────────────────────────────┐
│  目录扫描 · 清单解析 · 求解（纯数据，不执行任何代码）            │
│  M1 不存在。M1 的 LoadPlan 是手写 struct（D12），M2 的          │
│  PluginCatalog::Solve 产出同一个类型——装配路径零返工。          │
└───────────────────────────────────────────────────────────────┘
                              │
┌─ 进程层 PluginHost（VaseHost 动态库）─────────────────────────┐
│  进程唯一 · 绑定创建线程 · 拥有 Loader / ScopePool / 计数 /     │
│  依赖账本 / 活 Pod 表 · 对外只给 PodHandle                     │
└───────────────────────────────────────────────────────────────┘
                              │
┌─ 实例层 Pod（VasePod 动态库）─────────────────────────────────┐
│  一次运行的边界 · 服务表 · 事件总线 · 根 Context · 每插件一个   │
│  EffectScope · 析构即归零                                      │
└───────────────────────────────────────────────────────────────┘
```

**链接方向是单向的**：`VaseHost ──PUBLIC──▶ VasePod`。这一条不是约定，是链接期事实：

- 插件 target **只链 `VasePod`**（D11），手搭 `add_library(SHARED)` 会漏掉这条约束——所以插件一律经 `vase_add_plugin_fixture` 立。
- 反过来，`VasePod` 看不见 `VaseHost`。这就是为什么 `PodReport` / `PodHandle` / `FailedPluginRecord` 定义在 `Include/Vase/Pod/Pod.h` 而不是 `PluginHost.h`：**类型归属跟着最底层的消费者走**，能进 `Pod` 的类型才能被 `Pod` 存储。

---

## 2. 目录 ↔ 层

| 目录 | 层 | 内容 |
|---|---|---|
| `Include/Vase/Plugin.h` | 插件面 | **伞形头**：插件作者唯一需要包含的入口，转出宏、基类、`Context`、`Result`、`Error` |
| `Include/Vase/PluginDescriptor.h` | 插件面 | `PluginMeta` / `PluginDescriptor` / `Plugin` 基类 / `kHeaderVersion` / `VASE_PLUGIN` 宏 |
| `Include/Vase/Service/`、`Event/` | 插件面 | 服务与事件的**标识约定**（`kName` + `kVersion` 概念） |
| `Include/Vase/Effect/` | 插件面 | `IEffect` / `EffectScope` / `EffectHandle`——注册与回收通道 |
| `Include/Vase/Pod/` | 实例层 | `Pod` / `Context` / `DependencyLedger` |
| `Include/Vase/Host/` | 进程层 | `PluginHost` / `Loader` / `LoadPlan` / `Evidence` |
| `Include/Vase/Detail/` | 实现内部 | 上文两者都要见到的共享件：`Result` / `Fail` / `Export` / `Counters` / `ScopePool` / `RegistryBus` / `MetaArray` / `ImageInspect` |

`Detail/` 的存在理由是"**两个层都要见同一份布局，而它们都不该包含对方的头**"——`Counters.h` 与 `RegistryBus.h` 的注释就写明了这一点。往里加东西前先问：真的两边都要见吗？

---

## 3. 概念集

| 概念 | 是什么 | 关键约束 |
|---|---|---|
| `PluginHost` | 进程唯一、绑定线程的二进制层 | 三个进程级容器在**类型上**装不下实例级对象（见第 5 节） |
| `Pod` | 一次运行的边界 | 由 Host 以 `unique_ptr` 独占；拷贝与移动全删，值语义从不存在 |
| `PodHandle` | `{Index, Generation}` 句柄 | `Generation` 从 1 起——0 是默认构造的句柄，不该解析到任何槽 |
| `Context` | Pod 内的服务访问入口 | 查找是**一张扁平面**：子 Context 不是查找链的一环 |
| `Plugin` | 插件基类 | 只有 `OnLoad` / `OnStart` 两个虚函数 + 虚析构；拷贝与移动全删 |
| `PluginDescriptor` | 二进制契约 | `HeaderVersion` 必须是**第一个**字段（`static_assert` 承重） |
| `IEffect` / `EffectScope` / `EffectHandle` | 副作用与它的归属容器 | `Scope` 逆序回收、`Dispose` 幂等、回收后不得再注册 |
| `ServiceRegistry` / `EventBus` | 实例级的两个容器 | Pod 各持一个；`ServiceRegistry` 一个标识一个实现 |
| `DependencyLedger` | 进程级账本 | 只存非拥有 cookie + 借用字符串；`DestroyPod` 整批清零 |
| `DiagnosticCounters` | 五项计数 | **不做原子、不加锁**——依据是单线程契约（§1.4） |
| `ScopePool` | size-class 空闲链表 | **只持有空闲节点内存，不持有任何实例级对象**；这是 D10「稳态零分配」的实现 |
| `Loader` / `BinaryRecord` | 平台差异的唯一收口 | "一文件一记录"：同一绝对路径只调一次平台加载 |

---

## 4. 谁拥有谁

```
PluginHost（进程级，unique_ptr 独占）
├── Loader          ── 拥有 BinaryRecord 表（path + void* 句柄）
├── ScopePool       ── 拥有空闲内存块
├── Counters        ── 值成员
├── Ledger          ── 值成员，存非拥有指针
└── Slots ──▶ Pod（unique_ptr 独占）
             ├── ServiceRegistry   ── 拥有服务项（含移交式注册的堆壳）
             ├── EventBus          ── 拥有订阅项
             ├── RootScope         ── 宿主服务的归属
             ├── RootContext       ── SelfMeta == nullptr
             └── Instances ──▶ LiveInstance（unique_ptr 独占）
                               ├── Desc         非拥有（指进镜像的静态数据）
                               ├── Instance     由 Desc->Create/Destroy 配对管理
                               ├── Binary       非拥有（实体在 Loader 表里）
                               ├── Scope        ── 该插件一切注册的归属
                               ├── Ctx          ── SelfMeta == 本插件
                               └── OwnerLabel   拥有型字符串拷贝
```

**`EffectHandle` 不是所有权**。它是一个索引 + 代际句柄，可平凡拷贝；拷贝出去的第二份在槽位被回收并复用后自动失效。**但句柄不得活得比它的 `EffectScope` 久**——代际只护"槽位复用"那一侧，护不了"Scope 已死"。陈旧句柄的安全性是**不完整的**。

---

## 5. 谁可以访问谁

- **插件**只能看到 `Context`。它拿不到 `Pod`、拿不到 `PluginHost`、拿不到别的插件的实例——插件间一切调用经服务解析。
- **宿主**通过 `PodHandle` 拿 `Pod*`（`Resolve`），或者干脆只用 `CreatePod` / `DestroyPod` / `Adopt` / `Eject` 四个 API。
- **`Context` 的四条边界是刻意的设计取舍，不是未实现的功能**：
  1. 不做惰性属性解析——服务必须显式取（`ctx.Get<T>()`），取不到是显式的运行期错误；
  2. 不做**稳态运行期**的服务自动重载。注意限定词：**装配期**（`OnLoad` + `OnStart`）内部服务仍可能消失（见 `plugin-lifecycle.md` 的递归拆除）；
  3. 不做隔离域——一次 Pod 一个服务集合；
  4. 有意不支持同名服务多实例。需要"两个物理世界"的场景应由**服务自身的方法签名**承载（`GetPhysics(worldId)`），不由注册表承载。

**新增字段 / 新增容器时的自检**：它可以被谁访问？它是进程级还是实例级？被进程级持有它会不会破坏铁律？——答不上来就先别写。

---

## 6. 铁律的覆盖范围（别高估它）

> **实例级对象不得被进程级对象持有。** 反向允许。

§1.2 说明它由 Debug 下的实例归属追踪器抓违规，但**必须说清覆盖范围**：

- 追踪器只看得住 **Vase 自己的容器**——能抓框架内部的实现疏漏；
- **管不了宿主和插件**：宿主把 `IPhysics*` 存进自己的成员变量、插件把服务指针塞进未登记的静态变量，都在它的视野之外。

后两类是**契约束**（§9.3），不在这里假装由机制保证。宿主侧的防线只有一条纪律：**每次现取、不缓存**。

（M1 落地的形态：`EjectReport` / `PodReport::Residuals` 的残留归属报告机制已立住，完整属主追踪器属 M3。）

---

## 7. 明确定死的事（不要以"漏了"的形式重新提出）

- 一个进程多个 `PluginHost`（§1.4）
- 一个服务标识多个实现（§2.3）
- Pod 嵌套（§2.3）
- **级联热替换 / 强制 Eject**：v3 只放行叶节点。叶热替换已是核心承诺，级联热替换仍然不做（§13.4）
- **epoch / 全量 HMR / 依赖方原地重绑定**：账本从源头拒绝制造上下线
- C ABI：预留的边界收缩方案，当前不启用（§8.4）
- Android / iOS 上的任何热插拔——档 ① 也不给例外
