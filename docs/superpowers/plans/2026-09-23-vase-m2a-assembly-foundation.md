# Vase M2a 装配地基 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 按 spec（D18–D43）落地 M2 第一波：配置类型系统（POD 面 + 拥有面）与描述符 v2、`LoadPlan` 定形、结构性校验 + 装配预检 + 加载级联 + `OnStart` 递归拆除、3b 薄面（Eject/Adopt 报告结构化）——全程不碰 JSON、不加新依赖、不新增库 target。

**Architecture:** 三层不变（`Catalog/` 本波不立）。配置分 POD 面（`Include/Vase/Config/`，header-only，插件镜像与描述符可见）与拥有面（`ConfigBlob`，住 VaseHost）。装配期检查器（Id 唯一 / Provides 碰撞 / Requires 可绑）是 Host 层纯函数，M2b 的 `Solve` 同源复用。执法性拒绝走 `Ok + Status` 结构化报告，环境/身份类拒绝照旧 `Err` + 子串契约（D21）。

**Tech Stack:** C++20（关异常）、CMake + 六 preset、GoogleTest 1.18.0（vcpkg manifest）、`Result<T>`/`Error`、clang-tidy/format LLVM 23.1.0。

**Spec:** [`docs/superpowers/specs/2026-09-23-vase-m2a-assembly-foundation-design.md`](../specs/2026-09-23-vase-m2a-assembly-foundation-design.md)——本计划逐条对它的 §3–§12 落地；冲突处以 spec 为准并停下问需求方。

**实施前提（spec 已按现实改过两态形）**：M2a 无清单、声明住二进制里——预检与 Provides 碰撞检查都排在 `InspectBinary` 之后（D27/D33）；「Err 的局零副作用」只对 Id 唯一检查成立。

## Global Constraints

每个任务的隐含要求，值从 spec / CLAUDE.md 原样抄：

- **无异常**：不用 `throw`/`try`/`catch`；可恢复失败 `Result<T>`/`Error`，编程错误 `detail::ProgrammerError(std::string_view)`（两构建 abort）。`std::variant` 只用 `std::get_if`；`std::optional` 用 `*opt`/`has_value`，禁 `.value()`；禁 `std::stoi`（用 `from_chars`）、禁 `.at()`。
- **测试禁用 `EXPECT_THROW` 一族**。测终止用 `EXPECT_DEATH`（只对两构建都 abort 的 `ProgrammerError` 路径）。
- **包含**：Vase 自己的头一律引号 `#include "Vase/..."`（规矩 3）。
- **target**：新测试 fixture 插件一律 `vase_add_plugin_fixture(...)`（规矩 5）；`ConfigBlob.cpp` 进 `VaseHost`（已链 `VaseBuildOptions`）。
- **命名**（tidy 强制）：类型/函数/成员 `CamelCase`；参数/局部 `camelBack`；常量与枚举值 `k`+`CamelCase`；成员无前后缀；成员与访问器撞名时成员让位。
- **格式**：Allman、`PointerAlignment: Left`、构造初始化表每式一行逗号在行首、`BreakTemplateDeclarations: Yes`（`template <...>` 头与签名永远两行）、宏参数内花括号单行不折（`VASE_PLUGIN`/`VASE_CONFIG` 的作者侧写法）。
- **注释**：中文、单条 ≤2 行为宜硬上限 3 行；写「为什么」与指针，不写推演流水账。
- **提交信息**：标题 + 中文正文，**不加任何 AI 署名尾注**。
- **每任务收尾三件套**：新/改文件 `git add` 后跑 `git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' | xargs -0 clang-format --dry-run --Werror`；构建主走 `cmake --build --preset win-x64-clang-debug`；测试 `ctest --preset win-x64-clang-debug`（全量，不只新用例）。**六线全矩阵、三条 tidy 线、Linux 证据、基数重测统一在 Task 11 收口**——中间任务不跑（tidy 全量单轮约 20 分钟/线）。
- 改账本/Eject/Adopt/描述符/HeaderVersion 的任何提交，其回归判据（`-R 'HotSwap|Eject|Adopt'` 全选择子、双平台）在 Task 11 兑现；任务内先保证 win-debug 线全绿。

## File Structure

**新增**（职责一句话）：

| 文件 | 职责 |
|---|---|
| `Include/Vase/Config/Value.h` | `ValueKind`/`Value`（Kind+64bit 位形的 POD）/`KindOf<T>` |
| `Include/Vase/Config/FieldInfo.h` | `Meta`/`FieldInfo`（对外契约）/`ApplyTo` |
| `Include/Vase/Config/ConfigInfo.h` | `ConfigInfo`/`HasVaseConfigFields`/`FieldsOf<T>()` |
| `Include/Vase/Config/ConfigMacros.h` | `VASE_CONFIG` 与 FOR_EACH 展开机制 |
| `Include/Vase/Host/ConfigBlob.h` + `Source/Host/ConfigBlob.cpp` | 拥有面：键拥有字符串，浅合并唯一实现点 |
| `Tests/Unit/ConfigValueTests.cpp` / `ConfigMacroTests.cpp` / `ConfigBlobTests.cpp` | T1/T2 的判据 |
| `Tests/Integration/fixtures/M2Common.h` | M2a 新 fixture 群的测试服务标识（住 fixture 层） |
| `Tests/Integration/fixtures/{DeadProvider,DeadConsumer,CycleA,CycleB}Plugin/` | T5：归因链与环退化 |
| `Tests/Integration/fixtures/{ConfigConsumer,ConfigLayoutMisuse}Plugin/` | T6：配置应用 |
| `Tests/Integration/fixtures/{StartFailProvider,BehindStrictUsed,BehindStrictUnused,BehindOptUsed,BehindOptUnused,BehindFar}Plugin/` | T7：递归拆除四形态 + transitive |
| `Tests/Integration/fixtures/{CollisionProvider,SharedConsumer2}Plugin/` | T8/T9：碰撞与多消费者点名 |
| `Tests/Integration/MultiPluginAssemblyTests.cpp` / `ConfigApplyTests.cpp` / `RecursiveTeardownTests.cpp` | T4/T5、T6、T7 的集成判据 |
| `Tests/Lifecycle/MultiPluginCycleTests.cpp` | T7：多插件反复归零 |

**修改**：`Include/Vase/Plugin.h`（转出 Config）、`Include/Vase/PluginDescriptor.h`（两字段 + bump）、`Include/Vase/Pod/Context.h`/`Source/Pod/Context.cpp`（`Config<T>`、凭声明扩展）、`Include/Vase/Host/LoadPlan.h`（定形）、`Include/Vase/Pod/Pod.h`/`Source/Pod/Pod.cpp`（Skips）、`Include/Vase/Host/Evidence.h`（结构化字段）、`Include/Vase/Host/PluginHost.h`/`Source/Host/PluginHost.cpp`（装配改造主战场）、`Source/Host/CMakeLists.txt`、`Tests/CMakeLists.txt`、`Tests/Integration/fixtures/CMakeLists.txt`、`Tests/Integration/FailureSemanticsTests.cpp`（death→skip 改写）、`Tests/Unit/DescriptorTests.cpp`、`Tests/HotSwap/EjectTests.cpp`（①a 迁移）、`Samples/HelloPlugin`/`Samples/HelloPluginPrime`（配置示例形态）、`Tools/VaseConsole/Console.cpp`（Status 诚实化）、`CLAUDE.md`/`wiki/vase-architecture.md`/技能（T11 文档波）。

**不动**：三工具链文件、两承重 flag、`Cmake/VaseThirdParty.cmake`、`vcpkg.json`、`ThirdParty/cli`、`Tests/HotSwap/HotSwapLoopTests.cpp`（回归即可）。

---

## Task 1: 配置 POD 面（`Include/Vase/Config/`）+ 宏探针

**Spec:** §3.2（D22、D24、D25 的类型侧）。本任务立起作者与描述符共见的类型，不接描述符、不接装配。

**Files:**
- Create: `Include/Vase/Config/Value.h`、`Include/Vase/Config/FieldInfo.h`、`Include/Vase/Config/ConfigInfo.h`、`Include/Vase/Config/ConfigMacros.h`
- Modify: `Include/Vase/Plugin.h:15-26`（exports 块）
- Create: `Tests/Unit/ConfigValueTests.cpp`、`Tests/Unit/ConfigMacroTests.cpp`
- Modify: `Tests/CMakeLists.txt`（源列表 + 两行）

**Interfaces:**
- Consumes: 无（只依赖 `<bit>`/`<array>`/`<memory>` 与 `Vase/Detail/Fail.h`、`Export.h`）。
- Produces（T2/T3/T6 按这些名字用）：`vase::ValueKind`、`vase::Value`（`From<T>`/`GetAs<T>`/`operator==`）、`vase::KindOf<T>()`、`vase::Meta`（`MetaLabel/MetaMin/MetaMax`）、`vase::FieldInfo`、`vase::ApplyTo<&T::Field>`、`vase::ConfigInfo`、`vase::FieldsOf<T>()`、宏 `VASE_CONFIG(Type, (TY, NAME, DEFAULT, vase::Meta{...}), ...)`。

- [ ] **Step 1: 写 `Include/Vase/Config/Value.h`**

```cpp
#pragma once

// 配置值的 POD 面（spec 3.2 / D24）：Kind 标签 + 64 位位形。没有 union——门禁的
// cppcoreguidelines-pro-type-union-access 对 union 裸读必报，位形 + bit_cast 是它的代码级出路。
// kString 存的是 const char* 的位形：字面量或宿主 blob 存储的**借用**（窗口见 ConfigBlob.h）。
// enum 不做（D22）：它随清单 schema 在 M2b 定案，届时布局要变就再 bump。

#include <bit>
#include <cstdint>
#include <type_traits>

namespace vase
{

enum class ValueKind : std::uint8_t { kNone, kBool, kInt32, kInt64, kFloat, kDouble, kString };

template <typename T>
struct AlwaysInvalidConfigType : std::false_type
{
};

template <typename T>
consteval ValueKind KindOf()
{
    if constexpr (std::is_same_v<T, bool>)
    {
        return ValueKind::kBool;
    }
    else if constexpr (std::is_same_v<T, std::int32_t>)
    {
        return ValueKind::kInt32;
    }
    else if constexpr (std::is_same_v<T, std::int64_t>)
    {
        return ValueKind::kInt64;
    }
    else if constexpr (std::is_same_v<T, float>)
    {
        return ValueKind::kFloat;
    }
    else if constexpr (std::is_same_v<T, double>)
    {
        return ValueKind::kDouble;
    }
    else if constexpr (std::is_same_v<T, const char*>)
    {
        return ValueKind::kString;
    }
    else
    {
        static_assert(AlwaysInvalidConfigType<T>::value,
                      "config type must be bool/int32_t/int64_t/float/double/const char* (enum deferred to M2b, D22)");
        return ValueKind::kNone;
    }
}

struct Value
{
    ValueKind Kind = ValueKind::kNone;
    std::uint64_t Bits = 0; // 4 字节类型住在低位

    template <typename T>
    [[nodiscard]] static constexpr Value From(T value)
    {
        Value out{};
        out.Kind = KindOf<T>();
        if constexpr (std::is_same_v<T, bool>)
        {
            out.Bits = value ? 1U : 0U;
        }
        else if constexpr (std::is_same_v<T, std::int32_t>)
        {
            out.Bits = static_cast<std::uint64_t>(static_cast<std::uint32_t>(value));
        }
        else if constexpr (std::is_same_v<T, std::int64_t>)
        {
            out.Bits = static_cast<std::uint64_t>(value);
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            out.Bits = std::bit_cast<std::uint32_t>(value);
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            out.Bits = std::bit_cast<std::uint64_t>(value);
        }
        else
        {
            out.Bits = std::bit_cast<std::uint64_t>(value); // const char*
        }
        return out;
    }

    // 无 Kind 检查的解码：调用方持有类型（宏生成的 Apply / blob 装换都按 Kind 分派过），
    // 读错 Kind 的防线在 ApplyToImpl 那唯一一处运行时检查里。
    template <typename T>
    [[nodiscard]] constexpr T GetAs() const
    {
        if constexpr (std::is_same_v<T, bool>)
        {
            return Bits != 0U;
        }
        else if constexpr (std::is_same_v<T, std::int32_t>)
        {
            return static_cast<std::int32_t>(static_cast<std::uint32_t>(Bits));
        }
        else if constexpr (std::is_same_v<T, std::int64_t>)
        {
            return static_cast<std::int64_t>(Bits);
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            return std::bit_cast<float>(static_cast<std::uint32_t>(Bits));
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            return std::bit_cast<double>(Bits);
        }
        else
        {
            return std::bit_cast<const char*>(Bits);
        }
    }

    constexpr bool operator==(const Value&) const = default; // 平凡两成员，POD 相等 = 逐成员相等
};

static_assert(std::is_trivially_copyable_v<Value>);
static_assert(std::is_trivially_destructible_v<Value>);

} // namespace vase
```

- [ ] **Step 2: 写 `Include/Vase/Config/FieldInfo.h`**

```cpp
#pragma once

// FieldInfo 是**对外契约、布局即 ABI**（spec 3.2「把 FieldInfo 当接口对待」）：
// 任何布局改动 → kHeaderVersion 递增（D26）。Min/Max 无执法（D35）：纯展示元信息，
// 越界判定归 M2b 的 Preset 校验。

#include "Vase/Config/Value.h"
#include "Vase/Detail/Fail.h"

#include <bit>
#include <cstdint>
#include <type_traits>

namespace vase
{

// 作者书写面。设计成**可整体传值的 constexpr 聚合**（D35 执法不进库内，所以只这三个成员）。
struct Meta
{
    const char* Label = "";
    Value Min{}; // kNone = 未设
    Value Max{};
};

// constexpr 身份 helper：展开式里不能对括号内 designated-init 直接取成员，走函数调用形。
constexpr const char* MetaLabel(Meta m) { return m.Label; }
constexpr Value MetaMin(Meta m) { return m.Min; }
constexpr Value MetaMax(Meta m) { return m.Max; }

struct FieldInfo
{
    const char* Name = "";
    ValueKind Kind = ValueKind::kNone;
    Value Default{};
    Value Min{};
    Value Max{};
    const char* Label = "";
    void (*Apply)(void* ConfigStruct, const Value& V) = nullptr; // = ApplyTo<&T::Field>
};

static_assert(std::is_trivially_copyable_v<FieldInfo>);

template <typename MemberPtr>
struct MemberTraits;

template <typename Config, typename Field>
struct MemberTraits<Field Config::*>
{
    using Owner = Config;
    using Type = Field;
};

template <auto MemberPtr>
void ApplyToImpl(void* configStruct, const Value& v)
{
    using Traits = MemberTraits<decltype(MemberPtr)>;
    // 宏保证「表里 Kind == 成员类型」是构造的（同一次展开）；这里运行时兜底拦的是
    // 宿主灌错类型的 Value——那种正常路径在装配 Apply 点已被 D32 提前拦掉，走到这就是编程错误。
    if (v.Kind != KindOf<typename Traits::Type>())
    {
        detail::ProgrammerError("VASE_CONFIG Apply: Value kind mismatch");
    }
    auto* typed = std::bit_cast<typename Traits::Owner*>(configStruct);
    (typed->*MemberPtr) = v.GetAs<typename Traits::Type>();
}

template <auto MemberPtr>
constexpr auto ApplyTo = &ApplyToImpl<MemberPtr>;

} // namespace vase
```

- [ ] **Step 3: 写 `Include/Vase/Config/ConfigInfo.h`**

```cpp
#pragma once

// ConfigInfo = PluginMeta.Config 槽的形状（spec 3.2/D25/D26）。Create/Destroy 是
// **插件镜像内配对**的工厂指针——与 Plugin Create/Destroy 同一理由：跨 DLL 不过 CRT 边界。

#include "Vase/Config/FieldInfo.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace vase
{

struct ConfigInfo
{
    const FieldInfo* Fields = nullptr;
    std::uint32_t Count = 0;
    std::uint32_t StructSize = 0;
    std::uint32_t StructAlign = 0;
    void* (*CreateConfig)() = nullptr;
    void (*DestroyConfig)(void*) = nullptr;
};

static_assert(std::is_trivially_copyable_v<ConfigInfo>);
// 全零 = 「该插件无配置」：忘写 .Config = FieldsOf<T> 的静默点原样成立（spec 3.4，§13.3 既受）。
static_assert(ConfigInfo{}.Fields == nullptr);

template <typename T>
concept HasVaseConfigFields = requires {
    T::kFields.size();
    T::kFields.data();
    requires std::is_same_v<decltype(T::VaseConfigCreate()), void*>;
    requires std::is_same_v<decltype(T::VaseConfigDestroy(nullptr)), void>;
};

template <typename T>
constexpr ConfigInfo FieldsOf()
{
    static_assert(HasVaseConfigFields<T>, "FieldsOf<T>: T must be generated by VASE_CONFIG");
    return ConfigInfo{
        .Fields = T::kFields.data(),
        .Count = static_cast<std::uint32_t>(T::kFields.size()),
        .StructSize = static_cast<std::uint32_t>(sizeof(T)),
        .StructAlign = static_cast<std::uint32_t>(alignof(T)),
        .CreateConfig = &T::VaseConfigCreate,
        .DestroyConfig = &T::VaseConfigDestroy,
    };
}

} // namespace vase
```

- [ ] **Step 4: 写 `Include/Vase/Config/ConfigMacros.h`（VASE_CONFIG 展开机制）**

机制三件套：括号元组 = 参数保护壳（§3.2 原文），拆元组靠**自右向左参数预扫描**（`DECOMPOSE` 展开后的逗号在实参定界前重分裂）；FOR_EACH 靠 NARGS 计数 + CAT 分发；上限 32 字段（超出即编译错——`FE_0` 不存在）。ConfigInfo 用**指针+计数**而非 `MetaArray`，故无 16 上限（上限是 Requires/Provides 那三组的，不是配置的）。

