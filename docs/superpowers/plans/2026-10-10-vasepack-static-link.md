# VasePack 静态链入（进程内来源）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 Vase 宿主能从「已在进程内的入口」装配插件（静态链入那一跳），并补上 §8.1「一个库装 N 个插件对 Vase 完全等价」这句从未被验过的话——顺带修掉这条路径上暴露的一处既有缺陷。

**Architecture:** `LoadPlanEntry` 增第二个来源（入口函数指针 `PluginEntryPoint`，非空即进程内来源）；`Loader` 的 `BinaryRecord` 增 `InProcess` 标记与「按入口指针去重」的登记入口，其 `Symbol` 直接从入口取值、**分支整支住共享层**（三条平台线的平台文件一个字不动）。装配循环只分流「取记录」那一句，其余逐字不动。进程内条目上的 `Eject` 一律拒（档二结构性不可达），`Adopt` 结构上无法表达。另修 `MakeInstance` 把每插件状态表写进共享记录导致「一库 N 插件时只重置最后一个」的缺陷（改二进制级并集）。

**Tech Stack:** C++20（全项目关闭异常）、CMake 4.0 + Ninja、vcpkg（gtest / nlohmann-json）、GoogleTest 1.18.0 + ctest、clang-23.1.x（clang-cl / cl.exe / libc++ 三条线）。

**Spec:** `docs/superpowers/specs/2026-10-09-vasepack-static-link-design.md`（决定 D216–D239）

## Global Constraints

- **C++20，全项目关闭异常**：不写 `throw` / `try` / `catch`（Windows `/EHs-c-`、POSIX `-fno-exceptions`）。错误一律经 `Result<T>` / `Error` 显式返回。
- **新 target 必须链接 `VaseBuildOptions`**（规矩 1：它是编译选项的唯一出口，漏了 `/utf-8` 会以与「编码」毫无字面关联的方式炸掉构建）。
- **测试里禁用 `EXPECT_THROW` / `ASSERT_THROW` / `EXPECT_ANY_THROW` / `EXPECT_NO_THROW`**（规矩 2）：它们是**编译期硬失败**（`cannot use 'try' with exceptions disabled` / `error C4530`）。测「必须失败」只能用 `Result<T>` 的显式返回值。
- **Vase 自己的头一律用引号包含**（规矩 3）：`#include "Vase/Plugin.h"`，不写尖括号（尖括号会被 `/external:*` 静默豁免警告）。
- **插件 target 一律经 `vase_add_plugin_fixture`**（规矩 5）；**编译期输入必须在调用点给**，调用之后不得再对同一 target 追加源 / include / 编译定义。
- **规矩 6**：本波动了 `Loader`、装配路径与 `MakeInstance`/`EjectPlugin` ⇒ 必须跑三平台 `-R 'HotSwap|Eject|Adopt'` 族并各留证据。
- **`kHeaderVersion` 与描述符布局不动**（D227）——本波只动宿主面结构，不碰插件二进制契约。
- **注释**：单条 ≤2 行、硬上限 3 行；只留「删掉会让读者踩坑」的原因与约束，**不写论证、不写实测流水账**（论证归 spec / plan）。
- **提交信息**：中文正文；**不得含任何 AI 署名尾注**（不写 `Co-Authored-By:` / `Generated-with:` / `Signed-off-by:`）——`CLAUDE.md` 明文，且优先于任何工具默认值。
- **命名**（`.clang-tidy` 强制）：成员 `CamelCase`、参数/局部 `camelBack`、常量与 constexpr `k` + `CamelCase`、类型 `CamelCase`、命名空间 `lower_case`。
- **格式**：Allman、`PointerAlignment: Left`、构造初始化表**每式一行、逗号在行首**、`template <...>` 头永远与签名分两行。
- **中间产物目录（本波新立）**：`VASE_PACK_INTERMEDIATE_DIR = "${CMAKE_BINARY_DIR}/pack-objects"`，由 `Cmake/VasePluginHelpers.cmake` **定义一次**、两处引用（规矩 7）。
- **构建命令**（八 preset，详见 `CLAUDE.md`「构建与测试」）：
  - Windows clang-cl：`cmake --preset win-x64-clang-debug` / `cmake --build --preset …` / `ctest --preset …`
  - Windows cl.exe：**必须经** `Scripts/msvc-env.cmd cmake --preset win-x64-msvc-debug`
  - Linux：`wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --preset linux-x64-clang-debug'`（**必须登录 shell**）
  - 基数必须另跑 `ctest --preset <p> -N` 对「构建与测试」的按线分账表——`ctest` 在一条测试都没发现时同样返回 0。

## File Structure

| 文件 | 职责 | 本波动作 |
|---|---|---|
| `Include/Vase/PluginDescriptor.h` | 插件↔宿主的静态契约（`PluginMeta` / `PluginDescriptor` / `VASE_PLUGIN`） | 追加 `PluginEntryPoint` 类型（§8.1 唯一一跳的签名） |
| `Include/Vase/Host/Loader.h` | 平台差异的唯一收口 + 每实例的二进制表 | `BinaryRecord` 增 `InProcess` / `Entry`；新增 `EnsureInProcess` |
| `Source/Host/Loader.cpp` | 上者的共享层（三平台同源） | 进程内登记、`Symbol` 分支、`Unload` 直返、`FindResident` 排除 |
| `Include/Vase/Host/LoadPlan.h` | 宿主面的计划输入形 | `LoadPlanEntry` 增 `InProcessEntry` |
| `Source/Host/PluginHost.cpp` | 进程级二进制层：装配 / Eject / Adopt | 取记录分流、两条早闸、关机跳过、（T9）状态表并集 |
| `Cmake/VasePluginHelpers.cmake` | 插件形态与组合库的唯一出口 | 中间目录常量；`.Pack` 落位改准；`vase_pack_plugins` 增 `STATIC` |
| `Tests/Integration/InProcessSourceTests.cpp` | 机制与宿主级证人（不需要静态链接） | 新建 |
| `Tests/StaticLink/` | 静态链入的端到端见证（独立可执行） | 新建 |

---

### Task 1: `PluginEntryPoint` 类型与 `Loader` 的进程内记录

**Files:**
- Modify: `Include/Vase/PluginDescriptor.h:77`（`PluginDescriptor` 定义之后）
- Modify: `Include/Vase/Host/Loader.h`（includes、`BinaryRecord`、公开方法）
- Modify: `Source/Host/Loader.cpp:20-56` / `:58-86` / `:88-97` / `:126-132`
- Test: `Tests/Unit/LoaderTests.cpp`（文件末尾追加）

**Interfaces:**
- Consumes: 无（本波第一件）
- Produces:
  - `vase::PluginEntryPoint` = `const vase::PluginDescriptor* (*)(const char*)`
  - `vase::detail::BinaryRecord::InProcess`（`bool`，默认 `false`）、`vase::detail::BinaryRecord::Entry`（`PluginEntryPoint`，默认 `nullptr`）
  - `vase::Result<vase::detail::BinaryRecord*> vase::detail::Loader::EnsureInProcess(vase::PluginEntryPoint entry)`

> **为什么类型住 `PluginDescriptor.h`**：spec §2.2 写的是「`LoadPlan.h` 新增」，但 `Loader.h` 也要用它，而两处各定义一份就是同类型的第二份声明（规矩 7）。`PluginDescriptor.h` 定义的是 `VASE_PLUGIN` 生成的那个入口的**契约**，是这个名字唯一的家。**这是对 spec 的一处形状偏离，落进文末偏离登记第 1 条。**

- [ ] **Step 1: 写失败测试**

在 `Tests/Unit/LoaderTests.cpp` 末尾追加（该文件已有 `FixturePath` 帮手与 `VASE_FIXTURE_*` 宏）：

```cpp
using GetPluginFn = const vase::PluginDescriptor* (*)(const char*);

TEST(Loader, InProcessEntryResolvesWithoutPlatformLoad)
{
    vase::detail::Loader loader;
    // 先用平台装载拿到一枚真入口，再把它当「已在进程内的入口」登记——
    // 本用例测的是**登记与解析**这条路，入口怎么来的与它无关（静态链入的端到端见 Tests/StaticLink）。
    vase::Result<vase::detail::BinaryRecord*> loaded = loader.EnsureResident(FixturePath(VASE_FIXTURE_LOADPROBE));
    ASSERT_TRUE(loaded.IsOk()) << loaded.GetError().Message();
    const vase::Result<void*> raw = vase::detail::Loader::Symbol(*loaded.Value(), "VasePlugin_GetPlugin");
    ASSERT_TRUE(raw.IsOk()) << raw.GetError().Message();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) void* → 函数指针只有这条路（同 PluginHost.cpp:70）
    const auto entry = reinterpret_cast<GetPluginFn>(raw.Value());

    vase::Result<vase::detail::BinaryRecord*> inProcess = loader.EnsureInProcess(entry);
    ASSERT_TRUE(inProcess.IsOk());
    EXPECT_TRUE(inProcess.Value()->InProcess);
    EXPECT_EQ(loader.ResidentBinaryCount(), 2U); // 平台记录 + 进程内记录，两条

    // 同入口再登记一次 = 复用同一记录（去重键是入口指针，不是路径）
    vase::Result<vase::detail::BinaryRecord*> again = loader.EnsureInProcess(entry);
    ASSERT_TRUE(again.IsOk());
    EXPECT_EQ(again.Value(), inProcess.Value());
    EXPECT_EQ(loader.ResidentBinaryCount(), 2U);

    // Symbol 直接由入口给出，不需要平台查找
    const vase::Result<void*> resolved = vase::detail::Loader::Symbol(*inProcess.Value(), "VasePlugin_GetPlugin");
    ASSERT_TRUE(resolved.IsOk()) << resolved.GetError().Message();
    EXPECT_EQ(resolved.Value(), raw.Value());
    // 进程内记录只有这一个符号（枚举面 VasePlugin_Descriptors 不在本来源内，见 spec §6.2 的已知边界）
    EXPECT_FALSE(vase::detail::Loader::Symbol(*inProcess.Value(), "VasePlugin_Descriptors").IsOk());

    // FindResident 按路径查——空路径**不得**命中进程内记录（spec §6.2 地雷 1，D239 主防）
    EXPECT_EQ(loader.FindResident(""), nullptr);

    // Unload 不卸进程内记录：表不动（spec §3.1 档二不适用）
    static_cast<void>(loader.Unload(*inProcess.Value()));
    EXPECT_EQ(loader.ResidentBinaryCount(), 2U);

    static_cast<void>(loader.Unload(*loaded.Value()));
}
```

