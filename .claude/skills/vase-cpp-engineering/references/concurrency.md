# 并发与线程

Vase 当前的并发模型只有一句话：**它是单线程的，而且这条契约是承重的。**

不要把它读成"还没做多线程"。它是 `§1.4` 里明文的**设计决定**，并且是另外几件事能自洽的**必要条件**。

---

## 1. 契约

**一个进程只有一个 `PluginHost`。** 这不是可选取舍，而是进程级状态能自洽的必要条件：进程级状态的定义（"跟着二进制走"）建在"进程唯一"之上。**两个 Host 会有两张二进制表，同一份 DLL 被加载两次、得到两份静态数据——"进程级"当场失去唯一定义。**

宿主需要并发装配的话，**`Pod` 本来就是可多开的**（只要都在 Host 那个线程上）。

**`PluginHost` 绑定一个线程**：创建它时隐式取**调用创建函数的那个线程**。此后一切 Vase API 必须来自那个线程，Debug 下断言线程 id。

| 为什么是"绑定创建线程"，不是"宿主主线程" | |
|---|---|
| C++ 没有"主线程"这个语言概念 | 只能靠平台 API 猜，而 Vase 不该碰平台 |
| 引擎的玩法逻辑未必跑在主线程 | UE 跑在 GameThread。写死"主线程"会把引擎适配器坑死 |
| 绑定落在 Host 上，不落在 Pod 上 | Pod 的创建线程可能本来就活不长；Host 是进程级的，它的绑定线程自然长寿 |
| **隐式取而非传 `thread::id`** | 传参允许宿主传一个既非当前、也不属于自己的 id，错误会推迟到第一次使用才爆。隐式取的话"绑定的线程"与"创建的线程"永远一致，**创建那一刻就能验** |

宿主若选了个短命线程来创建 Host，那是它自己的设计错误——**第一次调用 API 就会断言失败，是响的**。

`PluginHost::AssertBoundThread` 在非绑定线程上**终止**进程，且**消息必须含子串 `not the bound thread`**——T8 的 death test 按它匹配。改动这句消息 = 改接口。

---

## 2. 单线程契约换来了什么（拆它会一并拆掉的东西）

**这是本节最重要的一段。** 谁日后为"支持多线程装配"拆掉这条契约，谁就得连下面这些一起重做：

| 免费获得的东西 | 依据 |
|---|---|
| **诊断计数不需要原子、不需要锁** | `DiagnosticCounters` 就是裸 `std::uint64_t`，头注释明确写了"不做原子、不加锁——依据是 §1.4 的绑定线程契约" |
| **`ServiceRegistry` / `EventBus` 不需要并发保护** | 它们的 `std::vector` + 墓碑式删除（`Alive` 标志）在单线程下就是正确的 |
| **`DependencyLedger` 的线性扫是正确的复杂度** | 边数以十计；"别预防性建索引" |
| **Adopt / Eject 不需要并发守卫** | 不存在"Eject 撞上半程 Adopt"的交叠态——这是平台难度预算里省得最狠的一条 |
| **`ScopePool` 的空闲链表不需要同步** | 它只被绑定线程碰 |
| **诊断报告的一致性** | 报告读的五项计数与账本边数来自同一段控制流 |

**想做多线程装配的话，正确的做法是在 Host 之上再包一层**（多个 Pod 各自在自己的线程上？——不行，那也破契约）。当前唯一的合法形态是：**一个 Host、一个线程、多开 Pod。**

---

## 3. 仓库里没有内部线程

**Vase 自己不创建线程。** `PluginHost` 只存一个 `std::thread::id ThreadId` 用于断言，`Loader` 的 `dlopen` / `FreeLibrary` 也在绑定线程上跑。

**线程是插件的事，但插件的线程必须能被干净停掉。** 手法只有一个：

```cpp
Result<void> OnStart(Context& ctx) {
    Worker.Start();
    // 注册得比服务注册更晚 → 回收得更早 → 线程先停、服务后注销
    ctx.GetScope().Create<StopWorkerEffect>(&Worker);
    return Result<void>::Ok();
}
```

**这是在构造函数里起线程会崩掉的根因**（见 `ownership-lifetime.md` 第 8 节）。

---

## 4. 事件派发的重入

`EventBus::Emit` 是**同步派发**，于是在 handler 里 `Remove` 自己的订阅会与正在走的派发链打架。`EventBus` 用两个字段解决：

```cpp
std::vector<Subscription> DeferredDestroy; // Emit 期间的 Remove 先记这里，派完再真删
std::uint32_t EmitDepth = 0;
```

**推论（写 handler 时的硬约束）：**

> **派发是同步的；handler 不得保存事件对象或其任何视图。**

`ctx.Emit(WorldLoadedEvent{ ... })` 传的通常是个临时对象，派发返回后即失效。存它的 `const&`、存它的 `string_view` 成员、把它塞进队列延迟处理——全部是悬垂。

**递归 Emit 是允许的**（`EmitDepth` 计数不是布尔）。但仍要小心：嵌套派发期间的 `Remove` 同样进 `DeferredDestroy`，直到最外层派发返回才真删。

`Tests/Unit/RegistryBusTests.cpp` 的 `EventBus.SelfUnsubscribeDuringEmitIsSafe` 守的就是这条重入边界（它断言 `DeferredDestroy` 到最外层派发返回才真删）。别与 `Tests/HotSwap/TimerNoReentryTests.cpp` 搞混——那条（`HotSwap.NoCallbackFiresIntoEjectedCode`）守的是 §7.6「Eject 之后的事件风暴不许落进已卸代码」，是另一件事、另一套判据。

---

## 5. 锁、原子、内存序

**当前仓库里几乎没有锁，这是结果而不是疏漏。** 需要引入同步原语时，先回到第 1 节问一句：这件事为什么不能在绑定线程上做？

如果确实要引入：

- 不写裸 `lock()` / `unlock()`，用 RAII（`std::lock_guard` / `std::scoped_lock`），**并且给锁命名**——未命名的 `lock_guard` 当场析构，是个静默的经典错误；
- 多把锁用 `std::scoped_lock` 一次拿，别手写加锁顺序；
- **不要 `volatile` 做同步**（它只用于硬件 I/O）；
- 不在持锁期间调用插件回调——那是死锁的直接来源，也与"绑定线程"契约冲突；
- **不要 lock-free**——除非你已经写了基准证明它必须。

**有一个契约性的例外要留意**：`_HAS_EXCEPTIONS=0`（语义与代价见根 `CLAUDE.md` 规矩 4）下 MSVC STL 的前置条件失败是进程终止，`std::mutex` 相关路径也不例外。别把 STL 的失败当成可恢复的。

---

## 6. 关停期的顺序

关停是**逆拓扑序**的 `EffectScope` 回收。线程类资源的正确顺序由 Effect 的注册序自动给出：

```
线程启动 → 注册 StopThread Effect（晚）
服务注册 → （更早）
```

回收时 StopThread 先跑、服务注销后跑。**只要线程的停止动作被注册成 Effect，就不需要 `OnStop` 钩子，也不需要手写同步。**

反过来，任何"在 Effect 之外起的线程"都活不到干净关停——它既不在回收序里，也不在诊断计数里。

---

## 7. 写测试时的线程

- 测试代码也在绑定线程上跑。**`ThreadGuardTests` 这类用例专测"从别的线程调用会怎样"**，那是刻意的越界探针。
- 需要验"非绑定线程 → 终止"的场景用 **death test**，并且匹配 `not the bound thread` 子串。
- 测试里不要起后台线程做"顺手清理"——那会破掉计数能归零的前提，且这类失败往往是偶发的。