```cpp
#pragma once

// VASE_CONFIG：字段列表写一次、展开两次（成员 + kFields），一致性是构造出来的（spec 3.2）。
// 作者形态（元组 4 项，类型可含 ::；Meta 的 brace-init 由元组外层括号保护）：
//   VASE_CONFIG(CombatConfig,
//       (float, CriticalMultiplier, 2.0f, vase::Meta{.Label = "暴击倍率",
//                                                    .Min = vase::Value::From<float>(1.0f),
//                                                    .Max = vase::Value::From<float>(10.0f)}),
//       (bool, FriendlyFire, false, vase::Meta{.Label = "友军伤害"}));
// 展开物 = struct Type{ 成员(默认值 = 元组第3项) ; static constexpr kFields ; 配对工厂 }。
// 宏参数内花括号不受 Allman 管辖（与 VASE_PLUGIN 同例外）。至少 1 个字段；无配置就别用本宏。

#include "Vase/Config/ConfigInfo.h"
#include "Vase/Config/FieldInfo.h"
#include "Vase/Config/Value.h"

#include <array>
#include <cstdint>
#include <memory>

// clang-format off  // 机制宏的定义体不参与 Allman 折叠，与 VASE_PLUGIN 头注释同一处置
#define VASE_CONFIG_DETAIL_DECOMPOSE(A, B, C, D) A, B, C, D

#define VASE_CONFIG_DETAIL_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16,               \
                                 _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32,      \
                                 _33, N, ...)                                                                        \
    N
#define VASE_CONFIG_DETAIL_RSEQ_N()                                                                                  \
    33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5,   \
    4, 3, 2, 1
#define VASE_CONFIG_DETAIL_NARG_(...) VASE_CONFIG_DETAIL_ARG_N(__VA_ARGS__)
#define VASE_CONFIG_DETAIL_NARG(...) VASE_CONFIG_DETAIL_NARG_(__VA_ARGS__, VASE_CONFIG_DETAIL_RSEQ_N())

#define VASE_CONFIG_DETAIL_CAT_(A, B) A##B
#define VASE_CONFIG_DETAIL_CAT(A, B) VASE_CONFIG_DETAIL_CAT_(A, B)

#define VASE_CONFIG_DETAIL_FE_1(M, S, T) M(S, T)
#define VASE_CONFIG_DETAIL_FE_2(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_1(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_3(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_2(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_4(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_3(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_5(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_4(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_6(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_5(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_7(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_6(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_8(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_7(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_9(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_8(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_10(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_9(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_11(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_10(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_12(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_11(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_13(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_12(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_14(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_13(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_15(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_14(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_16(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_15(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_17(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_16(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_18(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_17(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_19(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_18(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_20(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_19(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_21(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_20(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_22(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_21(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_23(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_22(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_24(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_23(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_25(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_24(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_26(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_25(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_27(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_26(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_28(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_27(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_29(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_28(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_30(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_29(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_31(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_30(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_32(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_31(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FOR_EACH(M, S, ...) VASE_CONFIG_DETAIL_CAT(VASE_CONFIG_DETAIL_FE_, VASE_CONFIG_DETAIL_NARG(__VA_ARGS__))(M, S, __VA_ARGS__)

#define VASE_CONFIG_DETAIL_MEMBER(S, T) VASE_CONFIG_DETAIL_MEMBER_I(S, VASE_CONFIG_DETAIL_DECOMPOSE T)
#define VASE_CONFIG_DETAIL_MEMBER_I(S, TYPE, NAME, DEFAULT, META) TYPE NAME = DEFAULT;

#define VASE_CONFIG_DETAIL_FIELD(S, T) VASE_CONFIG_DETAIL_FIELD_I(S, VASE_CONFIG_DETAIL_DECOMPOSE T)
#define VASE_CONFIG_DETAIL_FIELD_I(S, TYPE, NAME, DEFAULT, META)                                                        \
    vase::FieldInfo{.Name = #NAME,                                                                                    \
                    .Kind = vase::KindOf<TYPE>(),                                                                     \
                    .Default = vase::Value::From<TYPE>(DEFAULT),                                                      \
                    .Min = vase::MetaMin(META),                                                                       \
                    .Max = vase::MetaMax(META),                                                                       \
                    .Label = vase::MetaLabel(META),                                                                   \
                    .Apply = vase::ApplyTo<&S::NAME>},

#define VASE_CONFIG(Type, ...)                                                                                        \
    struct Type                                                                                                       \
    {                                                                                                                 \
        VASE_CONFIG_DETAIL_FOR_EACH(VASE_CONFIG_DETAIL_MEMBER, Type, __VA_ARGS__)                                     \
        static constexpr std::array<vase::FieldInfo, VASE_CONFIG_DETAIL_NARG(__VA_ARGS__)> kFields =                  \
            {{VASE_CONFIG_DETAIL_FOR_EACH(VASE_CONFIG_DETAIL_FIELD, Type, __VA_ARGS__)}};                             \
        static void* VaseConfigCreate() { return std::make_unique<Type>().release(); }                                \
        static void VaseConfigDestroy(void* raw) { const std::unique_ptr<Type> owning{static_cast<Type*>(raw)}; }     \
    }
// clang-format on
```

- [ ] **Step 5: `Include/Vase/Plugin.h` 的 exports 块加四行**（在 `#include "Vase/Effect/IEffect.h"` 之后、按字母序插入 Config 组）

```cpp
#include "Vase/Config/ConfigInfo.h"
#include "Vase/Config/ConfigMacros.h"
#include "Vase/Config/FieldInfo.h"
#include "Vase/Config/Value.h"
```

- [ ] **Step 6: 写测试 `Tests/Unit/ConfigValueTests.cpp`**

```cpp
#include "Vase/Config/Value.h"

#include <gtest/gtest.h>
#include <type_traits>

namespace
{

constexpr vase::Value kProbeInt{vase::Value::From<std::int32_t>(-7)};
constexpr vase::Value kProbeFloat{vase::Value::From<float>(2.5f)};

static_assert(kProbeInt.Kind == vase::ValueKind::kInt32);
static_assert(kProbeInt.GetAs<std::int32_t>() == -7);
static_assert(kProbeFloat.Kind == vase::ValueKind::kFloat);
static_assert(kProbeFloat.GetAs<float>() == 2.5f);
static_assert(std::is_trivially_copyable_v<vase::Value>);

TEST(ConfigValue, RoundTripsEveryKind)
{
    EXPECT_TRUE(vase::Value::From<bool>(true).GetAs<bool>());
    EXPECT_EQ(vase::Value::From<std::int64_t>(-1234567890123LL).GetAs<std::int64_t>(), -1234567890123LL);
    EXPECT_DOUBLE_EQ(vase::Value::From<double>(1.25).GetAs<double>(), 1.25);
    const char* greeting = "你好"; // UTF-8 字面量：/utf-8 在 VaseBuildOptions 里钉着
    EXPECT_STREQ(vase::Value::From<const char*>(greeting).GetAs<const char*>(), greeting);
}

TEST(ConfigValue, EqualityIsKindSensitive)
{
    // 1 (int32) 与 1.0 (float) 位形可能相同——Kind 不参与相等就是静默串型。
    EXPECT_NE(vase::Value::From<std::int32_t>(1), vase::Value::From<float>(1.0f));
    EXPECT_EQ(vase::Value::From<std::int32_t>(1), vase::Value::From<std::int32_t>(1));
    EXPECT_EQ(vase::Value{}, vase::Value{}); // kNone 默认值可比较
}

TEST(ConfigValue, DefaultIsNone)
{
    EXPECT_EQ(vase::Value{}.Kind, vase::ValueKind::kNone);
    EXPECT_EQ(vase::Value{}.Bits, 0U);
}

} // namespace
```

- [ ] **Step 7: 写测试 `Tests/Unit/ConfigMacroTests.cpp`（宏展开探针）**

```cpp
#include "Vase/Config/ConfigMacros.h"
#include "Vase/Plugin.h"

#include <array>
#include <gtest/gtest.h>
#include <memory>
#include <string_view>
#include <type_traits>

namespace
{

// clang-format off  作者侧形态：元组紧贴、Meta 的 brace-init 在括号保护壳内（spec 3.2）
VASE_CONFIG(CombatConfig,
    (float, CriticalMultiplier, 2.0f, vase::Meta{.Label = "暴击倍率",
                                                 .Min = vase::Value::From<float>(1.0f),
                                                 .Max = vase::Value::From<float>(10.0f)}),
    (bool, FriendlyFire, false, vase::Meta{.Label = "友军伤害"}));
// clang-format on

VASE_CONFIG(StringProbeConfig,
    (const char*, Banner, "hello", vase::Meta{.Label = "横幅"}));

static_assert(std::is_trivially_copyable_v<CombatConfig>); // 标量成员的类型化结构体——kFields 的 POD 要求由 FieldInfo 自查
static_assert(std::is_trivially_copyable_v<vase::FieldInfo>);

TEST(ConfigMacro, MembersAndFieldsComeFromOneExpansion)
{
    CombatConfig cfg; // 成员默认值 = 元组第 3 项
    EXPECT_FLOAT_EQ(cfg.CriticalMultiplier, 2.0f);
    EXPECT_FALSE(cfg.FriendlyFire);

    constexpr auto& fields = CombatConfig::kFields;
    static_assert(fields.size() == 2);
    static_assert(fields[0].Name == std::string_view{"CriticalMultiplier"});
    static_assert(fields[0].Kind == vase::ValueKind::kFloat);
    static_assert(fields[0].Default.GetAs<float>() == 2.0f); // 「同一次展开」的一致性：成员默认 == 表默认
    static_assert(fields[0].Min.GetAs<float>() == 1.0f);
    static_assert(fields[0].Max.GetAs<float>() == 10.0f);
    static_assert(fields[0].Label == std::string_view{"暴击倍率"});
    static_assert(fields[1].Name == std::string_view{"FriendlyFire"});
    static_assert(fields[1].Min.Kind == vase::ValueKind::kNone); // Meta 未写的 Min/Max = kNone（D35）

    EXPECT_NE(CombatConfig::kFields[0].Apply, nullptr);
}

TEST(ConfigMacro, ApplyWritesTypedMember)
{
    CombatConfig cfg;
    CombatConfig::kFields[1].Apply(&cfg, vase::Value::From<bool>(true));
    EXPECT_TRUE(cfg.FriendlyFire);
    CombatConfig::kFields[0].Apply(&cfg, vase::Value::From<float>(3.5f));
    EXPECT_FLOAT_EQ(cfg.CriticalMultiplier, 3.5f); // D35：越过 Meta.Min 的 0.5f 照写不误——库内无执法
}

TEST(ConfigMacro, FieldsOfDescribesTheStruct)
{
    constexpr vase::ConfigInfo info = vase::FieldsOf<CombatConfig>();
    static_assert(info.Count == 2);
    static_assert(info.StructSize == sizeof(CombatConfig));
    static_assert(info.StructAlign == alignof(CombatConfig));
    EXPECT_NE(info.Fields, nullptr);

    void* store = info.CreateConfig(); // 镜像内配对工厂（测试 exe = 本 TU 的镜像）
    ASSERT_NE(store, nullptr);
    info.Fields[1].Apply(store, vase::Value::From<bool>(true));
    EXPECT_TRUE(static_cast<CombatConfig*>(store)->FriendlyFire);
    info.DestroyConfig(store);
}

TEST(ConfigMacro, StringKindWorksEndToEnd)
{
    constexpr vase::ConfigInfo info = vase::FieldsOf<StringProbeConfig>();
    static_assert(info.Count == 1);
    static_assert(info.Fields[0].Default.GetAs<const char*>() != nullptr);
    void* store = info.CreateConfig();
    info.DestroyConfig(store);
}

} // namespace
```

- [ ] **Step 8: 注册进 `Tests/CMakeLists.txt`**：源列表在 `Unit/EffectTests.cpp` 之后插入

```cmake
    Unit/ConfigValueTests.cpp
    Unit/ConfigMacroTests.cpp
```

- [ ] **Step 9: 构建 + 跑新用例 + 全量回归**

```bash
cmake --preset win-x64-clang-debug
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R 'ConfigValue|ConfigMacro'
ctest --preset win-x64-clang-debug          # 全量：既有用例一条不许红
```

预期：新增 8 条用例全过。若 `VASE_CONFIG` 的 NARGS/FOR_EACH 编译报错，回读 Step 4 的预扫描注释（这是探针要撞的墙，撞了按机制改，不改作者侧形态）。

- [ ] **Step 10: format 门禁 + 提交**

```bash
git add Include/Vase/Config Include/Vase/Plugin.h Tests/Unit/ConfigValueTests.cpp Tests/Unit/ConfigMacroTests.cpp Tests/CMakeLists.txt
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git commit -m "M2a-T1 配置 POD 面：Value/FieldInfo/ConfigInfo/VASE_CONFIG 宏与展开探针

Kind+64 位形避开 union 裸读的门禁形态；一致性由同一次展开构造（成员默认 == 表默认）。
enum 不做（D22），Min/Max 无执法（D35）。§10 布局修订 Include/Vase/Config/ 见 spec D31。"
```

---

## Task 2: `ConfigBlob` 拥有面（Host）+ 浅合并穷举

**Spec:** §3.3（D24、D32 的数据侧、§4.2 的可穷测承诺）。

**Files:**
- Create: `Include/Vase/Host/ConfigBlob.h`、`Source/Host/ConfigBlob.cpp`
- Modify: `Source/Host/CMakeLists.txt`（`add_library` 源列表加一行）
- Create: `Tests/Unit/ConfigBlobTests.cpp`；Modify: `Tests/CMakeLists.txt`

**Interfaces:**
- Consumes: T1 的 `vase::Value`/`ValueKind`/`ConfigInfo`。
- Produces（T4/T6/T7/T8/T10 按这些名字用）：`vase::ConfigBlob`（默认构造=空、可拷贝可移动）、`Set(std::string_view, const Value&)`、`Find(std::string_view) -> std::optional<Value>`、`MergeShallow(const ConfigBlob&)`、`Size()`、`Entries()`、静态 `FromDefaults(const ConfigInfo&)`。

- [ ] **Step 1: 写 `Include/Vase/Host/ConfigBlob.h`**

```cpp
#pragma once

// 配置的拥有面（spec 3.3 / D24）：只在宿主侧存在，插件作者不可见（作者只见类型化结构体）。
// 无 JSON、无插件代码——§4.2「合并可被单元测试穷举」以这个形状为前提。
// 导出类带 STL 成员走全局 /wd4251（CLAUDE.md 规矩 1；前提由 D13 + §8.5 矩阵钉着）。
// kString 的借用视图指向 Entry::Stored 里 std::string 的数据——窗口 = 该字段下一次改动之前；
// 消费点（装配 Apply）当场取用，不外带指针。

#include "Vase/Config/ConfigInfo.h"
#include "Vase/Detail/Export.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace vase
{

class VASE_HOST_API ConfigBlob
{
public:
    using Storage = std::variant<bool, std::int32_t, std::int64_t, float, double, std::string>;

    struct Entry
    {
        std::string Key;
        Storage Stored;
    };

    void Set(std::string_view key, const Value& view); // upsert；kString 深拷入拥有存储

    // 借用语义见头注释。禁 .value()——optional 在 kNone 上的 value() 抛 bad_optional_access。
    [[nodiscard]] std::optional<Value> Find(std::string_view key) const;

    void MergeShallow(const ConfigBlob& over); // §4.2：逐 key upsert，嵌套=不可分割整体（本层无嵌套可言）

    [[nodiscard]] std::size_t Size() const { return Fields.size(); }
    [[nodiscard]] const std::vector<Entry>& Entries() const { return Fields; }

    [[nodiscard]] static ConfigBlob FromDefaults(const ConfigInfo& info);

private:
    std::vector<Entry> Fields; // 插入序；线性查找（字段数量级 = 十）
};

} // namespace vase
```

- [ ] **Step 2: 写 `Source/Host/ConfigBlob.cpp`**

```cpp
#include "Vase/Host/ConfigBlob.h"

#include "Vase/Config/Value.h"
#include "Vase/Detail/Fail.h"

#include <iterator>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace vase
{

namespace
{

// variant → 借用视图。get_if 一族：std::get 抛 bad_variant_access，关异常后没东西接得住（规矩 4）。
Value ToView(const ConfigBlob::Storage& stored)
{
    if (const auto* text = std::get_if<std::string>(&stored); text != nullptr)
    {
        return Value::From<const char*>(text->c_str());
    }
    if (const auto* flag = std::get_if<bool>(&stored); flag != nullptr)
    {
        return Value::From<bool>(*flag);
    }
    if (const auto* small = std::get_if<std::int32_t>(&stored); small != nullptr)
    {
        return Value::From<std::int32_t>(*small);
    }
    if (const auto* wide = std::get_if<std::int64_t>(&stored); wide != nullptr)
    {
        return Value::From<std::int64_t>(*wide);
    }
    if (const auto* single = std::get_if<float>(&stored); single != nullptr)
    {
        return Value::From<float>(*single);
    }
    if (const auto* doublePtr = std::get_if<double>(&stored); doublePtr != nullptr)
    {
        return Value::From<double>(*doublePtr);
    }
    detail::ProgrammerError("ConfigBlob::ToView: unreachable variant alternative");
}

ConfigBlob::Entry* FindMutable(std::vector<ConfigBlob::Entry>& fields, std::string_view key)
{
    for (ConfigBlob::Entry& entry : fields)
    {
        if (entry.Key == key)
        {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace

void ConfigBlob::Set(std::string_view key, const Value& view)
{
    if (view.Kind == ValueKind::kNone)
    {
        detail::ProgrammerError("ConfigBlob::Set with kNone value");
    }
    Entry* existing = FindMutable(Fields, key);
    if (existing == nullptr)
    {
        Fields.push_back(Entry{.Key = std::string(key), .Stored = {}});
        existing = &Fields.back();
    }
    switch (view.Kind)
    {
    case ValueKind::kBool:
        existing->Stored = view.GetAs<bool>();
        break;
    case ValueKind::kInt32:
        existing->Stored = view.GetAs<std::int32_t>();
        break;
    case ValueKind::kInt64:
        existing->Stored = view.GetAs<std::int64_t>();
        break;
    case ValueKind::kFloat:
        existing->Stored = view.GetAs<float>();
        break;
    case ValueKind::kDouble:
        existing->Stored = view.GetAs<double>();
        break;
    case ValueKind::kString:
        existing->Stored = std::string(view.GetAs<const char*>()); // 深拷入：此后拥有存储就是唯一所有者
        break;
    default:
        detail::ProgrammerError("ConfigBlob::Set unknown kind");
    }
}

std::optional<Value> ConfigBlob::Find(std::string_view key) const
{
    for (const Entry& entry : Fields)
    {
        if (entry.Key == key)
        {
            return ToView(entry.Stored);
        }
    }
    return std::nullopt;
}

void ConfigBlob::MergeShallow(const ConfigBlob& over)
{
    for (const Entry& entry : over.Fields)
    {
        Set(entry.Key, ToView(entry.Stored));
    }
}

ConfigBlob ConfigBlob::FromDefaults(const ConfigInfo& info)
{
    ConfigBlob blob;
    for (std::uint32_t index = 0; index < info.Count; ++index)
    {
        const FieldInfo& field = *std::next(info.Fields, static_cast<std::ptrdiff_t>(index));
        blob.Set(field.Name, field.Default);
    }
    return blob;
}

} // namespace vase
```

（`ToView` 的自由函数返回类型写全为 `vase::Value`——它在 `namespace vase` 内，照上面即可编译。）

- [ ] **Step 3: `Source/Host/CMakeLists.txt` 的 `add_library(VaseHost SHARED ...)` 里 `PluginHost.cpp` 后加 `ConfigBlob.cpp`。**

- [ ] **Step 4: 写测试 `Tests/Unit/ConfigBlobTests.cpp`（穷举 §4.2 的合并代数）**

```cpp
#include "Vase/Host/ConfigBlob.h"

#include <gtest/gtest.h>
#include <string_view>

namespace
{

constexpr vase::Value Int(int value) { return vase::Value::From<std::int32_t>(value); }

TEST(ConfigBlob, EmptyFindIsNullopt)
{
    const vase::ConfigBlob blob;
    EXPECT_FALSE(blob.Find("absent").has_value());
    EXPECT_EQ(blob.Size(), 0U);
}

TEST(ConfigBlob, SetUpsertsAndKindCanChange)
{
    vase::ConfigBlob blob;
    blob.Set("Echo", Int(1));
    blob.Set("Echo", Int(2));
    EXPECT_EQ(blob.Size(), 1U);
    EXPECT_EQ(*blob.Find("Echo"), Int(2));
    blob.Set("Echo", vase::Value::From<float>(3.5f)); // 层间合并改类型：blob 无类型可言（D32 的检出点在 Apply）
    EXPECT_EQ(blob.Size(), 1U);
}

TEST(ConfigBlob, StringValueIsOwnedAfterSet)
{
    vase::ConfigBlob blob;
    char literal[] = "temporary"; // 非 static：拷入后源缓冲报废也不影响 Find
    blob.Set("Banner", vase::Value::From<const char*>(literal));
    literal[0] = 'X';
    EXPECT_STREQ(blob.Find("Banner")->GetAs<const char*>(), "temporary");
}

TEST(ConfigBlob, MergeShallowOverridesAddsKeeps)
{
    vase::ConfigBlob base;
    base.Set("A", Int(1));
    base.Set("B", Int(2));
    vase::ConfigBlob over;
    over.Set("B", Int(20));
    over.Set("C", Int(30));
    base.MergeShallow(over);
    EXPECT_EQ(base.Size(), 3U);
    EXPECT_EQ(*base.Find("A"), Int(1));   // 未提及 = 保留
    EXPECT_EQ(*base.Find("B"), Int(20));  // 提及 = 覆盖（整体值，无逐字段深合并——§4.2）
    EXPECT_EQ(*base.Find("C"), Int(30));  // 新键 = 追加
}

TEST(ConfigBlob, MergeWithEmptyIsIdentity)
{
    vase::ConfigBlob base;
    base.Set("A", Int(1));
    base.MergeShallow(vase::ConfigBlob{});
    EXPECT_EQ(base.Size(), 1U);
    EXPECT_EQ(*base.Find("A"), Int(1));
}

TEST(ConfigBlob, ThreeLayerStackingOrder)
{
    // §4.3：清单默认 ⊕ Preset ⊕ 本局临时。合并是左折叠。
    vase::ConfigBlob temp;
    temp.Set("Echo", Int(3));
    vase::ConfigBlob preset;
    preset.Set("Echo", Int(2));
    preset.Set("Other", Int(9));
    preset.MergeShallow(temp);
    vase::ConfigBlob resolved = vase::ConfigBlob::FromDefaults(vase::ConfigInfo{}); // 空表 = 空默认层
    resolved.MergeShallow(preset);
    EXPECT_EQ(*resolved.Find("Echo"), Int(3));
    EXPECT_EQ(*resolved.Find("Other"), Int(9));
}

} // namespace
```