- [ ] **Step 2: 跑测试确认它失败**

Run: `cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R "Loader.InProcessEntryResolvesWithoutPlatformLoad"`
Expected: **编译失败** —— `'EnsureInProcess': is not a member of 'vase::detail::Loader'`、`'InProcess': is not a member of 'vase::detail::BinaryRecord'`。

- [ ] **Step 3: 加类型**

`Include/Vase/PluginDescriptor.h`，紧接 `struct PluginDescriptor { … };` 定义之后、`static_assert` 之前插入：

```cpp
// §8.1 那条唯一一跳的签名。VASE_PLUGIN 生成的 VasePlugin_GetPlugin 逐字是这个类型。
// 宿主面（LoadPlan / Loader）用它表达「入口已在本进程内」——那是静态链入（iOS .a）的形态。
// 定义住在这里：它命名的就是上面那个契约的入口，别处再写一份即是同类型的第二份声明。
using PluginEntryPoint = const PluginDescriptor* (*)(const char*);
```

- [ ] **Step 4: 改 `BinaryRecord` 与 `Loader` 的声明**

`Include/Vase/Host/Loader.h`：

① includes 段加一行（`"Vase/Detail/Result.h"` 之后）：

```cpp
#include "Vase/PluginDescriptor.h"
```

② `struct BinaryRecord`（现 `:31-40`）改为：

```cpp
struct BinaryRecord
{
    std::filesystem::path Path; // 绝对化后的路径（表内去重键）；进程内来源为空
    void* Raw = nullptr;        // 平台句柄（HMODULE / dlopen 返回值）；进程内来源无意义
    // 进程内来源（本波，D218）：镜像即宿主本身，入口由宿主链接期链入。与既有「resident」
    // （已被本 Loader 装载、尚未卸载）**不同义**——那个词维持原义不动。
    bool InProcess = false;
    PluginEntryPoint Entry = nullptr; // 仅 InProcess 有意义；去重键
    // M3/D92：描述符里的进程级状态表（指向镜像只读数据段，**不拥有**）。记在**记录**上而不是
    // 实例上：进程级状态跟着二进制走（§9.1），而记录的生命周期正是镜像驻留期；空壳与 Failed
    // 两条 Eject 分支没有实例、只有记录，Reset 要靠它。
    const ProcessStateDesc* ProcessStates = nullptr;
    std::size_t ProcessStateCount = 0;
};
```

③ 公开方法区，`EnsureResident` 声明之后加：

```cpp
    // 进程内来源的登记入口（本波，D218）。去重键 = entry 指针（一份组合库一个入口）。
    // **永不失败**：不读盘、不解析，没有可失败的平台动作；返回 Result 只为与 EnsureResident
    // 并列、让装配循环的分流保持同形。别为它编失败路径。
    Result<BinaryRecord*> EnsureInProcess(PluginEntryPoint entry);
```

④ `FindResident` 的契约注释（现 `:86-90`）末尾补一句：

```cpp
    // 进程内记录**不参与**本查询：它的 Path 为空，而「按路径查」的语义本就不该命中它。
```

- [ ] **Step 5: 实现**

`Source/Host/Loader.cpp`：

① `EnsureResident` 之后加（`#include` 段不动——`PluginEntryPoint` 经 `Loader.h` 传递可见）：

```cpp
Result<BinaryRecord*> Loader::EnsureInProcess(PluginEntryPoint entry)
{
    const auto existing = std::ranges::find_if(Binaries, [entry](const std::unique_ptr<BinaryRecord>& record)
                                               { return record->InProcess && record->Entry == entry; });
    if (existing != Binaries.end())
    {
        return Result<BinaryRecord*>::Ok(existing->get()); // 同入口复用同一记录（「一文件一记录」的同形）
    }
    Binaries.push_back(std::make_unique<BinaryRecord>(BinaryRecord{.InProcess = true, .Entry = entry}));
    return Result<BinaryRecord*>::Ok(Binaries.back().get());
}
```

② `Unload` 开头（`UnloadEvidence evidence;` 之后）插入早返：

```cpp
    if (record.InProcess)
    {
        // 进程内来源：镜像即宿主，无货可卸（Eject 已在早闸拒绝、~PluginHost 亦跳过）。
        // 全 false 是如实的读法——「没卸成」，不是「卸失败」；记录留在表里不动。
        return evidence;
    }
```

③ `Symbol` 改为：

```cpp
Result<void*> Loader::Symbol(const BinaryRecord& record, std::string_view name)
{
    if (record.InProcess)
    {
        // 入口由宿主直接持有，不走平台查找。进程内来源**只有这一个符号**——枚举面
        // （VasePlugin_Descriptors）不在本来源内，见 spec §6.2 的已知边界。
        if (name == "VasePlugin_GetPlugin")
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) 函数指针 → void* 只有这条路（同 PluginHost.cpp:70 的反向）
            return Result<void*>::Ok(reinterpret_cast<void*>(record.Entry));
        }
        return Result<void*>::Err(Error{"symbol not found: " + std::string(name)});
    }
    const std::string zeroTerminated(name);
    void* address = PlatformSymbol(record.Raw, zeroTerminated.c_str()); // 平台文件各一行实现
    if (address == nullptr)
    {
        return Result<void*>::Err(Error{"symbol not found: " + zeroTerminated});
    }
    return Result<void*>::Ok(address);
}
```

④ `FindResident` 的谓词改为排除进程内记录：

```cpp
    const auto it = std::ranges::find_if(Binaries, [&path](const std::unique_ptr<BinaryRecord>& entry)
                                         { return !entry->InProcess && entry->Path == path; });
```

- [ ] **Step 6: 跑测试确认它通过**

Run: `cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R "^Loader\."`
Expected: PASS（含新增一条；既有 `Loader.*` 全绿）。

- [ ] **Step 7: 提交**

```bash
git add Include/Vase/PluginDescriptor.h Include/Vase/Host/Loader.h Source/Host/Loader.cpp Tests/Unit/LoaderTests.cpp
git commit -m "Loader 进程内来源：PluginEntryPoint 类型 + InProcess 记录（D218/D232/D239）"
```

---

### Task 2: `LoadPlanEntry` 的第二个来源与装配分流

**Files:**
- Modify: `Include/Vase/Host/LoadPlan.h:38-49`
- Modify: `Source/Host/PluginHost.cpp:402`（取记录那一句）
- Modify: `Source/Host/PluginHost.cpp`（`CreatePodImpl` 的结构性校验段，现 `:285-323`）+ `AdoptImpl` 门口（现 `:975` 起）
- Test: `Tests/Integration/InProcessSourceTests.cpp`（新建）+ `Tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 的 `EnsureInProcess` / `BinaryRecord::InProcess`
- Produces:
  - `vase::LoadPlanEntry::InProcessEntry`（`vase::PluginEntryPoint`，默认 `nullptr`）
  - 误用契约子串：`plan rejected: both binary path and in-process entry set for plugin `
  - Adopt 误用契约子串：`adopt refused: file path required`

- [ ] **Step 1: 写失败测试**

新建 `Tests/Integration/InProcessSourceTests.cpp`：

```cpp
// 进程内来源的机制与宿主级证人（本波 D216/D225；不需要静态链接，故住 VaseTests）。
// 静态链入的端到端（真 .a 链进可执行）住 Tests/StaticLink，见 spec §5.1 的分工。
#include "Vase/Detail/Result.h"
#include "Vase/Host/Loader.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/PluginDescriptor.h"

#include <filesystem>
#include <string>

#include <gtest/gtest.h>

namespace
{

using GetPluginFn = const vase::PluginDescriptor* (*)(const char*);

std::filesystem::path FixturePath(const char* defineValue) { return std::filesystem::path{defineValue}; }

// 取一枚真入口当「进程内入口」用：先经平台装载拿到它。本文件所有用例都只需「宿主持有入口」
// 这一件事，而入口怎么来的与它无关——这正是 spec §2.2 说的「入口从哪来」是唯一的分岔。
//
// Loader 是局部量：它析构**不卸货**（§8.1——卸货只发生在 Eject / Shutdown），故取出的
// 函数指针在用例期间一直有效。与 Tests/Integration/VasePackTests.cpp 的 PackedEntry 同法。
GetPluginFn HelloEntry()
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> record = loader.EnsureResident(FixturePath(VASE_FIXTURE_HELLO));
    if (!record.IsOk())
    {
        return nullptr;
    }
    const vase::Result<void*> raw = vase::detail::Loader::Symbol(*record.Value(), "VasePlugin_GetPlugin");
    if (!raw.IsOk())
    {
        return nullptr;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) 同 PluginHost.cpp:70
    return reinterpret_cast<GetPluginFn>(raw.Value());
}

