# 组 7 并发不白付

`cpp20-zero-overhead` 的第 7 组细则。元规则与例外登记见 `../SKILL.md`。

并发这一组的账分三种，别混：**同步原语本身的指令成本**、**它带来的访存序列化**、**验证它的工程成本**。前两种能测，第三种最贵但通常没人记账。

### 7.1 不共享就不欠账（CGL CP.3）
共享可写数据的**面积**决定后面所有的账。能不共享就不共享：线程局部状态、任务边界上的所有权移交、每线程一份再加归约——都比「共享加锁」便宜，且便宜在无需论证。
- **执法**：`cppcoreguidelines-avoid-non-const-global-variables`、`misc-static-initialization-cycle`。
- **依据**：CP.1（假设你的代码会被放进多线程程序里跑）、CP.2（避免数据竞争）。

### 7.2 默认内存序不是免费的，但省钱要看平台
`std::atomic` 默认 `seq_cst`。各平台的真实账单：

| 平台 | relaxed / acquire load | release store | seq_cst store |
|---|---|---|---|
| x86-64（TSO） | 普通 `mov` | 普通 `mov` + 编译器屏障 | **全屏障**：`xchg` 或 `mov` + `mfence` |
| AArch64 | `ldar`（独占性更弱的序，带 barrier 语义） | `stlr` | `dmb` ishst 一类屏障指令 |

- **结论**：在 x86-64 上把 seq_cst 降成 release/acquire，省的**只有 store 那一侧**；在 AArch64 上 acquire/release 本身就是有语义的指令，降到 relaxed 才有可见差异。**只对某一个平台做这条优化，等于在另一个平台上写下错误的前提。**
- **执法**：阶梯 1 是唯一可靠手段——看屏障指令是否真的消失。降级必须附 happens-before 论证与两平台的反汇编差异。
- **钉住假设**：`static_assert(std::atomic<T>::is_always_lock_free)`——运行期 fallback 到内部锁的 `atomic`，成本和竞态行为都变了。

### 7.3 临界区里的账算在持有时间上
无竞争的 mutex 本身只是几轮 CAS/原子 RMW，不贵。**贵的是你在里面干了什么**：临界区内一次分配（组 4）、一次虚调用（1.2）、一次 I/O，都是把别的组的账搬进锁的持有时间，然后由竞争放大。
- **做法**：临界区只碰受保护的数据本身；准备工作和善后放外面。
- **执法**：`cppcoreguidelines-no-suspend-with-lock`（持锁跨越挂起点）、`misc-coroutine-hostile-raii`（协程里的 RAII 锁）、`concurrency-mt-unsafe`（`strtok` / `localtime` / `rand` 一族非线程安全 API）。

### 7.4 无锁的成本主要在验证
无锁数据结构的运行期账常常低于加锁，**但验证账高得多**：内存序推导、ABA、活性。省下的运行期成本抵不上多出来的错误预算，除非它确实在热路径上且锁已被证明是瓶颈。
- **执法**：TSan（`-fsanitize=thread`）+ 针对并发模型的压力/交错测试；工具可用性按平台确认（MSVC 线无 TSan）。
- **申诉路径**：主张「这里必须无锁」的，要拿出阶梯 3 或 4 的数字证明锁是瓶颈。

### 7.5 上下文切换（Per.30）
临界路径上一次 involuntary 切换是微秒级——比这一组上面所有条目加起来都贵。自旋还是阻塞取决于**核预算与等待时长**：核数富余且等待短才轮到自旋，否则自旋是在烧别人的核。
- **执法**：阶梯 3 的 `context-switches` 计数；阶梯 4 才谈端到端影响。

### 7.6 任务优先于线程（CGL CP.4）
`std::async` / 任务队列把线程的生命周期成本（创建、栈内存、TLS 分配）摊到复用池上。反复创建线程是组 4 的分配账在并发侧的复现。