（`FromDefaults(vase::ConfigInfo{})` 那支走空 Count 分支——不需要描述符就能钉折叠律。）

- [ ] **Step 5: 注册 `Tests/CMakeLists.txt`**：`Unit/ConfigValueTests.cpp` 后加 `Unit/ConfigBlobTests.cpp`。

- [ ] **Step 6: 构建 + 测试 + format + 提交**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R 'ConfigBlob' && ctest --preset win-x64-clang-debug
git add Include/Vase/Host/ConfigBlob.h Source/Host/ConfigBlob.cpp Source/Host/CMakeLists.txt Tests/Unit/ConfigBlobTests.cpp Tests/CMakeLists.txt
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git commit -m "M2a-T2 ConfigBlob 拥有面：键拥有字符串、浅合并唯一实现点

variant 访问一律 get_if（关异常没有可接住的东西）；kString 深拷入后拥有存储是唯一所有者。
§4.2 的合并代数在此穷举——这是它被选成无类型纯数据层的兑现。"
```

---

## Task 3: 描述符 v2（`OptionalRequires` + `Config`）与 `Context::Config<T>`

**Spec:** §3.4（D26、凭声明扩展）、§3.2（D25 的读取端）。

**Files:**
- Modify: `Include/Vase/PluginDescriptor.h`（两字段、`kHeaderVersion`、头注释）
- Modify: `Include/Vase/Pod/Context.h`（`Config<T>()` + 两成员）、`Source/Pod/Context.cpp`（`DeclaredInRequires` 扩展、未声明消息）
- Modify: `Tests/Unit/DescriptorTests.cpp`（探针升级）

**Interfaces:**
- Consumes: T1 的 `ConfigInfo`/`FieldsOf`/`VASE_CONFIG`。
- Produces: `PluginMeta::OptionalRequires`、`PluginMeta::Config`、`kHeaderVersion == 2`、`ctx.Config<T>()`；T4 起所有装配代码经 `desc->Meta->OptionalRequires` / `->Config` 读声明。

- [ ] **Step 1: 改 `Include/Vase/PluginDescriptor.h`**——常量置 2、注释记 bump 理由：

```cpp
// §8.3：「插件与宿主包含同一份 Vase 头文件」这条前提唯一的执行点（§3.1）。
// 每次不兼容改动递增；插件作者不需要知道它的存在。
// 1 → 2（M2a-T3）：PluginMeta 布局变更——新增 OptionalRequires 与 Config 两槽（D26）。
inline constexpr std::uint32_t kHeaderVersion = 2U;
```

`PluginMeta` 改为（`Requires` 与 `Provides` 之间插 `OptionalRequires`，尾部加 `Config`）：

```cpp
struct PluginMeta
{
    std::string_view Id;
    std::string_view DisplayName;
    std::string_view Version;
    MetaArray<ServiceRef, 16> Requires;
    MetaArray<ServiceRef, 16> OptionalRequires; // 缺失不跳过（§3.3）；凭声明执法覆盖它（spec 3.4）
    MetaArray<ServiceRef, 16> Provides;
    ConfigInfo Config{}; // .Config = vase::FieldsOf<T>() 显式引用；忘写的静默点照旧（§13.3）
};
```

头部 `#include "Vase/Detail/MetaArray.h"` 旁加 `#include "Vase/Config/ConfigInfo.h"`。

- [ ] **Step 2: `Include/Vase/Pod/Context.h`**——`GetScope()` 声明前加读取端：

```cpp
    // 配置读取端（spec 3.2/D25）：T 必须是作者侧 VASE_CONFIG 的那个结构体。
    // 校验只到布局（Size/Align）：跨 DLL 没有可信的类型身份，同布局张冠李戴是作者契约。
    template <typename T>
    [[nodiscard]] const T& Config() const
    {
        if (ConfigStore == nullptr || ConfigMeta == nullptr)
        {
            detail::ProgrammerError("ctx.Config<T>(): plugin declares no config (.Config = FieldsOf<T> missing?)");
        }
        if (sizeof(T) != ConfigMeta->StructSize || alignof(T) != ConfigMeta->StructAlign)
        {
            detail::ProgrammerError("ctx.Config<T>(): layout mismatch with declared config struct");
        }
        return *static_cast<const T*>(ConfigStore);
    }
```

私有成员区（T9 接线点注释之后）加：

```cpp
    // —— M2a（T6 填充）：配置对象的镜像内所有权在 LiveInstance::ConfigObject，这里只借读 ——
    void* ConfigStore = nullptr;
    const ConfigInfo* ConfigMeta = nullptr;
```

类头注释第 6 行的「M2 把它提前到求解期硬拒」一句**本任务不动**——那句话要在 T8/T10 落地执法代码的同一批提交里改写（承诺不能先于兑现存在）。

- [ ] **Step 3: `Source/Pod/Context.cpp` 扩展 `DeclaredInRequires`**（匿名 ns 内整函数替换）：

```cpp
bool DeclaredInRequires(const vase::PluginMeta& meta, std::string_view name, std::uint32_t version)
{
    const auto contains = [name, version](const vase::MetaArray<vase::ServiceRef, 16>& items)
    {
        for (std::size_t index = 0; index < items.Size(); ++index)
        {
            const vase::ServiceRef& ref = *std::next(items.Begin(), static_cast<std::ptrdiff_t>(index));
            if (ref.Name == name && ref.Version == version)
            {
                return true;
            }
        }
        return false;
    };
    return contains(meta.Requires) || contains(meta.OptionalRequires);
}
```

并把 `ReportUndeclaredResolution` 的消息尾部 `" without declaring it in Requires"` 改成 `" without declaring it in Requires/OptionalRequires"`（子串非契约——先 grep 确认测试只匹配 `'Vase\....*' vN` 形态：`EXPECT_DEATH(..., "Vase\\.UndeclaredGetConsumer.*Vase\\.Test\\.Shared' v1")` 一类，尾部措辞不入判据；若发现被钉，改测试而不是改消息）。

- [ ] **Step 4: `Tests/Unit/DescriptorTests.cpp` 探针升级**——匿名 ns 前加配置结构体，brace 块补两槽，用例补断言：

```cpp
VASE_CONFIG(ProbeConfig, (float, Volume, 2.5f, vase::Meta{.Label = "音量"}));
```

```cpp
VASE_PLUGIN(DescriptorProbePlugin){
    .Id = "Vase.DescriptorProbe",
    .DisplayName = "描述符探针",
    .Version = "0.0.1",
    .Requires = {{.Name = "Vase.World", .Version = 1}, {.Name = "Vase.Audio", .Version = 2}},
    .OptionalRequires = {{.Name = "Vase.Optional", .Version = 1}},
    .Provides = {{.Name = "Vase.Probe.Service", .Version = 1}},
    .Config = vase::FieldsOf<ProbeConfig>(),
};
```

`MetaPopulatedThroughBraceBlock` 追加：

```cpp
    ASSERT_EQ(d->Meta->OptionalRequires.Size(), 1U);
    EXPECT_EQ(d->Meta->OptionalRequires.Begin()->Name, "Vase.Optional");
    EXPECT_EQ(d->Meta->Config.Count, 1U);
    EXPECT_EQ(d->Meta->Config.StructSize, sizeof(ProbeConfig));
    ASSERT_NE(d->Meta->Config.Fields, nullptr);
    EXPECT_STREQ(d->Meta->Config.Fields[0].Name, "Volume");
```

既有 `StaleHeaderPlugin` 的 `.Requires = {}, .Provides = {},` designated 写法在新字段下编译不变（省略项取默认），`HeaderVersion=999 ≠ 2` 仍被拒——该 fixture 与钉它的用例**零改动**（bump 的回归证据因此免费）。

- [ ] **Step 5: 全量回归**（所有既有 VASE_PLUGIN 使用点不写新字段 = 默认，行为不变）：

```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
```

预期：全绿。`Config<T>()` 本任务还没有生产填充点（T6 接装配），只在探针里以 meta 形态验证——不要提前造调用。

- [ ] **Step 6: format + 提交**

```bash
git add -A
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git commit -m "M2a-T3 描述符 v2：OptionalRequires + Config 槽，kHeaderVersion 1→2

bump 理由 = PluginMeta 布局变更（D26）；StaleHeaderPlugin 取 999，旧头拒绝路径免费保持有效。
凭声明执法扩到 OptionalRequires；未声明的 TryGet 照旧终止（Get-optional-缺失也终止，作者契约见 Context.h）。"
```

---

## Task 4: `LoadPlan` 定形 + `SkippedRecord` + Id 前置校验

**Spec:** §3.1（D23）、§5.1（D29/D38）、§4.2 之 ①（D33/D39 的 Id 半）、D30。

**Files:**
- Rewrite: `Include/Vase/Host/LoadPlan.h`
- Modify: `Include/Vase/Pod/Pod.h`（SkipClass/SkippedRecord/存储/访问器 + `PodReport::Skips`）、`Source/Host/PluginHost.cpp`（CreatePodImpl：Id 校验、kSkip 分支、注册点移动；DestroyPod 带出 Skips）
- Create: `Tests/Integration/MultiPluginAssemblyTests.cpp`；Modify: `Tests/CMakeLists.txt`

**Interfaces:**
- Consumes: T2 的 `ConfigBlob`。
- Produces: `LoadDecision`/`SkipReason`/`LoadPlanEntry{Id,BinaryPath,Decision,Reason,ResolvedConfig}`、`SkipClass`/`SkippedRecord`、`Pod::Skips()`、`PodReport::Skips`。T5 在同一循环加预检，T6 改 `MakeInstance` 签名，T8 在循环里插碰撞检查。

- [ ] **Step 1: 重写 `Include/Vase/Host/LoadPlan.h`**

```cpp
#pragma once

// §5.1 的输入形状，M2a 定形（D23）：提议形 + BinaryPath（Host 得知道文件在哪；M2b 由清单
// stem + 目录填）+ 三层合并后的 ResolvedConfig。kLoad 条目的数组序 = 加载序 = 关停逆序
// （§5.4）；应当是拓扑序——M2a 由手写者负责、装配预检兜底，M2b 起是 Solve 的构造性保证。
// Id 与 M1 同：借用计划拥有者持有的串，只在这次 CreatePod 调用期间有效。

#include "Vase/Host/ConfigBlob.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string_view>
#include <vector>

namespace vase
{

class Context; // 前向声明足矣：Stage0 回调只接引用

enum class LoadDecision : std::uint8_t { kLoad, kSkip };

// 全部是静态跳过类（§4.4）。后两个值由 M2b 的 Solve 产出；M2a 手写计划至多用 kDisabled
// （「不进局但留在计划里示众」）。运行时跳过不进计划——它活在报告的 Skips 里（§5.1）。
enum class SkipReason : std::uint8_t { kDisabled, kMissingDependency, kVersionMismatch };

struct LoadPlanEntry
{
    std::string_view Id;
    std::filesystem::path BinaryPath;          // 绝对或相对 CWD；Host 内部绝对化（T6 原文）
    LoadDecision Decision = LoadDecision::kLoad;
    SkipReason Reason{};                       // 仅 kSkip 有意义
    ConfigBlob ResolvedConfig;                 // 缺字段回退 kFields 默认（D23）；空 = 全默认
};

struct LoadPlan
{
    std::vector<LoadPlanEntry> Ordered;
};

struct PodOptions
{
    bool Strict = false; // §5.5 + D36：Strict 只对 Failed 触发；预检/静态跳过不算失败
    std::function<void(Context&)> Stage0; // §5.3 阶段 0：宿主服务注册点
};

} // namespace vase
```

- [ ] **Step 2: `Include/Vase/Pod/Pod.h`**——`FailedPluginRecord` 定义之后加：

```cpp
// §4.4 两类跳过，从类型上就不许混（D29）。住 Pod.h 的理由与 FailedPluginRecord 同款：
// 装配期生成、Pod 自持，链接方向不许它上 Host 头。
enum class SkipClass : std::uint8_t { kStatic, kRuntime };

struct SkippedRecord
{
    std::string Id;
    SkipClass Class = SkipClass::kStatic;
    std::string Cause;    // 静态：SkipReason 的名字化；运行时：missing service <k>@<v> / cascade from <id>
    std::string CausedBy; // 运行时归因（D41）；序缺陷与静态为空串
};
```

`PodReport` 的 `Failures` 行后加 `std::vector<SkippedRecord> Skips;`。`Pod` public 区 `Failures()` 后加：

```cpp
    [[nodiscard]] const std::vector<SkippedRecord>& Skips() const { return SkipRecords; }
```

private 区 `FailureRecords` 行后加一行存储：

```cpp
    std::vector<SkippedRecord> SkipRecords; // §5.1：跳过者无实例无边——不进活集合、不进 Clean（D38）
```

- [ ] **Step 3: `Source/Host/PluginHost.cpp` 的 `CreatePodImpl` 三处改动**

3a. `AssertBoundThread("CreatePod");` 之后、槽扫描之前，插入 Id 前置校验（真·零副作用：此刻没占槽、没注册、没加载）：

```cpp
    // ①' 结构性校验·Id 唯一（D33 ①/D39：含 kSkip——同 Id 两次 = 路径覆盖 + 反查歧义）。
    for (std::size_t first = 0; first < plan.Ordered.size(); ++first)
    {
        const LoadPlanEntry& outer = *std::next(plan.Ordered.begin(), static_cast<std::ptrdiff_t>(first));
        for (std::size_t second = first + 1; second < plan.Ordered.size(); ++second)
        {
            const LoadPlanEntry& inner = *std::next(plan.Ordered.begin(), static_cast<std::ptrdiff_t>(second));
            if (outer.Id == inner.Id)
            {
                return Result<PodHandle>::Err(Refusal(
                    outer.Id, Phase::kLoad,
                    "plan rejected: duplicate plugin id " + std::string(outer.Id) + " (entry order " +
                        std::to_string(first) + " and " + std::to_string(second) + ")"));
            }
        }
    }
```

3b. 阶段 1 循环（`for (const LoadPlanEntry& entry : plan.Ordered)`）体内：**删掉首行的**
`KnownBinaries[std::string(entry.Id)] = entry.BinaryPath;`（注册点移到 3d），在 `①' 结构性校验` 之后、`EnsureResident` 之前插入 kSkip 分支：

```cpp
        if (entry.Decision == LoadDecision::kSkip)
        {
            // §5.1：计划里只有静态跳过；Host 的职责是把它们如实记进报告，不参与即不加载。
            static const char* const reasonText[] = {"disabled", "missing dependency", "version mismatch"};
            const std::size_t reasonIndex = static_cast<std::size_t>(entry.Reason);
            pod.SkipRecords.push_back(SkippedRecord{
                .Id = std::string(entry.Id),
                .Class = SkipClass::kStatic,
                .Cause = std::string("static skip: ") +
                         (reasonIndex < 3 ? reasonText[reasonIndex] : "unknown"),
                .CausedBy = {},
            });
            continue;
        }
```

3d. 函数尾部、⑥ Strict 判定**之后**（`return Ok` 之前）统一注册（D30——失败的尝试不注册，「Err 的局什么都没发生过」）：

```cpp
    // D30：KnownBinaries 为**全部**条目注册（含 kSkip 与装载失败者）——Adopt 不关心它当初为何没进局。
    for (const LoadPlanEntry& entry : plan.Ordered)
    {
        KnownBinaries[std::string(entry.Id)] = entry.BinaryPath;
    }
```

（T6 会往这个循环里补 `slot->Replays[std::string(entry.Id)] = entry.ResolvedConfig;` 与槽复用处的 `clear()`——本任务**只放上面这几行**，回放表成员那时还不存在。）

- [ ] **Step 4: `DestroyPod` 带出 Skips**——`report.Failures = ...` 行后加 `report.Skips = slot->Inner->Skips();`。

- [ ] **Step 5: 写 `Tests/Integration/MultiPluginAssemblyTests.cpp`**（本任务 4 条；T5 继续追加）

```cpp
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Pod.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace
{

vase::LoadPlan Plan(std::initializer_list<std::pair<std::string_view, std::filesystem::path>> items)
{
    vase::LoadPlan plan;
    for (const auto& [id, path] : items)
    {
        plan.Ordered.push_back({.Id = id, .BinaryPath = path});
    }
    return plan;
}

// 服务标识与 fixture 同源（测试材料，不是公开 API）：本任务只用到 SharedCommon 的两个。
#include "../Integration/fixtures/SharedCommon.h"

class HostMarker final : public samples_fixture::IHostOnlyService
{
public:
    [[nodiscard]] int Marker() const override { return 1; }
};

TEST(MultiPluginAssembly, DuplicateIdRejectedBeforeAnySideEffect)
{
    vase::PluginHost host;
    const vase::Result<vase::PodHandle> r = host.CreatePod(
        Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}, {"Vase.Hello", VASE_FIXTURE_SHAREDPROVIDER}}));
    ASSERT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("duplicate plugin id"), std::string::npos);
    const auto& counters = host.ForTestCounters(); // 真·零副作用：五项全 0，没有半个镜像被读、没有半条注册
    EXPECT_EQ(counters.PluginInstances, 0U);
    EXPECT_EQ(counters.Effects, 0U);
    EXPECT_EQ(counters.Services, 0U);
    EXPECT_EQ(host.Resolve(vase::PodHandle{.Index = 0, .Generation = 1}), nullptr); // 槽根本没被占
}

TEST(MultiPluginAssembly, StaticSkipRecordedAndAdoptableLater)
{
    // kSkip 条目：不进局、如实记进报告、路径照注册（D30）——之后可被 Adopt 捞回。
    vase::PluginHost host;
    vase::LoadPlan plan = Plan({{"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER}});
    plan.Ordered.push_back({.Id = "Vase.EdgeConsumer",
                            .BinaryPath = VASE_FIXTURE_EDGECONSUMER,
                            .Decision = vase::LoadDecision::kSkip,
                            .Reason = vase::SkipReason::kDisabled});
    HostMarker marker;
    vase::PodOptions options;
    options.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::IHostOnlyService>(marker); };
    const vase::PodHandle h = host.CreatePod(plan, options).Value();

    vase::Pod* pod = host.Resolve(h);
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(pod->PluginCount(), 1U); // 只进局了 provider
    ASSERT_EQ(pod->Skips().size(), 1U);
    EXPECT_EQ(pod->Skips().begin()->Class, vase::SkipClass::kStatic);
    EXPECT_EQ(pod->Skips().begin()->Id, "Vase.EdgeConsumer");
    EXPECT_NE(pod->Skips().begin()->Cause.find("disabled"), std::string::npos);

    ASSERT_TRUE(host.AdoptPlugin(h, "Vase.EdgeConsumer").IsOk()); // 当初为什么没进局，Adopt 不关心（D30）
    EXPECT_EQ(host.Resolve(h)->PluginCount(), 2U);
    const vase::PodReport report = host.DestroyPod(h);
    EXPECT_TRUE(report.Clean());                       // D38：Skips 不进 Clean
    ASSERT_EQ(report.Skips.size(), 1U);                // 静态记录随报告带出
    EXPECT_EQ(report.Skips.begin()->Class, vase::SkipClass::kStatic);
}

TEST(MultiPluginAssembly, StrictIgnoresStaticSkips)
{
    // D36 的静态侧：Strict 只看 Failed。kSkip 不触发整局失败（运行时侧在 T5 补）。
    vase::PluginHost host;
    vase::LoadPlan plan = Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}});
    plan.Ordered.push_back({.Id = "Vase.SharedProvider",
                            .BinaryPath = VASE_FIXTURE_SHAREDPROVIDER,
                            .Decision = vase::LoadDecision::kSkip});
    vase::PodOptions options;
    options.Strict = true;
    const vase::Result<vase::PodHandle> r = host.CreatePod(plan, options);
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    host.DestroyPod(r.Value());
}

TEST(MultiPluginAssembly, SkipsDoNotEnterLiveSetOrClean)
{
    // D38 钉死：跳过既不留资源也不进活集合——Clean() 与 PluginIds 都不为它变脸。
    vase::PluginHost host;
    vase::LoadPlan plan = Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}});
    plan.Ordered.push_back({.Id = "Vase.StaleHeader",
                            .BinaryPath = VASE_FIXTURE_STALEHEADER,
                            .Decision = vase::LoadDecision::kSkip});
    const vase::PodHandle h = host.CreatePod(plan).Value();
    const vase::Pod* pod = host.Resolve(h);
    EXPECT_EQ(pod->PluginIds(), std::vector<std::string>{"Vase.Hello"});
    EXPECT_FALSE(pod->HasPlugin("Vase.StaleHeader"));
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

} // namespace
```

