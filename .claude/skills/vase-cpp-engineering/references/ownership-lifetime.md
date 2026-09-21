# 所有权与生命周期

这份讲 Vase 里每个东西**归谁所有、能活多久、什么时候析构**。这份和 `architecture.md` 是同一件事的两面：那里讲结构，这里讲时间。

---

## 1. 所有权阶梯

```
PluginHost  ──进程级，活得最久────────────────────────┐
   │                                                  │
   └─▶ Pod ──一次运行────────────────────────────┐    │
          │                                       │    │
          └─▶ Plugin 实例 ──装配到关停──────┐      │    │
                 │                         │      │    │
                 └─▶ Effect / Service / 事件订阅    │    │
                     ──随各自 Scope 回收───────────┘    │
                                                       │
        DestroyPod 后 Pod 整体析构 ──「销毁即归零」是    │
        字面意义的 unique_ptr 释放─────────────────────┘
```

**每一层都能比上一层活得短，绝不能比上一层活得长。** 这就是 §1.2 铁律（实例级对象不得被进程级对象持有）的时间表述。

---

## 2. 服务指针的生命周期不变式

```text
服务指针的存活期  ⊆  提供方的实例存活区间（⊆ Pod ⊂ 二进制的驻留区间）
```

这条是 C++ 里替代 GC 的关键保证。**v3 的关键变化在最里面那层括弧**：

- v2 里它自动成立——提供方随局同死，"服务不跨局"就是全部防线；
- **v3 的提供方可能在局内被 Eject 提前送走**，这层括弧不再自动成立。补上它的是**依赖账本**：Eject 前账本必须证明没有任何活着的实例引用被卸方的服务。

推论：**宿主提供的服务也随 Pod 销毁。** `ServiceOrigin::kHost`（GPU、引擎桥接这类）的注册入口同样是 `ctx.Provide<T>()`，而 `ctx` 属于某个 Pod——所以即便服务是宿主自己实现的，也必须**每个 Pod 重新注册一次**。**没有任何服务的生命周期能长过 Pod。**

宿主侧纪律（不变）：**不跨边界持有服务指针、不缓存、每次现取**。这是契约束，机制管不了（见 `architecture.md` 第 6 节）。

---

## 3. 非拥有指针：cookie 与借用字符串

账本 `LedgerEdge` 与 `ServiceEntry` 用的是同一套写法：

```cpp
const void* Consumer = nullptr;  // 插件实例 cookie
std::string_view ConsumerId;     // 借用字符串
```

- **cookie 是非拥有标识**，不是要解引用的指针（`Context::ConsumerCookie` 同理）。比它活多久的问题是问错了问题——它的用途是"相等比较"。
- **借用字符串指进描述符字面量**，安全性来自这个包含关系：`镜像驻留 ⊇ 实例存活`，而 `PluginMeta` 的字面量活在镜像的只读数据段里。**所以只要镜像还映射着，`Id` 就有效**——这也正是 Adopt 必须验身份特征、Eject 必须等实例先拆净的原因。
- 但**别赌**：需要跨过镜像边界的字符串要拷贝。`LiveInstance::OwnerLabel` 就是 `Meta->Id` 的拥有型拷贝，注释写着"别赌字面量生命周期"。

**`DependencyLedger::EdgesTo` / `EdgesFrom` 返回的指针指进账本内部存储**：下一次改动（`Record` / `RemoveByInstance` / `ClearPod`）即失效。调用点都是"查到就当场读"，**别把它们存过界**。

---

## 4. EffectScope：注册与回收的唯一容器

- **逆序回收**：回收顺序 = 注册顺序的逆序。这就是关停不需要 `OnStop` 钩子的全部原因。一个在 `OnStart` 里起线程的插件，只要把"停止线程"也注册为 Effect，它注册得更晚、回收得更早——**先停线程、再注销服务，自动成立**。
- **`Dispose` 幂等**，且回收后不得再注册（终态）。
- **销毁路径 = `Recycle()` + 显式析构调用，不走 `delete`**。对象内存归 `ScopePool` 所有：

```cpp
// EffectScope::Create<T> —— 析构必须由 T 自己跑
[](IEffect* object) { static_cast<T*>(object)->~T(); }
```

`~IEffect` 是**保护非虚**的：`delete IEffect*` 编不过，销毁权只属于 `EffectScope`。多继承下 `void* Memory`（placement-new 的原始块）与 `IEffect* Object` **可能不同址**——`Release` 时原样把 `Memory` 还给池。

- **句柄不得活得比 Scope 久**（见 `architecture.md` 第 4 节末）。

---

## 5. 这个仓库的 RAII 写法：消灭裸 `new` / `delete` 表达式

不是"尽量别写"，而是**裸 `new` / `delete` 表达式过不了门禁**（`cppcoreguidelines-owning-memory` 一族）。既有代码里的三条固定手法：

