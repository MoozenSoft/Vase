# 组 3 数据不搬运 · 组 4 内存不白拿

`cpp20-zero-overhead` 的第 3、4 组细则。元规则与例外登记见 `../SKILL.md`。

## 组 3 数据不搬运

### 3.1 传参按意图分类
入参：廉价可拷贝类型按值，其余按 `const&`（CGL F.16）。in-out 用 `T&`（F.17）。will-move-from 用 `T&&` 并 `std::move`（F.18）。转发用 `TP&&` 且**只** `std::forward`（F.19）。
「廉价」按目标 ABI 定，不按感觉：SysV x64 两个整数寄存器、Win64 一个；经验线是 **≤2 个机器字且平凡可拷贝**。
- **执法**：`performance-unnecessary-value-param`、`readability-non-const-parameter`、`misc-unused-parameters`；`cppcoreguidelines-rvalue-reference-param-not-moved` 抓「收了 `T&&` 却拷走」。
- **注意**：跨动态库边界时，按值传平凡类型常常反而最优（省一次间接解引用）。这是一笔要登记的账，不是疏漏。

### 3.2 别 `return std::move(local)`
它阻断 NRVO / 隐式移动，把一次拷贝省略变成一次真移动甚至多余一跳（CGL F.48）。直接 `return local;`。
- **执法**：**编译器警告** `-Wpessimizing-move`（clang 直接报，可进 `-Werror`）；`performance-move-const-arg` 抓 move 一个 const 对象。

### 3.3 移动操作必须 `noexcept`
`std::vector` 扩容只在元素移动构造 `noexcept` 时才敢移动，否则为了强异常保证**退化为逐个拷贝**。这是全条约里单笔最贵、又最容易被写错的一条。
- **执法**：`performance-noexcept-move-constructor`、`cppcoreguidelines-noexcept-move-operations`、`modernize-use-noexcept`；再加一句 `static_assert(std::is_nothrow_move_constructible_v<T>)`。
- **例外**：移动确实可能抛（少见）——登记，并写清容器选型怎么改。

### 3.4 只读序列走视图
文本用 `std::string_view`，字节/元素序列用 `std::span`：省一次构造与一次分配。**视图不延长生命周期**——别返回指向局部的视图，别把视图存进比所指对象更活的成员。
- **执法**：`bugprone-dangling-handle`、`modernize-use-string-view`、`performance-string-view-conversions`。
- **依据**：CGL SL.str.\*、F.44（不想拷就返回 `T&`，且「没有对象」不是合法结果时）。

### 3.5 循环里的隐式拷贝
`for (auto x : c)` 对非廉价拷贝类型＝每元素一次构造析构。写 `const auto&`（要改就 `auto&`）。
- **执法**：`performance-for-range-copy`、`performance-implicit-conversion-in-loop`、`modernize-loop-convert`。

### 3.6 原地构造与预留
有参数就直接 `emplace_back`，别先造临时对象再移动。规模已知就先 `reserve`：不预留的增长序列要付 ⌈log₂N⌉ 次重分配加 N 次元素搬运。
- **执法**：`modernize-use-emplace`、`performance-inefficient-vector-operation`。
- **边界**：`reserve` 是**提前**付费，不减少总量；小容器上它可能只是多占一次内存。

### 3.7 字符串与流的固定几笔
- 链式 `operator+` 每步一次分配 → `performance-inefficient-string-concatenation`。
- `find("x")` 比 `find('x')` 多一次长度扫描 → `performance-faster-string-find`。
- `std::endl` 是一次 flush，真 I/O 成本 → `performance-avoid-endl`。
- `float` 实参调 `std::sin` 等提升为 `double` → `performance-type-promotion-in-math-fn`。
- 在无序容器上 `std::find` 再 `erase` 是线性查找顶替常数 → `performance-inefficient-algorithm`。

### 3.8 别把形态规则当成本规则
`size() == 0` 改 `empty()`、`auto` 与 `const` 之类，在标准容器上是常数时间——它们属可读性，已由 linter 覆盖，**不进本条约的账**。分不开这两类，登记表会被无成本差异的条目占满。

## 组 4 内存不白拿

### 4.1 分配次数是最贵的单一来源（CGL Per.14，正文空壳 → 写实）
每次分配的账不止是拿到内存：**可能的锁竞争 + 页首次触碰 + 此后每次访问的缓存不友好 + 释放时的二次遍历**。三种手法按收益排序：
1. **合并**：N 个对象一次分配（数组、池、单 arena）。摊掉 N−1 次记账。
2. **预留**：已知规模一次到位（见 3.6）。
3. **复用**：作用域结束时整体回收，而不是逐个 free——回收也是账。
- **执法**：阶梯 2 的分配计数；`bugprone-multiple-new-in-one-expression`、`clang-analyzer-cplusplus.NewDeleteLeaks` 管配平不管次数。

### 4.2 临界路径不分配（CGL Per.15，正文空壳 → 写实）
先给「热路径」一个**可测定义**，否则这条无法执行也无法申诉。本条约采用：*每帧、每对外事件、或每次公共入口调用都会走到的分支*；登记时把阈值写成数字（一帧、一次对外调用、每千次事件皆可）。
- **做法**：热路径上的对象在**进入之前**分配好（池、arena、构造期预留），路径内只搬运所有权，不制造生命周期。
- **执法**：阶梯 2 —— 优先显式 RAII 计数器，`operator new` 替换当兜底（它的跨镜像漏计见 `../SKILL.md` 文末）；断言该路径 **0 次分配**。
- **例外**：首次进入的建池、失败路径的诊断字符串——登记，写明它为什么不在稳态路径上。

### 4.3 存储期与所有权的成本阶梯
栈对象（0 成本）＜ `unique_ptr`（一次分配，控制开销为零）＜ `shared_ptr`（控制块 + 原子引用计数；不用 `make_shared` 时是**两次**分配）。
默认停在这一阶梯尽量靠上；`shared_ptr` 只在所有权确实共享时用（CGL R.21 / R.22 / R.5）。
- **执法**：`modernize-make-unique`、`modernize-make-shared`、`cppcoreguidelines-owning-memory`、`R.30`（别拿智能指针当参数表达非生命周期语义）。

### 4.4 让析构可平凡跳过
容器里的元素类型若可平凡析构，`vector` 的销毁就退化为一次释放，而不是逐元素调用析构。给只持有 POD 或裸资源的类型加 trivially-destructible 设计（`std::is_trivially_destructible_v`）。
- **执法**：`performance-trivially-destructible`。
- **另一笔**：`performance-enum-size` —— `enum class` 默认底层类型 `int`，进结构体就是 4 字节；能窄就窄，这笔账最终落在组 5 的布局上。