- [ ] **Step 6: 注册 `Tests/CMakeLists.txt`**：`Integration/ThreadGuardTests.cpp` 后加 `Integration/MultiPluginAssemblyTests.cpp`。

- [ ] **Step 7: 构建 + 全量 + format + 提交**

```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
git add Include/Vase/Host/LoadPlan.h Include/Vase/Pod/Pod.h Source/Host/PluginHost.cpp Tests/Integration/MultiPluginAssemblyTests.cpp Tests/CMakeLists.txt
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git commit -m "M2a-T4 LoadPlan 定形 + SkippedRecord：静态/运行时跳过从类型上分开

提议形 + BinaryPath + ResolvedConfig（D23）；Id 唯一前置校验真·零副作用（D33 ①/D39）；
kSkip 如实记报告、路径全量注册供 Adopt 捞回（D30）；Skips 不进 Clean 钉死（D38）。
Strict 只看 Failed 的静态侧（D36），运行时侧随预检在 T5。"
```

---

## Task 5: 装配预检 + 归因（D27/D41）+ 加载级联 + 环退化 + 改写 M1 爆炸形态用例

**Spec:** §4.2（预检与声明登记账）、§5.2（Skipped[运行时]）、D40/D41。

**Files:**
- Create: `Tests/Integration/fixtures/M2Common.h` + 四个 fixture 目录（DeadProvider/DeadConsumer/CycleA/CycleB）
- Modify: `Tests/Integration/fixtures/CMakeLists.txt`、`Tests/CMakeLists.txt`（4 个宏）
- Modify: `Source/Host/PluginHost.cpp`（CreatePodImpl 循环主体重构——本计划的心脏，Step 3 给全量）
- Modify: `Tests/Integration/FailureSemanticsTests.cpp`（MissingDeclaredService 一条改写）
- Modify: `Tests/Integration/MultiPluginAssemblyTests.cpp`（追加归因/级联/环用例）

**Interfaces:**
- Consumes: T4 的循环骨架与 SkippedRecord。
- Produces: 阶段 1 循环最终形（T6 在其上换 `MakeInstance` 调用、T8 插碰撞步）；`M2Common.h` 的 `Vase.Test.Dead`/`CycleA`/`CycleB` 标识。

- [ ] **Step 1: 写 `Tests/Integration/fixtures/M2Common.h`**

```cpp
#pragma once

// M2a 装配语义 fixture 群的测试服务标识（T5–T9 共用；与 SharedCommon.h 同款定位：测试材料）。
// 解析靠 kName 字符串跨模块匹配（§6.1），与类型是否同名无关。

#include <cstdint>
#include <string_view>

namespace m2_fixture
{

// 声明了 Provides 但 OnLoad 返回 Err 的提供者：级联归因（D41「提供者已死」支）的主角。
class IDeadService
{
public:
    static constexpr std::string_view kName = "Vase.Test.Dead";
    static constexpr std::uint32_t kVersion = 1;
    IDeadService() = default;
    IDeadService(const IDeadService&) = delete;
    IDeadService& operator=(const IDeadService&) = delete;
    IDeadService(IDeadService&&) = delete;
    IDeadService& operator=(IDeadService&&) = delete;
    virtual ~IDeadService() = default;
    [[nodiscard]] virtual int Value() const = 0;
};

// 环退化互缺（D40）：A strict 需 B、B strict 需 A。
class ICycleA
{
public:
    static constexpr std::string_view kName = "Vase.Test.CycleA";
    static constexpr std::uint32_t kVersion = 1;
    ICycleA() = default;
    ICycleA(const ICycleA&) = delete;
    ICycleA& operator=(const ICycleA&) = delete;
    ICycleA(ICycleA&&) = delete;
    ICycleA& operator=(ICycleA&&) = delete;
    virtual ~ICycleA() = default;
};

class ICycleB
{
public:
    static constexpr std::string_view kName = "Vase.Test.CycleB";
    static constexpr std::uint32_t kVersion = 1;
    ICycleB() = default;
    ICycleB(const ICycleB&) = delete;
    ICycleB& operator=(const ICycleB&) = delete;
    ICycleB(ICycleB&&) = delete;
    ICycleB& operator=(ICycleB&&) = delete;
    virtual ~ICycleB() = default;
};

} // namespace m2_fixture
```

- [ ] **Step 2: 四个 fixture 源文件**（形态全列，不许「同 Task N」）

`Tests/Integration/fixtures/DeadProviderPlugin/DeadProviderPlugin.cpp`：

```cpp
// 声明 Provides 但 OnLoad 必败：下游按声明登记账归因到它（D41），按注册表预检跳过（D27）。
#include "Vase/Plugin.h"

#include "../M2Common.h"

namespace
{
class DeadProviderPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Err(vase::Error{"intentional provider failure"});
    }
};
} // namespace

VASE_PLUGIN(DeadProviderPlugin){
    .Id = "Vase.DeadProvider",
    .DisplayName = "失败提供者探针",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.Dead", .Version = 1}},
};
```

`Tests/Integration/fixtures/DeadConsumerPlugin/DeadConsumerPlugin.cpp`：

```cpp
// strict 需要 Dead：提供者死了它就被预检跳过，归因应点名 Vase.DeadProvider。
#include "Vase/Plugin.h"

#include "../M2Common.h"

namespace
{
class DeadConsumerPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx); // 正常路径根本到不了这里（预检先拦）
        return vase::Result<void>::Ok();
    }
};
} // namespace

VASE_PLUGIN(DeadConsumerPlugin){
    .Id = "Vase.DeadConsumer",
    .DisplayName = "死亡依赖消费者探针",
    .Version = "0.0.1",
    .Requires = {{.Name = "Vase.Test.Dead", .Version = 1}},
    .Provides = {},
};
```

`Tests/Integration/fixtures/CycleAPlugin/CycleAPlugin.cpp`：

```cpp
// 环的一边：strict 需 B、提供 A。装配预检让双方都「缺对方」→ 双双运行时跳过（D40）。
#include "Vase/Plugin.h"

#include "../M2Common.h"

namespace
{
class CycleAImpl final : public m2_fixture::ICycleA {};
class CycleAPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<m2_fixture::ICycleA>(Instance);
        return vase::Result<void>::Ok();
    }

private:
    CycleAImpl Instance;
};
} // namespace

VASE_PLUGIN(CycleAPlugin){
    .Id = "Vase.CycleA",
    .DisplayName = "环探针 A",
    .Version = "0.0.1",
    .Requires = {{.Name = "Vase.Test.CycleB", .Version = 1}},
    .Provides = {{.Name = "Vase.Test.CycleA", .Version = 1}},
};
```

`Tests/Integration/fixtures/CycleBPlugin/CycleBPlugin.cpp`：

```cpp
// 环的另一边：strict 需 A、提供 B。与 CycleAPlugin 对偶。
#include "Vase/Plugin.h"

#include "../M2Common.h"

namespace
{
class CycleBImpl final : public m2_fixture::ICycleB {};
class CycleBPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<m2_fixture::ICycleB>(Instance);
        return vase::Result<void>::Ok();
    }

private:
    CycleBImpl Instance;
};
} // namespace

VASE_PLUGIN(CycleBPlugin){
    .Id = "Vase.CycleB",
    .DisplayName = "环探针 B",
    .Version = "0.0.1",
    .Requires = {{.Name = "Vase.Test.CycleA", .Version = 1}},
    .Provides = {{.Name = "Vase.Test.CycleB", .Version = 1}},
};
```

`Tests/Integration/fixtures/CMakeLists.txt` 末尾追加：

```cmake
# M2a 装配语义探针（T5）：归因链与环退化。
vase_add_plugin_fixture(DeadProviderPlugin
    SOURCES DeadProviderPlugin/DeadProviderPlugin.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(DeadConsumerPlugin
    SOURCES DeadConsumerPlugin/DeadConsumerPlugin.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(CycleAPlugin
    SOURCES CycleAPlugin/CycleAPlugin.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(CycleBPlugin
    SOURCES CycleBPlugin/CycleBPlugin.cpp
    LINK_LIBRARIES VasePod)
```

`Tests/CMakeLists.txt` 宏列表追加：

```cmake
    VASE_FIXTURE_DEADPROVIDER="$<PATH:CMAKE_PATH,$<TARGET_FILE:DeadProviderPlugin>>"
    VASE_FIXTURE_DEADCONSUMER="$<PATH:CMAKE_PATH,$<TARGET_FILE:DeadConsumerPlugin>>"
    VASE_FIXTURE_CYCLEA="$<PATH:CMAKE_PATH,$<TARGET_FILE:CycleAPlugin>>"
    VASE_FIXTURE_CYCLEB="$<PATH:CMAKE_PATH,$<TARGET_FILE:CycleBPlugin>>"
```

- [ ] **Step 3: `CreatePodImpl` 阶段 1 循环重构（心脏步骤，整段替换 ④ 注释块到 `}` 收尾的循环）**

循环前加声明登记账；循环体定型为「inspect 成功 → **自 Provides 记账** → **预检 Requires** → 装配」。
（自记账在预检**前**：提供者自己 OnLoad 失败时它的声明仍要在账上，D41 才能点名它。）

```cpp
    struct DeclaredProvider
    {
        std::string Service;
        std::uint32_t Version = 0;
        std::string ProviderId; // 已 inspect 的 kLoad 条目（Loaded/Failed/Skipped 都在账上）
    };
    std::vector<DeclaredProvider> declaredProviders;

    // ④ 阶段 1（§5.3）：逐条目按数组序（= 拓扑序）装载。宽容语义：单条失败只落记录（§0.3-4）。
    // M2a 形状（D27/D33 ②）：声明住二进制里，预检只能在 InspectBinary 之后——镜像驻留是读声明的代价，
    // M2b 清单到位后本段整体提前，此注释随之删。
    for (const LoadPlanEntry& entry : plan.Ordered)
    {
        if (entry.Decision == LoadDecision::kSkip)
        {
            // （T4 Step 3b 的 kSkip 分支原样留在循环首——位置不变）
        }
        // …kSkip 分支之后 continue 的路径不变，下面从「非 skip 条目」开始描述改动：

        const Result<detail::BinaryRecord*> resident = Loader.EnsureResident(entry.BinaryPath);
        if (!resident.IsOk())
        {
            recordFailure(entry.Id, Phase::kLoad, resident.GetError().Message());
            continue; // 无镜像可记（M1 原语义）；其声明不在账上 = D41/碰撞的已知失明面
        }

        const Result<const PluginDescriptor*> inspected = InspectBinary(*resident.Value(), entry.Id);
        if (!inspected.IsOk())
        {
            recordFailure(entry.Id, Phase::kLoad, inspected.GetError().Message());
            recordFailedBinary(entry.Id, resident.Value());
            continue;
        }
        const PluginDescriptor* desc = inspected.Value();

        // (a) 自 Provides 记账（先于预检：本条目的声明从此对**后来者**可见，也对归因可见）。
        for (std::size_t index = 0; index < desc->Meta->Provides.Size(); ++index)
        {
            const ServiceRef& provided =
                *std::next(desc->Meta->Provides.Begin(), static_cast<std::ptrdiff_t>(index));
            declaredProviders.push_back(DeclaredProvider{
                .Service = std::string(provided.Name), .Version = provided.Version, .ProviderId = std::string(entry.Id)});
        }

        // (b) 装配预检（D27）：每条 strict Requires 必须已在**服务注册表**可查（宿主 Stage0 ∪
        //     已 OnLoad 成功者）。查不到 → 运行时跳过：不建实例、不跑 OnLoad（镜像留架，宿主退出收）。
        std::string missName;
        std::uint32_t missVersion = 0;
        for (std::size_t index = 0; index < desc->Meta->Requires.Size(); ++index)
        {
            const ServiceRef& required =
                *std::next(desc->Meta->Requires.Begin(), static_cast<std::ptrdiff_t>(index));
            if (pod.Registry->Find(ServiceKey{.Name = required.Name, .Version = required.Version}) == nullptr)
            {
                missName = std::string(required.Name);
                missVersion = required.Version;
                break;
            }
        }
        if (!missName.empty())
        {
            // D41 归因三态：声明登记账里有提供者 → 点名（它已 Loaded/Failed/Skipped——Loaded 时本分支
            // 进不来，故实际点到的都是「倒了的那个」）；账里没有 → "" （序缺陷或计划外）。
            std::string causedBy;
            for (const DeclaredProvider& provider : declaredProviders)
            {
                if (provider.Service == missName && provider.Version == missVersion && provider.ProviderId != std::string(entry.Id))
                {
                    causedBy = provider.ProviderId;
                    break;
                }
            }
            pod.SkipRecords.push_back(SkippedRecord{
                .Id = std::string(entry.Id),
                .Class = SkipClass::kRuntime,
                .Cause = "missing service " + missName + "@" + std::to_string(missVersion),
                .CausedBy = std::move(causedBy),
            });
            continue;
        }

        std::unique_ptr<Pod::LiveInstance> live = MakeInstance(pod, *resident.Value(), desc);
        const Result<void> loaded = live->Instance->OnLoad(*live->Ctx);
        if (!loaded.IsOk())
        {
            // §5.2 失败即时回收 + 下游被预检接住（级联 = 本循环的重复应用，D27）——T8 之前这段原样保留。
            DiscardInstance(pod, *live);
            recordFailure(entry.Id, Phase::kLoad, loaded.GetError().Message());
            recordFailedBinary(entry.Id, live->Binary);
            pod.Instances.push_back(std::move(live));
            continue;
        }
        live->State = Pod::LiveInstance::InstanceState::kLoaded;
        pod.Instances.push_back(std::move(live));
    }
```

（上面 `if (entry.Decision == LoadDecision::kSkip)` 的块内注释行是**占位提醒**，实施时保留 T4 写入的完整 kSkip 分支实体，不重复两份——落盘后该分支在 `EnsureResident` 之前、`continue` 语义与 T4 一致。实施者核对：kSkip 分支 → resident → inspect → (a) → (b) → 装配。）

- [ ] **Step 4: 改写 `Tests/Integration/FailureSemanticsTests.cpp` 的爆炸形态用例**（M1 的最后一道墙被预检提前，用例按新语义改写；spec D37 注记）

`MissingDeclaredServiceTerminatesWithFullIdentity` **整条替换**：

```cpp
TEST(FailureSemantics, MissingDeclaredServiceNowSkippedAtAssembly)
{
    // M1 形态（Get 撞终止）在 M2a 被装配预检提前拦下（D27）：ghost 从未在计划里，
    // 归因空（D41「不在计划」态）。运行期那道墙仍守着 Adopt⑤ 与运行中违约（D37 注记）。
    vase::PluginHost host;
    const vase::Result<vase::PodHandle> r = host.CreatePod(Plan({{"Vase.RequiresMissingConsumer", VASE_FIXTURE_MISSINGCONSUMER}}));
    ASSERT_TRUE(r.IsOk()); // 宽容模式照开
    const vase::Pod* pod = host.Resolve(r.Value());
    EXPECT_EQ(pod->PluginCount(), 0U);
    EXPECT_TRUE(pod->Failures().empty());          // 它不是 Failed——是运行时跳过
    ASSERT_EQ(pod->Skips().size(), 1U);
    EXPECT_EQ(pod->Skips().begin()->Class, vase::SkipClass::kRuntime);
    EXPECT_NE(pod->Skips().begin()->Cause.find("Vase.Ghost@1"), std::string::npos);
    EXPECT_TRUE(pod->Skips().begin()->CausedBy.empty());
    EXPECT_TRUE(host.DestroyPod(r.Value()).Clean());
}
```

（`RequiresMissingConsumer` 的 `Get` 违约消息仍被 `ReportMissingRequiredService` 守着——LedgerSemantics 的未声明解析 death 用例覆盖它，不新增。）

- [ ] **Step 5: `MultiPluginAssemblyTests.cpp` 追加归因/级联/环/Strict 运行时侧用例**

```cpp
TEST(MultiPluginAssembly, LoadCascadeAttributesToFailedProvider)
{
    // D41「提供者已死」支：DeadProvider Failed → DeadConsumer 预检跳过且点名它。
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(Plan({{"Vase.DeadProvider", VASE_FIXTURE_DEADPROVIDER},
                                                   {"Vase.DeadConsumer", VASE_FIXTURE_DEADCONSUMER}}))
                                  .Value();
    const vase::Pod* pod = host.Resolve(h);
    EXPECT_EQ(pod->PluginCount(), 0U);
    ASSERT_EQ(pod->Failures().size(), 1U);
    EXPECT_EQ(pod->Failures().begin()->Id, "Vase.DeadProvider");
    ASSERT_EQ(pod->Skips().size(), 1U);
    const vase::SkippedRecord& skipped = *pod->Skips().begin();
    EXPECT_EQ(skipped.Class, vase::SkipClass::kRuntime);
    EXPECT_EQ(skipped.CausedBy, "Vase.DeadProvider"); // 级联有名（§4.4 编辑器形态）
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(MultiPluginAssembly, BadOrderSkipsWithNoCauseNamed)
{
    // 序缺陷支：消费者在前、提供者在后 → 消费者被预检跳过，CausedBy 空（不能赖给还没轮到的人）。
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(Plan({{"Vase.DeadConsumer", VASE_FIXTURE_DEADCONSUMER},
                                                   {"Vase.DeadProvider", VASE_FIXTURE_DEADPROVIDER}}))
                                  .Value();
    const vase::Pod* pod = host.Resolve(h);
    EXPECT_EQ(pod->PluginCount(), 1U); // provider 自己无依赖，照常进局
    ASSERT_EQ(pod->Skips().size(), 1U);
    EXPECT_EQ(pod->Skips().begin()->Id, "Vase.DeadConsumer");
    EXPECT_TRUE(pod->Skips().begin()->CausedBy.empty());
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(MultiPluginAssembly, CycleDegradesToMutualSkips)
{
    // D40：环不检测——互缺互跳、不挂不炸；报告里没有「cycle」字样（点名归 M2b Solve）。
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(
        Plan({{"Vase.CycleA", VASE_FIXTURE_CYCLEA}, {"Vase.CycleB", VASE_FIXTURE_CYCLEB}}))
                                  .Value();
    const vase::Pod* pod = host.Resolve(h);
    EXPECT_EQ(pod->PluginCount(), 0U);
    EXPECT_EQ(pod->Failures().size(), 0U);
    EXPECT_EQ(pod->Skips().size(), 2U);
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(MultiPluginAssembly, StrictIgnoresRuntimeSkipsButFailsOnRealFailure)
{
    // D36 运行时侧：Strict 下「只有跳过」照常交付（上面 BadOrder 的 Strict 化断言）；
    // 「有 Failed」才整局 Err——两个形态钉在同一条用例里，读的人不必在两条测试之间拼语义。
    {
        vase::PluginHost host;
        vase::PodOptions options;
        options.Strict = true;
        const vase::Result<vase::PodHandle> r = host.CreatePod(
            Plan({{"Vase.DeadConsumer", VASE_FIXTURE_DEADCONSUMER}, {"Vase.DeadProvider", VASE_FIXTURE_DEADPROVIDER}}),
            options);
        ASSERT_TRUE(r.IsOk()) << r.GetError().Message(); // 跳过不是失败
        host.DestroyPod(r.Value());
    }
    {
        vase::PluginHost host;
        vase::PodOptions options;
        options.Strict = true;
        const vase::Result<vase::PodHandle> r = host.CreatePod(
            Plan({{"Vase.DeadProvider", VASE_FIXTURE_DEADPROVIDER}, {"Vase.DeadConsumer", VASE_FIXTURE_DEADCONSUMER}}),
            options);
        ASSERT_FALSE(r.IsOk()); // provider Failed → 整局 Err（预检跳过随半局一起丢）
        EXPECT_NE(r.GetError().Message().find("intentional provider failure"), std::string::npos);
    }
}
```