**① 工厂 + 析构配对**（`VASE_PLUGIN` 宏）：

```cpp
::vase::Plugin* VasePluginCreate_##Type() { return std::make_unique<Type>().release(); }
void VasePluginDestroy_##Type(::vase::Plugin* raw) { const std::unique_ptr<::vase::Plugin> owning{raw}; }
```

**② 堆壳**：把"移交进来的那个 `unique_ptr`"本身放到堆上，再用 `release()` 把外壳交给注册表项：

```cpp
// Context::Provide(std::unique_ptr<T>)
std::unique_ptr<std::unique_ptr<T>> shell = std::make_unique<std::unique_ptr<T>>(std::move(owned));
return ProvideRaw(key, instance, shell.release(), &DestroyShell<std::unique_ptr<T>>);
```

**③ 统一的销毁端**，用 `unique_ptr` 接住再析构：

```cpp
template <typename Cell>
static void DestroyShell(void* cell) { const std::unique_ptr<Cell> owning{static_cast<Cell*>(cell)}; }
```

**为什么这么绕**：`Provide` / `On` 是模板，模板实例化在**调用方的 TU**（也就是插件自己的 TU）里，而注册表的销毁回调要存成函数指针跨 TU 调用。外壳把"类型"这件事固化进了一个非模板的 `void(*)(void*)`——`DestroyShell<Cell>` 是 `Context.h` 里的成员模板，随 `Provide<T>` / `On<E>` 在**插件 TU 内**被隐式实例化（定义在头文件里正是这件事的前提；`Source/Pod/Context.cpp` 不实例化它）。于是 `delete` 落在持有该类型的那个镜像里执行，跨 DLL 也一致。

**迁移式注册的 `delete` 发生在适配器的 `Recycle()` 内部**，虚调用在持有该类型的镜像里执行——与 `VasePluginDestroy_` 同一性质。这是"对象内存归 `ScopePool`、但 `unique_ptr` 移交的堆内存归它自己"的分界。

---

## 6. 析构顺序与不完整类型

- **`Pod::~Pod()` 声明在头里、定义在 `Pod.cpp`。** 类内默认会让每个用到 `unique_ptr<Pod>` 的 TU 都实例化 `Pod` 的成员析构，而它们要 `ServiceRegistry` / `EventBus` 的完整类型——Host 侧于是被迫包含对它无用的 `RegistryBus.h`（实测报错：`invalid application of 'sizeof' to an incomplete type`）。**加持有 `unique_ptr<不完整类型>` 的成员时，跟着照做。**
- **`Pod` 的拷贝与移动全删**：它由 Host 以 `unique_ptr` 独占，值语义从不存在；引用成员（`Pool` / `Counters`）本来也删掉值语义。
- **`TeardownInstancesAndRoot()` 是逆序回收的机器**：先逆序拆实例、再收根 Scope。

---

## 7. 插件实例的状态与"失败"

```
kLoading ──▶ kLoaded ──▶ kStarted
    │            │
    └────────────┴──▶ kFailed
```

`Failed` **不是真正的终态**——它保留的是**记录**，不是实例。

- 失败时**立刻回收本插件的 `EffectScope`**：`OnLoad` 里注册到一半的服务与订阅必须当场拆干净，否则就是泄漏。状态机里 `Failed --本局销毁--> Unloaded` 这条边存在，只是那时回收的是一个已经空了的 Scope。
- 保留下来的是 `FailedPluginRecord{ Id, Stage, Message }`——一份**不含 Effect 的轻量诊断记录**。它**不算"插件实例"**，所以"插件实例数 → 0"的断言仍然成立。
- **"保留实例以便诊断"要改读成"保留记录以便诊断"。**

**两张表分开是必须的**（`Pod::FailureRecords` 与 `Pod::FailedBinaries`）：二进制根本没驻留（`EnsureResident` 自己失败）也是一种失败，那种只有记录、没有镜像。Eject 靠后一张表把镜像一并摘掉。

---

## 8. 三条容易写反的规矩

| 规矩 | 理由 |
|---|---|
| **构造函数只做初始化，不注册副作用、不启动活动** | 上面的逆序回收论证依赖"`OnStart` 里注册的 Effect 比 `OnLoad` 里的晚"。在构造函数里起线程并注册停止 Effect，那个 Effect 会比服务注册**更早**——回收更晚，线程就会在服务已注销之后继续跑。那时也还没有 `Context` 可用，这条与其说是纪律，不如说是接口形状的自然结果 |
| **`Plugin` 的默认构造必须显式 `= default`** | 删拷贝 / 移动会一并抑制隐式默认构造，少了这行 `VASE_PLUGIN` 的 `make_unique<Type>()` 当场编不过 |
| **`Plugin` 的虚析构必须存在** | `VasePluginDestroy_` 在插件镜像内 `delete`，这条路径的正确性以它为前提 |
