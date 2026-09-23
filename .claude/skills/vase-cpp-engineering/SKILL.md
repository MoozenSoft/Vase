---
name: vase-cpp-engineering
description: Vase 插件框架的 C++ 工程约束。在 Vase 仓库里设计、实现、修改、评审任何 C++ 代码时使用——覆盖架构不变量（铁律、三层分层、依赖账本）、所有权与生命周期、跨 DLL / ABI 边界、Adopt/Eject 热插拔、错误通道（Result<T>，全项目无异常）、单线程并发契约、性能口径与六 preset 验证制度。只要任务涉及 Include/Vase、Source/{Pod,Host}、插件 target、Effect / Service / Event / Loader，或 Tests/ 下的用例，就先读本 skill：这个仓库里"编得过、测试也绿"常常正是架构已经坏掉的形态，而坏掉的那部分恰好是本库存在的理由。
---

# Vase C++ 工程

这份 skill 只回答一个问题：**在 Vase 里，什么时候允许什么、什么边界绝对不能跨、改什么必须验证什么。**

通用的 C++ 最佳实践不在这里——那是 `references/cpp-core-guidelines.md` 的内容，需要时按路由表去读。

## 工程权重

这决定了遇到取舍时先顾什么。上层盖过下层：

```text
                    Vase Engineering
                          │
         ┌────────────────┼────────────────┐
         │                │                │
     Architecture      Lifetime          ABI
         │                │                │
     Invariants       Ownership        Boundary
     Dependency       Teardown          Loader
     Contracts        HotSwap           Version
         │                │                │
         └────────────────┼────────────────┘
                          │
                     Correctness
                          │
            ┌─────────────┼─────────────┐
            │             │             │
         Errors       Concurrency    Portability
            │             │             │
         Result        Thread        Win / Linux
         Error         Atomic        Toolchain
            │             │             │
            └─────────────┼─────────────┘
                          │
                      Performance
                          │
              Allocation / Cache / CPU
                          │
                          ▼
                     Verification
```

**代价是"零开销"不在一级目录里。** 它是贯穿各层的约束——任何抽象都必须解释自己的成本，但不是禁止抽象。这份图也是路由表的排序依据：**越靠上的问题，答错的代价越高。**

## 仓库当前状态（读其余内容前先记住）

- **M0（构建地基）与 M1（Pod 闭环 + 热插拔骨架）已完成。** 已落地的机制以**磁盘上的头文件与实现**为准。
- `wiki/vase-architecture.md` v3 里**未落成代码的部分仍是提案**（清单 / Preset / Catalog / 配置宏反射 / 级联热替换 / C ABI / macOS 与移动端）。不要把提案写成既定事实；新增或变更机制前先与需求方确认。
- 仓库根 `CLAUDE.md` 是命令、规矩与基数的唯一真值来源。本 skill 与它冲突时以它为准，并回来修正本 skill。

## 一、路由表：现在该读哪一份

| 你正在做 | 读 |
|---|---|
| 搞不清谁依赖谁、谁拥有谁、某个类属于哪一层 | `references/architecture.md` |
| 决定某个东西归谁所有、能活多久、能不能借用、什么时候析构 | `references/ownership-lifetime.md` |
| 要返回失败、要写校验、要用标准库的会抛错 API | `references/error-handling.md` |
| 要新增 / 改动跨 DLL 可见的东西：导出符号、描述符字段、结构体布局、虚函数、服务或事件的 `kName` | `references/abi-boundary.md` |
| 碰 `OnLoad` / `OnStart` / EffectScope 回收 / Adopt / Eject / 依赖账本 / Loader 驻留 | `references/plugin-lifecycle.md` |
| 写线程、写锁、写原子量、怀疑有重入或数据竞争 | `references/concurrency.md` |
| 加一层间接、担心分配 / 缓存 / 代码体积 / 启动关停开销 | `references/performance.md` |
| 碰平台条件编译、工具链 flag、STL 选型，或要让某段代码在另一平台成立 | `references/portability.md` |
| 要跑测试、要宣布"做完了"、要判断某类改动必须验什么 | `references/verification.md` |
| 需要通用 C++20 语言层面的写法参考（concepts / ranges / constexpr / 初始化 / 命名） | `references/cpp-core-guidelines.md` |