- [ ] **Step 6: 构建 + 全量 + format + 提交**

```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
git add Tests/Integration/fixtures Source/Host/PluginHost.cpp Tests/Integration/FailureSemanticsTests.cpp Tests/Integration/MultiPluginAssemblyTests.cpp Tests/CMakeLists.txt
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git commit -m "M2a-T5 装配预检与归因：加载级联 = 预检在拓扑序上的重复应用

D27/D41：预检排在 inspect 后（M2a 声明住二进制里）；归因三态——提供者已死点名、序缺陷留空。
D40：环退化为互缺互跳，不检测。M1 的 RequiresMissingConsumer 爆炸用例按新语义改写为跳过断言。"
```

---

## Task 6: 配置应用（`MakeInstance`）+ D34 回放表 + Hello 示例形态

**Spec:** §4.1（D32、D34、D35、D25 的填充端）。

**Files:**
- Modify: `Source/Host/PluginHost.cpp`（`MakeInstance` 换签名带 blob；`CreatePodImpl`/`AdoptPlugin` 两调用点；`PodSlot` 加 Replays）、`Include/Vase/Host/PluginHost.h`（成员函数声明 + PodSlot）
- Modify: `Include/Vase/Pod/Pod.h`（`LiveInstance::ConfigObject`）
- Create: `Tests/Integration/fixtures/{ConfigConsumerPlugin,ConfigLayoutMisusePlugin}/`、`Tests/Integration/ConfigApplyTests.cpp`；Modify fixtures/CMakeLists、Tests/CMakeLists（宏 + 源）
- Modify: `Samples/HelloPlugin/HelloPlugin.cpp`、`Samples/HelloPluginPrime/HelloPlugin.cpp`（配置示例形态）

**Interfaces:**
- Consumes: T1 `ConfigInfo`/`FieldsOf`、T2 `ConfigBlob::Find`、T3 `ctx.Config<T>()`。
- Produces: `MakeInstance(Pod&, BinaryRecord&, const PluginDescriptor*, const ConfigBlob&) -> Result<std::unique_ptr<Pod::LiveInstance>>`（T7/T8 依赖此签名）；`PodSlot::Replays`。

- [ ] **Step 1: `PluginHost.h`**——`MakeInstance` 声明替换、`PodSlot` 加成员：

```cpp
    // 装配件：造配置对象 → Apply → 建实例挂 Context。配置键/类型不匹配 = 宿主给的数据错，
    // 可恢复 → Err（D32；不走 ProgrammerError）。blob 由调用方给：CreatePod 用计划条目、
    // Adopt 用回放表（D34），查无 = 空 blob = 全默认。
    [[nodiscard]] Result<std::unique_ptr<Pod::LiveInstance>> MakeInstance(Pod& pod, detail::BinaryRecord& record,
                                                                         const PluginDescriptor* desc,
                                                                         const ConfigBlob& resolvedConfig);
```

```cpp
        std::unordered_map<std::string, ConfigBlob> Replays; // D34：计划灌入的 Id→ResolvedConfig，Adopt 回放
```

（`#include "Vase/Host/ConfigBlob.h"` 经 LoadPlan.h 已到位。）

- [ ] **Step 2: `Include/Vase/Pod/Pod.h` 的 `LiveInstance`**——`Ctx` 成员后加：

```cpp
        // 配置对象：镜像内 CreateConfig 造、DestroyConfig 毁（D25）；deleter 与指针成对，空 = 无配置。
        std::unique_ptr<void, void (*)(void*)> ConfigObject{nullptr, nullptr};
```

- [ ] **Step 3: `PluginHost.cpp` 的 `MakeInstance` 重写**

```cpp
Result<std::unique_ptr<Pod::LiveInstance>> PluginHost::MakeInstance(Pod& pod, detail::BinaryRecord& record,
                                                                    const PluginDescriptor* desc,
                                                                    const ConfigBlob& resolvedConfig)
{
    // 配置先建（D32 的检出点）：Kind 不匹配 → 整插件 kLoad Failed，不触 ProgrammerError。
    std::unique_ptr<Pod::LiveInstance> live = std::make_unique<Pod::LiveInstance>();
    live->Desc = desc;
    live->Binary = &record;

    const ConfigInfo& info = desc->Meta->Config;
    void* store = nullptr;
    if (info.Count > 0)
    {
        store = info.CreateConfig();
        for (std::uint32_t index = 0; index < info.Count; ++index)
        {
            const FieldInfo& field = *std::next(info.Fields, static_cast<std::ptrdiff_t>(index));
            const std::optional<Value> found = resolvedConfig.Find(field.Name);
            const Value chosen = found.has_value() ? *found : field.Default; // D23 缺字段回退默认
            if (chosen.Kind != field.Kind)
            {
                info.DestroyConfig(store);
                std::string message{"config field "};
                message.append(field.Name);
                message.append(" kind mismatch");
                ErrorContext context;
                context.PluginId = std::string(desc->Meta->Id);
                context.Stage = Phase::kLoad;
                return Result<std::unique_ptr<Pod::LiveInstance>>::Err(Error{std::move(message), std::move(context)});
            }
            field.Apply(store, chosen); // D35：Min/Max 不查——纯展示元信息
        }
        live->ConfigObject = std::unique_ptr<void, void (*)(void*)>(store, info.DestroyConfig);
    }

    ++Counters.PluginInstances;
    live->Instance = desc->Create();
    live->OwnerLabel = std::string(desc->Meta->Id);
    live->Scope = std::make_unique<EffectScope>(CountersPool, &Counters, live->OwnerLabel.c_str());
    live->Ctx = std::unique_ptr<Context>{new Context(*live->Scope, *pod.Registry, *pod.Bus, &Counters, desc->Meta)};
    live->Ctx->Ledger = pod.Ledger;
    live->Ctx->ConsumerCookie = live->Instance;
    live->Ctx->PodIndex = pod.PodIndex;
    live->Ctx->ConfigStore = store;
    live->Ctx->ConfigMeta = store == nullptr ? nullptr : &info;
    return Result<std::unique_ptr<Pod::LiveInstance>>::Ok(std::move(live));
}
```

（原实现的 `desc->Create()` 失败路径不存在——工厂在镜像内构造，返回 null 只可能是插件自己写错；M1 未防，M2a 维持。）

- [ ] **Step 4: 两调用点适配**——`CreatePodImpl` 循环装配段替换：

```cpp
        Result<std::unique_ptr<Pod::LiveInstance>> built = MakeInstance(pod, *resident.Value(), desc, entry.ResolvedConfig);
        if (!built.IsOk())
        {
            recordFailure(entry.Id, Phase::kLoad, built.GetError().Message());
            recordFailedBinary(entry.Id, resident.Value());
            continue;
        }
        std::unique_ptr<Pod::LiveInstance> live = std::move(built.Value());
```

（`built.Value()` 返回引用、`std::move` 走 once——取两次会拷贝失败；如编译器对 `std::move(built.Value())` 的 const 性有微词，改用 `GetError`/`Value` 的既有用法形态照 PlanFile 里同类搬运的写法。**执行者核对**：`Result<T>::Value()` 有非常量引用重载 ✓（Result.h:78）。）

`AdoptPlugin` 的 ⑥ 装配段（原 `MakeInstance(pod, *record, desc)` 行）替换：

```cpp
    const ConfigBlob replay = [&]() -> ConfigBlob {
        const auto found = slot->Replays.find(id);
        return found == slot->Replays.end() ? ConfigBlob{} : found->second; // D34：查无 = 空 = 全默认
    }();
    Result<std::unique_ptr<Pod::LiveInstance>> built = MakeInstance(pod, *record, desc, replay);
    if (!built.IsOk())
    {
        return Result<AdoptReport>::Err(
            Refusal(id, Phase::kAdopt, "adopt failed at config apply: " + built.GetError().Message()));
    }
    std::unique_ptr<Pod::LiveInstance> live = std::move(built.Value());
```

`CreatePodImpl` 在 T4 的统一注册块里补回放表（成功路径，Adopt 查用）：

```cpp
        slot->Replays[std::string(entry.Id)] = entry.ResolvedConfig;
```

并在槽复用块（`slot->HotSwapLog.clear()` 旁）加 `slot->Replays.clear();`。

- [ ] **Step 5: fixtures**——`M2Common.h` 追加（`IDeadService` 之后同型）：

```cpp
class IConfigEcho // 配置应用判据：把 ctx.Config<T>().Echo 原样吐回来（值 = 计划 blob 的，或被默认）。
{
public:
    static constexpr std::string_view kName = "Vase.Test.ConfigEcho";
    static constexpr std::uint32_t kVersion = 1;
    IConfigEcho() = default;
    IConfigEcho(const IConfigEcho&) = delete;
    IConfigEcho& operator=(const IConfigEcho&) = delete;
    IConfigEcho(IConfigEcho&&) = delete;
    IConfigEcho& operator=(IConfigEcho&&) = delete;
    virtual ~IConfigEcho() = default;
    [[nodiscard]] virtual int Value() const = 0;
};
```

`Tests/Integration/fixtures/ConfigConsumerPlugin/ConfigConsumerPlugin.cpp`：

```cpp
// 配置应用探针（spec 4.1/D32/D34/D35 的现场）：OnLoad 读 ctx.Config<T>() 并把 Echo 注册成服务。
#include "Vase/Plugin.h"

#include "../M2Common.h"

namespace
{

VASE_CONFIG(ConsumerConfig,
    (std::int32_t, Echo, 1, vase::Meta{.Label = "回声",
                                       .Min = vase::Value::From<std::int32_t>(0),
                                       .Max = vase::Value::From<std::int32_t>(10)}));

class EchoImpl final : public m2_fixture::IConfigEcho
{
public:
    explicit EchoImpl(int value) : Stored(value) {} // 成员不带下划线后缀（tidy 命名表）
    [[nodiscard]] int Value() const override { return Stored; }

private:
    int Stored;
};

class ConfigConsumerPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        Echo = std::make_unique<EchoImpl>(ctx.Config<ConsumerConfig>().Echo);
        ctx.Provide<m2_fixture::IConfigEcho>(*Echo);
        return vase::Result<void>::Ok();
    }

private:
    std::unique_ptr<EchoImpl> Echo;
};

} // namespace

VASE_PLUGIN(ConfigConsumerPlugin){
    .Id = "Vase.ConfigConsumer",
    .DisplayName = "配置应用探针",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.ConfigEcho", .Version = 1}},
    .Config = vase::FieldsOf<ConsumerConfig>(),
};
```

`Tests/Integration/fixtures/ConfigLayoutMisusePlugin/ConfigLayoutMisusePlugin.cpp`：

```cpp
// 作者契约反例：读配置用错结构体（尺寸不符）→ ProgrammerError（D25 的布局校验）。
#include "Vase/Plugin.h"

namespace
{

VASE_CONFIG(RealConfig, (std::int32_t, Echo, 1, vase::Meta{}));

struct WrongShape
{
    std::int64_t First = 0;
    std::int64_t Second = 0; // sizeof 与 RealConfig 不等
};

class ConfigLayoutMisusePlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx.Config<WrongShape>()); // 必炸：layout mismatch
        return vase::Result<void>::Ok();
    }
};

} // namespace

VASE_PLUGIN(ConfigLayoutMisusePlugin){
    .Id = "Vase.ConfigLayoutMisuse",
    .DisplayName = "配置布局误用探针",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {},
    .Config = vase::FieldsOf<RealConfig>(),
};
```

fixtures/CMakeLists.txt 追加两 `vase_add_plugin_fixture` 调用（同既有形态，LINK_LIBRARIES VasePod）；Tests/CMakeLists.txt 宏追加 `VASE_FIXTURE_CONFIGCONSUMER`、`VASE_FIXTURE_CONFIGLAYOUTMISUSE`，源列表加 `Integration/ConfigApplyTests.cpp`。

- [ ] **Step 6: 写 `Tests/Integration/ConfigApplyTests.cpp`**

```cpp
#include "Vase/Host/ConfigBlob.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Pod.h"
#include "Vase/Plugin.h" // 服务接口标识（测试侧只借 kName/kVersion 查表，不跨界 new）

#include <filesystem>
#include <gtest/gtest.h>
#include <string_view>

#include "../Integration/fixtures/M2Common.h"

namespace
{

vase::LoadPlan Plan(std::initializer_list<std::pair<std::string_view, std::filesystem::path>> items)
{
    vase::LoadPlan plan;
    for (const auto& [id, path] : items)
    {
        plan.Ordered.push_back({.Id = id, .BinaryPath = path});
    }
    return plan;
}

int EchoValue(vase::PluginHost& host, const vase::PodHandle& h)
{
    vase::Pod* pod = host.Resolve(h);
    return pod->Root().Get<m2_fixture::IConfigEcho>().Value();
}

TEST(ConfigApply, BlobOverrideReachesTypedStruct)
{
    vase::PluginHost host;
    vase::LoadPlan plan = Plan({{"Vase.ConfigConsumer", VASE_FIXTURE_CONFIGCONSUMER}});
    plan.Ordered.begin()->ResolvedConfig.Set("Echo", vase::Value::From<std::int32_t>(7));
    const vase::PodHandle h = host.CreatePod(plan).Value();
    EXPECT_EQ(EchoValue(host, h), 7); // D23：blob 有键用 blob 值
    host.DestroyPod(h);
}

TEST(ConfigApply, EmptyBlobFallsBackToDescriptorDefaults)
{
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(Plan({{"Vase.ConfigConsumer", VASE_FIXTURE_CONFIGCONSUMER}})).Value();
    EXPECT_EQ(EchoValue(host, h), 1); // ConsumerConfig.Echo 的元组默认值
    host.DestroyPod(h);
}

TEST(ConfigApply, KindMismatchIsPluginFailureNotAbort)
{
    // D32：宿主给的数据错 = 可恢复装配失败。float 灌 int32 字段 → Failed 记录，局照开。
    vase::PluginHost host;
    vase::LoadPlan plan = Plan({{"Vase.ConfigConsumer", VASE_FIXTURE_CONFIGCONSUMER}});
    plan.Ordered.begin()->ResolvedConfig.Set("Echo", vase::Value::From<float>(2.5f));
    const vase::PodHandle h = host.CreatePod(plan).Value();
    EXPECT_EQ(host.Resolve(h)->PluginCount(), 0U);
    ASSERT_EQ(host.Resolve(h)->Failures().size(), 1U);
    EXPECT_NE(host.Resolve(h)->Failures().begin()->Message.find("config field Echo"), std::string::npos);
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(ConfigApply, OutOfRangeAppliesAnyway)
{
    // D35 钉「无执法」：Meta 写着 0..10，999 照写不误。执法归 M2b Preset 校验。
    vase::PluginHost host;
    vase::LoadPlan plan = Plan({{"Vase.ConfigConsumer", VASE_FIXTURE_CONFIGCONSUMER}});
    plan.Ordered.begin()->ResolvedConfig.Set("Echo", vase::Value::From<std::int32_t>(999));
    const vase::PodHandle h = host.CreatePod(plan).Value();
    EXPECT_EQ(EchoValue(host, h), 999);
    host.DestroyPod(h);
}

TEST(ConfigApply, LayoutMismatchTerminates)
{
    // D25 读取端防线：sizeof 与 ConfigInfo.StructSize 不符 → ProgrammerError（两构建 abort）。
    EXPECT_DEATH(
        {
            vase::PluginHost host;
            static_cast<void>(host.CreatePod(Plan({{"Vase.ConfigLayoutMisuse", VASE_FIXTURE_CONFIGLAYOUTMISUSE}})));
        },
        "layout mismatch");
    SUCCEED();
}

TEST(ConfigApply, AdoptReplaysPlanBlobAndNewIdGetsDefaults)
{
    // D34：eject+adopt 配置如如不动；从未入局的新 Id 走默认。
    vase::PluginHost host;
    vase::LoadPlan plan = Plan({{"Vase.ConfigConsumer", VASE_FIXTURE_CONFIGCONSUMER}});
    plan.Ordered.begin()->ResolvedConfig.Set("Echo", vase::Value::From<std::int32_t>(7));
    const vase::PodHandle h = host.CreatePod(plan).Value();
    EXPECT_EQ(EchoValue(host, h), 7);
    ASSERT_TRUE(host.EjectPlugin(h, "Vase.ConfigConsumer").IsOk());
    ASSERT_TRUE(host.AdoptPlugin(h, "Vase.ConfigConsumer").IsOk());
    EXPECT_EQ(EchoValue(host, h), 7); // 回放：与 eject 前一致（覆盖没丢）
    EXPECT_TRUE(host.DestroyPod(h).Clean());

    const vase::PodHandle second = host.CreatePod(Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}})).Value();
    ASSERT_TRUE(host.AdoptPlugin(second, "Vase.ConfigConsumer").IsOk()); // KnownBinaries 仍在（Host 级）
    EXPECT_EQ(EchoValue(host, second), 1);                                // 本局计划没它 → 默认层
    host.DestroyPod(second);
}

} // namespace
```

- [ ] **Step 7: Hello 示例形态**（§14：补 `.Config`/`OptionalRequires` 的作者侧证人；Embedding 无需改）

`Samples/HelloPlugin/HelloPlugin.cpp`：类区加

```cpp
VASE_CONFIG(HelloConfig,
    (std::int32_t, Repeats, 1, vase::Meta{.Label = "问候次数"}));
```

`GreeterImpl` 改为携带拥有型文本：

```cpp
class GreeterImpl final : public samples::IGreeter
{
public:
    void SetGreeting(std::string text) { Greeting = std::move(text); }
    [[nodiscard]] std::string_view Greet() const override { return Greeting; }

private:
    std::string Greeting = "hello from Vase.Hello x0"; // OnLoad 立刻被配置真值覆写
};
```

（加 `#include <string>`。）`HelloPlugin::OnLoad` 首行前加：

```cpp
        const auto& cfg = ctx.Config<HelloConfig>();
        Greeter.SetGreeting("hello from Vase.Hello x" + std::to_string(cfg.Repeats));
```

`VASE_PLUGIN` brace 块补：

```cpp
    .OptionalRequires = {}, // 示例形态：显式写出来，作者看得见这槽存在（§3.4）
    .Provides = {{.Name = "Vase.Hello.Greeter", .Version = 1}},
    .Config = vase::FieldsOf<HelloConfig>(),
```

`Samples/HelloPluginPrime/HelloPlugin.cpp` 同型改（文本 `"... prime v2 x"` + Repeats；两版结构体一致是 swapdemo 的前提——Prime 的 `.Version = "0.1.0"` 保持）。改完 `VaseConsole` 的 `prime v2` 正则仍命中（"prime v2 x1"）。

- [ ] **Step 8: 构建 + 全量 + 手工冒烟 + format + 提交**

```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
./build-win/win-x64-clang-debug/bin/VaseEmbedding.exe play   # 期望 "play: IGreeter says hello from Vase.Hello x1"
git add Source/Host/PluginHost.cpp Include/Vase/Host/PluginHost.h Include/Vase/Pod/Pod.h Tests/Integration/fixtures Tests/Integration/ConfigApplyTests.cpp Tests/CMakeLists.txt Samples/HelloPlugin Samples/HelloPluginPrime
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git commit -m "M2a-T6 配置应用与 D34 回放表：blob 优先、缺字段回退、Adopt 如如不动

MakeInstance 换 Result 签名（D32 Kind 不匹配 = Failed）；配置对象镜像内配对工厂
（D25）；PodSlot.Replays 只由计划写、Adopt 查表，新 Id 走默认。Hello/Prime 带上
真实 VASE_CONFIG 作作者侧证人，Embedding play 肉眼可见 x1。"
```