constexpr std::string_view kHelloId = "Vase.Hello";

vase::LoadPlanEntry MakeHelloEntry(bool throughInProcessSource)
{
    vase::LoadPlanEntry entry{};
    entry.Id = kHelloId;
    if (throughInProcessSource)
    {
        entry.InProcessEntry = HelloEntry();
    }
    else
    {
        entry.BinaryPath = FixturePath(VASE_FIXTURE_HELLO);
    }
    return entry;
}

} // namespace

TEST(InProcessSource, BothSourcesSetIsRejected)
{
    vase::PluginHost host;
    vase::LoadPlan plan;
    vase::LoadPlanEntry entry = MakeHelloEntry(true);
    entry.BinaryPath = FixturePath(VASE_FIXTURE_HELLO); // 两个来源同填 = 误用
    plan.Ordered.push_back(entry);

    const vase::Result<vase::PodHandle> created = host.CreatePod(plan);
    ASSERT_FALSE(created.IsOk());
    EXPECT_NE(created.GetError().Message().find("both binary path and in-process entry set"),
              std::string::npos)
        << created.GetError().Message();
}
```

在 `Tests/CMakeLists.txt` 的 `add_executable(VaseTests …)` 源表里，`Integration/VasePackTests.cpp` 那行之后加一行：

```cmake
    Integration/InProcessSourceTests.cpp
```

- [ ] **Step 2: 跑测试确认它失败**

Run: `cmake --preset win-x64-clang-debug && cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R "^InProcessSource\."`
Expected: **编译失败** —— `'InProcessEntry': is not a member of 'vase::LoadPlanEntry'`。

- [ ] **Step 3: 加计划字段**

`Include/Vase/Host/LoadPlan.h`，`LoadPlanEntry` 内 `BinaryPath` 之后插入：

```cpp
    // 来源二选一：BinaryPath（磁盘装载）或 InProcessEntry（入口已在本进程内，静态链入的形态）。
    // 同填 = 误用，CreatePod 拒（见 PluginHost 的「plan rejected」一族）。
    // NOLINTNEXTLINE(readability-redundant-member-init) NSDMI 为省略豁免所必需，同 Expected。
    PluginEntryPoint InProcessEntry = nullptr;
```

同文件 includes 段加：

```cpp
#include "Vase/PluginDescriptor.h"
```

- [ ] **Step 4: 加误用闸与装配分流**

`Source/Host/PluginHost.cpp` 的 `CreatePodImpl` 里，紧跟现有的「①' 结构性校验·Id 唯一」循环（`:285-323`）之后、槽分配之前插入：

```cpp
    // ①'' 结构性校验·来源唯一（本波 D217）：两个来源同填 = 宿主给错了，是可恢复的输入错。
    for (const LoadPlanEntry& entry : plan.Ordered)
    {
        if (entry.InProcessEntry != nullptr && !entry.BinaryPath.empty())
        {
            return Result<PodHandle>::Err(Refusal(entry.Id, Phase::kLoad,
                                                  "plan rejected: both binary path and in-process entry set for plugin " +
                                                      std::string(entry.Id)));
        }
    }
```

把 `:402` 那一句

```cpp
        const Result<detail::BinaryRecord*> resident = Loader.EnsureResident(entry.BinaryPath);
```

改为

```cpp
        // 取记录的分流（本波 §2.4）：进程内来源走 EnsureInProcess，其余逐字不变。
        const Result<detail::BinaryRecord*> resident =
            entry.InProcessEntry != nullptr ? Loader.EnsureInProcess(entry.InProcessEntry)
                                            : Loader.EnsureResident(entry.BinaryPath);
```

- [ ] **Step 5: 跑测试确认它通过**

Run: `cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R "^InProcessSource\."`
Expected: PASS。

- [ ] **Step 6: 加 Adopt 的空路径早闸（D239 第二层）**

`Source/Host/PluginHost.cpp` 的 `AdoptImpl` 开头——在 `expected->Id != id` 那条检查之后、`⓪ 身份唯一` 之前插入：

```cpp
    // 路径面表达不了进程内来源（spec §3.3）：空路径 = 宿主给错了。这是第二层防护，
    // 主防在 Loader::FindResident 的「排除进程内记录」（D239）。
    if (absPath.empty())
    {
        return Result<AdoptReport>::Err(Refusal(id, Phase::kAdopt, "adopt refused: file path required"));
    }
```

- [ ] **Step 7: 提交**

```bash
git add Include/Vase/Host/LoadPlan.h Source/Host/PluginHost.cpp Tests/Integration/InProcessSourceTests.cpp Tests/CMakeLists.txt
git commit -m "计划增进程内来源与误用闸；装配循环分流；Adopt 空路径早闸（D216/D217/D220/D239）"
```

---

### Task 3: `EjectPlugin` 的进程内早闸与宿主关机的跳过

**Files:**
- Modify: `Source/Host/PluginHost.cpp`（`EjectPlugin` 入口、`~PluginHost`）
- Test: `Tests/Integration/InProcessSourceTests.cpp`（追加）

**Interfaces:**
- Consumes: Task 2 的 `InProcessEntry`
- Produces: 契约子串 `linked-in binary cannot be ejected`

- [ ] **Step 1: 写失败测试**

`Tests/Integration/InProcessSourceTests.cpp` 末尾追加：

```cpp
TEST(InProcessSource, EjectIsRefusedBeforeAnyTeardown)
{
    vase::PluginHost host;
    vase::LoadPlan plan;
    plan.Ordered.push_back(MakeHelloEntry(true));
    const vase::Result<vase::PodHandle> created = host.CreatePod(plan);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();

    const vase::Result<vase::EjectReport> ejected = host.EjectPlugin(created.Value(), kHelloId);
    ASSERT_FALSE(ejected.IsOk());
    EXPECT_NE(ejected.GetError().Message().find("linked-in binary cannot be ejected"), std::string::npos)
        << ejected.GetError().Message();

    // 早闸在任何拆除之前：实例仍在（下一句 DestroyPod 能干净拆掉就是证据）
    ASSERT_NE(host.Resolve(created.Value()), nullptr);
    static_cast<void>(host.DestroyPod(created.Value()));
}

TEST(InProcessSource, DestroyPodStillWorks)
{
    vase::PluginHost host;
    vase::LoadPlan plan;
    plan.Ordered.push_back(MakeHelloEntry(true));
    const vase::Result<vase::PodHandle> created = host.CreatePod(plan);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
    const vase::PodReport report = host.DestroyPod(created.Value()); // 拆局不卸货（§8.1）
    static_cast<void>(report);
    EXPECT_EQ(host.Resolve(created.Value()), nullptr);
}
```

> **这条测试此刻应该红，且红法要说清**：Task 2 之后 `CreatePod` 已经能装上（分流已就位），但 `EjectPlugin` 会**一路走到卸载路径并返回 `Ok`**——进程内记录的 `Unload` 返回全 false 证据，于是它被读成「没卸成」而不是错误。Step 3 的早闸就是把这个**假成功**变成响亮的拒绝。若 Step 2 跑出来是绿的，说明 `EjectPlugin` 那边已经有别的闸抢先拒绝了——停下查清楚是哪一条，别直接进 Step 3。

- [ ] **Step 2: 跑测试确认它失败**

Run: `cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R "^InProcessSource\."`
Expected: `EjectIsRefusedBeforeAnyTeardown` FAIL —— `ejected.IsOk()` 为真（Eject 没被拒）。

- [ ] **Step 3: 加早闸**

`Source/Host/PluginHost.cpp` 的 `EjectPlugin`，在函数体取到 `binary` 之后、任何拆除动作之前（即在「① 实例」那一段之前）插入。实现者在实现时按实际位置择一：**闸必须早于 `report` 的任何状态写入**。

```cpp
    // 进程内来源的 Eject 一律拒（本波 D219）：档二结构性不可达（镜像即宿主，无货可卸），
    // 放行就是 §8.7 说的「Eject 假成功」。§12.3 / §13.4 的「移动端不做热插拔」自此由机制承载。
    // 判据取**记录**（Eject 的目标是二进制，不是 Id）——先按 Id 找到该实例的记录再问。
```

闸的取记录方式：Eject 已有的 `binary`（`live->Binary`）就是答案——早闸写成

```cpp
    if (binary != nullptr && binary->InProcess)
    {
        return Result<EjectReport>::Err(Refusal(id, Phase::kEject, "linked-in binary cannot be ejected"));
    }