一次改动常常横跨三四个文件——**按需读，不要一次性全读**。但上面任意一条命中时，「先读再动手」不是可选项：这个仓库的坑几乎都藏在「接口签名看着对、语义已经错」的地方。

## 二、五条不可违反

### 1. Architecture First：不要绕过既有体系

在这个仓库里最容易犯的错，不是写错某个 C++20 API，而是**为了把一个小功能做完，绕过原有的生命周期 / ABI / 依赖账本体系**。这类代码往往编得过、测试甚至能跑绿，但架构已经坏了——而坏掉的那部分（干净关停、热插拔、跨平台 ABI）恰恰是这个库存在的理由。

| 常见念头 | 为什么不行 |
|---|---|
| `plugin->SomeService = ...`，顺手存下服务指针 | 绕过账本 → 该插件从此不可 Eject（§1.3 / §5.6） |
| 自己开一张注册表 / 全局 map 存服务 | 平行注册通道 → 不在任何 EffectScope 名下 → 关停不归零（§7.1） |
| `static std::vector<Plugin*> gAll;` 缓存插件实例 | 隐藏全局状态 → 实例级对象被进程级对象持有，破铁律（§1.2） |
| 跨 DLL 用 `dynamic_cast` / `typeid` 判类型 | RTTI 跨模块比较不可靠；`RTLD_LOCAL` 下更会**静默**失效（§8.3 / §8.7） |
| 给 Loader 加一个"跳过身份校验"的开关 | 绕过档三 ABI 验证 → 热插拔假成功（§8.2） |
| `throw` / `try` / `catch` | 全项目关闭异常，错误一律 `Result<T>`（§0.3-7） |
| 在插件**构造函数**里起线程 / 注册副作用 | 回收序反了，线程会活过服务注销（§5.4） |
| 为省事让插件 A 直接链接插件 B | 加载器会拉一条账本看不见的边，还把引用计数焊死 → Eject 假成功（§8.7） |

判据很短：**你新增的这条路径，账本看得见吗？会被 EffectScope 回收吗？关停后计数能归零吗？** 三个都答不上来，就是绕过了。

### 2. 铁律：实例级对象不得被进程级对象持有

反向（实例级引用进程级）允许。这是"每一次运行都如同首次"的全部依据。

`PluginHost` 持有的三个进程级容器**在类型上就装不下实例级对象**，这不是靠插入点断言拦住的：

- `Loader::Binaries` 只存 `BinaryRecord{ path, void* }`；
- `DependencyLedger::Edges` 只存非拥有 cookie 与借用字符串；
- `ScopePool` 只存空闲内存。

**新增进程级容器时，先问它的元素类型能不能装下一个 Pod 内的对象。** 能装下就是设计缺陷，不要靠运行期检查补救。

### 3. 单线程契约

一个进程只有一个 `PluginHost`（§1.4），它绑定创建它的那个线程；此后一切 Vase API 必须来自该线程。Debug 下断言线程 id，非绑定线程调用直接终止。

由此**免费获得**的是：诊断计数不需要原子、不需要锁（`DiagnosticCounters` 就故意不是原子的）；Adopt / Eject 不存在"撞上半程装配"的交叠态，状态机不需要并发守卫。**谁日后为"支持多线程装配"拆掉这条契约，谁就得连计数、账本、EffectScope 一起重做。**

### 4. 无异常

`VaseBuildOptions` 全局带 `/EHs-c-` / `-fno-exceptions`（flag 全清单见根 `CLAUDE.md` 规矩 1）。连带约束不是"风格建议"，是硬事实：