---

## Task 7: `OnStart` 递归拆除（判据 5）+ 四形态波及集 + D42 空壳钉死 + 多插件归零

**Spec:** §4.3（D28）、§4.5 之 Eject 空壳（D42）、§9 的 Lifecycle 行。

**Files:**
- Modify: `Include/Vase/Host/PluginHost.h`（两私有方法声明）、`Source/Host/PluginHost.cpp`（⑤ 阶段 2 整块替换 + 两新函数）
- Create: `Tests/Integration/fixtures/{StartFailProvider,BehindStrictUsed,BehindStrictUnused,BehindOptUsed,BehindOptUnused,BehindFar}Plugin/`、`Tests/Integration/RecursiveTeardownTests.cpp`、`Tests/Lifecycle/MultiPluginCycleTests.cpp`
- Modify: fixtures/CMakeLists.txt、Tests/CMakeLists.txt（6 宏 + 2 源）

**Interfaces:**
- Consumes: T5 的循环骨架与 `SkippedRecord`；`DependencyLedger::EdgesTo`；`PluginMeta::Provides/Requires/OptionalRequires`。
- Produces: `PluginHost::ComputeDownstreamClosure` / `CascadeTeardown`；阶段 2 最终形（T8 的碰撞 Err 复用 Strict 拆除块）。

- [ ] **Step 1: `M2Common.h` 追加两个标识**（同型五处置，字段省略处照 `ICycleA` 形态补全）：

```cpp
class IBehind // Vase.Test.Behind：StartFailProvider 声明并注册，OnStart 必败——拆它的下游。
{
public:
    static constexpr std::string_view kName = "Vase.Test.Behind";
    static constexpr std::uint32_t kVersion = 1;
    virtual ~IBehind() = default;
    // …拷贝/移动五个处置照 ICycleA 逐行补齐…
};

class IBehind2 // 传递链第二跳：由 BehindStrictUsed 提供。
{
public:
    static constexpr std::string_view kName = "Vase.Test.Behind2";
    static constexpr std::uint32_t kVersion = 1;
    virtual ~IBehind2() = default;
    // …同上补齐…
};
```

- [ ] **Step 2: 六个 fixture**（共用形态：`Behind*Impl` 为空实现类，成员持有，OnLoad `Provide` 引用）

`StartFailProviderPlugin.cpp`：

```cpp
// 判据 5 主角：OnLoad 成功注册 Behind（声明与注册一致），OnStart 必败——下游已在跑，只能拆。
#include "Vase/Plugin.h"

#include "../M2Common.h"

namespace
{
class BehindImpl final : public m2_fixture::IBehind {};
class StartFailProviderPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<m2_fixture::IBehind>(Instance);
        return vase::Result<void>::Ok();
    }
    vase::Result<void> OnStart(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Err(vase::Error{"behind failure surfaces at start"});
    }

private:
    BehindImpl Instance;
};
} // namespace

VASE_PLUGIN(StartFailProviderPlugin){
    .Id = "Vase.StartFailProvider",
    .DisplayName = "启动失败的提供者",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.Behind", .Version = 1}},
};
```

`BehindStrictUsedPlugin.cpp`（strict 声明 **且** 解析过 + 传递链中继）：

```cpp
#include "Vase/Plugin.h"

#include "../M2Common.h"

namespace
{
class Behind2Impl final : public m2_fixture::IBehind2 {};
class BehindStrictUsedPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        Consumed = &ctx.Get<m2_fixture::IBehind>(); // 落一条账本边（也是声明边，双保险命中）
        ctx.Provide<m2_fixture::IBehind2>(Second);
        return vase::Result<void>::Ok();
    }

private:
    Behind2Impl Second;
    const m2_fixture::IBehind* Consumed = nullptr; // 只承担消费者身份；指针不外用
};
} // namespace

VASE_PLUGIN(BehindStrictUsedPlugin){
    .Id = "Vase.BehindStrictUsed",
    .DisplayName = "strict 声明且用过",
    .Version = "0.0.1",
    .Requires = {{.Name = "Vase.Test.Behind", .Version = 1}},
    .Provides = {{.Name = "Vase.Test.Behind2", .Version = 1}},
};
```

`BehindStrictUnusedPlugin.cpp`：同文件把 `OnLoad` 换成 `static_cast<void>(ctx); return Ok;`，去掉 `Second`/`Consumed`，Id `Vase.BehindStrictUnused`，`.Requires = {{Vase.Test.Behind,1}}`，`.Provides = {}`，注释「strict 声明但从没解析——契约上它假定必然拿得到，之后任何一次 Get 都会悬空，故仍被波及（D28 保守半边）」。

`BehindOptUsedPlugin.cpp`：`OnLoad` 里 `ctx.TryGet<m2_fixture::IBehind>()` 存成员（结果非空——注册表里有），Id `Vase.BehindOptUsed`，`.OptionalRequires = {{.Name = "Vase.Test.Behind", .Version = 1}}`，注释「optional 声明 + 解析过 → 账本边命中波及（D28：指针已经交出去了）」。

`BehindOptUnusedPlugin.cpp`：`OnLoad` 空操作，Id `Vase.BehindOptUnused`，`.OptionalRequires = {{Vase.Test.Behind,1}}`，注释「只声明没用 → 不波及，留下继续跑；此后 TryGet 得 nullptr 合法（§6.3）」。

`BehindFarPlugin.cpp`：`OnLoad` 里 `ctx.Get<m2_fixture::IBehind2>()`，Id `Vase.BehindFar`，`.Requires = {{Vase.Test.Behind2,1}}`，注释「传递链第二跳：StrictUsed 被拆 → 它被拆」。

fixtures/CMakeLists.txt 追加 6 个 `vase_add_plugin_fixture`（形态同 T5 Step 2 那段），Tests/CMakeLists.txt 追加宏 `VASE_FIXTURE_STARTFAILPROVIDER`、`VASE_FIXTURE_BEHINDSTRICTUSED`、`VASE_FIXTURE_BEHINDSTRICTUNUSED`、`VASE_FIXTURE_BEHINDOPTUSED`、`VASE_FIXTURE_BEHINDOPTUNUSED`、`VASE_FIXTURE_BEHINDFAR` 与源 `Integration/RecursiveTeardownTests.cpp`、`Lifecycle/MultiPluginCycleTests.cpp`。

- [ ] **Step 3: `PluginHost.h` 私有区加两声明**（`MakeInstance` 附近）：

```cpp
    // §5.2 判据 5：OnStart 失败的递归拆除。波及 = strict 声明边 ∪ 账本实边的传递闭包（D28），
    // 闭包在**拆之前**一次算全（拆除会改账本，判定必须看完整初始态）；拆按数组序倒序 = 逆拓扑序。
    [[nodiscard]] static std::vector<Pod::LiveInstance*> ComputeDownstreamClosure(
        Pod& pod, const Pod::LiveInstance& origin);
    void TearDownLiveInstances(Pod& pod, const std::vector<Pod::LiveInstance*>& doomed, std::string_view causedById);
```

- [ ] **Step 4: `PluginHost.cpp` 阶段 2（⑤ 块）整块替换** + 文件尾追加两函数

```cpp
    // ⑤ 阶段 2（§5.3）：仅对已 Loaded 的实例跑 OnStart，仍按数组序；失败 = Failed + 递归拆除（§5.2）。
    for (const std::unique_ptr<Pod::LiveInstance>& holder : pod.Instances)
    {
        Pod::LiveInstance& instance = *holder;
        if (instance.State != Pod::LiveInstance::InstanceState::kLoaded)
        {
            continue; // 可能已被更上游的拆除波及（拆在集合层，循环在实例层，两边以 State 对账）
        }
        const Result<void> started = instance.Instance->OnStart(*instance.Ctx);
        if (!started.IsOk())
        {
            // 先算闭包（此时账本/声明都还是初始态），再拆自己（Failed 语义不变），最后倒序拆波及者。
            const std::vector<Pod::LiveInstance*> doomed = ComputeDownstreamClosure(pod, instance);
            DiscardInstance(pod, instance);
            pod.FailureRecords.push_back(FailedPluginRecord{
                .Id = instance.OwnerLabel,
                .Stage = Phase::kStart,
                .Message = started.GetError().Message(),
            });
            recordFailedBinary(instance.OwnerLabel, instance.Binary);
            TearDownLiveInstances(pod, doomed, instance.OwnerLabel);
            continue;
        }
        instance.State = Pod::LiveInstance::InstanceState::kStarted;
    }
```

```cpp
std::vector<Pod::LiveInstance*> PluginHost::ComputeDownstreamClosure(Pod& pod, const Pod::LiveInstance& origin)
{
    std::vector<Pod::LiveInstance*> closure;
    std::vector<Pod::LiveInstance*> frontier;
    frontier.push_back(const_cast<Pod::LiveInstance*>(&origin));
    while (!frontier.empty())
    {
        Pod::LiveInstance* current = frontier.back();
        frontier.pop_back();
        for (const std::unique_ptr<Pod::LiveInstance>& candidate : pod.Instances)
        {
            Pod::LiveInstance* c = candidate.get();
            if (c->Instance == nullptr || c->State == Pod::LiveInstance::InstanceState::kFailed)
            {
                continue; // 只波及仍活着的
            }
            if (c == &origin || std::find(closure.begin(), closure.end(), c) != closure.end())
            {
                continue; // 去重（含 origin 自己）
            }
            bool ripple = false;
            for (std::size_t r = 0; r < c->Desc->Meta->Requires.Size() && !ripple; ++r)
            {
                const ServiceRef& want = *std::next(c->Desc->Meta->Requires.Begin(), static_cast<std::ptrdiff_t>(r));
                for (std::size_t p = 0; p < current->Desc->Meta->Provides.Size() && !ripple; ++p)
                {
                    const ServiceRef& gives =
                        *std::next(current->Desc->Meta->Provides.Begin(), static_cast<std::ptrdiff_t>(p));
                    ripple = (want.Name == gives.Name && want.Version == gives.Version);
                }
            }
            for (const detail::LedgerEdge* edge : pod.Ledger->EdgesTo(current->Instance))
            {
                ripple = ripple || (edge->Consumer == static_cast<const void*>(c->Instance));
            }
            if (ripple)
            {
                closure.push_back(c);
                frontier.push_back(c);
            }
        }
    }
    return closure;
}

void PluginHost::TearDownLiveInstances(Pod& pod, const std::vector<Pod::LiveInstance*>& doomed,
                                       std::string_view causedById)
{
    // 数组序倒着拆 = 逆拓扑序（数组序 = 拓扑序是 §5.1 的定形契约；M2a 手写计划违序者已被
    // 预检拦在门外，走到这里的集合必然满足）。空壳留在 Instances：与 Failed 空壳同形，
    // DestroyPod 统一收（镜像不解除——中途卸货是 Eject 的事，D42）。
    for (const std::unique_ptr<Pod::LiveInstance>& candidate : std::views::reverse(pod.Instances))
    {
        if (std::find(doomed.begin(), doomed.end(), candidate.get()) == doomed.end() ||
            candidate->Instance == nullptr)
        {
            continue;
        }
        pod.SkipRecords.push_back(SkippedRecord{
            .Id = candidate->OwnerLabel,
            .Class = SkipClass::kRuntime,
            .Cause = "cascade from " + std::string(causedById),
            .CausedBy = std::string(causedById),
        });
        DiscardInstance(pod, *candidate);
    }
}
```

（`DiscardInstance` 会把 `State` 置 `kFailed`——拆除空壳与失败空壳从此在实例层同形，报告层用 Skips/Failures 两张表区分身份；这正是 D42「干净拒绝」不炸的既有原因：Eject ① 只认 `Instance != nullptr`。）

- [ ] **Step 5: 写 `Tests/Integration/RecursiveTeardownTests.cpp`**

```cpp
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Pod.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <string_view>

namespace
{

vase::LoadPlan BehindPlan() // 拓扑序：提供者 → 四下游 → 第二跳。
{
    vase::LoadPlan plan;
    plan.Ordered.push_back({.Id = "Vase.StartFailProvider", .BinaryPath = VASE_FIXTURE_STARTFAILPROVIDER});
    plan.Ordered.push_back({.Id = "Vase.BehindStrictUsed", .BinaryPath = VASE_FIXTURE_BEHINDSTRICTUSED});
    plan.Ordered.push_back({.Id = "Vase.BehindStrictUnused", .BinaryPath = VASE_FIXTURE_BEHINDSTRICTUNUSED});
    plan.Ordered.push_back({.Id = "Vase.BehindOptUsed", .BinaryPath = VASE_FIXTURE_BEHINDOPTUSED});
    plan.Ordered.push_back({.Id = "Vase.BehindOptUnused", .BinaryPath = VASE_FIXTURE_BEHINDOPTUNUSED});
    plan.Ordered.push_back({.Id = "Vase.BehindFar", .BinaryPath = VASE_FIXTURE_BEHINDFAR});
    return plan;
}

const vase::SkippedRecord* FindSkip(const vase::Pod& pod, std::string_view id)
{
    for (const vase::SkippedRecord& record : pod.Skips())
    {
        if (record.Id == id)
        {
            return &record;
        }
    }
    return nullptr;
}

TEST(RecursiveTeardown, FourMorphologiesAndTransitiveChain)
{
    // §5.2 判据 5 / D28 的四形态 + 传递闭包。过不了的读法见 spec §13（epoch 请回来那条）。
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(BehindPlan()).Value();
    const vase::Pod* pod = host.Resolve(h);
    ASSERT_NE(pod, nullptr);

    EXPECT_EQ(pod->PluginCount(), 1U); // 只剩 BehindOptUnused（只声明没用，合法幸存）
    EXPECT_TRUE(pod->HasPlugin("Vase.BehindOptUnused"));
    ASSERT_EQ(pod->Failures().size(), 1U);
    EXPECT_EQ(pod->Failures().begin()->Id, "Vase.StartFailProvider");
    EXPECT_EQ(pod->Failures().begin()->Stage, vase::Phase::kStart);

    ASSERT_EQ(pod->Skips().size(), 4U); // StrictUsed / StrictUnused / OptUsed / Far
    for (std::string_view id : {"Vase.BehindStrictUsed", "Vase.BehindStrictUnused", "Vase.BehindOptUsed",
                                "Vase.BehindFar"})
    {
        const vase::SkippedRecord* record = FindSkip(*pod, id);
        ASSERT_NE(record, nullptr) << id;
        EXPECT_EQ(record->Class, vase::SkipClass::kRuntime);
        EXPECT_EQ(record->CausedBy, "Vase.StartFailProvider"); // 多层都记最初失败者（4.3）
        EXPECT_NE(record->Cause.find("cascade from Vase.StartFailProvider"), std::string::npos);
    }
    EXPECT_TRUE(host.DestroyPod(h).Clean()); // 空壳与驻留镜像全收干净
}

TEST(RecursiveTeardown, TornShellRejectsEjectAndAdoptWithoutCrashing)
{
    // D42：级联空壳（无实例、无入边、镜像在架、不在 FailedBinaries）中途被进出——两边都干净拒绝。
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(BehindPlan()).Value();

    const vase::Result<vase::EjectReport> ejected = host.EjectPlugin(h, "Vase.BehindStrictUnused");
    ASSERT_FALSE(ejected.IsOk()); // 不许解引用 null，也不许当真能卸
    EXPECT_NE(ejected.GetError().Message().find("not in pod"), std::string::npos);

    const vase::Result<vase::AdoptReport> adopted = host.AdoptPlugin(h, "Vase.BehindStrictUnused");
    ASSERT_FALSE(adopted.IsOk()); // OwnerLabel 唯一性覆盖空壳：本局还"记得"它
    EXPECT_NE(adopted.GetError().Message().find("already in pod"), std::string::npos);

    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(RecursiveTeardown, AdoptFailureStaysZeroCascade)
{
    // §5.6 规则②：入局者皆为叶，Adopt 失败零级联——拆的那台机器对这条路径备而不用。
    vase::PluginHost host;
    {
        // 先用一个宽容局把 DeadProvider 的路径注册上（D30；局成功即注册，其 Failed 记录随局销毁）。
        const vase::PodHandle registrar = host.CreatePod(Plan({{"Vase.DeadProvider", VASE_FIXTURE_DEADPROVIDER}})).Value();
        host.DestroyPod(registrar); // 拆局不卸货（§8.1），镜像还在架上
    }
    const vase::PodHandle h = host.CreatePod(Plan({{"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER}})).Value();
    const auto dead = host.AdoptPlugin(h, "Vase.DeadProvider"); // 路径已知 → 走到 OnLoad 必败
    ASSERT_FALSE(dead.IsOk());
    EXPECT_NE(dead.GetError().Message().find("adopt failed at OnLoad"), std::string::npos);
    EXPECT_EQ(host.Resolve(h)->PluginCount(), 1U); // 无人受累
    EXPECT_EQ(host.Resolve(h)->Skips().size(), 0U); // 级联机器没被接线
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

} // namespace
```

（文件顶部 `BehindPlan()` 之前定义与其他测试文件同款的 `Plan(initializer_list<pair<string_view,path>>)` 助手——本文件全部用例走它。）

- [ ] **Step 6: 写 `Tests/Lifecycle/MultiPluginCycleTests.cpp`**

```cpp
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Pod.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <utility>

#include "../Integration/fixtures/SharedCommon.h"

namespace
{

class HostMarker final : public samples_fixture::IHostOnlyService
{
public:
    [[nodiscard]] int Marker() const override { return 1; }
};

vase::LoadPlan ConsumerPlan()
{
    vase::LoadPlan plan;
    plan.Ordered.push_back({.Id = "Vase.SharedProvider", .BinaryPath = VASE_FIXTURE_SHAREDPROVIDER});
    plan.Ordered.push_back({.Id = "Vase.EdgeConsumer", .BinaryPath = VASE_FIXTURE_EDGECONSUMER});
    return plan;
}

TEST(MultiPluginCycle, RepeatedMultiPluginRoundZeroes)
{
    // §9.2 的反复 Play/Stop 判据从单插件升到多插件：每轮差分全零、账本全空。
    vase::PluginHost host;
    HostMarker marker;
    vase::PodOptions options;
    options.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::IHostOnlyService>(marker); };
    for (int round = 0; round < 3; ++round)
    {
        const vase::PodHandle h = host.CreatePod(ConsumerPlan(), options).Value();
        EXPECT_EQ(host.Resolve(h)->PluginCount(), 2U);
        EXPECT_TRUE(host.DestroyPod(h).Clean()) << "round " << round;
        EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U);
    }
}

TEST(MultiPluginCycle, CascadeRoundStillZeroesEverything)
{
    // 判据 5 的局同样要归零：拆除不留半个 Scope、不留一条边、不留一个实例计数。
    vase::PluginHost host;
    for (int round = 0; round < 3; ++round)
    {
        vase::LoadPlan plan;
        plan.Ordered.push_back({.Id = "Vase.StartFailProvider", .BinaryPath = VASE_FIXTURE_STARTFAILPROVIDER});
        plan.Ordered.push_back({.Id = "Vase.BehindStrictUsed", .BinaryPath = VASE_FIXTURE_BEHINDSTRICTUSED});
        plan.Ordered.push_back({.Id = "Vase.BehindFar", .BinaryPath = VASE_FIXTURE_BEHINDFAR});
        const vase::PodHandle h = host.CreatePod(plan).Value();
        EXPECT_EQ(host.Resolve(h)->PluginCount(), 0U); // 全被拆/跳；opt-unused 缺席版
        EXPECT_TRUE(host.DestroyPod(h).Clean()) << "round " << round;
        EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U);
    }
}

} // namespace
```

- [ ] **Step 7: 构建 + 全量 + format + 提交**

