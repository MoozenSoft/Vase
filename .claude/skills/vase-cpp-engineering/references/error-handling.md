# 错误处理

**全项目以关闭异常的方式编译。** 这不是风格偏好，是硬事实：`VaseBuildOptions` 在 Windows 侧带 `/EHs-c-`、Linux 侧带 `-fno-exceptions`，且 MSVC 侧显式定义 `_HAS_EXCEPTIONS=0`。所以：

- **不写 `throw` / `try` / `catch`**；
- **不写任何指望"抛出后有人接住"的代码**；
- 错误一律经 `Result<T>` / `Error` 显式返回。

---

## 1. 三种失败，三个出口

| 失败种类 | 出口 | 语义 |
|---|---|---|
| **可恢复失败** | `Result<T>` / `Result<void>` 的 `Err(Error)` | 调用方应该检查、可以处理、可以继续跑 |
| **编程错误** | `detail::ProgrammerError(msg)` | 调用方违反了接口契约（空指针、重复 Provide、在错误结果上取 `Value()`）。**Debug 与 Release 都终止** |
| ~~运行期错误用裸 `assert`~~ | —— | **不要**。它让 Debug 终止、Release 静默走过，两个构建行为分叉 |

`ProgrammerError` 是"一死到底"的出口：无异常编译下没有可接住的东西，这是唯一正确形态。它的形态是**具名函数 + `std::source_location` 默认参数**，不是宏——C++20 的 `source_location` 补上了 `__FILE__` / `__LINE__` 这点宏唯一不可替代的能力。实现里不分配、不抛：行号用 `std::to_chars` 就地写成十进制，再 `fwrite` 到 stderr。

**判据：这个失败是"外部世界不配合"还是"代码自己错了"？** 前者走 `Result`，后者走 `ProgrammerError`。

---

## 2. `Result<T>` 的形状与用法

```cpp
template <typename T> class [[nodiscard]] Result {
    static Result Ok(T value);
    static Result Err(Error error);
    [[nodiscard]] bool IsOk() const;
    [[nodiscard]] T& Value();          // 在错误结果上调用 → ProgrammerError
    [[nodiscard]] const Error& GetError() const;  // 在成功结果上调用 → ProgrammerError
};
// Result<void> 是特化：只有 Ok() / Err(error) / IsOk() / GetError()，没有 Value()
```

三条注意：

1. **`[[nodiscard]]` 在类上**，所以 `CreatePod(...)` 的返回值不能丢掉不管——这是刻意的，丢弃一个 `Result` 等于丢弃一次失败。
2. **`Value()` / `GetError()` 用错方向是编程错误，不是未定义行为**。它们当场 `ProgrammerError`，而不是返回垃圾。
3. **`Error` 全拥有（`std::string`）**。它是返回值，必须活得比调用久，且**可能比插件镜像活得久**——`DestroyPod` 之后宿主才读报告的路径就踩这条。**不要把它改成 `string_view`。**

`Error` 带 `ErrorContext{ PluginId, ServiceName, ServiceVersion, Stage }`，`Stage` 是 `Phase`：

```cpp
enum class Phase : std::uint8_t { kLoad, kStart, kAdopt, kEject, kUnload };
```

**写错误时把上下文填满。** 一个只有 `"failed"` 的 `Error` 在 `PodReport::Failures` 里等于没写。

---

## 3. 错误消息里的判据子串是契约

`EjectPlugin` / `AdoptPlugin` 是 `Result<T>`：**失败 = 拒绝（`Err`），报告只在成功时存在**。拒绝的点名信息在 `Error::Message()` 里，形如：

```
eject refused: <id> is provided by [c1, c2]
```

**这些子串是被测试匹配的契约**，不是随便写的话术。改动它们等于改接口。同类还有 `PluginHost::AssertBoundThread` 的消息——**必须含子串 `not the bound thread`**，T8 的 death test 按它匹配。

新增拒绝路径时：写清"谁拒绝的、为什么、受影响的都有谁"，并同步更新匹配它的测试。

---

## 4. 边界上的错误

- **异常不跨边界**——没有异常可跨。Vase 自己的插件**返回** `Result`。
- 由**外部工具链**构建、开着异常的插件若真的 `throw`，异常会穿过没有异常支持的栈帧，是**未定义行为**，且宿主在加载期无法检测。这条属于契约束，只能靠文档约束插件作者（§13.3）。
- 相邻的一条残余风险：Linux 侧 libc++ 自身是**带异常**编译的，其内部抛错路径若被触达，同样会穿过我们关闭异常的栈帧。正向路径已实测正常，错误路径行为待实测。

---

## 5. 标准库的连带约束（最容易踩的一类）

`_HAS_EXCEPTIONS=0` 把 MSVC STL 的**前置条件失败从"抛异常"变成"进程终止"**。以下 API 在本项目里没有可恢复的失败路径，一旦失败就是 abort：

- `vector::at` / `map::at` 越界（`std::next` 有边界检查的说法是错的——它同样不做检查，只是**不触发 tidy 的 `-avoid-unchecked-container-access`**）
- `std::stoi` / `std::stol` 解析失败
- `<filesystem>` 的 **error overload**（未传 `std::error_code&` 的那些）

**所以错误处理必须走 `Result<T>` / `Error`，不能指望 STL 的前置条件检查给出可恢复的失败。** 需要解析数字就用 `std::from_chars` / `std::to_chars`（不抛）；需要文件系统操作就用带 `std::error_code&` 的重载并检查它。

`/external:*` 那套管不着这类问题——`/external:W0` 只压警告，压不住 C4530 一类的错误诊断。理由与边界写在根 `CMakeLists.txt` 的注释里。

---

## 6. 测试里怎么测"必须失败"

**禁用 `EXPECT_THROW` 一族**（`ASSERT_THROW` / `EXPECT_ANY_THROW` / `EXPECT_NO_THROW`）。

**为什么是硬失败而不是风格问题：机制推导、两侧 `GTEST_HAS_EXCEPTIONS` 不一致的根因、以及两条工具链的具体报错文本，都写在根 `CLAUDE.md` 规矩 2**（那里是唯一真值来源，本文件不重抄）。要记住的只有一件：它**不是静默退化，是编译期就编不过**，所以没有"先这么写再改"的余地。

**正确写法**：断言 `Result<T>` 的显式返回值。

```cpp
const auto result = host.EjectPlugin(handle, "Vase.Test.Provider");
ASSERT_FALSE(result.IsOk());
EXPECT_NE(result.GetError().Message().find("is provided by"), std::string::npos);
```

测"必须终止"的场景用 **death test**（`ASSERT_DEATH` / `EXPECT_DEATH`），比如 `ProgrammerError` 与线程断言那两条。注意它受 `#ifndef NDEBUG` 门保护，所以 release 线的测试基数比 debug 少 1 条——这是设计，不是漏注册。

---

## 7. 出处

- 无异常决策与全部连带约束：架构文档 §0.3 原则 7、§13.2
- `Result<T>` 为什么手写而不引第三方（不把 ABI 面交给别人）：spec 第 7 节
- `Error` 为什么全拥有：spec 3.3(3)
- `ProgrammerError` 的形态与理由：`Include/Vase/Detail/Fail.h` 的头注释
- 构建侧的全部 flag：根 `CMakeLists.txt` 的 `VaseBuildOptions`