- 可恢复失败 → `Result<T>` / `Error`；编程错误 → `detail::ProgrammerError()`（两个构建都终止）。
- **不要用裸 `assert` 表达运行期错误**：它会让 Debug 终止、Release 静默走过，两个构建行为分叉。
- `_HAS_EXCEPTIONS=0` 下 MSVC STL 的前置条件失败从"抛异常"变成"进程终止"——`vector::at` 越界、`std::stoi` 解析失败、`<filesystem>` 的 error overload 全部直接 abort，**没有可接住的东西**。
- 测试里**禁用 `EXPECT_THROW` 一族**（含 `ASSERT_THROW` / `EXPECT_ANY_THROW` / `EXPECT_NO_THROW`）：gtest 宏体是无条件展开的裸 `try/catch`，本项目全 TU 编译期硬失败。测"必须失败"只能用 `Result<T>` 的返回值。

上面四条里，后两条的**机制根因与完整边界**在根 `CLAUDE.md` 规矩 2 与规矩 4（含 `/external:W0` 管不着 C4530 那条）；本节只留"写代码时该怎么办"，细节见 `references/error-handling.md`。

### 5. Effect 是唯一的注册通道

服务注册、事件订阅、以及一切需要"随插件一起干净撤销"的动作，都经 `Context::Provide` / `Context::On` 落到某个 `EffectScope` 上，由 Scope 逆序回收。**这是关停不需要 `OnStop` 钩子的全部原因**：你在 `OnStart` 里注册的 Effect 比 `OnLoad` 里的回收更早，于是"先停线程、再注销服务"自动成立。

推论：**构造函数只做初始化，不注册副作用、不启动活动**——那时还没有 `Context`，而且会破坏上面那条时序。

## 三、干活流程

1. **先定位再动手。** 这个仓库有 `.codegraph/` 索引，查代码优先用 `codegraph_explore`，而不是 grep + 逐个读文件。
2. **读接口的注释，不只是签名。** Vase 的头文件注释里写的是**契约与理由**（为什么 `~Pod()` 定义在 .cpp、为什么 `Result::Value()` 失败即终止、为什么 `MetaArray` 不用 `std::span`）。签名看不出这些。
3. **改之前先问"这属于哪一层"。** `Include/Vase/` 按 `Detail / Effect / Event / Host / Pod / Service` 分目录，目录就是分层：`VaseHost` 链 `VasePod`，插件**只**链 `VasePod`（D11，链接期事实）。类型归属跟着**最底层的消费者**走——`PodReport` 住在 `Pod.h` 而不是 `PluginHost.h` 就是这个道理。
4. **写测试，并且写在该在的分层里**：`Unit / Lifecycle / Integration / HotSwap / Abi`，各自回答不同的问题。跨 DLL 的事在 `Unit` 里测不出来。
5. **改完按第五节验，验完才说"完成"。**
6. **引入了本文件 / 根 `CLAUDE.md` 没记的规矩或命令，同步更新文档。**

## 四、提交前自查清单

架构：

- [ ] 新增的注册都走了 `Context` / `EffectScope`，没有平行通道
- [ ] 没有新增进程级容器去存实例级对象（铁律）
- [ ] **宿主侧**没有跨边界持有服务指针、没有缓存服务引用（这条只管宿主：宿主解析不落账本边、无机制兜底，见 `ownership-lifetime.md` §2。插件侧的解析会落一条永久边，由 Eject 反查兜住——别把两边混成一条普遍禁令）
- [ ] 插件侧没有 `throw` / `try` / `catch`；失败走 `Result<T>`
- [ ] 运行期错误没有用裸 `assert` 表达

C++ 与边界：