```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
git add Include/Vase/Host/PluginHost.h Source/Host/PluginHost.cpp Tests/Integration/fixtures Tests/Integration/RecursiveTeardownTests.cpp Tests/Lifecycle/MultiPluginCycleTests.cpp Tests/CMakeLists.txt
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git commit -m "M2a-T7 OnStart 递归拆除：strict 声明边 ∪ 账本边的闭包，逆数组序拆

判据 5 落地（过不了的读法见 spec 13）。闭包在拆之前对完整初始态算全；四形态用例
（strict 用过/没用、optional 用过/没用）+ transitive 第二跳；D42 空壳进出双拒钉死、
Adopt 失败零级联证明级联机器未接线；Lifecycle 把反复归零升到多插件与级联局。"
```

---

## Task 8: Provides 碰撞执法（装载循环内，D33 ②）

**Spec:** §4.2 的 ②；§5.3 通道表的「计划结构性」行。

**Files:**
- Create: `Tests/Integration/fixtures/CollisionProviderPlugin/`（提供 `Vase.Test.Shared` 的重复者）
- Modify: `Source/Host/PluginHost.cpp`（循环 (a) 记账前插碰撞步）、fixtures/CMakeLists.txt、Tests/CMakeLists.txt（宏）
- Modify: `Tests/Integration/MultiPluginAssemblyTests.cpp`（追加 3 条）

**Interfaces:**
- Consumes: T5 的 `declaredProviders` 与循环骨架；`pod.Registry->Find`。
- Produces: 阶段 1 循环最终形（T9/T10 不再动它）；Err 消息 `provides collision`（新子串族）。

- [ ] **Step 1: fixture `CollisionProviderPlugin.cpp`**（形态照 `SharedProviderPlugin`，服务实现空类）：

```cpp
// 与 SharedProviderPlugin 提供同一个 Vase.Test.Shared：计划结构性碰撞的执法对象（D33 ②）。
#include "Vase/Plugin.h"

#include "../SharedCommon.h"

namespace
{
class CollisionService final : public samples_fixture::ISharedService
{
public:
    [[nodiscard]] int Value() const override { return -1; } // 与正主的 42 区分（本任务不取到它就该被拒）
};
class CollisionProviderPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<samples_fixture::ISharedService>(Instance); // 正常路径根本到不了（碰撞先拒）
        return vase::Result<void>::Ok();
    }

private:
    CollisionService Instance;
};
} // namespace

VASE_PLUGIN(CollisionProviderPlugin){
    .Id = "Vase.CollisionProvider",
    .DisplayName = "重复提供者探针",
    .Version = "0.0.1",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.Shared", .Version = 1}},
};
```

fixtures/CMakeLists.txt 与 Tests/CMakeLists.txt 各加一条（`VASE_FIXTURE_COLLISIONPROVIDER`）。

- [ ] **Step 2: 循环 (a) 之前插入碰撞步**（`declaredProviders.push_back` 的循环整体改为先查后记）：

```cpp
        // (a') Provides 碰撞（D33 ②，M2a 两态形：声明要读镜像才看得见，检出必在 inspect 后）。
        // 查两处：声明登记账（含已倒台者——计划级缺陷不豁免「他者反正失败了」）∪ 注册表（宿主 Stage0 也算）。
        for (std::size_t index = 0; index < desc->Meta->Provides.Size(); ++index)
        {
            const ServiceRef& provided =
                *std::next(desc->Meta->Provides.Begin(), static_cast<std::ptrdiff_t>(index));
            std::string otherProvider;
            for (const DeclaredProvider& declared : declaredProviders)
            {
                if (declared.Service == std::string(provided.Name) && declared.Version == provided.Version &&
                    declared.ProviderId != std::string(entry.Id))
                {
                    otherProvider = declared.ProviderId;
                    break;
                }
            }
            if (otherProvider.empty() &&
                pod.Registry->Find(ServiceKey{.Name = provided.Name, .Version = provided.Version}) != nullptr)
            {
                otherProvider = "host";
            }
            if (!otherProvider.empty())
            {
                // 计划级缺陷：整局 Err，半成品按 Strict 同形拆。镜像驻留残留同 M1 先例（宿主退出收）。
                pod.TeardownInstancesAndRoot();
                slot->Alive = false;
                slot->Inner.reset();
                return Result<PodHandle>::Err(Refusal(entry.Id, Phase::kLoad,
                                                      "plan rejected: provides collision " + std::string(provided.Name) +
                                                          "@" + std::to_string(provided.Version) + " claimed by " +
                                                          otherProvider));
            }
            declaredProviders.push_back(DeclaredProvider{
                .Service = std::string(provided.Name), .Version = provided.Version, .ProviderId = std::string(entry.Id)});
        }
```

（任一碰撞对必在第二个被 inspect 者处检出，与计划顺序无关；第二个若从未轮到（第一个在它之后且被预检跳过）——跳过者不 inspect、不记账，无碰撞可言，正确。）

- [ ] **Step 3: 追加用例（`MultiPluginAssemblyTests.cpp`）**

```cpp
TEST(MultiPluginAssembly, ProvidesCollisionKillsWholePodWithoutRegistering)
{
    // D33 ②：两个 kLoad 条目抢同一服务 → 整局 Err。注册表/实例双清零（镜像驻留残留 = Strict 先例，
    // 计数为 0 即「Pod 视角什么都没发生」）。两个方向各钉一次：检出在第二个被装载者。
    for (bool firstIsShared : {true, false})
    {
        vase::PluginHost host;
        vase::LoadPlan plan = Plan({firstIsShared ? std::pair{"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER}
                                                  : std::pair{"Vase.CollisionProvider", VASE_FIXTURE_COLLISIONPROVIDER},
                                    firstIsShared ? std::pair{"Vase.CollisionProvider", VASE_FIXTURE_COLLISIONPROVIDER}
                                                  : std::pair{"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER}});
        const vase::Result<vase::PodHandle> r = host.CreatePod(plan);
        ASSERT_FALSE(r.IsOk()) << "order firstIsShared=" << firstIsShared;
        EXPECT_NE(r.GetError().Message().find("provides collision Vase.Test.Shared@1"), std::string::npos);
        const auto& counters = host.ForTestCounters();
        EXPECT_EQ(counters.PluginInstances, 0U); // 被拆干净：实例层零残留
        EXPECT_EQ(counters.Services, 0U);
        EXPECT_EQ(counters.Effects, 0U);
        EXPECT_EQ(counters.Scopes, 0U);
    }
}

TEST(MultiPluginAssembly, HostProvidedServiceIsAlsoCollisionTarget)
{
    // 碰撞登记账 ∪ 注册表双查的后半：宿主 Stage0 提供的同名服务也是碰撞对象。
    struct SharedMarker final : public samples_fixture::ISharedService
    {
        [[nodiscard]] int Value() const override { return 0; }
    };

    vase::PluginHost host;
    SharedMarker marker; // 借用注册：对象必须活过整局（§5.3 阶段 0 的既有契约）
    vase::PodOptions options;
    options.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::ISharedService>(marker); };
    const vase::Result<vase::PodHandle> r =
        host.CreatePod(Plan({{"Vase.CollisionProvider", VASE_FIXTURE_COLLISIONPROVIDER}}), options);
    ASSERT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("claimed by host"), std::string::npos);
    EXPECT_EQ(host.ForTestCounters().Services, 0U); // 半局的宿主注册随根 Scope 一起拆净
}
```

- [ ] **Step 4: 构建 + 全量 + format + 提交**

```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
git add Source/Host/PluginHost.cpp Tests/Integration/fixtures/CollisionProviderPlugin Tests/Integration/MultiPluginAssemblyTests.cpp Tests/Integration/fixtures/CMakeLists.txt Tests/CMakeLists.txt
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git commit -m "M2a-T8 Provides 碰撞执法（装载循环内检出，M2a 两态形）

duplicate-Provide 的运行期终止从此不可达于 CreatePod 路径——兑现 Context.h
「提前到硬拒」的装配层一半。声明登记账与注册表双查（宿主服务也是碰撞对象）；
已知失明面（EnsureResident 失败者的 provides 不在账上）在循环注释挂账，随 M2b 清单闭合。"
```

---

## Task 9: `EjectReport` 结构化（拒绝点名 + 拆除边清单）+ 迁移 `EjectTests`

**Spec:** §5.1/§5.2（D20/D21 的 Eject 半）。

**Files:**
- Modify: `Include/Vase/Host/Evidence.h`（`LedgerEdgeRef`/`EjectStatus`/新字段）、`Source/Host/PluginHost.cpp`（`EjectPlugin` 拒绝与拆除两段）、`Tests/HotSwap/EjectTests.cpp`（`RefusedWithConsumersNamed` 改写）
- Create: `Tests/Integration/fixtures/SharedConsumer2Plugin/`（第二消费者，点名要成清单）；fixtures/Tests CMakeLists 各一条

**Interfaces:**
- Consumes: `DependencyLedger::EdgesTo/EdgesFrom`；`LedgerEdge`（借字段拷入报告）。
- Produces: `vase::LedgerEdgeRef{ConsumerId,ProviderId,Service,Version}`、`EjectStatus::{kRejectedConsumers,kEjected}`（默认 `kRejectedConsumers`）、`EjectReport::{Status,Consumers,RemovedEdges}`。T10 复用 `LedgerEdgeRef`。

- [ ] **Step 1: `Evidence.h`**——文件顶部加类型，`EjectReport` 首两行后插入字段：

```cpp
struct LedgerEdgeRef // 账本边的一次性快照：报告是返回值，必须拥有（LedgerEdge 的 string_view 不外带）。
{
    std::string ConsumerId;
    std::string ProviderId;
    std::string Service;
    std::uint32_t Version = 0;
};

inline LedgerEdgeRef SnapshotEdge(const vase::detail::LedgerEdge& edge) // 依赖 DependencyLedger.h include
{
    return LedgerEdgeRef{
        .ConsumerId = std::string(edge.ConsumerId),
        .ProviderId = std::string(edge.ProviderId),
        .Service = std::string(edge.ServiceName),
        .Version = edge.ServiceVersion,
    };
}
```

```cpp
enum class EjectStatus : std::uint8_t { kRejectedConsumers, kEjected };

struct EjectReport
{
    std::string PluginId;
    EjectStatus Status = EjectStatus::kRejectedConsumers; // 默认悲观：成功态必须被显式置上（spec 5.2）
    std::vector<LedgerEdgeRef> Consumers;    // kRejectedConsumers：入边逐条点名（3b 兑现，D21）
    std::vector<LedgerEdgeRef> RemovedEdges; // kEjected：拆除时账上双向边清单（「解析记录」Eject 侧）
    // …既有三档字段与 ProcessStatesReset / HotSwapNote 原样…
};
```

（`Evidence.h` 需 `#include "Vase/Pod/DependencyLedger.h"`——VaseHost 内合法（Host→Pod 单向）。）

- [ ] **Step 2: `EjectPlugin` 两段改**

2a. 拒绝分支（现构造 `consumers` 文本后 `Err` 的那段）整体替换为结构化（**消息文本保留原样**——console 的 `eject refused:` 前缀习惯依赖它，D42 之后 Err 只剩 not-in-pod 一类，拒绝文本从此只为 Err 兼容留？不：D21 已定执法走 Ok。执行以下形态，文本搬进字段、消息不再由 Err 携带）：

```cpp
        const std::vector<const detail::LedgerEdge*> incoming = pod.Ledger->EdgesTo(live->Instance);
        if (!incoming.empty())
        {
            // §5.6 规则③的 3b 兑现（D21）：执法性拒绝 = Ok + Status + 逐条点名；Err 通道留给误用。
            EjectReport rejected;
            rejected.PluginId = id;
            rejected.Status = EjectStatus::kRejectedConsumers;
            for (const detail::LedgerEdge* edge : incoming)
            {
                rejected.Consumers.push_back(SnapshotEdge(*edge));
            }
            // 不进 HotSwapLog：日志语义维持「真实进出才记名」，M1 的 Err 拒绝本就不记——
            // console 的 hotswap log (N) 正则因此零扰动（计划「出入记录」第 2 条）。
            return Result<EjectReport>::Ok(std::move(rejected));
        }
```

2b. 活实例拆除支：在 `pod.Ledger->RemoveByInstance(live->Instance);` **之前**取快照，`report.Status = kEjected;` 与 `report.RemovedEdges` 填入（出边 = `EdgesFrom`，入边此处必空——上面已拒）：

```cpp
        binary = live->Binary;
        for (const detail::LedgerEdge* edge : pod.Ledger->EdgesFrom(live->Instance))
        {
            report.RemovedEdges.push_back(SnapshotEdge(*edge));
        }
        pod.Ledger->RemoveByInstance(live->Instance);
```

（①' Failed-record 支：`Status = kEjected` + `RemovedEdges` 空 + 原 note——无实例必无边。）

- [ ] **Step 3: fixture `SharedConsumer2Plugin.cpp`**：照 `BehindOptUsed` 形态，`.Requires = {{.Name = "Vase.Test.Shared", .Version = 1}}`，OnLoad `ctx.Get<samples_fixture::ISharedService>()` 存指针，Id `Vase.SharedConsumer2`。（宏 `VASE_FIXTURE_SHAREDCONSUMER2`。）

- [ ] **Step 4: 迁移 `EjectTests.cpp` 的 `RefusedWithConsumersNamed`（子串→字段，M1「子串是契约」对本族作废）**

```cpp
TEST(Eject, RefusedWithConsumersNamedInStructuredReport)
{
    // 3b 的 M2a 兑现（D21）：执法拒绝走 Ok + Status + 逐条点名，两个消费者都要出现。
    vase::PluginHost host;
    vase::LoadPlan plan;
    plan.Ordered.push_back({.Id = "Vase.SharedProvider", .BinaryPath = VASE_FIXTURE_SHAREDPROVIDER});
    plan.Ordered.push_back({.Id = "Vase.EdgeConsumer", .BinaryPath = VASE_FIXTURE_EDGECONSUMER});
    plan.Ordered.push_back({.Id = "Vase.SharedConsumer2", .BinaryPath = VASE_FIXTURE_SHAREDCONSUMER2});
    vase::PodOptions options;
    HostMarker marker;
    options.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::IHostOnlyService>(marker); };
    const vase::PodHandle h = host.CreatePod(plan, options).Value();

    const vase::Result<vase::EjectReport> r = host.EjectPlugin(h, "Vase.SharedProvider");
    ASSERT_TRUE(r.IsOk()); // 执法拒绝从此不是 Err（D21）；bad handle / not-in-pod 那族留在 Err
    const vase::EjectReport& report = r.Value();
    EXPECT_EQ(report.Status, vase::EjectStatus::kRejectedConsumers);
    ASSERT_EQ(report.Consumers.size(), 2U);
    bool seenEdge = false;
    bool seenSecond = false;
    for (const vase::LedgerEdgeRef& consumer : report.Consumers)
    {
        EXPECT_EQ(consumer.ProviderId, "Vase.SharedProvider");
        EXPECT_EQ(consumer.Service, "Vase.Test.Shared");
        EXPECT_EQ(consumer.Version, 1U);
        seenEdge = seenEdge || consumer.ConsumerId == "Vase.EdgeConsumer";
        seenSecond = seenSecond || consumer.ConsumerId == "Vase.SharedConsumer2";
    }
    EXPECT_TRUE(seenEdge);
    EXPECT_TRUE(seenSecond);
    EXPECT_EQ(host.Resolve(h)->PluginCount(), 3U); // 被拒 = 什么都没发生
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}
```

同文件加一条 RemovedEdges 判据：

```cpp
TEST(Eject, RemovedEdgesSnapshotRecordsLedgerTruth)
{
    // 「解析记录」Eject 侧：拆 EdgeConsumer，出边（Edge→Shared）逐条进报告。
    vase::PluginHost host;
    vase::LoadPlan plan;
    plan.Ordered.push_back({.Id = "Vase.SharedProvider", .BinaryPath = VASE_FIXTURE_SHAREDPROVIDER});
    plan.Ordered.push_back({.Id = "Vase.EdgeConsumer", .BinaryPath = VASE_FIXTURE_EDGECONSUMER});
    vase::PodOptions options;
    HostMarker marker;
    options.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::IHostOnlyService>(marker); };
    const vase::PodHandle h = host.CreatePod(plan, options).Value();
    const vase::Result<vase::EjectReport> r = host.EjectPlugin(h, "Vase.EdgeConsumer");
    ASSERT_TRUE(r.IsOk());
    EXPECT_EQ(r.Value().Status, vase::EjectStatus::kEjected);
    ASSERT_EQ(r.Value().RemovedEdges.size(), 1U);
    EXPECT_EQ(r.Value().RemovedEdges.begin()->ConsumerId, "Vase.EdgeConsumer");
    EXPECT_EQ(r.Value().RemovedEdges.begin()->ProviderId, "Vase.SharedProvider");
    host.DestroyPod(h);
}
```

`UnknownOrStaleRejected` **不改**（not-in-pod / stale handle 仍走 Err——通道边界见 D21/§5.3）。

- [ ] **Step 5: 构建 + 全量 + format + 提交**

```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
git add Include/Vase/Host/Evidence.h Source/Host/PluginHost.cpp Tests/HotSwap/EjectTests.cpp Tests/Integration/fixtures/SharedConsumer2Plugin Tests/Integration/fixtures/CMakeLists.txt Tests/CMakeLists.txt
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git commit -m "M2a-T9 EjectReport 结构化：拒绝点名 + 拆除边清单（3b 薄面 Eject 侧）

执法拒绝走 Ok+Status（D21），Consumers 逐条可枚举；RemovedEdges 钉「解析记录」。
EjectTests 的点名用例从消息子串迁移到字段断言——M1「子串是契约」对本族作废，
not-in-pod 一族原样留 Err。"
```

---

## Task 10: `AdoptReport` 结构化 + D43 双向执法 + VaseConsole 诚实化

**Spec:** §5.2/§5.3（D21 的 Adopt 半、D43）、§4.5。

**Files:**
- Modify: `Include/Vase/Host/Evidence.h`（`RequirementRef`/`CollisionRef`/`AdoptStatus`/新字段）、`Source/Host/PluginHost.cpp`（`AdoptPlugin` ③' 碰撞步 + ⑤ Missing 结构化 + Outgoing 快照）、`Tests/HotSwap/AdoptTests.cpp`（新用例）
- Modify: `Tools/VaseConsole/Console.cpp`（Eject/Adopt/swap 三处 Status 检查——正确性适配，非功能扩展）

**Interfaces:**
- Consumes: T9 的 `LedgerEdgeRef`/`SnapshotEdge`；`ServiceEntry::ProviderId`（Detail/RegistryBus.h，借用串拷入报告）。
- Produces: `AdoptStatus::{kRejectedDependencies,kRejectedCollision,kAdopted}`（默认悲观）、`AdoptReport::{Status,Missing,Collisions,Outgoing}`。

- [ ] **Step 1: `Evidence.h`**——`AdoptReport` 前加类型：

```cpp
struct RequirementRef // 拥有型需求引用。描述符侧 ServiceRef 是 string_view 借用，报告活不过镜像驻留期。
{
    std::string Service;
    std::uint32_t Version = 0;
};

struct CollisionRef // D43：碰撞报告要多一个现提供者（与 RequirementRef 的差别仅此）。
{
    std::string   Service;
    std::uint32_t Version = 0;
    std::string   ProvidedBy; // 活集合中当前提供者 Id；宿主提供时为 "host"
};

enum class AdoptStatus : std::uint8_t { kRejectedDependencies, kRejectedCollision, kAdopted };
```

`AdoptReport` 字段区（`PluginId` 后）加：

```cpp
    AdoptStatus Status = AdoptStatus::kRejectedDependencies; // 默认悲观：成功态必须显式置上
    std::vector<RequirementRef> Missing;    // kRejectedDependencies：缺哪条（§5.6③「报告缺哪条」兑现）
    std::vector<CollisionRef> Collisions;   // kRejectedCollision：碰撞服务 + 现提供者（D43）
    std::vector<LedgerEdgeRef> Outgoing;    // kAdopted：本次入局新落账边清单；OutgoingEdges = .size()
```