```

若 `binary` 在该点尚未取得，则取 `Pod::LiveInstance::Binary`（空壳 / Failed 记录两支同理各查一次）。
**三支都要拦**（活实例 / 级联空壳 / Failed 记录）——它们共享同一记录指针。

- [ ] **Step 4: 关机跳过**

`~PluginHost` 的卸货循环（现 `:226-239`）里，取记录后立即跳过进程内记录：

```cpp
    for (const detail::BinaryRecord& record : std::views::reverse(resident))
    {
        if (record.InProcess)
        {
            continue; // 进程内来源无货可卸（§3.4）：不跳就会打「still resident」的假警报
        }
        const detail::UnloadEvidence evidence = Loader.Unload(record);
        // …既有内容不动
```

- [ ] **Step 5: 跑测试确认它通过**

Run: `cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R "^InProcessSource\."`
Expected: PASS（两条）。

- [ ] **Step 6: 跑规矩 6 的族（本波第一次）**

Run: `ctest --preset win-x64-clang-debug -R "HotSwap|Eject|Adopt"`
Expected: 全绿（58 条，见 `CLAUDE.md`「构建与测试」）。

- [ ] **Step 7: 提交**

```bash
git add Source/Host/PluginHost.cpp Tests/Integration/InProcessSourceTests.cpp
git commit -m "Eject 拒进程内来源 + 宿主关机跳过（D219）"
```

---

### Task 4: 宿主装配共享组合库（证人 #1，补 §8.1「等价」的零证据）

**Files:**
- Test: `Tests/Integration/InProcessSourceTests.cpp`（追加）
- Modify: `Tests/CMakeLists.txt`（无需改动——`VASE_FIXTURE_VASEPLUGINS` 已在）

**Interfaces:**
- Consumes: Task 2 的 `BinaryPath` 来源（既有）；`VASE_FIXTURE_VASEPLUGINS` 编译定义（既有）
- Produces: 无新公开面

> **这是本波最重要的一条**：它第一次把组合库交给 `PluginHost`（桌面波那六条证人只到 `Loader`）。三个条目共享一个 `BinaryPath`，靠 `Loader` 的「一文件一记录」拿到同一记录。

- [ ] **Step 1: 写失败测试**

追溯 `Samples/` 三个插件的 Id（`grep -n 'Id =' Samples/*/[A-Z]*.cpp` 取真值——**别照抄下面的例子**，写测试前先核）。以实测值为准，形如：

```cpp
// 三个条目共享一个 BinaryPath：组合库「一库 N 插件」经真宿主的**首条**证人（spec §5.1 #1 / D225）。
TEST(CombinedLibraryHost, ThreeEntriesShareOneBinaryPath)
{
    vase::PluginHost host;
    vase::LoadPlan plan;
    std::vector<std::string> ids = {"Vase.Hello", "Vase.Dependent", "Vase.Failing"};
    for (const std::string& id : ids)
    {
        vase::LoadPlanEntry entry{};
        entry.Id = id;
        entry.BinaryPath = FixturePath(VASE_FIXTURE_VASEPLUGINS);
        plan.Ordered.push_back(entry);
    }

    const vase::Result<vase::PodHandle> created = host.CreatePod(plan);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
    vase::Pod* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);
    // 三条都得在（宽容语义下 FailingPlugin 的 OnStart 失败会落 Failed 记录——按实际读数调整这一断言，
    // 并在测试里写清是哪一支）
    static_cast<void>(pod);

    static_cast<void>(host.DestroyPod(created.Value()));
}
```

- [ ] **Step 2: 跑测试确认它失败**

Run: `cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R "^CombinedLibraryHost\."`
Expected: 先失败在你写错的 Id 上（`CreatePod` 报 `HeaderVersion mismatch` 或该 Id 装不上）。**逐条改正 Id 直到装得上**——这一步就是 spec §5.5 的首验证点 1。

- [ ] **Step 3: 把它改成真断言**

装得上之后，把 `static_cast<void>(pod)` 换成真读数（按 `Pod` 的公开访问器；照 `Tests/Integration/MultiPluginAssemblyTests.cpp` 的既有读法）：

- 三个 Id 各自的实例都在；
- 至少一条服务解析（如 `Vase.Hello` 提供的服务被 `Vase.Dependent` 解析到）走通；
- `host.ForTestLedgerEdgeCount()` 反映该依赖边。

- [ ] **Step 4: 跑测试确认它通过**

Run: `cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R "^CombinedLibraryHost\."`
Expected: PASS。

- [ ] **Step 5: 提交**

```bash
git add Tests/Integration/InProcessSourceTests.cpp
git commit -m "宿主装配共享组合库：三条目共享一 BinaryPath（D225，补 §8.1 等价的零证据）"
```

---

### Task 5: 中间产物目录与 `vase_pack_plugins` 的 `STATIC`

**Files:**
- Modify: `Cmake/VasePluginHelpers.cmake`（文件作用域、`if(FIX_PACK)` 块、`vase_pack_plugins`）

**Interfaces:**
- Consumes: 无
- Produces: `VASE_PACK_INTERMEDIATE_DIR`（CMake 变量）；`vase_pack_plugins(<name> PLUGINS … [STATIC])`

- [ ] **Step 1: 在函数之前定义中间目录（一处字面真值）**

`Cmake/VasePluginHelpers.cmake` 顶部注释块之后、`function(vase_add_plugin_fixture name)` 之前插入：

```cmake
# 统一的中间产物目录（本波 D231）：打包链路的**链接输入**都落这里——`.Pack` 归档与静态组合库。
# 判据是「打包链路的中转」，不是「任何静态库」：`VaseCliCore` 有自己的消费者、不算；
# 生成的 dispatch TU 是**源**（进 tidy 面，D205）、也不算。
# 位置取产物根（bin/ / lib/）之外的兄弟目录——「会不会被发布」因此是结构事实，不靠约定。
set(VASE_PACK_INTERMEDIATE_DIR "${CMAKE_BINARY_DIR}/pack-objects")
```

- [ ] **Step 2: `.Pack` 归档改指它**

`if(FIX_PACK)` 块里，把

```cmake
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pack-objects")
```

改为

```cmake
            ARCHIVE_OUTPUT_DIRECTORY "${VASE_PACK_INTERMEDIATE_DIR}")
```

并同步更新文件顶部那段注释里描述 `.Pack` 落位的那一行（「归档落 `pack-objects/`」→「归档落 `VASE_PACK_INTERMEDIATE_DIR`」）。

- [ ] **Step 3: `vase_pack_plugins` 增 `STATIC`**

① 解析段（现 `:60-61`）：

```cmake
function(vase_pack_plugins target)
    cmake_parse_arguments(PACK "STATIC" "" "PLUGINS" ${ARGN})
```

② 建库段（现 `:97`）改为按关键字选链接方式，并在 STATIC 支设输出目录：

```cmake
    # 缺省 SHARED（发布形，D201）；STATIC 是链接输入（本波 D222/D231），落中间目录。
    set(linkage SHARED)
    if(PACK_STATIC)
        set(linkage STATIC)
    endif()
    add_library(${target} ${linkage} "${CMAKE_CURRENT_BINARY_DIR}/${target}.dispatch.cpp")
    target_link_libraries(${target} PRIVATE VaseBuildOptions VasePod)
    set_target_properties(${target} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
    if(PACK_STATIC)
        set_target_properties(${target} PROPERTIES
            POSITION_INDEPENDENT_CODE ON
            ARCHIVE_OUTPUT_DIRECTORY "${VASE_PACK_INTERMEDIATE_DIR}")
    endif()
```

③ 头注释补一句（该函数块上方）：

```cmake
# 裸关键字 STATIC（本波 D222）：出静态组合库而非 SHARED——它是**链接输入**（宿主链接期链入），
# 故落 VASE_PACK_INTERMEDIATE_DIR 而非产物根；SHARED 支的平铺落位（D201）逐字不变。
```

- [ ] **Step 4: 验证既有行为逐字不变**

Run: `cmake --preset win-x64-clang-debug && cmake --build --preset win-x64-clang-debug`
Expected: 构建成功；且产物根里 `VasePlugins` 仍在原位（`ls build-win/win-x64-clang-debug/bin` 或 `lib`，按平台）。
Run: `ctest --preset win-x64-clang-debug -R "^VasePack\."`
Expected: PASS（六条，行为不变）。

- [ ] **Step 5: 跑一条 `STATIC` 的冒烟**

在 `Tests/StaticLink/CMakeLists.txt`（Task 6 会建）之前，先临时在 `Samples/VasePlugins/CMakeLists.txt` 末尾加一行验证，**验证完删掉**：

```cmake
vase_pack_plugins(VasePluginsStaticSmoke PLUGINS HelloPlugin DependentPlugin FailingPlugin STATIC)
```

Run: `cmake --preset win-x64-clang-debug && cmake --build --preset win-x64-clang-debug --target VasePluginsStaticSmoke`
Expected: 构建成功；产物落在 `build-win/win-x64-clang-debug/pack-objects/`（Windows 为 `VasePluginsStaticSmoke.lib`，POSIX 为 `libVasePluginsStaticSmoke.a`）。

- [ ] **Step 6: 移除冒烟并提交**

删掉 Step 5 那一行，然后：

```bash
git add Cmake/VasePluginHelpers.cmake
git commit -m "统一中间产物目录 VASE_PACK_INTERMEDIATE_DIR + vase_pack_plugins 的 STATIC（D222/D231）"
```

---

### Task 6: 见证可执行与「宿主装配静态链入组合库」（证人 #2）

**Files:**
- Create: `Tests/StaticLink/CMakeLists.txt`
- Create: `Tests/StaticLink/StaticLinkTests.cpp`
- Modify: `Tests/CMakeLists.txt`（末尾 `add_subdirectory(StaticLink)`）

**Interfaces:**
- Consumes: Task 5 的 `STATIC` 关键字；Task 2 的 `InProcessEntry`
- Produces: 第二个 gtest 可执行 `VaseStaticLinkTests`

- [ ] **Step 1: 写失败测试**

新建 `Tests/StaticLink/StaticLinkTests.cpp`：

```cpp
// 静态链入的端到端见证（spec §5.1 #2）：组合库的真静态形**链进本可执行**。
// 与 VasePackTests 的六条恰相反——那些刻意经 Loader 按路径装载（D204），
// 本文件刻意**真链**：宿主取 &VasePlugin_GetPlugin 这个动作本身即 §8.6 要的硬引用。
#include "Vase/Detail/Result.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/PluginDescriptor.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace
{

// 链接期硬引用：不写这一笔，静态库会被整个丢掉（§8.6 末条 iOS 例外，本波由类型承载）。
constexpr vase::PluginEntryPoint kPackedEntry = &VasePlugin_GetPlugin;

} // namespace

TEST(StaticLink, HostAssemblesStaticallyLinkedCombinedLibrary)
{
    vase::PluginHost host;
    vase::LoadPlan plan;
    // Id 逐条核对过（同 Task 4 的核法），别照抄。
    const std::vector<std::string> ids = {"Vase.Hello", "Vase.Dependent", "Vase.Failing"};
    for (const std::string& id : ids)
    {
        vase::LoadPlanEntry entry{};
        entry.Id = id;
        entry.InProcessEntry = kPackedEntry;
        plan.Ordered.push_back(entry);
    }

    const vase::Result<vase::PodHandle> created = host.CreatePod(plan);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
    EXPECT_NE(host.Resolve(created.Value()), nullptr);
    static_cast<void>(host.DestroyPod(created.Value()));
}

TEST(StaticLink, EjectIsRefusedOnStaticallyLinkedPlugins)
{
    vase::PluginHost host;
    vase::LoadPlan plan;
    vase::LoadPlanEntry entry{};
    entry.Id = "Vase.Hello";
    entry.InProcessEntry = kPackedEntry;
    plan.Ordered.push_back(entry);
    const vase::Result<vase::PodHandle> created = host.CreatePod(plan);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();

    const vase::Result<vase::EjectReport> ejected = host.EjectPlugin(created.Value(), "Vase.Hello");
    ASSERT_FALSE(ejected.IsOk());
    EXPECT_NE(ejected.GetError().Message().find("linked-in binary cannot be ejected"), std::string::npos);
    static_cast<void>(host.DestroyPod(created.Value()));
}
```

- [ ] **Step 2: 接线**

新建 `Tests/StaticLink/CMakeLists.txt`：

```cmake
# 第二个 gtest 可执行（spec D223）：静态链入的见证必须自己是一个二进制——
# 链进 VaseTests 会让三个 Sample 从启动起驻留，与别处动态装的同名 Id 相撞。
#
# 组合库的真静态形：与 Samples/VasePlugins 的 SHARED 支共用同一套分发表生成器（D222），
# 只是多给一个 STATIC。产物落 VASE_PACK_INTERMEDIATE_DIR（D231）。
vase_pack_plugins(VasePluginsStatic PLUGINS HelloPlugin DependentPlugin FailingPlugin STATIC)

add_executable(VaseStaticLinkTests
    StaticLinkTests.cpp)

target_link_libraries(VaseStaticLinkTests
    PRIVATE VaseBuildOptions
            VaseHost
            VasePluginsStatic
            GTest::gtest_main)

# 入口声明见 StaticLinkTests.cpp 顶部的 extern "C" 声明：静态链入时 VasePlugin_GetPlugin
# 就是本可执行里的一个普通符号（由分发表 TU 定义，Cmake/VasePackDispatch.cpp.in），
# 没有 dllexport/import 那一套可谈。

include(GoogleTest)
gtest_discover_tests(VaseStaticLinkTests
    DISCOVERY_MODE PRE_TEST)
```

在 `Tests/StaticLink/StaticLinkTests.cpp` 顶部（`namespace` 之前）加入口声明：

```cpp
// 分发表入口（§8.1 的唯一一跳）：静态链入时它就是本可执行里的一个普通符号。
// 名字是**字面契约**（`Cmake/VasePackDispatch.cpp.in` 与 VASE_PLUGIN 同名生成）。
extern "C" const vase::PluginDescriptor* VasePlugin_GetPlugin(const char* id);
```

`Tests/CMakeLists.txt` 末尾（`gtest_discover_tests(VaseTests …)` 之后）加：

```cmake
# 静态链入见证（本波）：独立可执行，理由见该目录的 CMakeLists（spec D223）。
add_subdirectory(StaticLink)
```

- [ ] **Step 3: 确认「入口即链接期硬引用」这件事会红**

把 `Tests/StaticLink/CMakeLists.txt` 的 `target_link_libraries` 里 `VasePluginsStatic` 那一行**暂时注释掉**，再跑：

Run: `cmake --preset win-x64-clang-debug && cmake --build --preset win-x64-clang-debug`
Expected: **链接失败**，未定义符号 `VasePlugin_GetPlugin`。

> **这一步是 §8.6 末条那句「宿主必须真的引用到包导出的符号」的实证**：不链那个库，宿主连入口都拿不到——不是「拿不到插件」，是**编不出来**。这正是本波把保活从文档告诫升格成类型约束要说的事（D216）。

跑完**把那一行加回去**。（不要用「空路径」去造红：空路径下 `CreatePod` 宽容语义只落 Failed 记录并返回 `Ok`，测试会**假绿**。）

- [ ] **Step 4: 恢复并跑通**

撤销 Step 3 的两处临时改动，跑：

Run: `cmake --preset win-x64-clang-debug && cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R "^StaticLink\."`
Expected: PASS（两条）。

- [ ] **Step 5: 验证「假绿」已被 #1 覆盖（对照检查）**

Run: `ctest --preset win-x64-clang-debug -R "^CombinedLibraryHost\."`
Expected: PASS。#1（动态）与 #2（静态）都在，且共用同一组 Id 与同一套读数——「等价」自此有比较面。

- [ ] **Step 6: 提交**

```bash
git add Tests/StaticLink Tests/CMakeLists.txt
git commit -m "静态链入见证可执行：宿主装配真静态组合库（D223/D225）"
```

---

### Task 7: 保活常驻证人（弱符号探针 + 对照组）

**Files:**
- Create: `Tests/StaticLink/fixtures/KeepAliveProbe/KeepAliveProbe.cpp`
- Create: `Tests/StaticLink/fixtures/KeepAliveProbe/CMakeLists.txt`
- Create: `Tests/StaticLink/KeepAliveProbeFallback.cpp`（仅 Windows 用）
- Modify: `Tests/StaticLink/CMakeLists.txt`
- Modify: `Tests/StaticLink/StaticLinkTests.cpp`

**Interfaces:**
- Consumes: Task 6 的可执行
- Produces: 无新公开面

> **本证人证什么、不证什么**（写清楚，免得日后被读强）：它证的是「**未列入 `PLUGINS` 的 `.Pack` 对象没被静态链接器提取**」。两条判据必须**用同一套机制**——承重位与对照组都走弱引用；只用「入口指针非空」当对照组是不够的（那测的是另一回事，且对「弱探针恒 null」这一坏法完全失明）。

- [ ] **Step 1: 写探针 fixture**

新建 `Tests/StaticLink/fixtures/KeepAliveProbe/KeepAliveProbe.cpp`（骨架照 `Tests/Integration/fixtures/` 任一文件；`PluginMeta` 的字段以磁盘为准，**别照抄猜测**）：

```cpp
#include "Vase/Plugin.h"

// 保活探针（spec §5.2）：这个插件**不列入** vase_pack_plugins 的 PLUGINS——它的对象
// 因此不该被静态链接器提取，其描述符符号在见证可执行里应当是**未定义**的。
// 这正是 §8.6「静态库按需提取、无人引用即丢弃」那句可证伪的一半。
class KeepAliveProbePlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Ok();
    }
};

VASE_PLUGIN(KeepAliveProbePlugin)
{
    .Id = "Vase.Test.KeepAliveProbe",
    .DisplayName = "KeepAliveProbe",
    .Version = "1.0.0",
    .Requires = {},
};
```

新建 `Tests/StaticLink/fixtures/KeepAliveProbe/CMakeLists.txt`：

```cmake
# PACK 造出 KeepAliveProbe.Pack（静态库）——它会被链进见证可执行，但**不列入** PLUGINS。
vase_add_plugin_fixture(KeepAliveProbe
    SOURCES KeepAliveProbe.cpp
    LINK_LIBRARIES VasePod
    PACK)
```

- [ ] **Step 2: 接线（链入但**不**列入 PLUGINS）**

`Tests/StaticLink/CMakeLists.txt`：

① 顶部（`vase_pack_plugins` 之前）加：

```cmake
add_subdirectory(fixtures/KeepAliveProbe)
```

② `VaseStaticLinkTests` 的 `target_link_libraries` 里，`VasePluginsStatic` 之后加一行——**这就是「无人引用」的形态**：

```cmake
            KeepAliveProbe.Pack
```

③ Windows 支（COFF 无弱符号，spec D233 的「先换写法」）：把未定义符号退到一个返回 `nullptr` 的替身。

> **⚠️ 本步（含下面整段代码与 Step 3 的替身声明）是「被证伪的初版」，不要照做**——T7 实测 `/alternatename` 不阻止归档提取、COFF 弱引用会崩，实际改走「读产物导出表」。**勘误见文末偏离登记第 4 条、spec §5.2 与 D240**；`KeepAliveProbeFallback.cpp` 最终未创建。

```cmake
if(WIN32)
    # COFF 没有 __attribute__((weak))。/alternatename 是 MSVC 的等价物：
    # 若 VasePluginDesc_KeepAliveProbePlugin 未定义，就用替身把它解析掉。
    # 「对象没被提取」因此表现为「入口等于替身」——判据与 ELF 侧的 nullptr 同义。
    target_link_options(VaseStaticLinkTests PRIVATE
        "/alternatename:VasePluginDesc_KeepAliveProbePlugin=VaseKeepAliveProbeAbsent")
    target_sources(VaseStaticLinkTests PRIVATE KeepAliveProbeFallback.cpp)
endif()
```

新建 `Tests/StaticLink/KeepAliveProbeFallback.cpp`（**仅 Windows 链**）：

```cpp
// Windows 支的替身（spec §5.2 / D233）：被 /alternatename 拿来做未定义符号的兜底。
// 返回 nullptr —— 于是「探针等于它」与 ELF 侧的「探针等于 nullptr」是同一个读数。
#include "Vase/PluginDescriptor.h"

extern "C" const vase::PluginDescriptor* VaseKeepAliveProbeAbsent() { return nullptr; }
```

- [ ] **Step 3: 写测试（承重位 + 对照组，**同一个机制**）**

`Tests/StaticLink/StaticLinkTests.cpp`：在 `namespace {` **之前**加两枚符号声明（它们的名字是生成的 ABI 契约名，与 `Cmake/VasePackDispatch.cpp.in` 同源）：

```cpp
// 两枚描述符符号的**弱声明**。对照组的符号（HelloPlugin）确实会被提取，承重位的不该。
// 两枚走的是同一套弱机制——这是本证人成立的前提（只测一枚的话，一个恒 null 的探针
// 会让承重位「通过」而毫无意义，spec §5.2）。
#ifdef _WIN32
// COFF 无弱符号：靠 /alternatename 退到替身（见本目录 CMakeLists）。声明成普通符号即可。
extern "C" const vase::PluginDescriptor* VasePluginDesc_KeepAliveProbePlugin();
extern "C" const vase::PluginDescriptor* VasePluginDesc_HelloPlugin();
extern "C" const vase::PluginDescriptor* VaseKeepAliveProbeAbsent();
#else
extern "C" const vase::PluginDescriptor* VasePluginDesc_KeepAliveProbePlugin() __attribute__((weak));
extern "C" const vase::PluginDescriptor* VasePluginDesc_HelloPlugin() __attribute__((weak));
#endif
```

在匿名命名空间里加一个收敛平台差异的读数（体内 `#ifdef` 是本仓库测试的既有形态，见 `VaseCliDoctorTests`）：

```cpp
namespace
{

// 承重位：探针符号是不是「不在」（未定义）。两个平台各说各话，但读数同义。
bool KeepAliveProbeAbsent()
{
#ifdef _WIN32
    return VasePluginDesc_KeepAliveProbePlugin == VaseKeepAliveProbeAbsent;
#else
    return VasePluginDesc_KeepAliveProbePlugin == nullptr;
#endif
}

// 对照组：已被提取时同一套机制给出「在」。
bool ListedDescriptorPresent() { return VasePluginDesc_HelloPlugin != nullptr; }

} // namespace
```

再加两条用例：

```cpp
TEST(StaticLink, ListedPackObjectIsExtracted) // 对照组 / 证伪线
{
    // 已列入 PLUGINS 的插件对象必被提取（分发表 TU 硬引用它）。本句**红了**就说明
    // 弱探针机制本身坏了（恒 null），承重位那条随即无意义——所以它是承重位的前提。
    EXPECT_TRUE(ListedDescriptorPresent());
}

TEST(StaticLink, UnlistedPackObjectIsNotExtracted) // 承重位
{
    EXPECT_TRUE(KeepAliveProbeAbsent())
        << "未列入 PLUGINS 的 .Pack 对象被提取了——§8.6 的保活链不成立";
}
```

- [ ] **Step 4: 跑测试，确认承重位此刻**红**（这是它必须红的证据）**

把 `Tests/StaticLink/CMakeLists.txt` 的 `vase_pack_plugins` 那一行**临时**改成把探针也列进去：

```cmake
vase_pack_plugins(VasePluginsStatic PLUGINS HelloPlugin DependentPlugin FailingPlugin KeepAliveProbe STATIC)
```

Run: `cmake --preset win-x64-clang-debug && cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R "^StaticLink\."`
Expected: `ListedPackObjectIsExtracted` **PASS**（对照组本来就该绿——**顺带证明弱探针机制真的能报「在」**），`UnlistedPackObjectIsNotExtracted` **FAIL**（引用了就被拉进来）。

> 这一跑同时是最好的**变异取证**：它把「引用链在不在」这一个变量单独翻了一次，其它一切不动。

- [ ] **Step 5: 撤销临时改动，跑通**

把 `KeepAliveProbe` 从 `PLUGINS` 里删掉（恢复 Task 6 的三员），跑：

Run: `cmake --preset win-x64-clang-debug && cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R "^StaticLink\."`
Expected: PASS（六条：Task 6 的两条 + 本任务的两条 + …）。

- [ ] **Step 6: 两条 Windows 线各跑一次**

Run: `Scripts\msvc-env.cmd cmake --preset win-x64-msvc-debug && Scripts\msvc-env.cmd cmake --build --preset win-x64-msvc-debug && ctest --preset win-x64-msvc-debug -R "^StaticLink\."`
Expected: 同结果。**若某条线编不过或探针不 armed**：按 spec D233 —— 先换写法（再找一条替代：`/alternatename` 的 CMake 级写法、或改探针为读产物符号表），两条线都试不通才按「降级 + 在 spec 留明缺口」处置，**不用 `GTEST_SKIP`**。

- [ ] **Step 7: 提交**

```bash
git add Tests/StaticLink
git commit -m "保活常驻证人：弱符号探针 + 弱引用对照组（D224/D233）"
```

### Task 8: 组合库上的进程级状态——**先红**（证人 #7）

**Files:**
- Create: `Tests/Integration/fixtures/StatefulPackA/…` + `StatefulPackB/…`（或复用既有带状态 fixture，见下）
- Modify: `Tests/Integration/fixtures/CMakeLists.txt`（加 `PACK`）
- Modify: `Tests/Integration/InProcessSourceTests.cpp`（加 #7）

**Interfaces:**
- Consumes: Task 5 的 `vase_pack_plugins`；既有 `StatefulPlugin`（`Tests/HotSwap/fixtures/StatefulPlugin/StatefulPlugin.cpp`）
- Produces: 无新公开面

> **本任务只写红的测试，不改实现**——Task 9 才修。任务边界这么切，是为了让「先红」这件事被人审到（spec §5.5 第 3 点：它既是 D236 的判决材料，也是它的一次性变异取证）。

- [ ] **Step 1: 造一个「一库两插件、都带进程级状态」的组合库**

`Tests/Integration/fixtures/CMakeLists.txt` 里，给两个**带 `ProcessStates`** 的 fixture 加 `PACK`（源选现有带状态者；若现成的只有一个，就照 `StatefulPlugin/StatefulPlugin.cpp` 复制出第二个、Id 不同、状态名不同）。然后在 `Tests/CMakeLists.txt` 里建这个动态组合库：

```cmake
# #7 的组合库（本波）：成员都带 ProcessStates——三员 Sample 没有，故 #1/#2 结构上覆盖不到
# MakeInstance 的覆盖缺陷（spec §2.5）。用**动态**面：不受「一宿主至多一个静态组合库」限制。
vase_pack_plugins(VasePluginsStateful PLUGINS StatefulPackA StatefulPackB)
target_compile_definitions(VaseTests PRIVATE
    VASE_FIXTURE_VASEPLUGINSSTATEFUL="$<PATH:CMAKE_PATH,$<TARGET_FILE:VasePluginsStateful>>")
```

- [ ] **Step 2: 写测试**

`Tests/Integration/InProcessSourceTests.cpp` 追加：

```cpp
// 一库 N 插件时的进程级状态（spec §2.5 / D236/D237）：Eject 必须重置**全部**成员的
// 登记状态，不是只剩最后装配那一个。本用例先以「未修」状态红——那是 D236 的判决材料。
TEST(CombinedLibraryHost, EjectResetsEveryMemberProcessState)
{
    vase::PluginHost host;
    vase::LoadPlan plan;
    const std::vector<std::string> ids = {"Vase.Test.StatefulPackA", "Vase.Test.StatefulPackB"};
    for (const std::string& id : ids)
    {
        vase::LoadPlanEntry entry{};
        entry.Id = id;
        entry.BinaryPath = FixturePath(VASE_FIXTURE_VASEPLUGINSSTATEFUL);
        plan.Ordered.push_back(entry);
    }
    const vase::Result<vase::PodHandle> created = host.CreatePod(plan);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();

    // 逐个 Eject：只有最后一个能把整份 binary 请出架、从而触发 Reset。
    // 前面的 Eject 会因「别局还有持有者」被 kept-resident（那是设计的），故本用例在一个 Pod 里
    // 依次拆——最后那次的 EjectReport 必须点名**两个**成员各自的状态名。
    vase::Result<vase::EjectReport> last = vase::Result<vase::EjectReport>::Err(vase::Error{"unset"});
    for (const std::string& id : ids)
    {
        last = host.EjectPlugin(created.Value(), id);
        ASSERT_TRUE(last.IsOk()) << last.GetError().Message();
    }
    ASSERT_EQ(last.Value().Status, vase::EjectStatus::kEjected);
    EXPECT_EQ(last.Value().ProcessStatesReset.size(), 2U)
        << "只有 " << last.Value().ProcessStatesReset.size() << " 个成员的状态被重置——"
        << "MakeInstance 的覆盖缺陷（spec §2.5）";
    static_cast<void>(host.DestroyPod(created.Value()));
}
```

- [ ] **Step 3: 跑测试确认它红**

Run: `cmake --preset win-x64-clang-debug && cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R "EjectResetsEveryMemberProcessState"`
Expected: **FAIL**，正文是 `只有 1 个成员的状态被重置`。

**把这条红记下来**（贴进 plan 的偏离登记或 commit message）——它是 D236 的判决材料。若它**不红**，说明 fixture 的状态没被登记或 Eject 走的不是那条路，**停下来查清楚再往下走**。

- [ ] **Step 4: 提交（红着的测试）**

```bash
git add Tests/Integration/InProcessSourceTests.cpp Tests/Integration/fixtures Tests/CMakeLists.txt
git commit -m "证人 #7 先红：一库 N 插件的进程级状态只重置了最后一个（D238 判决材料）"
```

---

### Task 9: 修 `MakeInstance` 的覆盖缺陷（D236/D237）——**后修**

**Files:**
- Modify: `Include/Vase/Host/Loader.h`（`BinaryRecord` 的状态字段）
- Modify: `Source/Host/PluginHost.cpp:596-597`（写点）与 `:914-919`（读点）
- Test: Task 8 的 `CombinedLibraryHost.EjectResetsEveryMemberProcessState` 转绿

**Interfaces:**
- Consumes: Task 8 的红
- Produces: `vase::detail::BinaryRecord::ProcessStateTables`（`std::vector<vase::detail::ProcessStateTable>`）、`vase::detail::BinaryRecord::RegisterProcessStates(std::string_view, const vase::ProcessStateDesc*, std::size_t)`

- [ ] **Step 1: 改记录的形状**

`Include/Vase/Host/Loader.h` 里把 `BinaryRecord` 的两行

```cpp
    const ProcessStateDesc* ProcessStates = nullptr;
    std::size_t ProcessStateCount = 0;
```

换成：

```cpp
    // 一个二进制里**每插件一份**的进程级状态表。旧形是单份裸指针——它把「一 binary ⇔ 一插件」
    // 当真，一二进制 N 插件（组合库）时后写覆盖先写（D236 记名）。新形按插件 Id 去重累积，
    // Eject 逐条 Reset（D237：粒度是**二进制级**，与 D91 的判据和 Eject 全局闸同粒度）。
    std::vector<ProcessStateTable> ProcessStateTables;
```

并在 `BinaryRecord` **之前**加：

```cpp
struct ProcessStateTable
{
    std::string_view PluginId; // 去重键：同一插件在多局里反复装配只记一份
    const ProcessStateDesc* Begin = nullptr;
    std::size_t Count = 0;
};
```

在 `BinaryRecord` 内加成员函数（**不破坏聚合性**——`.Path = …, .Raw = …` 的指定初始化照常）：

```cpp
    // 幂等：同 Id 只记一次（同一插件跨 Pod 反复装配不该累积）。
    void RegisterProcessStates(std::string_view pluginId, const ProcessStateDesc* begin, std::size_t count)
    {
        const auto found = std::ranges::find_if(ProcessStateTables, [pluginId](const ProcessStateTable& table)
                                                { return table.PluginId == pluginId; });
        if (found == ProcessStateTables.end())
        {
            ProcessStateTables.push_back(ProcessStateTable{.PluginId = pluginId, .Begin = begin, .Count = count});
        }
    }
```

（`Loader.h` 已 `#include <algorithm>`；若没有则补。）

- [ ] **Step 2: 改写点**

`Source/Host/PluginHost.cpp` 的 `MakeInstance`，把

```cpp
    // M3/D92：进程级状态表随装配点登记一次——CreatePod 与 Adopt 两条路径共用 MakeInstance，
    // 故单点覆盖；config 失败早退也在它之后，失败插件的进程级状态照样登记（D99 要 Eject 时重置它）。
    record.ProcessStates = desc->Meta->ProcessStates.Begin();
    record.ProcessStateCount = desc->Meta->ProcessStates.Size();
```

改成：

```cpp
    // M3/D92 + 本波 D237：进程级状态表按插件 Id **累积**登记——一二进制 N 插件时不能覆盖。
    // 仍住装配点：CreatePod 与 Adopt 共用本函数，config 失败早退也在它之后，
    // 失败插件的状态照样登记（D99 要 Eject 时重置它）。
    record.RegisterProcessStates(desc->Meta->Id, desc->Meta->ProcessStates.Begin(),
                                 desc->Meta->ProcessStates.Size());
```

- [ ] **Step 3: 改读点**

`Source/Host/PluginHost.cpp` 的 `EjectPlugin`，把 Reset 循环

```cpp
        for (std::size_t index = 0; index < binary->ProcessStateCount; ++index)
        {
            const ProcessStateDesc& state = *std::next(binary->ProcessStates, static_cast<std::ptrdiff_t>(index));
            state.Reset();
            report.ProcessStatesReset.emplace_back(state.Name); // 报告是返回值，必须拥有（借用教训同格）
        }
```

改成：

```cpp
        for (const detail::ProcessStateTable& table : binary->ProcessStateTables)
        {
            for (std::size_t index = 0; index < table.Count; ++index)
            {
                const ProcessStateDesc& state = *std::next(table.Begin, static_cast<std::ptrdiff_t>(index));
                state.Reset();
                report.ProcessStatesReset.emplace_back(state.Name); // 报告是返回值，必须拥有（借用教训同格）
            }
        }
```

- [ ] **Step 4: 跑 #7，确认转绿**

Run: `cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R "EjectResetsEveryMemberProcessState"`
Expected: PASS（`ProcessStatesReset.size() == 2`）。

- [ ] **Step 5: 跑规矩 6 的族（D236 的直接回归面）**

Run: `ctest --preset win-x64-clang-debug -R "HotSwap|Eject|Adopt"`
Expected: 全绿，**特别是** `Eject.ProcessStatesResetMakesReloadLikeFirstTime`、`Eject.ProcessStatesKeptWhenOtherPodHoldsLiveInstance`、`Eject.ProcessStatesResetCoexistsWithKeptResidentImage`、`…Shell` 四条——它们钉的就是这段语义。

- [ ] **Step 6: 提交**

```bash
git add Include/Vase/Host/Loader.h Source/Host/PluginHost.cpp
git commit -m "修 MakeInstance 覆盖共享记录进程级状态：改二进制级并集（D236/D237）"
```

---

### Task 10: 收口——八线全量、tidy、format、文书

**Files:**
- Modify: `CLAUDE.md`、`wiki/vase-architecture.md`、`README.md`
- Modify（按需）：`.claude/skills/vase-cpp-engineering/references/{abi-boundary,plugin-lifecycle}.md`

- [ ] **Step 1: 单线全量**

Run: `ctest --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -N`
Expected: 全绿；`Total Tests` 与「构建与测试」的按线分账表**逐位对上**（本波新增：`VaseTests` 里 4 条 + `VaseStaticLinkTests` 的 4 条；`ctest -N` 的数按实测落账）。

- [ ] **Step 2: 八线删树重配全量**

Run（Windows）：`Scripts\win-verify.cmd`
Run（Linux）：`wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && bash Scripts/linux-verify.sh'`
Run（macOS，在 macOS 开发机上）：`bash Scripts/macos-verify.sh`
Expected: 八线全绿、构建零警告。**退出码非 0 先重跑那一条线再判**（`CLAUDE.md` 记了四类环境性假红）。

- [ ] **Step 3: 规矩 6 的三平台证据**

Run: `ctest --preset win-x64-clang-debug -R "HotSwap|Eject|Adopt"`（Windows 两条 debug 线各一次）
Run: Linux 同选择子；macOS 同选择子（在 macOS 开发机上）
Expected: 三平台各自留证据；条数与 `CLAUDE.md` 记的现值对上（本波若新增含该子串的用例名，按实测更新那一段）。

- [ ] **Step 4: tidy 四线**

Run: `Scripts\win-clang-tidy.cmd`、`bash Scripts/linux-clang-tidy.sh`、`bash Scripts/macos-clang-tidy.sh`
Expected: **三判据全过**（退出 0 + 正文 `error:` 0 + 正文 `warning:` 0）。新 TU 会让 TU 数与抑制合计上跳——按实测改「静态检查与格式」那张表。

- [ ] **Step 5: format**

Run: `git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror`
Expected: RC=0。（macOS 侧另用 `clang-format-mp-23` 复核同一条。）

- [ ] **Step 6: 文书**

按 spec §9 逐条落：
- `CLAUDE.md`：项目状态一笔（进程内来源机制 + 七条证人 + 拒绝面 + `MakeInstance` 缺陷的发现与修复；甲/乙/丁余项）；规矩 5 段补 `STATIC` 与 `VASE_PACK_INTERMEDIATE_DIR`；「构建与测试」「静态检查与格式」两表按实测改。
- `wiki/vase-architecture.md`：§8.1 进/出表加「静态链入（进程内来源）」一行；§8.6 末条 iOS 例外挂现状注；§9.1 补「进程内来源上 Reset 无触发路径」；§13.2 加指针；**§13.3 补一条「接受的缺口」（D229）**；§12.3 的 M5 行补现状指针。
- `README.md`：产物落位段补 `pack-objects/` 中间目录与 STATIC 组合库的落位。
- 复核技能两处 references。

- [ ] **Step 7: 提交**

```bash
git add CLAUDE.md wiki/vase-architecture.md README.md .claude/skills/vase-cpp-engineering
git commit -m "静态链入波文书：项目状态 / §8.1 表 / §9.1 / §13.3 缺口 / 中间目录落位"
```

---

## 偏离登记

实现过程中发现的、与 spec 或本计划不同的地方，逐条记在这里（形制照仓库惯例：一句话说清「计划原文 → 实际」与理由）。

| # | 条目 |
|---|---|
| 1 | **`PluginEntryPoint` 的归属**：spec §2.2 写「`LoadPlan.h` 新增（`PluginDescriptor` 前向声明即可）」，实际住 `Include/Vase/PluginDescriptor.h`。理由：`Loader.h` 也要用它，两处各定义一份即同类型的第二份声明（规矩 7）。 |
| 2 | **进程内来源的枚举面**：本波 `Symbol` 对进程内记录只放行 `VasePlugin_GetPlugin`，`VasePlugin_Descriptors` 取不到（spec §5.2 未提）。理由：没有任何消费者需要它（`PluginHost` 只按 Id 取；四条 VaseCli 命令永远拿不到进程内来源）。首次有消费者时再加一个槽。 |
| 3 | **弱探针的对照组用哪个插件**：Task 7 取 `VasePluginDesc_HelloPlugin`（三员之一）。若实现时发现该符号因可见性收紧而在**测试可执行**里解析不到（`CXX_VISIBILITY_PRESET hidden` 对静态链入通常无影响，但别假设），换一个已列入的成员即可——**换的是哪一枚不重要，重要的是它走同一套弱机制**。 |
| 4 | **T7 的 Windows 探测机制被整段证伪，改走读产物导出表**（**本条即是 Task 7 Step 2 ③ 那段的勘误**，那段原文保留为「被证伪的初版」）：`/alternatename` 实测**不阻止归档提取**（lld-link 与 link.exe 同），且「比较两枚函数地址」被编译器**常量折叠、引用不发**；clang-cl 的 COFF `__attribute__((weak))` 亦不可用——符号被他处调用时分发表会把该调用解析成 null、进程启动即崩。**实际形**：探针描述符带 `VASE_EXPORT`（`dllexport`），对象被提取才在映像里留下导出 ⇒ `GetProcAddress(GetModuleHandleW(nullptr), "VasePluginDesc_…") == nullptr` ⇔ 未提取；两枚判据走同一套机制。**两条 Windows 线结论不同**：clang-cl 成立，**cl.exe 不成立**（`link.exe` 为 dllexport **强制提取**归档成员 ⇒ 探针恒在场），按 D233 降级为「对照组 + 一条平台事实金丝雀」。**未建** `KeepAliveProbeFallback.cpp`（依赖已证伪的机制）。见 spec §5.2 与 **D240**。 |
| 5 | **探针 fixture 需覆盖 `VASE_PLUGIN_CLASS`**：plan 的 fixture 类名 `KeepAliveProbePlugin` ≠ target 名 `KeepAliveProbe`，而 `vase_add_plugin_fixture` 无条件把该属性设成 target 名、`vase_pack_plugins` 按它拼 `VasePluginDesc_<类名>`。故在 fixture 的 `CMakeLists.txt` 显式覆盖。审查判**不违规**（该属性是被设计的扩展点，不属 D210 辖域；D198 辖的是「两函数不加 `CLASS` 关键字」）。 |
| 6 | **cl.exe 降级支的 `#if` 条件收窄**：helper 守卫用 `#if defined(_MSC_VER) && !defined(__clang__)` 而非 `#ifdef _WIN32`——后者在 clang-cl 上定义出无人调用的 helper，被 `/WX` 下的 `-Wunused-function` 打成构建失败（实测）。收窄后定义与使用同步消失，三种编译环境各自自洽。（**条目 8 已把这一整支删除**，本条保留为过程记录。） |
| 7 | **Mach-O 的观测手段改 `dlsym(RTLD_SELF, …)`（T10 实测，替代 `__attribute__((weak))`）**：T10 八线首跑 macOS ×2 在 `StaticLink.UnlistedPackObjectIsNotExtracted` 转红，调查（`task-10b-probe-investigation.md`）证 **Apple ld 会把弱未定义引用当提取压力**（与 GNU ld / lld-link 分岔），且**那处 weak 引用本身就是元凶**（决定性实验：拿走它，成员随即从映像消失）⇒ 弱探针在 Mach-O 上**自败**。macOS 支改用 `dlsym`（收字符串、无链接期引用、实测端到端绿）。**Linux 支维持弱符号**（实测不污染）。**controls 仍与承重位同机制**（spec §5.2 的硬要求按平台各自成立）。见 spec §5.2 / D241。 |
| 8 | **cl.exe 降级支整体删除（spec D240 的平台事实被证伪）**：T10 八线首跑 cl.exe ×2 红——金丝雀断言「探针在场」而冷构建实测**不在**。调查证 **`link.exe` 不**为 dllexport 强制提取归档成员（最小复现 + 真树同序列 + 控制三重锁定）；**T7 的假读数根因 = MSVC 增量链接（debug 线 `/INCREMENTAL`）+ 未删的 `bin/*.ilk` 保留幽灵导出**，且 T7 的「干净形态」没删 `bin/`。⇒ cl.exe 与 clang-cl 同支（断 `KeepAliveProbeAbsent()`），删 `KeepAliveProbePresent()` 与那两处 `#if defined(_MSC_VER) && !defined(__clang__)`（含条目 6 收窄的那处）。**D240 记的「8 线中的 2 线不可证」缺口不存在**。见 spec §5.2 / **D241**。 |
| 9 | **文书轮切分（R14）**：Task 10 Step 6 原定一个文书任务做完 `CLAUDE.md` + `wiki` + `README` + 技能；实际**切分**——`wiki` / `README` / 技能（正确性只取决于**机制**）并行派发，**`CLAUDE.md`（承载按线基数表与 tidy 表、数字要等最终验证轮）由控制方亲自做**。理由：解锁并行、少一轮串行等待。代价：两份文书的措辞由控制方在 `CLAUDE.md` 一侧对齐。 |
| 10 | **本波 gtest 用例净增量的 brief 少算**：plan Task 10 Step 1 写「`VaseTests` 里 4 条 + `VaseStaticLinkTests` 的 4 条」（共 8）；**实测净 +10**——`InProcessSourceTests` 5 条（非 4）+ `StaticLinkTests` 4 条 + `Loader.InProcessEntryResolvesWithoutPlatformLoad` 1 条（brief 整个漏记）。已按 `git diff main..HEAD -- Tests/ \| grep -c '^+TEST('` 实测定准，落进 `CLAUDE.md` 的基数表算式。 |
| 11 | **「#1 与 #2 共用同一份断言体」实现为两份逐字重复的断言**（spec §5.1 原文要「共用」）：两条证人住**两个不同的可执行**（`VaseTests` 与 `VaseStaticLinkTests`），共用需经 `Tests/TestingSupport/` 抽一个 helper。**判**：意图（同一批 Sample、同一组读数）已兑现，字面的「一份」未兑现，**如实记名、本波不抽 helper**（抽 helper 会给两个可执行各加一层间接，收益不抵）。 |

## Self-Review

**Spec coverage：** D216→T2、D217→T2、D218→T1、D219→T3、D220→T2(Step 6)、D221→（无代码，§3.1 的如实记录，由 T10 的文书承载）、D222→T5、D223→T6、D224→T7、D225→T4/T6、D226→（范围声明，T10 文书）、D227→（约束，Global Constraints）、D228→T3/T9/T10、D229→T3（行为）+T10（§13.3 记名）、D230→T5/T6（`STATIC` 一宿主一个的形态由见证可执行结构体现）+T10 记名、D231→T5、D232→T1、D233→T7(Step 6)、D234→T2、D235→T1、D236/D237→T8/T9、D238→T8、D239→T1(Step 5④)/T2(Step 6)。

**类型一致性：** `PluginEntryPoint`（T1 定义，T2/T6 使用）；`EnsureInProcess`（T1 定义，T2 使用）；`BinaryRecord::InProcess` / `Entry`（T1 定义，T3 使用）；`ProcessStateTable` / `RegisterProcessStates`（T9 定义并即刻使用，无跨任务）；契约子串三枚（`both binary path and in-process entry set` / `linked-in binary cannot be ejected` / `adopt refused: file path required`）在生产者与测试中逐字相同。