- [ ] Vase 自己的头一律**引号**包含（`#include "Vase/Plugin.h"`，不是尖括号）——尖括号会被 `/external:anglebrackets` 标记为外部头，警告静默绕过 `/W4 /WX`（机制与那条例外见根 `CLAUDE.md` 规矩 3）
- [ ] **第三方 fork（`ThirdParty/cli`）的头同样引号包含**（`#include "cli/cli.h"`，不是尖括号）——同一个 `/external:anglebrackets` 的反方向后果：尖括号包含会让"fork 不得带回 `throw`"这条不变式**静默**失效（现场见 `Cmake/VaseThirdParty.cmake`；规矩见根 `CLAUDE.md` 规矩 3，判据见 `references/architecture.md` §2）
- [ ] 新 target 链了 `VaseBuildOptions`；新插件 target 经 `vase_add_plugin_fixture`
- [ ] 跨 DLL 面没有 `type_index` / `dynamic_cast` / `typeid`
- [ ] 改了任何跨 DLL 可见的布局或签名 → 递增 `kHeaderVersion` 并说明理由
- [ ] 没有在插件构造函数里注册副作用或起线程

格式与门禁（**命令本体与 `.clang-format` 的完整偏离项都在根 `CLAUDE.md`**，下面只列踩坑面）：

- [ ] `clang-format --dry-run --Werror` 全绿（最容易写错的三项：`Allman`、`PointerAlignment: Left`、构造初始化表每式一行且逗号在行首）
- [ ] 三条 debug 线各自跑过 clang-tidy，**正文**零 `error:` 零 `warning:`（退出码 0 单独不够）
- [ ] 新文件已 `git add`（format 门禁靠 `git ls-files` 枚举，未暂存的新文件会被**静默跳过**、门禁照样绿）

## 五、改什么必须验证什么

这个仓库的门禁有**两个静默陷阱**，只用退出码判会假绿：`run-clang-tidy` 的 `WarningsAsErrors` 为空（退出 0 不代表没 warning）；`ctest` 在一个测试都没发现时同样返回 0。所以判据永远是"**退出码 + 正文/基数一起读**"。

| 改动类型 | 必须做的验证 |
|---|---|
| 任何提交 | 六 preset 全绿；`ctest --preset <p> -N` 的 `Total Tests` 逐位对上根 `CLAUDE.md` 那张按线分账的基数表；差值的成因要能说出来（`references/verification.md` 第 2 节） |
| **Loader、依赖账本、Eject / Adopt 路径、描述符布局、`HeaderVersion`** | 另加 `Tests/HotSwap/` **全部**用例，Windows 与 Linux 各自留证据。选择子见根 `CLAUDE.md` 规矩 6——**写成只匹配主循环那一类，会把守 Eject / Adopt 边界的用例整套漏掉** |
| 两条承重的工具链 flag（清单见根 `CLAUDE.md`「工具链 flag 是承重的」） | 同上（它们是档三身份特征的**构建要求**，摘掉的症状是运行期 Adopt 全线拒绝，不是编译失败） |
| 跨平台行为 | 双平台各自跑。**"Windows 两条线绿"推不出"Linux 绿"**，且成因有两种：STL 不同（同一份共享文件也不报），以及平台专属 TU（`*Posix.cpp` / `*Windows.cpp`）在另一条线上**根本不编译**、天然只有一条线看得见 |
| 新增 / 改动构建树 | 删树重配，不要用 `-D` 钉手工 override（`CMAKE_*_FLAGS_INIT` 只在首次 configure 进缓存，会把后续工具链改动一起遮住） |

命令块、preset 名、脚本形态与基数表都在根 `CLAUDE.md`；`references/verification.md` 只补判据与三条它没有的坑。

## 六、报告与收尾

- 说"测试过了"时，把 `ctest` 的正文与 `ctest -N` 的基数一起给出，不要只给一句"全绿"。
- 跳过某步验证要在汇报里**明说是哪步、为什么**——静默跳过是本项目最贵的失败形态。
- 交流与文档中文优先；提交信息只留标题 + 中文说明体，**不加任何 AI 工具或模型的署名尾注**。