- [ ] **Step 2: `AdoptPlugin` 三处改**

2a. ③ Inspect 之后、④ 之前插碰撞步：

```cpp
    // ③' Provides 不得与活集合已注册服务相碰（D43）：不做这步，错 Adopt 会走到 ⑥ 的
    // duplicate-Provide **终止**——§5.6「任一步失败 → X 干净退出」在此路径不成立。
    std::vector<CollisionRef> collisions;
    for (std::size_t index = 0; index < desc->Meta->Provides.Size(); ++index)
    {
        const ServiceRef& provided =
            *std::next(desc->Meta->Provides.Begin(), static_cast<std::ptrdiff_t>(index));
        const detail::ServiceEntry* taken =
            pod.Registry->Find(ServiceKey{.Name = provided.Name, .Version = provided.Version});
        if (taken != nullptr)
        {
            collisions.push_back(CollisionRef{
                .Service = std::string(provided.Name),
                .Version = provided.Version,
                .ProvidedBy = taken->ProviderId.empty() ? std::string{"host"} : std::string(taken->ProviderId),
            });
        }
    }
    if (!collisions.empty())
    {
        AdoptReport rejected; // 执法性拒绝走 Ok + Status（D21）；不进 HotSwapLog——没有进，就没有出入事件
        rejected.PluginId = id;
        rejected.Status = AdoptStatus::kRejectedCollision;
        rejected.Collisions = std::move(collisions);
        return Result<AdoptReport>::Ok(std::move(rejected));
    }
```

2b. ⑤ 声明全绑的 `unresolved` 字符串改结构化（判据子串 `unresolved declarations` 从此不再需要——**先 grep 确认无测试/无 console 正则钉它**，实测两空；宿主消息文本习惯在 Step 4 重建）：

```cpp
    std::vector<RequirementRef> missing;
    for (std::size_t index = 0; index < desc->Meta->Requires.Size(); ++index)
    {
        const ServiceRef& required =
            *std::next(desc->Meta->Requires.Begin(), static_cast<std::ptrdiff_t>(index));
        if (pod.Registry->Find(ServiceKey{.Name = required.Name, .Version = required.Version}) == nullptr)
        {
            missing.push_back(RequirementRef{.Service = std::string(required.Name), .Version = required.Version});
        }
    }
    if (!missing.empty())
    {
        AdoptReport rejected;
        rejected.PluginId = id;
        rejected.Status = AdoptStatus::kRejectedDependencies;
        rejected.Missing = std::move(missing);
        return Result<AdoptReport>::Ok(std::move(rejected));
    }
```

（optional 不进这个检查——「缺 optional ≠ 依赖不齐」（§6.3），Enforcement 只咬 `Requires`。
至此执法两半都已落地，把 `Context.h` 类头注释第 6 行的「M2 把它提前到求解期硬拒」**改写**为
「M2a 已把它提前到装配层硬拒（CreatePod 结构性校验 + Adopt 双向执法），求解期那一半随 M2b 清单到位」。
`AdoptPlugin` 头注释里「判定子串是契约」的清单同步改为：未知 Id / 身份不符 / 特征缺失 / 兄弟导入 / already in pod
（`unresolved declarations` 从清单划掉——它已进报告字段。））

2c. 成功报告段：

```cpp
    report.Status = AdoptStatus::kAdopted;
    for (const detail::LedgerEdge* edge : pod.Ledger->EdgesFrom(live->Instance))
    {
        report.Outgoing.push_back(SnapshotEdge(*edge));
    }
    report.OutgoingEdges = report.Outgoing.size(); // 计数保留（= .size()，向后自洽）
```

- [ ] **Step 3: `AdoptTests.cpp` 追加一条字段断言用例**（文件头的 `Plan`/`HostMarker` 复用；先读 1–30 行确认二者可见）：

```cpp
TEST(Adopt, StructuredRefusalsAndOutgoingRecord)
{
    // D21/D43/解析记录 Adopt 侧：三类判定各走各的通道，字段可枚举报。
    // 前置：EdgeConsumer / CollisionProvider 的路径都要先在某个成功局里注册过（D30）——用 kSkip 白拿注册。
    vase::PluginHost host;
    vase::LoadPlan barePlan = Plan({{"Vase.Hello", VASE_FIXTURE_HELLO}});
    barePlan.Ordered.push_back({.Id = "Vase.EdgeConsumer",
                                .BinaryPath = VASE_FIXTURE_EDGECONSUMER,
                                .Decision = vase::LoadDecision::kSkip});
    barePlan.Ordered.push_back({.Id = "Vase.CollisionProvider",
                                .BinaryPath = VASE_FIXTURE_COLLISIONPROVIDER,
                                .Decision = vase::LoadDecision::kSkip});
    const vase::PodHandle bare = host.CreatePod(barePlan).Value();

    const vase::Result<vase::AdoptReport> deps = host.AdoptPlugin(bare, "Vase.EdgeConsumer");
    ASSERT_TRUE(deps.IsOk()); // 执法拒绝不是 Err（D21）
    EXPECT_EQ(deps.Value().Status, vase::AdoptStatus::kRejectedDependencies);
    EXPECT_EQ(deps.Value().Missing.size(), 2U); // 无 Stage0：Test.Shared 与 Test.HostOnly 都没注册
    host.DestroyPod(bare);

    const vase::PodHandle withShared =
        host.CreatePod(Plan({{"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER}})).Value();
    const vase::Result<vase::AdoptReport> collision = host.AdoptPlugin(withShared, "Vase.CollisionProvider");
    ASSERT_TRUE(collision.IsOk());
    EXPECT_EQ(collision.Value().Status, vase::AdoptStatus::kRejectedCollision);
    ASSERT_EQ(collision.Value().Collisions.size(), 1U);
    EXPECT_EQ(collision.Value().Collisions.begin()->Service, "Vase.Test.Shared");
    EXPECT_EQ(collision.Value().Collisions.begin()->ProvidedBy, "Vase.SharedProvider");
    host.DestroyPod(withShared);

    HostMarker marker;
    vase::PodOptions fullOptions;
    fullOptions.Stage0 = [&](vase::Context& root) { root.Provide<samples_fixture::IHostOnlyService>(marker); };
    const vase::PodHandle full =
        host.CreatePod(Plan({{"Vase.SharedProvider", VASE_FIXTURE_SHAREDPROVIDER}}), fullOptions).Value();
    const vase::Result<vase::AdoptReport> ok = host.AdoptPlugin(full, "Vase.EdgeConsumer");
    ASSERT_TRUE(ok.IsOk());
    EXPECT_EQ(ok.Value().Status, vase::AdoptStatus::kAdopted);
    ASSERT_EQ(ok.Value().Outgoing.size(), 1U); // Edge→Shared 一条（宿主提供方不落边，§5.6）
    EXPECT_EQ(ok.Value().OutgoingEdges, 1U);
    host.DestroyPod(full);
}
```

- [ ] **Step 4: VaseConsole 诚实化（正确性适配——执法拒绝已不是 Err，不看 Status 会把拒绝报成通过）**

`Console.cpp` 匿名/成员区加两个小 helper（与 `BoolText` 同处）：

```cpp
std::string JoinConsumerIds(const std::vector<vase::LedgerEdgeRef>& refs)
{
    std::string text;
    for (const vase::LedgerEdgeRef& ref : refs)
    {
        if (!text.empty())
        {
            text.append(", ");
        }
        text.append(ref.ConsumerId);
    }
    return text;
}

std::string JoinRequirements(const std::vector<vase::RequirementRef>& refs) // "name@v, name@v"
{
    std::string text;
    for (const vase::RequirementRef& ref : refs)
    {
        if (!text.empty())
        {
            text.append(", ");
        }
        text.append(ref.Service).append("@").append(std::to_string(ref.Version));
    }
    return text;
}
```

`CmdEject` 在 `PrintEjectReport(out, result.Value());` 之前插（文本沿用 M1 的 `eject refused: …is provided by [ … ]` 习惯——console 侧人读格式是既有约定，字段是新的、话术不必变）：

```cpp
    if (result.Value().Status == vase::EjectStatus::kRejectedConsumers)
    {
        out << "eject refused: " << *args.begin() << " is provided by ["
            << JoinConsumerIds(result.Value().Consumers) << "]\n";
        MarkFailed();
        return;
    }
```

`CmdAdopt` 在 `PrintAdoptReport` 前插两分支（同理沿用 M1 前缀话术）：

```cpp
    if (result.Value().Status == vase::AdoptStatus::kRejectedDependencies)
    {
        out << "adopt refused: unresolved declarations [" << JoinRequirements(result.Value().Missing) << "]\n";
        MarkFailed();
        return;
    }
    if (result.Value().Status == vase::AdoptStatus::kRejectedCollision)
    {
        out << "adopt refused: provides collision [" << JoinRequirementsForCollision(result.Value().Collisions)
            << "]\n";
        MarkFailed();
        return;
    }
```

（`JoinRequirementsForCollision` = 上者加 `" by " + ProvidedBy` 尾。）`CmdSwap` 循环体内 eject/adopt 两处 `Print*Report` 前做同样检查：命中 → 打行、`MarkFailed()`、`return`（保持 M1「交换循环遇拒绝即停」的形状）。`PrintAdoptReport` 行尾追加 `<< " outgoing=" << std::to_string(report.OutgoingEdges)`（既有 regex 是前缀匹配，追加安全）。`Tools/VaseConsole/CMakeLists.txt` 的 21 条回放用例**一条不改**——它们的流程全部是叶进叶出，不触发新拒绝路径；若 T11 跑出任何一条变红，即为回归，停并查。

- [ ] **Step 5: 构建 + 全量（含 console 套件）+ format + 提交**

```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
git add Include/Vase/Host/Evidence.h Source/Host/PluginHost.cpp Tests/HotSwap/AdoptTests.cpp Tools/VaseConsole/Console.cpp
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git commit -m "M2a-T10 AdoptReport 结构化 + D43 双向执法 + console 诚实化

声明不齐/Provides 碰撞两类执法走 Ok+Status，环境身份类照旧 Err（§5.3 边界进头注释）；
Outgoing 快照兑现解析记录 Adopt 侧。VaseConsole 的 adopt/eject/swap 三处改按 Status
判失败（执法拒绝已不是 Err，不看 Status 会把拒绝报成通过），21 条回放用例零改动。"
```

---

## Task 11: 六线全矩阵、tidy/format、双平台 HotSwap 证据、基数重测与文档波

**Spec:** §10 验收全部、§11 文档同步义务。**本任务不写新代码**——跑、数、写文档、提交。

**Files:** Modify `CLAUDE.md`、`wiki/vase-architecture.md`（勘误注记）、`.claude/skills/vase-cpp-engineering/`（若 Step 6 判定需要）；`Tools/VaseConsole` 与 `Samples` 无改动（跑通即证）。

- [ ] **Step 1: 六 preset 删树全量**（两串脚本，前提 `VCPKG_ROOT` 已设）：

```bash
cmd //c Scripts/win-verify.cmd                                  # 四棵 Windows 树：删树→configure→build→ctest→ -N
wsl -d Ubuntu -- bash -lc 'bash /mnt/d/Git/Vase/Scripts/linux-verify.sh'
```

逐树记录退出码与 `Total Tests`。`win-x64-clang-release` 若撞 `z-applocal` 文件锁假红：**先重跑该线再判**（CLAUDE.md 实测记录）。

- [ ] **Step 2: 三条 tidy debug 线**（各自跑、各自读正文三判据）：

```bash
Scripts\win-clang-tidy.cmd clangcl
Scripts\win-clang-tidy.cmd msvc
wsl -d Ubuntu -- bash -lc 'bash /mnt/d/Git/Vase/Scripts/linux-clang-tidy.sh'
```

判据 = 脚本 stdout 的「退出码 + 正文 error: 0 + 正文 warning: 0」。**TU 数不做预测**——新增 14 个 fixture 源 + `ConfigBlob.cpp` + 若干测试源都会进编译数据库（`run-clang-tidy` 按路径去重的既有规则照算），实测值记进 Step 5 的基数表。新文件（Config 头群、fixture 群）命中任何正文 diagnostic 都要按「有没有代码级出路」逐处处置，确无出路的就地 NOLINT + 一句为什么（≤1 行）；`VASE_CONFIG` 展开面是本波 tidy 风险最高点（探针任务的 static_assert 群不豁免宏体诊断）。

- [ ] **Step 3: HotSwap 全选择子、双平台各自留证据**（规矩 6；本波改了账本报告/Eject/Adopt/描述符/HeaderVersion 四类）：

```bash
ctest --preset win-x64-clang-debug -R 'HotSwap|Eject|Adopt'
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && ctest --preset linux-x64-clang-debug -R "HotSwap|Eject|Adopt"'
```

两条都读**全量输出正文**（漏注册看 `Total Tests` 对数），贴进验收记录。

- [ ] **Step 4: format 门禁全仓 + 手工冒烟**：

```bash
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' \
  | xargs -0 clang-format --dry-run --Werror
./build-win/win-x64-clang-debug/bin/VaseEmbedding.exe play
./build-win/win-x64-clang-debug/bin/VaseEmbedding.exe loop 3
./build-win/win-x64-clang-debug/bin/VaseEmbedding.exe swapdemo
./build-win/win-x64-clang-debug/bin/VaseConsole.exe --script build-win/win-x64-clang-debug/Tools/VaseConsole/replay/…（或直接 ctest -R VaseConsole）
```

期望：play 打印 `x1`（配置默认值肉眼可见）；console 全套退出码 0。输出截验收记录。

- [ ] **Step 5: 基数重测写回 `CLAUDE.md`**（**按线、实测、逐位**——数值以本 Step 输出为准，执行者把下面表格里的 `<…>` 全部换成实测数）：
  - 「各线 `ctest -N` 基数」表：四值更新 + 差值成因复核（新死 test `ConfigApply.LayoutMismatchTerminates` 与既有 T3 的 gate 形态核对：它走 `EXPECT_DEATH` **不带** `#ifndef NDEBUG` 门 → debug/release 同计，不破坏 −1 差值；新 fixture 群无一 Linux-only → +2 差值不变。任何一条与预期不符，停下向需求方报差值成因）。
  - 「静态检查与格式」与「核门禁」两节的 TU 数、Suppressed 合计、NOLINT 命中数三线各自更新（Linux 线多 `NoBuildIdPlugin` 一个 TU 的既有 +1 关系不变）。
  - 「项目状态」节：M2a 完成定义一句话 + M2 两波进度；「M1 已落地最小形」处补一句「LoadPlan 已定形（D12 手写形的过渡结束），Catalog/Solve 归 M2b」。
  - 规矩 6 引言里「判据子串」的表述改为：「`unknown plugin id` / `already in pod` / `not in pod` / 身份类子串仍是契约；`provided by [ … ]` 与 `unresolved declarations` 两族自 M2a 起改按 EjectReport/AdoptReport 字段断言」。
- [ ] **Step 6: wiki 勘误四处 + 技能同步判定**：

```bash
grep -rn "Total Tests\|[0-9][0-9] TU\|DEBUG:FULL\|build-id=sha1\|EHs-c-\|HAS_EXCEPTIONS\|--cached --others\|WarningsAsErrors\|PRE_TEST\|ctest --preset\|run-clang-tidy -p\|cmake --build --preset" \
  .claude/skills/vase-cpp-engineering/ | grep -v cpp-core-guidelines.md
```

命中逐条判类别（规矩 7 表）。wiki 挂四处修订注记（不改史，条目式）：§5.1 `LoadPlan.Entry` 实际形状含 `BinaryPath` 与缺省回退语义；§5.6 报告 Status 结构化（执法 Ok / 环境 Err 的 D21 边界）；§3.2 配置类型 M2a 六型、`enum` 与 choices schema 同定 M2b；§10 树增 `Include/Vase/Config/`（header-only，无 Source 对称实体）。技能 `references/abi-boundary.md` 或 `plugin-lifecycle.md` 各加一句「图执法 Ok、环境 Err 的通道边界（M2a）」——**不带基数与命令副本**。
- [ ] **Step 7: 分支收尾**：按 `superpowers:finishing-a-development-branch` 与需求方过合入（squash 惯例沿 M1：细粒度历史压一条「M2a 装配地基：…」；推送前先问——main 有 force-push 分叉史）。
- [ ] **Step 8: 文档提交**

```bash
git add CLAUDE.md wiki/vase-architecture.md .claude/skills/vase-cpp-engineering
git commit -m "M2a-T11 文档同步：基数按线重测、wiki 四处勘误、技能通道边界一句"
```

---

## 与 spec 的出入记录（执行者无需再决策，均已有主）

1. D33/D27 的「加载前查声明」在 M2a 不成立（声明住二进制里），已在 spec 内改为两态形；T8/T5 按两态形实现，M2b 清单到位后回收提前形态。
2. 执法拒绝**不进** `HotSwapLog`（T9/T10）：日志语义维持「真实进出才记名」，M1 的 Err 拒绝本就不记；console `hotswap log (6)` 正则因此零扰动。
3. `RequiresMissingConsumer` 的运行期终止消息（`ReportMissingRequiredService`）在 CreatePod 路径变为不可达——墙仍在（Adopt⑤ 与运行中违约），D37 注记写明；对应 M1 死 test 按 T5 Step 4 改写。
4. 各新测试文件顶部的 `Plan`/`HostMarker` 助手**各自匿名命名空间内定义**（仓库既有形态就是每文件一份）；`EjectTests.cpp`/`AdoptTests.cpp` 用文件里已有的那套，不新增第二份。
5. `Context.h` 注释与 `AdoptPlugin` 头注释的「子串契约」改写统一落在 T10（两半执法都到位的那一刻），T3 不动——承诺不先于兑现。

## 实施期勘误（计划文本被真实工具链证伪处，执行中陆续落账；spec 为准）

- 勘误 1（T3，R3-1）：`OptionalRequires` 声明需带 NSDMI `{}`——`-Wmissing-designated-field-initializers`（clang 默认开、/WX 升 error）使「中间插字段、既有 designated 站点省略」当场编译失败。计划 T3 Step 1 代码块按原文不含 `{}`，实现已加。
- 勘误 2（T4，R4-1）：同一警告对**省略的尾字段**同样触发（只要无 NSDMI），故 `LoadPlanEntry::ResolvedConfig` 落地形为 `ConfigBlob ResolvedConfig = {};`；控制器此前「尾插安全」判断有误——**本仓库规则修正为：聚合体新增字段一律自带默认值**。
- 勘误 3（T5，R5-2）：§4.2「M2b 前的计划盘点」漏了 `LedgerSemantics.FailedConsumerDropsItsEdgesImmediately` 一条——T5 BLOCKED 轮实测浮出（删 (b) 支该用例转红，判据力在失败即摘边）；R5-2 已以 `FailingEdgeConsumer` 恢复判据，盘点漏账在此补齐，计划史实以本条为全。
- 后续任务 brief 若与本节冲突，以磁盘已落地形态为准。

## Self-Review 检查单（写完本计划后已跑）

- [x] spec §3–§12 每一节都有任务承接：§3.1→T4，§3.2→T1/T3/T6，§3.3→T2，§3.4→T3，§3.5→T1（头注释记账），§4.1→T6，§4.2→T4/T5/T8，§4.3→T7，§4.4/§4.5→T7/T10，§5.1–5.3→T9/T10，§6 错误清单→Global Constraints + 各任务，§7 ABI→T3，§8 可移植→T1/T2 形态 + T11 三线，§9→T4–T10 各测试步，§10→T11，§11→T11，§12→「出入记录」。
- [x] 无占位符语句（「TBD/同上/类似 Task N」零出现；重复 fixture 全部给全文）。
- [x] 跨任务类型名一致：`ConfigBlob::Find -> std::optional<Value>`（T2 定义 = T6 使用）；`MakeInstance` 五参 Result 形（T6 定义 = T7 的 ⑤ 块使用）；`LedgerEdgeRef/SnapshotEdge`（T9 定义 = T10 使用）；`SkippedRecord/SkipClass`（T4 定义 = T5/T7 使用）；fixture 宏名逐一在注册步骤给出。






