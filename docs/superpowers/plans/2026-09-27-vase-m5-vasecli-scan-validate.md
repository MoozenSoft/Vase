# Vase M5 第一波：VaseCli scan / validate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 兑现 `wiki` §11.1 的 `VaseCli scan` 与 `validate` 两条子命令——从二进制生成清单、并拿四项检查校验清单与二进制的一致性。

**Architecture:** 三段落：① **库面长四条必须的**——宏加枚举入口（纯追加）、Host 侧「读二进制全部描述符」+ 比对器提升为公开面、Catalog 侧命名正反函数提升 + 清单序列化面；② **工具面** `Tools/VaseCli`（内部静态库 `VaseCliCore` + 薄 `main`），编排住工具、库只出原语；③ **连带**：`Samples/DependentPlugin` 服务改名、M4 遗留同域两笔。**不动**插件描述符布局、`kHeaderVersion`、既有比对域规则。

**Tech Stack:** C++20 / CMake + vcpkg / GoogleTest / 六 preset（Win clang-cl & cl.exe、Linux clang + libc++）/ clang-tidy 23.1.0 / 全项目关异常 / nlohmann-json（只住 VaseCatalog 的 PRIVATE 面）。

**Spec:** `docs/superpowers/specs/2026-09-27-vase-m5-vasecli-scan-validate-design.md`（决定 D117–D140；执行时以 spec 为准，本计划是它的展开。spec 的「事实取证注」十六条是本文所有代码块引用的事实来源。）

## Global Constraints

- **不使用 C++ 异常**：不写 `throw` / `try` / `catch`；测试里**不用** `EXPECT_THROW` 一族（编译期硬失败）。错误一律经 `Result<T>` / `Error` 显式返回。
- **Vase 自己的头一律引号包含**：`#include "Vase/Plugin.h"`，不写尖括号（尖括号会让 `/W4 /WX` 静默失效）。
- **新 target 必须链 `VaseBuildOptions`**；**插件 target 一律经 `vase_add_plugin_fixture`**（它自动带 `VaseBuildOptions` 与 hidden 可见性），不手写 `add_library(... SHARED)`。
- **命名规范由 `.clang-tidy` 强制**：类/函数/成员 `CamelCase`，参数/局部 `camelBack`，常量 `k` + `CamelCase`，命名空间 `lower_case`。
- **格式化**：`.clang-format` 是 LLVM 基线 + 多处偏离（Allman、`PointerAlignment: Left`、构造初始化表每式一行且逗号在行首、`template <...>` 与签名分两行）。
- **注释判据**：删掉它读者会不会踩坑；单条 ≤2 行、硬上限 3 行；论证属 spec/plan，代码只留结论 + 指针。
- **提交信息不带任何 AI 署名尾注**（不写 `Co-Authored-By:` / `Generated-with:` / `Signed-off-by:`）。
- **`Tests/HotSwap` 按主干对待**：本计划动 Loader 与描述符导出面的提交，收口波必须跑 `-R 'HotSwap|Eject|Adopt'`，**Windows 与 Linux 各自留证据**。
- **基数与 tidy 阈值只在 `CLAUDE.md`**，任何任务都不得把数字抄到别处（含技能）。
- **本波不动**：`kHeaderVersion`（仍为 4）、`PluginMeta` 布局、`ProcessStateDesc`、`ManifestExpectation` 的字段集、既有比对域规则（D72）、`Tools/VaseConsole` 的任何文件。
- **新增 `.cpp` 必须手加进 `Tests/CMakeLists.txt:3-36` 的 `VaseTests` 源清单**（该清单无 GLOB）。
- **`add_subdirectory(Tools/VaseCli)` 必须排在 `add_subdirectory(Tests)` 之前**（`CMakeLists.txt:288` 与 `:290` 之间）——`VaseTests` 要链它的静态库。
- **开发线**：`win-x64-clang-debug`（Windows / Git Bash）；Linux 侧经 `wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && …'`。拼 WSL 命令时**别把 `$` 写进双引号**（外层 Git Bash 会先展开，命令照跑、结果假绿）。

---

## File Structure

| 文件 | 职责 | 动作 |
|---|---|---|
| `Include/Vase/PluginDescriptor.h` | `VASE_PLUGIN` 宏体 | 尾追加 `VasePlugin_Descriptors` 枚举入口（纯追加） |
| `Tests/Unit/fixtures/NoEnumeration/NoEnumeration.cpp` | 手写描述符、`HeaderVersion` 正确但**无枚举入口**的探针 | 新建 |
| `Tests/Unit/fixtures/NoEnumeration/CMakeLists.txt` | 该 fixture 的 target | 新建 |
| `Tests/Unit/DescriptorTests.cpp` | 宏侧用例 | 加一条进程内直调用例 |
| `Include/Vase/Host/Inspect.h` | 「读二进制全部描述符」的公开声明 | 新建 |
| `Source/Host/Inspect.cpp` | 上者的实现 + `HeaderVersion` 闸（两路读法共用） | 新建 |
| `Source/Host/Detail/InspectInternal.h` | 闸的内部声明（`PluginHost.cpp` 与 `Inspect.cpp` 共用） | 新建 |
| `Source/Host/PluginHost.cpp` | `InspectBinary` 改走共用闸 | 改（:84-89 那段换成一次调用） |
| `Source/Host/CMakeLists.txt` | VaseHost 源清单 | 加 `Inspect.cpp` |
| `Include/Vase/Host/ManifestExpectation.h` | 期望型 + **比对器声明**（提升） | 改（并入 `CompareDescriptor`） |
| `Source/Host/Detail/ManifestCompare.h` | 私有声明头 | **退役** |
| `Source/Host/Detail/ManifestCompare.cpp` | 比对器实现 | 改（加 `VASE_HOST_API`、改 include、改头注释） |
| `Include/Vase/Catalog/LibraryFileName.h` | 库文件命名**正反两函数**（公开面） | 新建（自 `Source/Catalog/Detail/` 提升） |
| `Source/Catalog/Detail/LibraryFileName.h` | 提升前的家 | **退役** |
| `Source/Catalog/Solve.cpp`、`PluginCatalog.cpp` | 两处 include 与调用点 | 改（换头路径） |
| `Tests/Integration/SolveTests.cpp` | `ExpectedBinaryPath` 那份平台规则副本 | 改（收编，改调公开函数） |
| `Include/Vase/Catalog/ManifestView.h` | 清单值形 + **序列化入口声明** | 改（加 `WriteManifestFile`） |
| `Source/Catalog/ManifestJson.cpp` | 解析面 + **序列化面**（同一 TU，键白名单同源） | 改 |
| `Tools/VaseCli/CMakeLists.txt` | `VaseCliCore` 静态库 + `VaseCli` 可执行 | 新建 |
| `Tools/VaseCli/main.cpp` | argv 解析与退出码 | 新建 |
| `Tools/VaseCli/Cli.h` / `Cli.cpp` | 子命令分发 | 新建 |
| `Tools/VaseCli/Scan.h` / `Scan.cpp` | `scan` 编排：发现 → 生成 → 回写 | 新建 |
| `Tools/VaseCli/Validate.h` / `Validate.cpp` | `validate` 编排：四项检查 | 新建 |
| `Tools/VaseCli/ManifestMerge.h` / `ManifestMerge.cpp` | 清单独有字段的保真合并（`scan` 专用） | 新建 |
| `Samples/HelloCommon/Farewell.h` | 服务接口名 | 改（`kName`） |
| `Samples/DependentPlugin/DependentPlugin.cpp` | 提供方声明 | 改（`Provides` + 注释） |
| `Tests/Integration/fixtures/manifests/dependent/plugin.json` | 该插件的手写清单 | 改（`provides[0].service`） |
| `Tests/Unit/LibraryFileNameTests.cpp` | 命名正反函数的 round-trip | 新建 |
| `Tests/Integration/VaseCliScanTests.cpp` | `scan` 的用例 | 新建 |
| `Tests/Integration/VaseCliValidateTests.cpp` | `validate` 的用例 | 新建 |
| `Tests/CMakeLists.txt` | 源清单、fixture 宏、`VaseCliCore` 链接 | 改 |
| `CMakeLists.txt` | `add_subdirectory(Tools/VaseCli)` | 改（:288 与 :290 之间） |
| `Tests/HotSwap/AdoptManifestTests.cpp` | M4 遗留的恒真断言 | 改（删一行） |
| `Tests/HotSwap/EjectTests.cpp` | D110 的跨 Pod falsifier | 改（加一条用例） |
| `wiki/vase-architecture.md` | 架构文档 | 改（spec §7 义务表） |
| `CLAUDE.md` | 命令、基数、规矩的唯一真值 | 改（项目状态 + 两张表） |

---

### Task 1: 宏加枚举入口

**Files:**
- Modify: `Include/Vase/PluginDescriptor.h:128-145`（`VASE_PLUGIN` 宏体，在 `VasePlugin_GetPlugin` 之后、`const ::vase::PluginMeta kVaseMeta_##Type` 之前）
- Test: `Tests/Unit/DescriptorTests.cpp`（该文件已展开过一次 `VASE_PLUGIN`，见其 `:60-70`）

**Interfaces:**
- Consumes: 无
- Produces: `extern "C" VASE_EXPORT const vase::PluginDescriptor* const* VasePlugin_Descriptors(std::uint32_t* outCount)` —— Task 2 的实现按这个名字取符号；`*outCount` 写回条数，返回值指向静态数组首元素，寿命 = 镜像驻留期。

- [ ] **Step 1: 写失败测试**

在 `Tests/Unit/DescriptorTests.cpp` 的 `TEST(Descriptor, GetPluginRoutesByIdentity)` 之后追加（该文件里 `VASE_PLUGIN(DescriptorProbePlugin)` 已展开，故这个入口可**进程内直调**，不需要 Loader）：

```cpp
TEST(Descriptor, EnumerationEntryReturnsExactlyTheOneDescriptor)
{
    std::uint32_t count = 0;
    const vase::PluginDescriptor* const* all = VasePlugin_Descriptors(&count);
    ASSERT_NE(all, nullptr);
    EXPECT_EQ(count, 1U); // 一个库一个插件（§3.1）：本宏恒 1 条，组合库由 VasePack 生成（§8.6）
    EXPECT_EQ(all[0], VasePluginDesc_DescriptorProbePlugin());
}
```

- [ ] **Step 2: 跑测试确认编译失败**

```bash
cmake --build --preset win-x64-clang-debug --target VaseTests
```

Expected: 编译错误，`VasePlugin_Descriptors` 未声明（`use of undeclared identifier`）。**这一条是本任务的存在理由**：宏没加之前它就是编不过。

- [ ] **Step 3: 宏体追加枚举入口**

在 `Include/Vase/PluginDescriptor.h` 的 `VASE_PLUGIN` 宏里，`VasePlugin_GetPlugin` 那个函数块**之后**、`const ::vase::PluginMeta kVaseMeta_##Type = ::vase::PluginMeta` 那行**之前**，插入：

```cpp
    /* 枚举面（M5/D118）：一个库里的全部描述符。纯追加——既有生成物一字不变 、*/                              \
    /* kHeaderVersion 不动。名字是契约：工具按字面取它；组合库由 VasePack 生成同名（§8.6）。  */              \
    extern "C" VASE_EXPORT const ::vase::PluginDescriptor* const* VasePlugin_Descriptors(::std::uint32_t* outCount) \
    {                                                                                                                  \
        static const ::vase::PluginDescriptor* const kAll[] = { VasePluginDesc_##Type() };                             \
        *outCount = 1;                                                                                                 \
        return kAll;                                                                                                   \
    }                                                                                                                  \
```

- [ ] **Step 4: 跑测试确认通过**

```bash
cmake --build --preset win-x64-clang-debug --target VaseTests
ctest --preset win-x64-clang-debug -R '^Descriptor\.' --output-on-failure
```

Expected: 该套件 5 条全过（新增 1 条 + 既有 4 条）。

- [ ] **Step 5: 跑一次全量确认没碰坏别处**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug
```

Expected: 全绿。**宏体是每个插件 TU 都展开的**，所以这一步不能省。

- [ ] **Step 6: 提交**

```bash
git add Include/Vase/PluginDescriptor.h Tests/Unit/DescriptorTests.cpp
git commit -m "M5-T1：VASE_PLUGIN 尾追加 VasePlugin_Descriptors 枚举入口（纯追加、不动布局）"
```

---

### Task 2: Host 侧读出全部描述符 + 缺入口的响亮失败

**Files:**
- Create: `Include/Vase/Host/Inspect.h`
- Create: `Source/Host/Inspect.cpp`
- Create: `Source/Host/Detail/InspectInternal.h`
- Create: `Tests/Unit/fixtures/NoEnumeration/NoEnumeration.cpp`
- Create: `Tests/Unit/fixtures/NoEnumeration/CMakeLists.txt`
- Modify: `Source/Host/PluginHost.cpp:59-91`（`InspectBinary` 的版本闸改走共用函数）
- Modify: `Source/Host/CMakeLists.txt:14-20`（加 `Inspect.cpp`）
- Modify: `Tests/CMakeLists.txt`（`:39` 旁加 `add_subdirectory(Unit/fixtures/NoEnumeration)`；`:121-122` 那块里加 `VASE_FIXTURE_NOENUMERATION`；`:3-36` 加新测试文件）
- Test: `Tests/Unit/InspectTests.cpp`（新建）

**Interfaces:**
- Consumes: Task 1 的 `VasePlugin_Descriptors`；`vase::detail::Loader::Symbol(const BinaryRecord&, std::string_view)`（已有，`Include/Vase/Host/Loader.h:72`）
- Produces:
  - `VASE_HOST_API vase::Result<std::vector<const vase::PluginDescriptor*>> vase::InspectDescriptors(const vase::detail::BinaryRecord& record);` —— 返回**借用镜像**的指针，寿命 = 该记录的驻留期
  - `vase::Result<void> vase::detail::CheckHeaderVersion(const vase::PluginDescriptor& desc);`（内部，`InspectInternal.h`）

- [ ] **Step 1: 造「缺枚举入口」的 fixture**

`Tests/Unit/fixtures/NoEnumeration/NoEnumeration.cpp`（形态照 `Tests/Integration/fixtures/StaleHeaderPlugin/StaleHeaderPlugin.cpp`，差别只有两条：`HeaderVersion` 用**正确的** `vase::kHeaderVersion`，且**不导出**枚举入口——单变量隔离）：

```cpp
// 缺枚举入口探针（M5/D118）：手写描述符，除「没有 VasePlugin_Descriptors」外一切正常。
// HeaderVersion 刻意取正确值——与 StaleHeaderPlugin 的区别就在这里：那条撞的是版本闸，
// 本条要撞的是枚举闸，单变量才判得出是哪一道闸在响（spec 事实取证注⑬/§2.1）。
#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"
#include "Vase/PluginDescriptor.h"
#include "Vase/Pod/Context.h"

#include <memory>
#include <string_view>

namespace
{

class NoEnumerationPluginImpl final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Ok();
    }
};

const vase::PluginMeta kMeta{
    .Id = "Vase.NoEnumeration",
    .DisplayName = "缺枚举入口探针",
    .Version = "0.0.0",
    .Requires = {},
    .Provides = {},
};

vase::Plugin* CreateStub() { return std::make_unique<NoEnumerationPluginImpl>().release(); }

void DestroyStub(vase::Plugin* raw) { const std::unique_ptr<vase::Plugin> owning{raw}; }

const vase::PluginDescriptor kDesc{
    .HeaderVersion = vase::kHeaderVersion, // ← 正确值：本条不该撞版本闸
    .Meta = &kMeta,
    .Create = &CreateStub,
    .Destroy = &DestroyStub,
};

} // namespace

// 只导出装载跳（§8.1）；**刻意不导出** VasePluginDesc_* 与 VasePlugin_Descriptors。
// 符号名是 ABI 契约（Loader 按字面查），不能改名——手写处没有宏展开那层豁免，故就地抑制。
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" VASE_EXPORT const vase::PluginDescriptor* VasePlugin_GetPlugin(const char* id)
{
    return std::string_view{id} == kMeta.Id ? &kDesc : nullptr;
}
```

`Tests/Unit/fixtures/NoEnumeration/CMakeLists.txt`：

```cmake
vase_add_plugin_fixture(NoEnumeration SOURCES NoEnumeration.cpp LINK_LIBRARIES VasePod)
```

- [ ] **Step 2: 接线（fixture 子目录 + 路径宏 + 测试文件）**

`Tests/CMakeLists.txt` 三处改动：

① `:39` 的 `add_subdirectory(Unit/fixtures/LoadProbe)` **之后**加一行：

```cmake
add_subdirectory(Unit/fixtures/NoEnumeration)
```

② `:121-122` 那块（`NoIdentityPlugin` 的定义之后）追加：

```cmake
target_compile_definitions(VaseTests PRIVATE
    VASE_FIXTURE_NOENUMERATION="$<PATH:CMAKE_PATH,$<TARGET_FILE:NoEnumeration>>")
```

③ `:3-36` 的 `add_executable(VaseTests ...)` 源清单里，`Unit/LoaderTests.cpp` 之后加：

```cmake
    Unit/InspectTests.cpp
```

- [ ] **Step 3: 写失败测试**

`Tests/Unit/InspectTests.cpp`：

```cpp
// Host 侧枚举读法（M5/D118/D123）：读二进制里的全部描述符，两条失败面各有单变量证人。
#include "Vase/Host/Inspect.h"

#include "Vase/Detail/Result.h"
#include "Vase/Host/Loader.h"
#include "Vase/PluginDescriptor.h"

#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace
{

std::filesystem::path FixturePath(const char* defineValue) { return std::filesystem::path{defineValue}; }

TEST(Inspect, ReadsEveryDescriptorFromARealBinary)
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> record = loader.EnsureResident(FixturePath(VASE_FIXTURE_LOADPROBE));
    ASSERT_TRUE(record.IsOk()) << record.GetError().Message();

    const vase::Result<std::vector<const vase::PluginDescriptor*>> all = vase::InspectDescriptors(*record.Value());
    ASSERT_TRUE(all.IsOk()) << all.GetError().Message();
    ASSERT_EQ(all.Value().size(), 1U);
    EXPECT_EQ(all.Value()[0]->Meta->Id, "Vase.LoadProbe"); // 借用镜像，仍在驻留期故可解引用
    loader.Unload(*record.Value());
}

TEST(Inspect, MissingEnumerationEntryIsLoudAndNamed)
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> record =
        loader.EnsureResident(FixturePath(VASE_FIXTURE_NOENUMERATION));
    ASSERT_TRUE(record.IsOk()) << record.GetError().Message();

    const vase::Result<std::vector<const vase::PluginDescriptor*>> all = vase::InspectDescriptors(*record.Value());
    ASSERT_FALSE(all.IsOk());
    // 响亮且点名原因：不许静默返回「零个插件」（D118 的静默陷阱）。
    EXPECT_NE(all.GetError().Message().find("VasePlugin_Descriptors"), std::string::npos);
    loader.Unload(*record.Value());
}

TEST(Inspect, StaleHeaderIsRejectedByTheSharedGate)
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> record = loader.EnsureResident(FixturePath(VASE_FIXTURE_STALEHEADER));
    ASSERT_TRUE(record.IsOk()) << record.GetError().Message();

    const vase::Result<std::vector<const vase::PluginDescriptor*>> all = vase::InspectDescriptors(*record.Value());
    ASSERT_FALSE(all.IsOk());
    // StaleHeaderPlugin 手写描述符、没有枚举入口，故先撞枚举闸；文案与加载期那条**同源**
    // （共用 CheckHeaderVersion）由 InspectBinary 一侧的既有用例守，这里只钉「它没过」。
    EXPECT_FALSE(all.GetError().Message().empty());
    loader.Unload(*record.Value());
}

} // namespace
```

- [ ] **Step 4: 跑测试确认失败**

```bash
cmake --build --preset win-x64-clang-debug --target VaseTests
```

Expected: 编译失败——`Vase/Host/Inspect.h` 不存在。

- [ ] **Step 5: 写公开头与内部头**

`Source/Host/Detail/InspectInternal.h`（Host 内部，不进公开面；`PluginHost.cpp` 与 `Inspect.cpp` 共用一道闸）：

```cpp
#pragma once

// 两条描述符读法共用的版本闸（M5/D123）：按 Id 读（InspectBinary，CreatePod/Adopt 走）
// 与枚举读（InspectDescriptors，工具走）必须过同一段代码——文案因此也只有一份。

#include "Vase/Detail/Result.h"
#include "Vase/PluginDescriptor.h"

namespace vase::detail
{

// 失败文案是契约（回放证人钉它）：加载线既有的原文一字不改地搬到这里。
[[nodiscard]] Result<void> CheckHeaderVersion(const PluginDescriptor& desc);

} // namespace vase::detail
```

`Include/Vase/Host/Inspect.h`：

```cpp
#pragma once

// 枚举读法（M5/D118/D123）：读一个二进制里的**全部**描述符。只给工具用——装载跳仍唯一
// （VasePlugin_GetPlugin，§8.1），宿主侧不走这里。
//
// 寿命纪律（借用面）：返回的指针指向镜像只读段，其内部的 string_view / MetaArray 同样如此。
// 寿命 = 传入 BinaryRecord 的驻留期——调用方必须在 Loader::Unload **之前**把需要的东西
// 物化成拥有形（scan → 清单值；validate → 期望与结论）。Unload 之后解引用是悬垂。

#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"
#include "Vase/PluginDescriptor.h"

#include <cstdint>
#include <vector>

namespace vase
{

namespace detail
{
struct BinaryRecord; // 声明在 Vase/Host/Loader.h；这里只取引用，不必拖进整个 Loader
} // namespace detail

// 装载/卸载的配对与归属留给调用方（工具自己 EnsureResident 与 Unload），与 CreatePod 同形。
// 缺枚举入口 / 入口返回空 / 条数为零 / 任一条 HeaderVersion 不符 —— 四种都是 Err，无静默降级。
VASE_HOST_API Result<std::vector<const PluginDescriptor*>> InspectDescriptors(const detail::BinaryRecord& record);

} // namespace vase
```

- [ ] **Step 6: 写实现**

`Source/Host/Inspect.cpp`：

```cpp
#include "Vase/Host/Inspect.h"

#include "Detail/InspectInternal.h"

#include "Vase/Host/Loader.h"

#include <cstdint>
#include <string>
#include <vector>

namespace vase::detail
{

Result<void> CheckHeaderVersion(const PluginDescriptor& desc)
{
    if (desc.HeaderVersion != kHeaderVersion)
    {
        return Result<void>::Err(Error{"HeaderVersion mismatch: binary " + std::to_string(desc.HeaderVersion) +
                                       ", host " + std::to_string(kHeaderVersion)});
    }
    return Result<void>::Ok();
}

} // namespace vase::detail

namespace
{

// 枚举入口的签名（§8.1 家族同形）：与 VASE_PLUGIN 生成的 VasePlugin_Descriptors 一致。
using EnumerationEntryPoint = const vase::PluginDescriptor* const* (*)(std::uint32_t*);

} // namespace

namespace vase
{

Result<std::vector<const PluginDescriptor*>> InspectDescriptors(const detail::BinaryRecord& record)
{
    const detail::Result<void*> symbol = detail::Loader::Symbol(record, "VasePlugin_Descriptors");
    if (!symbol.IsOk())
    {
        // 响亮且点名原因：老二进制照旧能**加载**（装载跳还在），只是工具读不了——不许返回空表。
        return Result<std::vector<const PluginDescriptor*>>::Err(
            Error{"binary has no VasePlugin_Descriptors entry (predates the M5 enumeration face)"});
    }

    // void* → 函数指针只有 reinterpret_cast 一条路（与 InspectBinary 同一处、同理由）。
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto enumerate = reinterpret_cast<EnumerationEntryPoint>(symbol.Value());
    std::uint32_t count = 0;
    const PluginDescriptor* const* all = enumerate(&count);
    if (all == nullptr || count == 0U)
    {
        return Result<std::vector<const PluginDescriptor*>>::Err(
            Error{"VasePlugin_Descriptors returned no descriptor"});
    }

    std::vector<const PluginDescriptor*> out;
    out.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const PluginDescriptor* desc = all[index];
        if (desc == nullptr)
        {
            return Result<std::vector<const PluginDescriptor*>>::Err(
                Error{"VasePlugin_Descriptors returned a null descriptor"});
        }
        const Result<void> version = detail::CheckHeaderVersion(*desc);
        if (!version.IsOk())
        {
            return Result<std::vector<const PluginDescriptor*>>::Err(version.GetError());
        }
        out.push_back(desc);
    }
    return Result<std::vector<const PluginDescriptor*>>::Ok(std::move(out));
}

} // namespace vase
```

- [ ] **Step 7: `PluginHost.cpp` 改走共用闸**

`Source/Host/PluginHost.cpp`：在 include 区加 `#include "Detail/InspectInternal.h"`，并把 `:83-89` 那一段

```cpp
    // 描述符的**第一个**字段先读：拦的是「插件与宿主 Vase 头版本不一致」（§3.1/§8.3，12 节 #12）
    if (desc->HeaderVersion != vase::kHeaderVersion)
    {
        return vase::Result<const vase::PluginDescriptor*>::Err(
            vase::Error{"HeaderVersion mismatch: binary " + std::to_string(desc->HeaderVersion) + ", host " +
                        std::to_string(vase::kHeaderVersion)});
    }
    return vase::Result<const vase::PluginDescriptor*>::Ok(desc);
```

替换为

```cpp
    // 描述符的**第一个**字段先读：拦的是「插件与宿主 Vase 头版本不一致」（§3.1/§8.3，12 节 #12）。
    // 闸抽到 detail::CheckHeaderVersion（M5/D123）：枚举读法过的是同一段代码、同一份文案。
    const vase::Result<void> version = vase::detail::CheckHeaderVersion(*desc);
    if (!version.IsOk())
    {
        return vase::Result<const vase::PluginDescriptor*>::Err(version.GetError());
    }
    return vase::Result<const vase::PluginDescriptor*>::Ok(desc);
```

- [ ] **Step 8: `Source/Host/CMakeLists.txt` 加源文件**

`:14-20` 的 `add_library(VaseHost SHARED ...)` 里，`Detail/ManifestCompare.cpp` 那一行之后加：

```cmake
    Inspect.cpp
```

- [ ] **Step 9: 跑测试确认通过**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R '^Inspect\.' --output-on-failure
```

Expected: 3 条全过。

- [ ] **Step 10: 确认没碰坏加载线（闸改动的回归面）**

```bash
ctest --preset win-x64-clang-debug -R 'HotSwap|Eject|Adopt' --output-on-failure
```

Expected: 全过。`InspectBinary` 是 CreatePod 与 Adopt 的必经之路，这一步是**这条改动唯一的回归闸**。

- [ ] **Step 11: 提交**

```bash
git add Include/Vase/Host/Inspect.h Source/Host/Inspect.cpp Source/Host/Detail/InspectInternal.h \
        Source/Host/PluginHost.cpp Source/Host/CMakeLists.txt \
        Tests/Unit/fixtures/NoEnumeration Tests/Unit/InspectTests.cpp Tests/CMakeLists.txt
git commit -m "M5-T2：Host 枚举读法 InspectDescriptors + 版本闸抽成两路共用（缺入口响亮点名）"
```

---

### Task 3: `CompareDescriptor` 提升为 VaseHost 公开面

**Files:**
- Modify: `Include/Vase/Host/ManifestExpectation.h`（并入声明）
- Modify: `Source/Host/Detail/ManifestCompare.cpp:1-2, 339-344`（改 include、加导出宏、改注释）
- Delete: `Source/Host/Detail/ManifestCompare.h`
- Modify: 任何 `#include "Detail/ManifestCompare.h"` 的调用方（`Source/Host/PluginHost.cpp`）

**Interfaces:**
- Consumes: 无
- Produces: `VASE_HOST_API [[nodiscard]] vase::Result<void> vase::CompareDescriptor(const vase::ManifestExpectation& expected, const vase::PluginDescriptor& desc);` —— Task 11（`validate` 第①项）直接调它

- [ ] **Step 1: 查全部调用方**

```bash
grep -rn "ManifestCompare.h" Source/ Include/ Tests/ Tools/
```

Expected: 只有 `Source/Host/Detail/ManifestCompare.cpp:1` 与 `Source/Host/PluginHost.cpp`（include 行）。把实际命中逐条记下——**声明头退役时这几处必须同时改**，漏一处会编译失败（响亮，不是静默）。

- [ ] **Step 2: 声明并入公开头**

`Include/Vase/Host/ManifestExpectation.h`：在文件头注释里补一句，并在 `struct ManifestExpectation` 之后、`} // namespace vase` 之前加：

```cpp
// 加载期期望 vs 二进制描述符的**唯一**判据（D72 域规则）与唯一消息格式器（spec §6）。
// M5/D123 起是公开面：validate 与加载期必须走同一份实现（§4.4），这是唯一能保证的形态。
[[nodiscard]] VASE_HOST_API Result<void> CompareDescriptor(const ManifestExpectation& expected,
                                                          const PluginDescriptor& desc);
```

同时把该头的 include 区补上（否则声明用不了这两个类型）：

```cpp
#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"
#include "Vase/PluginDescriptor.h"
```

- [ ] **Step 3: 实现侧改 include 与导出宏**

`Source/Host/Detail/ManifestCompare.cpp`：

① `:1` 的 `#include "ManifestCompare.h"` 换成

```cpp
#include "Vase/Host/ManifestExpectation.h"
```

② `:26` 的匿名命名空间**不动**（14 个 helper 全部内部链接，照旧）。

③ `:344` 的定义行加导出宏：

```cpp
VASE_HOST_API Result<void> CompareDescriptor(const ManifestExpectation& expected, const PluginDescriptor& desc)
```

④ 文件头注释按 spec §5 风险 3 改准：删掉「DLL 内私有面」那句（它已经是对外契约的一部分），补一句「改动会同时影响加载期与 `validate`」。

- [ ] **Step 4: 删私有声明头**

```bash
git rm Source/Host/Detail/ManifestCompare.h
```

- [ ] **Step 5: 改调用方 include**

`Source/Host/PluginHost.cpp`：把 `#include "Detail/ManifestCompare.h"` 一行删掉（`Vase/Host/ManifestExpectation.h` 已经经由 `Vase/Host/LoadPlan.h` 进到它，若编译报缺声明就在 include 区显式加 `#include "Vase/Host/ManifestExpectation.h"`）。

- [ ] **Step 6: 构建 + 跑比对线全量**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R 'LoadTimeComparison|Adopt|HotSwap|Eject' --output-on-failure
```

Expected: 全过。**行为一字不变**——本次只换可见性与 include 路径；若有用例变红，那是搬错了东西，不是预期。

- [ ] **Step 7: 确认导出符号真的出去了（这一条是本任务的验收）**

```bash
llvm-readobj --coff-exports build-win/win-x64-clang-debug/Source/Host/VaseHost.dll | grep -i CompareDescriptor
```

Expected: 命中一条 `CompareDescriptor`（`VASE_HOST_API` 生效）。**Linux 对位**（收口波做）：`llvm-readelf --dyn-syms build-linux/linux-x64-clang-debug/Source/Host/libVaseHost.so | grep CompareDescriptor`。

- [ ] **Step 8: 提交**

```bash
git add Include/Vase/Host/ManifestExpectation.h Source/Host/Detail/ManifestCompare.cpp Source/Host/PluginHost.cpp
git rm --cached Source/Host/Detail/ManifestCompare.h 2>/dev/null || true
git commit -m "M5-T3：CompareDescriptor 提升为 VaseHost 公开面（§4.4 同规则由代码承载）"
```

---

### Task 4: 库文件命名的正反两函数

**Files:**
- Create: `Include/Vase/Catalog/LibraryFileName.h`
- Delete: `Source/Catalog/Detail/LibraryFileName.h`
- Modify: `Source/Catalog/Solve.cpp:13`、`Source/Catalog/PluginCatalog.cpp:15`
- Modify: `Tests/Integration/SolveTests.cpp:104-113`（收编副本）
- Test: `Tests/Unit/LibraryFileNameTests.cpp`（新建，加进 `Tests/CMakeLists.txt` 源清单）

**Interfaces:**
- Consumes: 无
- Produces: `vase::catalog_detail::LibraryFileName(std::string_view stem) -> std::string`（正向，既有）与 `vase::catalog_detail::LibraryStem(std::string_view fileName) -> std::optional<std::string>`（反向，新增）—— Task 8（`scan` 的 `binary` 回写）与 Task 9（`validate` 的路径断言）用它们

- [ ] **Step 1: 写失败测试**

`Tests/Unit/LibraryFileNameTests.cpp`：

```cpp
// 库文件命名的正反两函数（M5/D129）：唯一能从磁盘文件名恢复 binary 值的实现。
// 判据是 round-trip——两侧平台各自的合法文件名必须能原样还原。
#include "Vase/Catalog/LibraryFileName.h"

#include <gtest/gtest.h>
#include <optional>
#include <string>

namespace
{

std::string RoundTrip(const std::string& stem)
{
    const std::optional<std::string> back =
        vase::catalog_detail::LibraryStem(vase::catalog_detail::LibraryFileName(stem));
    return back.has_value() ? *back : std::string{"<none>"};
}

TEST(LibraryFileName, RoundTripsOnThisPlatformsOwnNaming)
{
    EXPECT_EQ(RoundTrip("HelloPlugin"), "HelloPlugin");
    EXPECT_EQ(RoundTrip("VersionedA"), "VersionedA");
}

TEST(LibraryFileName, RejectsForeignAndMalformedNames)
{
    // 别的平台的形态本平台不认（这是刻意的：识别面只认自己的命名，不猜）。
    EXPECT_FALSE(vase::catalog_detail::LibraryStem("HelloPlugin.dll").has_value() ==
                 vase::catalog_detail::LibraryStem("libHelloPlugin.so").has_value());
    EXPECT_FALSE(vase::catalog_detail::LibraryStem("readme.txt").has_value());
    EXPECT_FALSE(vase::catalog_detail::LibraryStem("").has_value());
    // 前缀剥完是空串 ⇒ 不是合法的 stem。
#ifdef _WIN32
    EXPECT_FALSE(vase::catalog_detail::LibraryStem(".dll").has_value());
#else
    EXPECT_FALSE(vase::catalog_detail::LibraryStem("lib.so").has_value());
#endif
}

} // namespace
```

- [ ] **Step 2: 跑测试确认失败**

```bash
cmake --build --preset win-x64-clang-debug --target VaseTests
```

Expected: 编译失败——`Vase/Catalog/LibraryFileName.h` 不存在。（先把文件加进 `Tests/CMakeLists.txt` 的源清单，否则 CMake 不会编它。）

- [ ] **Step 3: 建公开头（搬 + 加反向）**

`Include/Vase/Catalog/LibraryFileName.h`：

```cpp
#pragma once

// 库文件命名的**唯一**知识（D54；M5/D129 起为公开面）：正向由清单的 binary 拼文件名，
// 反向由磁盘文件名恢复 binary。两函数互为反函数，由 round-trip 用例钉住（LibraryFileNameTests）。
// header-only 内联、不挂导出宏：纯字符串函数，每个消费者各编一份副本，行为一致、无 ABI 面。
// §13.1「平台差异收口在 Loader」管的是加载行为；命名不属之（spec 勘误第 4 条）。
// iOS 的 .a 形态与 macOS 的 .dylib 分支是 VasePack/M5 后续波次的问题（D138）。

#include <optional>
#include <string>
#include <string_view>

namespace vase::catalog_detail
{

#ifdef _WIN32
inline constexpr std::string_view kLibraryPrefix = "";
inline constexpr std::string_view kLibrarySuffix = ".dll";
#else
inline constexpr std::string_view kLibraryPrefix = "lib";
inline constexpr std::string_view kLibrarySuffix = ".so";
#endif

inline std::string LibraryFileName(std::string_view stem)
{
    return std::string(kLibraryPrefix) + std::string(stem) + std::string(kLibrarySuffix);
}

// 反函数：本平台的前缀与后缀两侧都要剥；不是本平台的形态 → nullopt（不猜）。
// 剥完为空串也算不合法——一个没有名字的库不是插件。
inline std::optional<std::string> LibraryStem(std::string_view fileName)
{
    if (fileName.size() <= kLibraryPrefix.size() + kLibrarySuffix.size())
    {
        return std::nullopt;
    }
    if (fileName.substr(0, kLibraryPrefix.size()) != kLibraryPrefix)
    {
        return std::nullopt;
    }
    if (fileName.substr(fileName.size() - kLibrarySuffix.size()) != kLibrarySuffix)
    {
        return std::nullopt;
    }
    const std::string_view stem =
        fileName.substr(kLibraryPrefix.size(), fileName.size() - kLibraryPrefix.size() - kLibrarySuffix.size());
    if (stem.empty())
    {
        return std::nullopt;
    }
    return std::string(stem);
}

} // namespace vase::catalog_detail
```

- [ ] **Step 4: 跑测试确认通过**

```bash
cmake --build --preset win-x64-clang-debug --target VaseTests
ctest --preset win-x64-clang-debug -R '^LibraryFileName\.' --output-on-failure
```

Expected: 2 条全过。

- [ ] **Step 5: 删旧头、改两处 include、收编测试副本**

```bash
git rm Source/Catalog/Detail/LibraryFileName.h
```

`Source/Catalog/Solve.cpp:13` 与 `Source/Catalog/PluginCatalog.cpp:15` 的 `#include "Detail/LibraryFileName.h"` → `#include "Vase/Catalog/LibraryFileName.h"`。

`Tests/Integration/SolveTests.cpp`：删掉 `:104-113` 的 `ExpectedBinaryPath` 整个 helper，把它的调用点改成

```cpp
      const std::filesystem::path expected =
          sandbox.Root / dir / vase::catalog_detail::LibraryFileName(stem);
```

并在 include 区加 `#include "Vase/Catalog/LibraryFileName.h"`。（两份平台规则合成一份——那正是 D129 的动机。）

- [ ] **Step 6: 构建 + 跑 Solve 与 Catalog 线**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R 'Solve|CatalogScan|AssemblyFromSolve' --output-on-failure
```

Expected: 全过。

- [ ] **Step 7: 提交**

```bash
git add Include/Vase/Catalog/LibraryFileName.h Tests/Unit/LibraryFileNameTests.cpp \
        Source/Catalog/Solve.cpp Source/Catalog/PluginCatalog.cpp Tests/Integration/SolveTests.cpp \
        Tests/CMakeLists.txt
git rm --cached Source/Catalog/Detail/LibraryFileName.h 2>/dev/null || true
git commit -m "M5-T4：库文件命名正反两函数提升为公开面，收编 SolveTests 的平台规则副本"
```

---

### Task 5: 清单序列化面（Catalog 侧，含原子写）

**Files:**
- Modify: `Include/Vase/Catalog/ManifestView.h`（加声明）
- Modify: `Source/Catalog/ManifestJson.cpp`（加实现，write 与 parse 同 TU——键白名单因此同源）
- Test: `Tests/Unit/ManifestJsonTests.cpp`（加 round-trip 用例）

**Interfaces:**
- Consumes: `vase::ManifestEntry`（`Include/Vase/Catalog/ManifestView.h`）、`vase::Value`（`Include/Vase/Config/Value.h`）
- Produces: `VASE_CATALOG_API vase::Result<void> vase::WriteManifestFile(const std::filesystem::path& file, const ManifestEntry& entry);` —— Task 9（`scan` 落盘）唯一调它

**已核的 schema 事实（写键时的依据，逐条来自 `ManifestJson.cpp`）**：
- 根白名单 11 键（`:570-589`）：`schemaVersion` / `id` / `displayName` / `version` / `binary` / `enabledByDefault` / `requires` / `optionalRequires` / `provides` / `config` / `processStates`。
- 服务数组元素白名单 `{service, version}`（`:252-258`），`version` 必须为 ≥1 的整数（`:275-283`）。
- config 元素白名单 `{key, type, default, min, max, displayName, choices}`（`:332-337`）。
- 类型名七个（`:84-108`）：`bool` / `int32` / `int64` / `float` / `double` / `string` / `enum`。
- `processStates` 是**非空字符串数组**（`:288-309`）。
- `schemaVersion` 只认整数 `1`（`:591-599`）。

- [ ] **Step 1: 写失败测试（round-trip 是本任务的判据）**

在 `Tests/Unit/ManifestJsonTests.cpp` 末尾追加（该文件已有解析侧用例，include 与 `namespace` 形态照它现有写法）：

```cpp
TEST(ManifestJson, RoundTripsThroughWriteAndParse)
{
    vase::ManifestEntry entry;
    entry.Id = "Vase.RoundTrip";
    entry.DisplayName = "往返探针";
    entry.Version = "1.2.3";
    entry.Subdirectory = "RoundTrip";
    entry.Binary = "RoundTripProbe";
    entry.EnabledByDefault = false; // ← 保真字段：必须原样过一趟
    entry.Requires = {{.Service = "Vase.Other", .Version = 2}};
    entry.Provides = {{.Service = "Vase.RoundTrip.Service", .Version = 1}};
    entry.ProcessStates = {"Vase.RoundTrip.Loads"};

    testing_support::CatalogSandbox sandbox("write-roundtrip");
    const std::filesystem::path file = sandbox.Root / "RoundTrip" / "plugin.json";
    const auto written = vase::WriteManifestFile(file, entry);
    ASSERT_TRUE(written.IsOk()) << written.GetError().Message();

    const auto parsed = vase::ParseManifestFile(file, "RoundTrip");
    ASSERT_TRUE(parsed.IsOk()) << parsed.GetError().Message();
    const vase::ManifestEntry& back = parsed.Value();
    EXPECT_EQ(back.Id, entry.Id);
    EXPECT_EQ(back.DisplayName, entry.DisplayName);
    EXPECT_EQ(back.Version, entry.Version);
    EXPECT_EQ(back.Binary, entry.Binary);
    EXPECT_EQ(back.EnabledByDefault, false);
    ASSERT_EQ(back.Requires.size(), 1U);
    EXPECT_EQ(back.Requires[0].Service, "Vase.Other");
    EXPECT_EQ(back.Requires[0].Version, 2U);
    ASSERT_EQ(back.Provides.size(), 1U);
    ASSERT_EQ(back.ProcessStates.size(), 1U);
    EXPECT_EQ(back.ProcessStates[0], "Vase.RoundTrip.Loads");
}

TEST(ManifestJson, WriteLeavesNoTempFileBehind)
{
    vase::ManifestEntry entry;
    entry.Id = "Vase.Temp";
    entry.DisplayName = "临时文件探针";
    entry.Binary = "TempProbe";

    testing_support::CatalogSandbox sandbox("write-atomic");
    const std::filesystem::path dir = sandbox.Root / "Temp";
    const auto written = vase::WriteManifestFile(dir / "plugin.json", entry);
    ASSERT_TRUE(written.IsOk()) << written.GetError().Message();

    // 原子写（D134）：盘上只有 plugin.json，没有半个 JSON、也没有 .tmp 残留。
    EXPECT_TRUE(std::filesystem::exists(dir / "plugin.json"));
    EXPECT_FALSE(std::filesystem::exists(dir / "plugin.json.tmp"));
}
```

- [ ] **Step 2: 跑测试确认失败**

```bash
cmake --build --preset win-x64-clang-debug --target VaseTests
```

Expected: 编译失败——`WriteManifestFile` 未声明。

- [ ] **Step 3: 声明**

`Include/Vase/Catalog/ManifestView.h`：在 `BuildExpectation` 声明之后加

```cpp
// 清单序列化（M5/D122/D134）：scan 的落盘口。**原子写**（临时文件 + rename 覆盖）——
// 盘上任何时刻不出现半个 JSON，而那份 JSON 正是加载期要读的权威副本。
// 写出的键必须全部落在 ParseManifestFile 的白名单内；两份清单同 TU，round-trip 用例守同源。
VASE_CATALOG_API Result<void> WriteManifestFile(const std::filesystem::path& file, const ManifestEntry& entry);
```

并把 `<filesystem>` 补进该头 include 区。

- [ ] **Step 4: 实现**

`Source/Catalog/ManifestJson.cpp`：在匿名命名空间内（`OnlyKeys` 附近）加三个 helper，并在文件末尾 `namespace vase` 里加写入函数。include 区需要补 `<fstream>`（已有）与 `<system_error>`。

```cpp
// ValueKind → 清单类型名（与上面解析侧的七支逐字对应：改一处必改另一处，round-trip 用例守）。
std::string_view KindName(ValueKind kind)
{
    if (kind == ValueKind::kBool) { return "bool"; }
    if (kind == ValueKind::kInt32) { return "int32"; }
    if (kind == ValueKind::kInt64) { return "int64"; }
    if (kind == ValueKind::kFloat) { return "float"; }
    if (kind == ValueKind::kDouble) { return "double"; }
    if (kind == ValueKind::kString) { return "string"; }
    return "enum";
}

json ValueToJson(const Value& value)
{
    switch (value.Kind)
    {
    case ValueKind::kBool:
        return value.GetAs<bool>();
    case ValueKind::kInt32:
    case ValueKind::kEnum: // enum 的 min/max 是数值；default 另走 label 支（D80）
        return value.GetAs<std::int32_t>();
    case ValueKind::kInt64:
        return value.GetAs<std::int64_t>();
    case ValueKind::kFloat:
        return value.GetAs<float>();
    case ValueKind::kDouble:
        return value.GetAs<double>();
    case ValueKind::kString:
    {
        const char* text = value.GetAs<const char*>();
        return text == nullptr ? json("") : json(std::string(text));
    }
    case ValueKind::kNone:
        break;
    }
    return json(nullptr); // kNone 不进清单：调用方只在有值时调本函数
}

json FieldToJson(const ManifestConfigField& field)
{
    json item;
    item["key"] = field.Key;
    item["type"] = std::string(KindName(field.Kind));
    if (!field.DisplayName.empty())
    {
        item["displayName"] = field.DisplayName;
    }
    if (field.Kind == ValueKind::kEnum)
    {
        item["default"] = field.DefaultStr; // D80：enum 中间形存 label，不是 value
        json choices = json::array();
        for (const ManifestChoice& choice : field.Choices)
        {
            choices.push_back(json{{"value", choice.Value}, {"label", choice.Label}});
        }
        item["choices"] = std::move(choices);
    }
    else
    {
        const Value def = field.DefaultValue();
        if (def.Kind != ValueKind::kNone)
        {
            item["default"] = ValueToJson(def);
        }
    }
    if (field.HasMin)
    {
        item["min"] = ValueToJson(field.MinValue());
    }
    if (field.HasMax)
    {
        item["max"] = ValueToJson(field.MaxValue());
    }
    return item;
}

json DepsToJson(const std::vector<ManifestDependency>& deps)
{
    json array = json::array();
    for (const ManifestDependency& dep : deps)
    {
        array.push_back(json{{"service", dep.Service}, {"version", dep.Version}});
    }
    return array;
}
```

然后文件末尾（`namespace vase` 内、`ParseManifestFile` 之后）：

```cpp
Result<void> WriteManifestFile(const std::filesystem::path& file, const ManifestEntry& entry)
{
    json root;
    root["schemaVersion"] = 1; // 格式常量，恒 1（D122）：它的语义是「这份清单的格式版本」，由写者决定
    root["id"] = entry.Id;
    root["displayName"] = entry.DisplayName;
    if (!entry.Version.empty())
    {
        root["version"] = entry.Version;
    }
    root["binary"] = entry.Binary; // 观察事实（D122）：缺失会让加载期按子目录名猜，故总是显式写
    if (!entry.EnabledByDefault)
    {
        root["enabledByDefault"] = false; // 只在 false 时写：缺键 ≡ true，输出因此稳定可重复跑
    }
    if (!entry.Requires.empty())
    {
        root["requires"] = DepsToJson(entry.Requires);
    }
    if (!entry.OptionalRequires.empty())
    {
        root["optionalRequires"] = DepsToJson(entry.OptionalRequires);
    }
    if (!entry.Provides.empty())
    {
        root["provides"] = DepsToJson(entry.Provides);
    }
    if (!entry.Config.empty())
    {
        json fields = json::array();
        for (const ManifestConfigField& field : entry.Config)
        {
            fields.push_back(FieldToJson(field));
        }
        root["config"] = std::move(fields);
    }
    if (!entry.ProcessStates.empty())
    {
        root["processStates"] = entry.ProcessStates;
    }

    // 原子写（D134）：先落临时文件、再 rename 覆盖。两侧 rename 都是覆盖语义。
    const std::filesystem::path temp = std::filesystem::path(file.string() + ".tmp");
    {
        std::ofstream out(temp, std::ios::binary);
        if (!out)
        {
            return Result<void>::Err(ManifestError(temp, "cannot open for writing"));
        }
        out << root.dump(2) << '\n';
        if (!out)
        {
            std::error_code ignored;
            std::filesystem::remove(temp, ignored);
            return Result<void>::Err(ManifestError(temp, "write failed"));
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp, file, ec);
    if (ec)
    {
        std::error_code ignored;
        std::filesystem::remove(temp, ignored); // 尽力清理，不留半个临时文件
        return Result<void>::Err(ManifestError(file, "rename failed: " + ec.message()));
    }
    return Result<void>::Ok();
}
```

- [ ] **Step 5: 跑测试确认通过**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R '^ManifestJson\.' --output-on-failure
```

Expected: 全过（新增 2 条 + 既有若干）。

- [ ] **Step 6: 提交**

```bash
git add Include/Vase/Catalog/ManifestView.h Source/Catalog/ManifestJson.cpp Tests/Unit/ManifestJsonTests.cpp
git commit -m "M5-T5：清单序列化面 WriteManifestFile（原子写、键与解析侧白名单同 TU 同源）"
```

---

### Task 6: `Tools/VaseCli` 骨架

**Files:**
- Create: `Tools/VaseCli/CMakeLists.txt`
- Create: `Tools/VaseCli/main.cpp`
- Create: `Tools/VaseCli/Cli.h` / `Tools/VaseCli/Cli.cpp`
- Modify: `CMakeLists.txt:288-290`（在 `add_subdirectory(Tools/VaseConsole)` 之后、`add_subdirectory(Tests)` 之前加一行）

**Interfaces:**
- Consumes: 无（本任务只立骨架）
- Produces:
  - `namespace tools::cli` 的 `int Run(int argc, char** argv, std::ostream& out, std::ostream& err);` —— `main` 唯一调它；Task 14（收口）的 ctest 端到端用例也吃这个退出码
  - CMake target `VaseCliCore`（STATIC，供 `VaseCli` 与 `VaseTests` 链）与 `VaseCli`（可执行）

- [ ] **Step 1: 建 target 与骨架**

`Tools/VaseCli/CMakeLists.txt`：

```cmake
# 内部静态库 + 薄 main（M5/D124）：编排与检查住库里，gtest 可直接链它做单变量用例；
# main 只做 argv 解析与打印。CMake 顺序要求：本目录必须排在 add_subdirectory(Tests) 之前
# （Tests 的 VaseTests 要链 VaseCliCore），与 VaseHost / VaseCatalog / VaseConsole 同例。
add_library(VaseCliCore STATIC
    Cli.cpp
    Scan.cpp
    Validate.cpp
    ManifestMerge.cpp)

target_link_libraries(VaseCliCore
    PRIVATE VaseBuildOptions)   # 只链 VaseBuildOptions：编排面自己用 VaseHost/VaseCatalog 的类型
                                # 由 VaseCli 侧链接（静态库不传播，避免 Tests 被迫全链）
target_include_directories(VaseCliCore
    PUBLIC "${PROJECT_SOURCE_DIR}/Include"
           "${CMAKE_CURRENT_SOURCE_DIR}")

# 不链 VaseThirdPartyCli：VaseCli 是批处理工具，argv 手写（D137）。
add_executable(VaseCli main.cpp)
target_link_libraries(VaseCli
    PRIVATE VaseBuildOptions
            VaseCliCore
            VaseHost
            VaseCatalog)

# 退出码是 CLI 层的机器判据（D135）：`main` / `Run` 的 argv 分发由这两条端到端守。
# 判据形状照 Tools/VaseConsole：WILL_FAIL 钉非零，兄弟用例只钉文本——
# 两者不可合并（ctest 4.4.3 实测：同设 WILL_FAIL + PASS_REGULAR_EXPRESSION 时命中反判 Failed）。
add_test(NAME VaseCliNoArgsIsUsageError COMMAND VaseCli)
set_tests_properties(VaseCliNoArgsIsUsageError PROPERTIES WILL_FAIL TRUE)
add_test(NAME VaseCliNoArgsReason COMMAND VaseCli)
set_tests_properties(VaseCliNoArgsReason PROPERTIES PASS_REGULAR_EXPRESSION "usage: VaseCli")

add_test(NAME VaseCliUnknownSubcommandIsUsageError COMMAND VaseCli frobnicate)
set_tests_properties(VaseCliUnknownSubcommandIsUsageError PROPERTIES WILL_FAIL TRUE)
add_test(NAME VaseCliUnknownSubcommandReason COMMAND VaseCli frobnicate)
set_tests_properties(VaseCliUnknownSubcommandReason PROPERTIES PASS_REGULAR_EXPRESSION "unknown command")
```

`Tools/VaseCli/main.cpp`（形态照 `Tools/VaseConsole/main.cpp`：用法错误 → 2、正常路径退出码来自编排）：

```cpp
// VaseCli —— 批处理工具（M5/§11.1）：scan 生成清单、validate 校验四项。
// 退出码三档（D135）：0 = 全过；1 = 有检查未过；2 = 用法或环境错误。
#include "Cli.h"

#include <iostream>

int main(int argc, char** argv)
{
    return tools::cli::Run(argc, argv, std::cout, std::cerr);
}
```

`Tools/VaseCli/Cli.h`：

```cpp
#pragma once

// 子命令分发（M5/D137）：手写 argv，不引 cli 库。退出码三档见主文件头。

#include <iosfwd>

namespace tools::cli
{

// 0 = 全过；1 = 有检查未过；2 = 用法或环境错误。不把四项编码进不同码位（D135）。
inline constexpr int kExitOk = 0;
inline constexpr int kExitCheckFailed = 1;
inline constexpr int kExitUsage = 2;

int Run(int argc, char** argv, std::ostream& out, std::ostream& err);

} // namespace tools::cli
```

`Tools/VaseCli/Cli.cpp`：

```cpp
#include "Cli.h"

#include "Scan.h"
#include "Validate.h"

#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{

std::vector<std::string> Arguments(int argc, char** argv)
{
    std::vector<std::string> args;
    for (int index = 1; index < argc; ++index)
    {
        args.emplace_back(argv[index]);
    }
    return args;
}

void PrintUsage(std::ostream& err)
{
    err << "usage: VaseCli scan <插件目录>\n"
           "       VaseCli validate <插件目录> [--host-provides <name>@<version>]...\n";
}

} // namespace

namespace tools::cli
{

int Run(int argc, char** argv, std::ostream& out, std::ostream& err)
{
    const std::vector<std::string> args = Arguments(argc, argv);
    if (args.empty())
    {
        PrintUsage(err);
        return kExitUsage;
    }

    const std::string_view command = args[0];
    if (command == "scan")
    {
        if (args.size() != 2U)
        {
            PrintUsage(err);
            return kExitUsage;
        }
        return RunScan(args[1], out, err);
    }
    if (command == "validate")
    {
        return RunValidate(args, out, err);
    }

    err << "unknown command: " << command << '\n';
    PrintUsage(err);
    return kExitUsage;
}

} // namespace tools::cli
```

`Tools/VaseCli/Scan.h` / `Validate.h`（本任务只立签名，实现在 Task 8 / Task 12 填——**注意本任务要同时建空的 `.cpp` 以便 CMake 有文件可编**，最小实现是「打印 unimplemented 并返回 2」，Task 8 会把它换成真实现）：

```cpp
// Scan.h
#pragma once

#include <iosfwd>
#include <string>

namespace tools::cli
{

// 生成清单：遍历一级子目录 → 每个子目录一个库 → 生成/回写 plugin.json。
// 退出码见 Cli.h。
int RunScan(const std::string& pluginDirectory, std::ostream& out, std::ostream& err);

} // namespace tools::cli
```

```cpp
// Scan.cpp
#include "Scan.h"

#include "Cli.h"

#include <ostream>

namespace tools::cli
{

int RunScan(const std::string& pluginDirectory, std::ostream& out, std::ostream& err)
{
    static_cast<void>(pluginDirectory);
    static_cast<void>(out);
    err << "scan: not implemented yet\n";
    return kExitUsage;
}

} // namespace tools::cli
```

```cpp
// Validate.h
#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace tools::cli
{

// 四项检查：① 清单↔二进制一致 ② Id 重复 ③ 依赖图/环/版本 ④ 服务命名前缀。
// hostProvides 形如 "Vase.Test.HostOnly@1"（Task 12 解析）。
int RunValidate(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

} // namespace tools::cli
```

```cpp
// Validate.cpp
#include "Validate.h"

#include "Cli.h"

#include <ostream>

namespace tools::cli
{

int RunValidate(const std::vector<std::string>& args, std::ostream& out, std::ostream& err)
{
    static_cast<void>(args);
    static_cast<void>(out);
    err << "validate: not implemented yet\n";
    return kExitUsage;
}

} // namespace tools::cli
```

`Tools/VaseCli/ManifestMerge.h` / `ManifestMerge.cpp`（Task 9 的实现位，本任务留最小形）：

```cpp
// ManifestMerge.h
#pragma once

// scan 的回写合并（M5/D122）：描述符背书字段来自二进制，清单独有字段按类处置。
// 单独成文件是因为它是全仓唯一知道「哪些字段是清单独有」的地方——与 D73 同源。

#include "Vase/Catalog/ManifestView.h"

#include <filesystem>

namespace tools::cli
{

// 合并 = 以 fromBinary 为底，从既有清单取回 enabledByDefault（手写语义，描述符无从推断）。
// 既有清单不存在 / 读不出 → 原样返回 fromBinary（首次生成走缺省）。
vase::ManifestEntry MergeWithExisting(const std::filesystem::path& manifestFile,
                                      vase::ManifestEntry fromBinary);

} // namespace tools::cli
```

```cpp
// ManifestMerge.cpp
#include "ManifestMerge.h"

#include "Vase/Catalog/PluginCatalog.h"

namespace tools::cli
{

vase::ManifestEntry MergeWithExisting(const std::filesystem::path& manifestFile,
                                      vase::ManifestEntry fromBinary)
{
    if (!std::filesystem::exists(manifestFile))
    {
        return fromBinary; // 首次生成：enabledByDefault 走缺省（true ⇒ 省略）
    }
    const vase::Result<vase::ManifestEntry> old = vase::ParseManifestFile(manifestFile, fromBinary.Subdirectory);
    if (!old.IsOk())
    {
        return fromBinary; // 旧清单读不出：以二进制为准（它就是权威），并把「读不出」留给调用方打印
    }
    // NOLINTNEXTLINE 唯一要保真的字段：enabledByDefault 是人手语义，描述符侧无对位物（D73）。
    fromBinary.EnabledByDefault = old.Value().EnabledByDefault;
    return fromBinary;
}

} // namespace tools::cli
```

- [ ] **Step 2: 根 CMakeLists 加子目录**

`CMakeLists.txt:288-290`，在 `add_subdirectory(Tools/VaseConsole)` 与 `add_subdirectory(Tests)` 之间加：

```cmake
# VaseCli（M5）：必须在 Tests 之前——VaseTests 要链它的 VaseCliCore（同 VaseHost 的既有范式）。
add_subdirectory(Tools/VaseCli)
```

- [ ] **Step 3: 配置 + 构建，确认骨架能跑**

```bash
cmake --preset win-x64-clang-debug
cmake --build --preset win-x64-clang-debug --target VaseCli
build-win/win-x64-clang-debug/bin/VaseCli ; echo "rc=$?"
build-win/win-x64-clang-debug/bin/VaseCli scan /tmp ; echo "rc=$?"
```

Expected: 第一条 `rc=2` 且打印 usage（无参数）；第二条 `rc=2` 且打印 `scan: not implemented yet`。

再跑一遍那四条端到端用例（它们钉的是 `main` → `Run` 这一层的分发与退出码）：

```bash
ctest --preset win-x64-clang-debug -R '^VaseCli' --output-on-failure
```

Expected: 4 条全过（两条 `WILL_FAIL` 的非零断言 + 两条文本断言）。

- [ ] **Step 4: 提交**

```bash
git add CMakeLists.txt Tools/VaseCli
git commit -m "M5-T6：Tools/VaseCli 骨架（VaseCliCore 静态库 + 薄 main + 三档退出码）"
```

---

### Task 7: `Samples/DependentPlugin` 服务改名

**Files:**
- Modify: `Samples/HelloCommon/Farewell.h:13`
- Modify: `Samples/DependentPlugin/DependentPlugin.cpp:2, 47`
- Modify: `Tests/Integration/fixtures/manifests/dependent/plugin.json`

**Interfaces:**
- Consumes: 无
- Produces: 服务名 `Vase.Dependent.Farewell`（前缀 = 完整 Id，D121）—— Task 13（`validate` 第④项）的「`Samples/` 零违规」验收吃它

**为什么单独一个任务**：它动的是 `Samples/` 与一份**手写清单**，本波唯一一处会碰既有比对材料的地方；越早做完，后面写 `validate` 用例时就不会先按旧名写、再回头改。

- [ ] **Step 1: 改三处名字**

`Samples/HelloCommon/Farewell.h:13`：

```cpp
    static constexpr std::string_view kName = "Vase.Dependent.Farewell";
```

`Samples/DependentPlugin/DependentPlugin.cpp:47`：

```cpp
    .Provides = {{.Name = "Vase.Dependent.Farewell", .Version = 1}},
```

同文件 `:2` 的注释里 `Vase.Farewell` 一并改成 `Vase.Dependent.Farewell`。

`Tests/Integration/fixtures/manifests/dependent/plugin.json` 的 `provides[0].service` 改成 `Vase.Dependent.Farewell`。

- [ ] **Step 2: 确认没有第五处**

```bash
grep -rn "Vase\.Farewell" --include=*.h --include=*.cpp --include=*.json --include=*.txt --include=*.cmake . | grep -v "^./build-"
```

Expected: **零命中**。若还有命中（尤其 `Tools/VaseConsole` 的回放资产），就地一并改——Console 的 `PASS_REGULAR_EXPRESSION` 若引用了它必须同步（spec 已核过应为零，这是复核）。

- [ ] **Step 3: 构建 + 跑依赖链与 Sample 证人**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R 'AssemblyFromSolve|LoadTimeComparison|Dependent' --output-on-failure
```

Expected: 全过。这条链同时证明「改名后清单与描述符仍然一致」——**若只改了一处，加载期比对会当场拒**，这正是本任务自带的判据。

- [ ] **Step 4: 提交**

```bash
git add Samples/HelloCommon/Farewell.h Samples/DependentPlugin/DependentPlugin.cpp \
        Tests/Integration/fixtures/manifests/dependent/plugin.json
git commit -m "M5-T7：DependentPlugin 服务改名 Vase.Farewell → Vase.Dependent.Farewell（§6.1 前缀自洽）"
```

---

### Task 8: `scan` 的目录发现与跳过口径

**Files:**
- Modify: `Tools/VaseCli/Scan.h` / `Tools/VaseCli/Scan.cpp`
- Test: `Tests/Integration/VaseCliScanTests.cpp`（新建，加进 `Tests/CMakeLists.txt` 源清单）

**Interfaces:**
- Consumes: `vase::catalog_detail::LibraryFileName` / `LibraryStem`（Task 4）
- Produces:
  - `struct tools::cli::DiscoveredPlugin { std::filesystem::path Directory; std::filesystem::path BinaryPath; std::string Stem; };`
  - `struct tools::cli::Discovery { std::vector<DiscoveredPlugin> Found; std::vector<std::string> Skipped; std::vector<std::string> Failed; };`
  - `vase::Result<Discovery> tools::cli::DiscoverPlugins(const std::filesystem::path& pluginDirectory);` —— Task 10 与 Task 11 都用它（`validate` 的 ①④ 两项同样要找到库）

**判据来自 spec §2.3 那张表**：一个库 → 收；两个及以上 → 响亮；无库但有清单 → 响亮；既无库也无清单 → 跳过 + 一行打印；整棵树零插件 → 由调用方按 D135 返回 2。

- [ ] **Step 1: 写失败测试**

`Tests/Integration/VaseCliScanTests.cpp`：

```cpp
// scan 的目录发现与跳过口径（M5/D133）：spec §2.3 那张表逐行一条用例。
// 沙箱复用 CatalogSandbox（temp 下建树、析构清理），二进制从 VASE_FIXTURE_* 取。
#include "Scan.h"

#include "Vase/Catalog/LibraryFileName.h"

#include "CatalogSandbox.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace
{

using testing_support::CatalogSandbox;
namespace fs = std::filesystem;

// 往沙箱里放一个插件子目录：库文件的**名字**按本平台规则拼（这正是 LibraryFileName 的用处）。
void StagePlugin(const CatalogSandbox& sandbox, const std::string& subdirectory, const std::string& stem,
                 const char* fixturePath)
{
    sandbox.CopyFile(subdirectory + "/" + vase::catalog_detail::LibraryFileName(stem), fixturePath);
}

TEST(VaseCliScan, FindsOneLibraryPerSubdirectory)
{
    const CatalogSandbox sandbox("scan-find");
    StagePlugin(sandbox, "Probe", "LoadProbe", VASE_FIXTURE_LOADPROBE);

    const vase::Result<tools::cli::Discovery> found = tools::cli::DiscoverPlugins(sandbox.Root);
    ASSERT_TRUE(found.IsOk()) << found.GetError().Message();
    ASSERT_EQ(found.Value().Found.size(), 1U);
    EXPECT_EQ(found.Value().Found[0].Stem, "LoadProbe");
    EXPECT_EQ(found.Value().Found[0].Directory.filename().string(), "Probe");
    EXPECT_TRUE(found.Value().Skipped.empty());
    EXPECT_TRUE(found.Value().Failed.empty());
}

TEST(VaseCliScan, SkipsSubdirectoriesThatAreNeitherLibraryNorManifest)
{
    const CatalogSandbox sandbox("scan-skip");
    StagePlugin(sandbox, "Probe", "LoadProbe", VASE_FIXTURE_LOADPROBE);
    sandbox.CreateDir("docs");        // 既无库也无清单的时间目录：跳过，不是错误
    sandbox.CreateDir("shaders");

    const vase::Result<tools::cli::Discovery> found = tools::cli::DiscoverPlugins(sandbox.Root);
    ASSERT_TRUE(found.IsOk()) << found.GetError().Message();
    EXPECT_EQ(found.Value().Found.size(), 1U);
    EXPECT_EQ(found.Value().Skipped.size(), 2U);
    EXPECT_TRUE(found.Value().Failed.empty());
}

TEST(VaseCliScan, TwoLibrariesInOneSubdirectoryIsLoud)
{
    const CatalogSandbox sandbox("scan-twolibs");
    StagePlugin(sandbox, "Probe", "LoadProbe", VASE_FIXTURE_LOADPROBE);
    StagePlugin(sandbox, "Probe", "UnloadProbe", VASE_FIXTURE_UNLOADPROBE);

    const vase::Result<tools::cli::Discovery> found = tools::cli::DiscoverPlugins(sandbox.Root);
    ASSERT_TRUE(found.IsOk()) << found.GetError().Message();
    EXPECT_TRUE(found.Value().Found.empty());
    ASSERT_EQ(found.Value().Failed.size(), 1U);
    EXPECT_NE(found.Value().Failed[0].find("Probe"), std::string::npos);
}

TEST(VaseCliScan, ManifestWithoutLibraryIsLoud)
{
    const CatalogSandbox sandbox("scan-manifestonly");
    sandbox.WriteFile("Ghost/plugin.json", R"({"schemaVersion":1,"id":"Vase.Ghost"})");

    const vase::Result<tools::cli::Discovery> found = tools::cli::DiscoverPlugins(sandbox.Root);
    ASSERT_TRUE(found.IsOk()) << found.GetError().Message();
    EXPECT_TRUE(found.Value().Found.empty());
    EXPECT_EQ(found.Value().Failed.size(), 1U);
}

TEST(VaseCliScan, MissingDirectoryIsAnErrorNotAnEmptySuccess)
{
    const CatalogSandbox sandbox("scan-nodir");
    const vase::Result<tools::cli::Discovery> found = tools::cli::DiscoverPlugins(sandbox.Root / "nope");
    EXPECT_FALSE(found.IsOk());
}

} // namespace
```

- [ ] **Step 2: 接线（源清单 + 链接 VaseCliCore）**

`Tests/CMakeLists.txt`：

① `:3-36` 源清单里加 `Integration/VaseCliScanTests.cpp`（`Integration/SolveTests.cpp` 之后）。
② `:50-54` 的 `target_link_libraries(VaseTests ...)` 里加 `VaseCliCore`：

```cmake
            VaseCliCore
```

- [ ] **Step 3: 跑测试确认失败**

```bash
cmake --build --preset win-x64-clang-debug --target VaseTests
```

Expected: 编译失败——`Discovery` / `DiscoverPlugins` 未声明。

- [ ] **Step 4: 实现**

`Tools/VaseCli/Scan.h`（在既有 `RunScan` 声明之前加数据结构与发现函数）：

```cpp
#include "Vase/Detail/Result.h"

#include <filesystem>
#include <string>
#include <vector>

namespace tools::cli
{

struct DiscoveredPlugin
{
    std::filesystem::path Directory;  // <插件目录>/<子目录>
    std::filesystem::path BinaryPath; // 该子目录里的那个库
    std::string Stem;                 // LibraryStem 的结果 = 清单 binary 该写的值（D122）
};

struct Discovery
{
    std::vector<DiscoveredPlugin> Found;
    std::vector<std::string> Skipped; // 既无库也无清单的子目录名（打印用，不影响退出码）
    std::vector<std::string> Failed;  // 真歧义与坏树（响亮，影响退出码）
};

// 目录不存在 / 不是目录 → Err。**零插件树不是 Err**：由调用方按 D135 判退出码 2
// （这里只如实报告「一个都没有」，不替调用方决定那算不算用法错误）。
[[nodiscard]] vase::Result<Discovery> DiscoverPlugins(const std::filesystem::path& pluginDirectory);

} // namespace tools::cli
```

`Tools/VaseCli/Scan.cpp`（`RunScan` 暂时保留 Task 6 的最小实现，Task 10 换掉）：

```cpp
#include "Vase/Catalog/LibraryFileName.h"

#include <algorithm>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace
{

bool IsNativeLibraryFile(const std::filesystem::path& path)
{
    return vase::catalog_detail::LibraryStem(path.filename().string()).has_value();
}

// 一级子目录按名字排序：输出确定性由工具保证（D136），不让文件系统枚举序漏进报告。
std::vector<std::filesystem::path> Subdirectories(const std::filesystem::path& root)
{
    std::vector<std::filesystem::path> out;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(root, ec), end; it != end; it.increment(ec))
    {
        if (ec)
        {
            break;
        }
        std::error_code probe;
        if (it->is_directory(probe) && !probe)
        {
            out.push_back(it->path());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace

namespace tools::cli
{

vase::Result<Discovery> DiscoverPlugins(const std::filesystem::path& pluginDirectory)
{
    std::error_code ec;
    if (!std::filesystem::is_directory(pluginDirectory, ec) || ec)
    {
        return vase::Result<Discovery>::Err(
            vase::Error{"not a plugin directory: " + pluginDirectory.string()});
    }

    Discovery found;
    for (const std::filesystem::path& subdirectory : Subdirectories(pluginDirectory))
    {
        std::vector<std::filesystem::path> libraries;
        std::error_code walk;
        for (std::filesystem::directory_iterator it(subdirectory, walk), end; it != end; it.increment(walk))
        {
            if (walk)
            {
                break;
            }
            std::error_code probe;
            if (it->is_regular_file(probe) && !probe && IsNativeLibraryFile(it->path()))
            {
                libraries.push_back(it->path());
            }
        }

        const std::string name = subdirectory.filename().string();
        const bool hasManifest = std::filesystem::exists(subdirectory / "plugin.json");
        if (libraries.size() > 1U)
        {
            found.Failed.push_back(name + ": more than one shared library in the subdirectory");
            continue;
        }
        if (libraries.empty())
        {
            // 有清单却没库是坏树（清单在指一个不存在的文件）；两样都没有则只是不是插件。
            if (hasManifest)
            {
                found.Failed.push_back(name + ": plugin.json present but no shared library");
            }
            else
            {
                found.Skipped.push_back(name);
            }
            continue;
        }

        const std::optional<std::string> stem =
            vase::catalog_detail::LibraryStem(libraries[0].filename().string());
        found.Found.push_back(DiscoveredPlugin{
            .Directory = subdirectory,
            .BinaryPath = libraries[0],
            .Stem = stem.has_value() ? *stem : std::string{},
        });
    }
    return vase::Result<Discovery>::Ok(std::move(found));
}

} // namespace tools::cli
```

- [ ] **Step 5: 跑测试确认通过**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R '^VaseCliScan\.' --output-on-failure
```

Expected: 5 条全过。

- [ ] **Step 6: 提交**

```bash
git add Tools/VaseCli/Scan.h Tools/VaseCli/Scan.cpp Tests/Integration/VaseCliScanTests.cpp Tests/CMakeLists.txt
git commit -m "M5-T8：scan 的目录发现与跳过口径（真歧义响亮、非插件目录跳过）"
```

---

### Task 9: 描述符 → 清单值的投影

**Files:**
- Modify: `Source/Catalog/Detail/ChoiceCoerce.h`（加回 `ValueToLabel` + 改注释）
- Modify: `Include/Vase/Catalog/ManifestView.h`（加 `MakeManifestConfigField` 声明）
- Modify: `Source/Catalog/PluginCatalog.cpp`（加实现）
- Modify: `Tools/VaseCli/Scan.h` / `Scan.cpp`（加 `ManifestEntryFromMeta`）
- Test: `Tests/Unit/ManifestJsonTests.cpp`（加投影用例）

**Interfaces:**
- Consumes: `vase::PluginMeta` / `vase::FieldInfo`（`Include/Vase/PluginDescriptor.h` / `Vase/Config/FieldInfo.h`）
- Produces:
  - `VASE_CATALOG_API vase::ManifestConfigField vase::MakeManifestConfigField(const FieldInfo& field);`
  - `tools::cli::ManifestEntry ManifestEntryFromMeta(const vase::PluginMeta& meta, const std::string& subdirectory, const std::string& binaryStem);` —— **借用 → 拥有**的唯一物化点（spec §2.2 的寿命纪律）

**这一步是 `scan` 里唯一有真实转换逻辑的地方**，其余都是 I/O 与编排。

- [ ] **Step 1: 写失败测试**

在 `Tests/Unit/ManifestJsonTests.cpp` 末尾追加：

```cpp
// 描述符 → 清单值的投影（M5/D122）：与 BuildExpectation（清单 → 期望）互为反方向的半条链。
// 三者串起来看：描述符 --投影--> 清单 --BuildExpectation--> 期望，故这条用例同时钉投影的正确性。
TEST(ManifestJson, ProjectsDescriptorMetaIntoManifestEntry)
{
    vase::PluginMeta meta;
    meta.Id = "Vase.Projected";
    meta.DisplayName = "投影探针";
    meta.Version = "2.0.0";
    meta.Requires = {};
    meta.Provides = {{.Name = "Vase.Projected.Service", .Version = 3}};

    const vase::ManifestEntry entry = tools::cli::ManifestEntryFromMeta(meta, "Projected", "ProjectedProbe");
    EXPECT_EQ(entry.Id, "Vase.Projected");
    EXPECT_EQ(entry.DisplayName, "投影探针");
    EXPECT_EQ(entry.Version, "2.0.0");
    EXPECT_EQ(entry.Subdirectory, "Projected");
    EXPECT_EQ(entry.Binary, "ProjectedProbe"); // 观察到的 stem，不是子目录名（D122）
    EXPECT_TRUE(entry.EnabledByDefault);       // 投影不碰清单独有字段——那是 Task 10 的合并职责
    ASSERT_EQ(entry.Provides.size(), 1U);
    EXPECT_EQ(entry.Provides[0].Service, "Vase.Projected.Service");
    EXPECT_EQ(entry.Provides[0].Version, 3U);

    // 借用已在投影处物化：meta 就地销毁，entry 仍自持全部字符串（寿命纪律在 §2.2）。
    meta = vase::PluginMeta{};
    EXPECT_EQ(entry.Provides[0].Service, "Vase.Projected.Service");
}
```

- [ ] **Step 2: 跑测试确认失败**

```bash
cmake --build --preset win-x64-clang-debug --target VaseTests
```

Expected: 编译失败——`ManifestEntryFromMeta` 未声明。

- [ ] **Step 3: 加回 `ValueToLabel`**

`Source/Catalog/Detail/ChoiceCoerce.h`：把文件头那句「反向的 value → label 曾在此预留，终态无消费者已删（YAGNI）」改成「反向的 value → label 由 M5 的 `scan` 投影（enum 的 default 在清单侧存 label，D80）重新引入」，并加：

```cpp
inline std::optional<std::string_view> ValueToLabel(std::int32_t value, const ChoiceInfo* items, std::uint32_t count)
{
    for (std::uint32_t index = 0; index < count; ++index)
    {
        if (std::next(items, static_cast<std::ptrdiff_t>(index))->Value == value)
        {
            return std::next(items, static_cast<std::ptrdiff_t>(index))->Label;
        }
    }
    return std::nullopt;
}
```

include 区补 `#include "Vase/Config/FieldInfo.h"` 与 `<iterator>`。

- [ ] **Step 4: 加投影工厂**

`Include/Vase/Catalog/ManifestView.h`：在 `BuildExpectation` 之后加

```cpp
// 描述符字段 → 清单字段（M5/D122）：BuildExpectation 的反方向。enum 的 default 在清单侧存
// **label**（D80），故此处走 value → label；标签查不到 = 描述符自相矛盾（D80 的 CHECK 点挡在前面），
// 走 ProgrammerError 与 BuildExpectation 同形。
VASE_CATALOG_API ManifestConfigField MakeManifestConfigField(const FieldInfo& field);
```

include 区补 `#include "Vase/Config/FieldInfo.h"`。

`Source/Catalog/PluginCatalog.cpp`（与 `BuildExpectation` 同文件、同命名空间）：

```cpp
namespace
{

// Value → ManifestConfigField 的位形编码（DefaultBits/MinBits/MaxBits）：与 ManifestView.h 的
// Decode* 访问器是同一套编码，两者必须同源——三条 round-trip 用例守它（本文件与 ManifestJsonTests）。
std::uint64_t EncodeBits(const Value& value)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) 与 Value::From 同机制：按 Kind 互斥取用
    return value.Bits;
}

} // namespace

ManifestConfigField MakeManifestConfigField(const FieldInfo& field)
{
    ManifestConfigField out;
    out.Key = field.Name;
    out.Kind = field.Kind;
    out.DisplayName = field.Label;

    if (field.Kind == ValueKind::kEnum)
    {
        const std::optional<std::string_view> label =
            catalog_detail::ValueToLabel(field.Default.GetAs<std::int32_t>(), field.Choices, field.ChoiceCount);
        if (!label.has_value())
        {
            detail::ProgrammerError("MakeManifestConfigField: enum default not in choices");
        }
        out.DefaultStr = std::string(*label); // D80 的 label 中间形
        out.Choices.reserve(field.ChoiceCount);
        for (std::uint32_t index = 0; index < field.ChoiceCount; ++index)
        {
            const ChoiceInfo& choice = *std::next(field.Choices, static_cast<std::ptrdiff_t>(index));
            out.Choices.push_back(ManifestChoice{.Value = choice.Value, .Label = choice.Label});
        }
        return out;
    }

    if (field.Kind == ValueKind::kString)
    {
        const char* text = field.Default.GetAs<const char*>();
        out.DefaultStr = text == nullptr ? std::string{} : std::string(text);
    }
    else if (field.Default.Kind != ValueKind::kNone)
    {
        out.DefaultBits = EncodeBits(field.Default);
    }

    // min/max 仅数值型可出现（解析期保证）；描述符侧用 kNone 表示未设。
    if (field.Min.Kind != ValueKind::kNone)
    {
        out.HasMin = true;
        out.MinBits = EncodeBits(field.Min);
    }
    if (field.Max.Kind != ValueKind::kNone)
    {
        out.HasMax = true;
        out.MaxBits = EncodeBits(field.Max);
    }
    return out;
}
```

（`include` 区补 `#include "Detail/ChoiceCoerce.h"`、`#include <iterator>`、`#include <optional>`。）

**注意**：`kString` 的 `Min/Max` 不该出现（解析期保证），故上面只对 kString 的 *Default* 走 `DefaultStr`；若描述符里塞了 string 的 min/max，那属于描述符侧误用，**本任务不额外设闸**（`Solve` 与 `BuildExpectation` 的既有路径同样不做），如实记录。

- [ ] **Step 5: 加投影函数（工具侧）**

`Tools/VaseCli/Scan.h` 加：

```cpp
#include "Vase/PluginDescriptor.h"

// 描述符元信息 → 清单值（M5/D122）。**借用 → 拥有的唯一物化点**：返回的每个字符串都已深拷，
// 调用方可在 Loader::Unload 之后安全使用（spec §2.2 的寿命纪律）。
// 清单独有字段（enabledByDefault）不在这里定——那是合并的职责（Task 10）。
[[nodiscard]] vase::ManifestEntry ManifestEntryFromMeta(const vase::PluginMeta& meta,
                                                       const std::string& subdirectory,
                                                       const std::string& binaryStem);
```

`Tools/VaseCli/Scan.cpp` 加（匿名命名空间里放一个 `Dep` 转换 helper）：

```cpp
namespace
{

std::vector<vase::ManifestDependency> ConvertDeps(const vase::MetaArray<vase::ServiceRef, 16>& declared)
{
    std::vector<vase::ManifestDependency> out;
    for (const vase::ServiceRef& ref : declared)
    {
        out.push_back(vase::ManifestDependency{.Service = std::string(ref.Name), .Version = ref.Version});
    }
    return out;
}

} // namespace

namespace tools::cli
{

vase::ManifestEntry ManifestEntryFromMeta(const vase::PluginMeta& meta, const std::string& subdirectory,
                                          const std::string& binaryStem)
{
    vase::ManifestEntry entry;
    entry.Id = std::string(meta.Id);
    entry.DisplayName = std::string(meta.DisplayName);
    entry.Version = std::string(meta.Version);
    entry.Subdirectory = subdirectory;
    entry.Binary = binaryStem;
    entry.Requires = ConvertDeps(meta.Requires);
    entry.OptionalRequires = ConvertDeps(meta.OptionalRequires);
    entry.Provides = ConvertDeps(meta.Provides);
    entry.Config.reserve(meta.Config.Count);
    for (std::uint32_t index = 0; index < meta.Config.Count; ++index)
    {
        const vase::FieldInfo& field = *std::next(meta.Config.Fields, static_cast<std::ptrdiff_t>(index));
        entry.Config.push_back(vase::MakeManifestConfigField(field));
    }
    for (const vase::ProcessStateDesc& state : meta.ProcessStates)
    {
        entry.ProcessStates.push_back(std::string(state.Name));
    }
    return entry;
}

} // namespace tools::cli
```

- [ ] **Step 6: 跑测试确认通过**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R '^ManifestJson\.' --output-on-failure
```

Expected: 全过（含本轮新增 1 条）。

- [ ] **Step 7: 补一条「投影 → 期望」的串联用例（本次最有价值的一条）**

在 `Tests/Unit/ManifestJsonTests.cpp` 再加：把一个**带 config 与 processStates 的真实描述符**（例如 `Tests/Unit/DescriptorTests.cpp` 里那只 `DescriptorProbePlugin` 的 meta——用 `VasePluginDesc_DescriptorProbePlugin()->Meta` 取）投影成 `ManifestEntry`，再 `BuildExpectation` 成期望，最后断言期望里的字段逐条对应：

```cpp
TEST(ManifestJson, ProjectionAndExpectationAgreeOnARealDescriptor)
{
    const vase::PluginDescriptor* desc = VasePluginDesc_DescriptorProbePlugin();
    const vase::ManifestEntry entry = tools::cli::ManifestEntryFromMeta(*desc->Meta, "Probe", "Probe");
    const vase::ManifestExpectation expected = vase::BuildExpectation(entry);
    EXPECT_EQ(expected.Id, "Vase.DescriptorProbe");
    ASSERT_EQ(expected.Requires.size(), 2U);
    EXPECT_EQ(expected.Provides[0].Name, "Vase.Probe.Service");
    ASSERT_EQ(expected.Config.size(), 1U);
    EXPECT_EQ(expected.Config[0].Key, "Volume");
    EXPECT_EQ(expected.Config[0].Kind, vase::ValueKind::kFloat);
    ASSERT_TRUE(expected.Config[0].Default.has_value());
    // ProbeConfig 的 Volume 是 float（Tests/Unit/DescriptorTests.cpp 的 VASE_CONFIG 行），
    // 故 Storage 的 variant 实持 float——这一行同时钉「描述符的 Value → Storage 解码」正确。
    EXPECT_FLOAT_EQ(std::get<float>(*expected.Config[0].Default), 2.5F);
    ASSERT_EQ(expected.ProcessStates.size(), 1U);
    EXPECT_EQ(expected.ProcessStates[0], "Vase.DescriptorProbe.State");
}
```

（`std::get<float>` 要求 `ConfigBlob::Storage` 就是 `std::variant<bool, int32_t, int64_t, float, double, std::string, EnumStored>`——它是，见 `Include/Vase/Host/ConfigBlob.h`。取错 alternative 会抛 `bad_variant_access`，而无异常构建下那是 abort——所以这一行**如果取错类型会当场终止**，不是静默红。）

（`Tests/Unit/ManifestJsonTests.cpp` 需要能看到 `VasePluginDesc_DescriptorProbePlugin`：它在 `Tests/Unit/DescriptorTests.cpp` 里是**匿名命名空间**内的符号，跨 TU 不可见。故本条用例的探针要**就地另写一份** `VASE_PLUGIN`——一个 TU 只能展一次宏，`ManifestJsonTests.cpp` 里此前没有展开过，可以展。执行时把 `VasePluginDesc_DescriptorProbePlugin()` 换成该 TU 自己那只探针的名字。）

- [ ] **Step 8: 提交**

```bash
git add Source/Catalog/Detail/ChoiceCoerce.h Include/Vase/Catalog/ManifestView.h \
        Source/Catalog/PluginCatalog.cpp Tools/VaseCli/Scan.h Tools/VaseCli/Scan.cpp \
        Tests/Unit/ManifestJsonTests.cpp
git commit -m "M5-T9：描述符→清单值的投影（ValueToLabel 回归 + MakeManifestConfigField + 借用物化点）"
```

---

### Task 10: `scan` 的落盘与「三处一致」的端到端

**Files:**
- Modify: `Tools/VaseCli/ManifestMerge.h` / `ManifestMerge.cpp`（改成带陈旧值的返回形）
- Modify: `Tools/VaseCli/Scan.cpp`（`RunScan` 换真实现）
- Test: `Tests/Integration/VaseCliScanTests.cpp`（加端到端）

**Interfaces:**
- Consumes: Task 5 的 `WriteManifestFile`、Task 8 的 `DiscoverPlugins`、Task 9 的 `ManifestEntryFromMeta`
- Produces: `struct tools::cli::MergeOutcome { vase::ManifestEntry Entry; std::optional<std::string> PreviousBinary; bool HadManifest = false; };` 与 `MergeOutcome tools::cli::MergeWithExisting(const std::filesystem::path&, vase::ManifestEntry);`

**退出码口径（本任务定死，D135）**：目录不存在 / 参数错 → **2**；树形态问题（多库、有清单无库）与非零个失败 → **1**；**整棵树零插件 → 2**（「什么都没做」不是成功）；全过 → 0。

- [ ] **Step 1: 写失败测试（端到端那条是三层一致的证人）**

在 `Tests/Integration/VaseCliScanTests.cpp` 追加：

```cpp
TEST(VaseCliScan, PreservesEnabledByDefaultAndReportsBinaryDrift)
{
    const CatalogSandbox sandbox("scan-preserve");
    StagePlugin(sandbox, "Probe", "LoadProbe", VASE_FIXTURE_LOADPROBE);
    // 旧清单：人手关了默认启用，且 binary 写的是个过时的名字。
    sandbox.WriteFile("Probe/plugin.json", R"({"schemaVersion":1,"id":"Vase.LoadProbe",
        "displayName":"装载探针","version":"0.0.1","binary":"StaleName","enabledByDefault":false})");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunScan(sandbox.Root.string(), out, err), tools::cli::kExitOk) << err.str();

    const auto written = vase::ParseManifestFile(sandbox.Root / "Probe" / "plugin.json", "Probe");
    ASSERT_TRUE(written.IsOk()) << written.GetError().Message();
    EXPECT_FALSE(written.Value().EnabledByDefault);           // 保真（D122）
    EXPECT_EQ(written.Value().Binary, "LoadProbe");           // 改成观察事实（D122）
    EXPECT_TRUE(out.str().find("StaleName") != std::string::npos); // 改动被打印，不静默
}

TEST(VaseCliScan, EmptyTreeIsNotASilentSuccess)
{
    const CatalogSandbox sandbox("scan-empty");
    sandbox.CreateDir("docs");
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunScan(sandbox.Root.string(), out, err), tools::cli::kExitUsage);
}

TEST(VaseCliScan, GeneratedManifestIsAcceptedByValidateAndByTheLoader)
{
    // 三处一致的证人（spec §4）：scan 生成的清单 → validate 四项全过 → 加载期也接受。
    // 三处走的是同一份 CompareDescriptor，所以这条同时证 D117 的连带收益。
    const CatalogSandbox sandbox("scan-e2e");
    StagePlugin(sandbox, "Hello", "HelloPlugin", VASE_FIXTURE_HELLO);

    std::ostringstream scanOut;
    std::ostringstream scanErr;
    ASSERT_EQ(tools::cli::RunScan(sandbox.Root.string(), scanOut, scanErr), tools::cli::kExitOk) << scanErr.str();

    std::ostringstream validateOut;
    std::ostringstream validateErr;
    ASSERT_EQ(tools::cli::RunValidate({"validate", sandbox.Root.string()}, validateOut, validateErr),
              tools::cli::kExitOk) << validateErr.str();

    // 加载期：按生成的清单建快照 → 求解 → 装局。
    vase::PluginCatalog catalog;
    const auto refreshed = catalog.Refresh(sandbox.Root);
    ASSERT_TRUE(refreshed.IsOk()) << refreshed.GetError().Message();
    const auto solved = catalog.Solve(vase::LoadRequest{});
    ASSERT_TRUE(solved.IsOk()) << solved.GetError().Message();
    vase::PluginHost host;
    const auto pod = host.CreatePod(solved.Value().Plan);
    ASSERT_TRUE(pod.IsOk()) << pod.GetError().Message();
    EXPECT_TRUE(host.DestroyPod(pod.Value()).Clean());
}
```

**注意**：`RunValidate` 此时只有 Task 6 的桩（返回 2），故最后一条用例在 T10 落地时**会红在 validate 那一步**——那是预期的：它把 T11 的验收提前钉在了这里。执行者可以在 T10 先跑前两条，把第三条第 4 步留到 T11 之后复跑；**不要**为了让它绿而放宽断言。

- [ ] **Step 2: 改 `MergeWithExisting` 的返回形**

`Tools/VaseCli/ManifestMerge.h` 换成：

```cpp
#pragma once

// scan 的回写合并（M5/D122）：描述符背书字段来自二进制，清单独有字段按类处置。
// 单独成文件是因为它是全仓唯一知道「哪些字段是清单独有」的地方——与 D73 同源。

#include "Vase/Catalog/ManifestView.h"

#include <filesystem>
#include <optional>
#include <string>

namespace tools::cli
{

struct MergeOutcome
{
    vase::ManifestEntry Entry;
    std::optional<std::string> PreviousBinary; // 有值 = 旧清单存在且 binary 与之不同（打印用）
    bool HadManifest = false;
};

// 以 fromBinary 为底，只从既有清单取回 enabledByDefault（人手语义，描述符无从推断，D73）。
// 既有清单不存在 / 读不出 → 原样返回 fromBinary（首次生成走缺省）。
MergeOutcome MergeWithExisting(const std::filesystem::path& manifestFile, vase::ManifestEntry fromBinary);

} // namespace tools::cli
```

`Tools/VaseCli/ManifestMerge.cpp`：

```cpp
#include "ManifestMerge.h"

#include "Vase/Catalog/PluginCatalog.h"

namespace tools::cli
{

MergeOutcome MergeWithExisting(const std::filesystem::path& manifestFile, vase::ManifestEntry fromBinary)
{
    MergeOutcome out;
    out.Entry = std::move(fromBinary);
    if (!std::filesystem::exists(manifestFile))
    {
        return out; // 首次生成：enabledByDefault 走缺省（true ⇒ 省略，D122）
    }
    out.HadManifest = true;
    const vase::Result<vase::ManifestEntry> old = vase::ParseManifestFile(manifestFile, out.Entry.Subdirectory);
    if (!old.IsOk())
    {
        return out; // 旧清单读不出：以二进制为准（它就是权威），读不出这件事由调用方打印
    }
    out.Entry.EnabledByDefault = old.Value().EnabledByDefault; // 唯一保真的字段（D122）
    if (old.Value().Binary != out.Entry.Binary)
    {
        out.PreviousBinary = old.Value().Binary;
    }
    return out;
}

} // namespace tools::cli
```

- [ ] **Step 3: `RunScan` 换真实现**

`Tools/VaseCli/Scan.cpp`：替换 Task 6 的桩。**注意卸载顺序**——物化必须在 `Unload` 之前（spec §2.2）。

本步需要把 include 区补全（Task 8 时只有命名面那几个）：

```cpp
#include "Vase/Catalog/ManifestView.h"   // WriteManifestFile
#include "Vase/Host/Inspect.h"           // InspectDescriptors
#include "Vase/Host/Loader.h"            // detail::Loader / BinaryRecord
#include "ManifestMerge.h"
#include <algorithm>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <vector>
```

```cpp
namespace
{

// 一个插件的完整处理结果。收集后再打印：输出按 Id 排序（D136），故不能边扫边打。
struct ScanResult
{
    std::string Id;
    std::string Subdirectory;
    std::optional<std::string> PreviousBinary;
    bool Written = false;
};

} // namespace

namespace tools::cli
{

int RunScan(const std::string& pluginDirectory, std::ostream& out, std::ostream& err)
{
    const vase::Result<Discovery> discovered = DiscoverPlugins(pluginDirectory);
    if (!discovered.IsOk())
    {
        err << discovered.GetError().Message() << '\n';
        return kExitUsage; // 环境错误（D135 第三档）
    }
    const Discovery& discovery = discovered.Value();
    for (const std::string& skipped : discovery.Skipped)
    {
        out << "skip " << skipped << " (neither a library nor a manifest)\n";
    }
    if (discovery.Found.empty() && discovery.Failed.empty())
    {
        err << "no plugins found under " << pluginDirectory << '\n';
        return kExitUsage; // 「什么都没做」不是成功（D135）
    }

    vase::detail::Loader loader;
    std::vector<ScanResult> results;
    bool failed = false;
    for (const std::string& problem : discovery.Failed)
    {
        err << "error: " << problem << '\n';
        failed = true;
    }

    for (const DiscoveredPlugin& plugin : discovery.Found)
    {
        const std::string subdirectory = plugin.Directory.filename().string();
        vase::Result<vase::detail::BinaryRecord*> record = loader.EnsureResident(plugin.BinaryPath);
        if (!record.IsOk())
        {
            err << "error: " << subdirectory << ": " << record.GetError().Message() << '\n';
            failed = true;
            continue;
        }
        const vase::Result<std::vector<const vase::PluginDescriptor*>> descriptors =
            vase::InspectDescriptors(*record.Value());
        if (!descriptors.IsOk())
        {
            err << "error: " << subdirectory << ": " << descriptors.GetError().Message() << '\n';
            loader.Unload(*record.Value());
            failed = true;
            continue;
        }
        if (descriptors.Value().size() != 1U)
        {
            // 组合库（一库 N 插件）本波不做（D126）：目录模型装不下它，响亮拒绝而不是猜。
            err << "error: " << subdirectory << ": combined libraries (N plugins in one binary) are not supported yet\n";
            loader.Unload(*record.Value());
            failed = true;
            continue;
        }
        // 借用 → 拥有必须发生在 Unload 之前（寿命纪律）。
        const MergeOutcome merged = MergeWithExisting(plugin.Directory / "plugin.json",
                                                     ManifestEntryFromMeta(*descriptors.Value()[0]->Meta,
                                                                           subdirectory, plugin.Stem));
        loader.Unload(*record.Value());

        const vase::Result<void> written = vase::WriteManifestFile(plugin.Directory / "plugin.json", merged.Entry);
        if (!written.IsOk())
        {
            err << "error: " << subdirectory << ": " << written.GetError().Message() << '\n';
            failed = true;
            continue;
        }
        results.push_back(ScanResult{.Id = merged.Entry.Id,
                                     .Subdirectory = subdirectory,
                                     .PreviousBinary = merged.PreviousBinary,
                                     .Written = true});
    }

    std::sort(results.begin(), results.end(),
              [](const ScanResult& left, const ScanResult& right) { return left.Id < right.Id; });
    for (const ScanResult& result : results)
    {
        out << "wrote " << result.Subdirectory << "/plugin.json id=" << result.Id << '\n';
        if (result.PreviousBinary.has_value())
        {
            out << "  binary: \"" << *result.PreviousBinary << "\" -> actual file name stem (D122)\n";
        }
    }
    return failed ? kExitCheckFailed : kExitOk;
}

} // namespace tools::cli
```

- [ ] **Step 4: 跑前两条用例确认通过**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R '^VaseCliScan\.' --output-on-failure
```

Expected: `PreservesEnabledByDefaultAndReportsBinaryDrift` 与 `EmptyTreeIsNotASilentSuccess` 通过；`GeneratedManifestIsAcceptedByValidateAndByTheLoader` 红在 validate 的桩上（预期，见 Step 1 的注意）。

- [ ] **Step 5: 提交**

```bash
git add Tools/VaseCli/ManifestMerge.h Tools/VaseCli/ManifestMerge.cpp Tools/VaseCli/Scan.cpp \
        Tests/Integration/VaseCliScanTests.cpp
git commit -m "M5-T10：scan 落盘（保真合并 + 原子写 + 按 Id 排序输出）与三处一致的端到端"
```

---

### Task 11: `validate` 第①②项

**Files:**
- Modify: `Tools/VaseCli/Validate.h` / `Validate.cpp`
- Test: `Tests/Integration/VaseCliValidateTests.cpp`（新建，加进 `Tests/CMakeLists.txt` 源清单）

**Interfaces:**
- Consumes: `vase::PluginCatalog::Refresh` / `Ids` / `Find` / `Directory`、`vase::BuildExpectation`、`vase::CompareDescriptor`（Task 3）、`vase::InspectDescriptors`（Task 2）、Task 4 的 `LibraryFileName`
- Produces:
  - `struct tools::cli::ValidateOptions { std::string PluginDirectory; std::vector<vase::ServiceRef> HostProvided; };`（`--host-provides` 的解析在 Task 12 填）
  - `int tools::cli::RunValidate(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);`

**流程（spec §3.1，manifest 驱动）**：`Refresh` → 成败即第二项 → 逐 `Ids()`：按 `Directory()/Subdirectory/LibraryFileName(Binary)` 装载 → `InspectDescriptors` → 与 `BuildExpectation(entry)` 比对（第①项）。第④项在 Task 13 挂进同一个循环。

- [ ] **Step 1: 写失败测试**

`Tests/Integration/VaseCliValidateTests.cpp`：

```cpp
// validate 的第①②项（M5/D120/D130）：快照不建成 = 第二项；逐插件比对 = 第一项。
#include "Validate.h"

#include "Vase/Catalog/LibraryFileName.h"

#include "CatalogSandbox.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

namespace
{

using testing_support::CatalogSandbox;
namespace fs = std::filesystem;

void StagePlugin(const CatalogSandbox& sandbox, const std::string& subdirectory, const std::string& stem,
                 const char* fixturePath)
{
    sandbox.CopyFile(subdirectory + "/" + vase::catalog_detail::LibraryFileName(stem), fixturePath);
}

// 与 LoadProbe.cpp 的描述符逐字对齐的清单（scan 会生成同样的东西，这里手写以隔离变量）。
// **`binary` 必须写**：缺它时解析器按子目录名（D54 缺省）拼路径，StagePlugin 放的是
// LibraryFileName("LoadProbe")，两者不一致 → 会红在「装载失败」而不是想验的那一项。
constexpr const char* kLoadProbeManifest = R"({
    "schemaVersion": 1,
    "id": "Vase.LoadProbe",
    "displayName": "装载探针",
    "version": "0.0.1",
    "binary": "LoadProbe"
})";

TEST(VaseCliValidate, AgreeingManifestAndBinaryPass)
{
    const CatalogSandbox sandbox("validate-ok");
    StagePlugin(sandbox, "Probe", "LoadProbe", VASE_FIXTURE_LOADPROBE);
    sandbox.WriteFile("Probe/plugin.json", kLoadProbeManifest);

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunValidate({"validate", sandbox.Root.string()}, out, err), tools::cli::kExitOk)
        << err.str();
}

TEST(VaseCliValidate, DriftedDisplayNameIsCaughtByComparison)
{
    const CatalogSandbox sandbox("validate-drift");
    StagePlugin(sandbox, "Probe", "LoadProbe", VASE_FIXTURE_LOADPROBE);
    // 只改 displayName 一个字段：单变量，命中的必须是第①项而不是别的。
    sandbox.WriteFile("Probe/plugin.json", R"({
        "schemaVersion": 1, "id": "Vase.LoadProbe", "displayName": "改过的名字", "version": "0.0.1",
        "binary": "LoadProbe"})");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunValidate({"validate", sandbox.Root.string()}, out, err), tools::cli::kExitCheckFailed);
    EXPECT_NE(out.str().find("displayName"), std::string::npos); // 比对器已给出字段路径
}

TEST(VaseCliValidate, DuplicateIdIsTheSecondCheckAndSaysTheSnapshotWasNotBuilt)
{
    const CatalogSandbox sandbox("validate-dup");
    StagePlugin(sandbox, "One", "LoadProbe", VASE_FIXTURE_LOADPROBE);
    StagePlugin(sandbox, "Two", "UnloadProbe", VASE_FIXTURE_UNLOADPROBE);
    // 两份清单声明同一个 Id → Refresh 硬报错（D50）。
    sandbox.WriteFile("One/plugin.json", kLoadProbeManifest);
    sandbox.WriteFile("Two/plugin.json", kLoadProbeManifest);

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunValidate({"validate", sandbox.Root.string()}, out, err), tools::cli::kExitCheckFailed);
    // D130：快照没建成时不许假装其余三项通过——输出里必须说清这件事。
    EXPECT_NE(out.str().find("snapshot not built"), std::string::npos);
}

TEST(VaseCliValidate, MissingDirectoryIsAUsageError)
{
    const CatalogSandbox sandbox("validate-nodir");
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunValidate({"validate", (sandbox.Root / "nope").string()}, out, err),
              tools::cli::kExitUsage);
}

} // namespace
```

- [ ] **Step 2: 接线**

`Tests/CMakeLists.txt` 的源清单加 `Integration/VaseCliValidateTests.cpp`。

- [ ] **Step 3: 跑测试确认失败**

```bash
cmake --build --preset win-x64-clang-debug --target VaseTests
```

Expected: 编译失败——`ValidateOptions` / `kExitCheckFailed` 未声明（`kExitCheckFailed` 在 Task 6 的 `Cli.h` 里已有，故实际会红在 `ValidateOptions`）。

- [ ] **Step 4: 实现**

`Tools/VaseCli/Validate.h`：

```cpp
#pragma once

#include "Vase/Detail/Result.h"
#include "Vase/PluginDescriptor.h"

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace tools::cli
{

struct ValidateOptions
{
    std::filesystem::path PluginDirectory;
    std::vector<vase::ServiceRef> HostProvided; // --host-provides 喂进 LoadRequest（D131，Task 12 解析）
};

// 四项检查（spec §3.2）：① 清单↔二进制一致 ② Id 重复 ③ 依赖图/环/版本 ④ 服务命名前缀。
// 退出码见 Cli.h；② 失败时快照不建成，只报该项（D130）。
int RunValidate(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

// 供用例直接调：跳过 argv 解析。
int ValidateDirectory(const ValidateOptions& options, std::ostream& out, std::ostream& err);

} // namespace tools::cli
```

`Tools/VaseCli/Validate.cpp`：

```cpp
#include "Validate.h"

#include "Cli.h"

#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Catalog/LibraryFileName.h"
#include "Vase/Host/Inspect.h"
#include "Vase/Host/Loader.h"
#include "Vase/Host/ManifestExpectation.h"

#include <ostream>
#include <string>
#include <vector>

namespace tools::cli
{

int ValidateDirectory(const ValidateOptions& options, std::ostream& out, std::ostream& err)
{
    vase::PluginCatalog catalog;
    const vase::Result<void> refreshed = catalog.Refresh(options.PluginDirectory);
    if (!refreshed.IsOk())
    {
        // 第②项（D130）：快照没建成 ⇒ 只报这一项，明说其余三项没跑。
        out << "check 2 (duplicate plugin id): FAIL\n";
        out << "  " << refreshed.GetError().Message() << '\n';
        out << "snapshot not built: checks 1/3/4 were not run\n";
        return kExitCheckFailed;
    }
    out << "check 2 (duplicate plugin id): ok\n";

    // D128：子目录缺 plugin.json 是**常态**（§4.3），Catalog 已按 D50 跳过后记在 Warnings 里。
    // 打印但**不计入退出码**——validate 报的是「存在的东西不对」，不是「少了个东西」。
    for (const vase::CatalogWarning& warning : catalog.Warnings())
    {
        out << "  warning: " << warning.Subdirectory << ": " << warning.Message << '\n';
    }

    vase::detail::Loader loader;
    bool failed = false;
    out << "check 1 (manifest vs binary):\n";
    for (const std::string& id : catalog.Ids())
    {
        const vase::ManifestEntry* entry = catalog.Find(id);
        if (entry == nullptr)
        {
            continue; // Ids() 与 Find() 同源，取不到是不可能的；防御性地跳过而非崩溃
        }
        const std::filesystem::path binary =
            catalog.Directory() / entry->Subdirectory / vase::catalog_detail::LibraryFileName(entry->Binary);
        vase::Result<vase::detail::BinaryRecord*> record = loader.EnsureResident(binary);
        if (!record.IsOk())
        {
            out << "  " << id << ": FAIL (binary not loadable: " << record.GetError().Message() << ")\n";
            failed = true;
            continue;
        }
        const vase::Result<std::vector<const vase::PluginDescriptor*>> descriptors =
            vase::InspectDescriptors(*record.Value());
        if (!descriptors.IsOk() || descriptors.Value().size() != 1U)
        {
            out << "  " << id << ": FAIL ("
                << (descriptors.IsOk() ? std::string{"expected exactly one descriptor"}
                                       : descriptors.GetError().Message())
                << ")\n";
            loader.Unload(*record.Value());
            failed = true;
            continue;
        }
        const vase::ManifestExpectation expected = vase::BuildExpectation(*entry);
        const vase::Result<void> compared = vase::CompareDescriptor(expected, *descriptors.Value()[0]);
        if (compared.IsOk())
        {
            out << "  " << id << ": ok\n";
        }
        else
        {
            out << "  " << id << ": FAIL\n    " << compared.GetError().Message() << '\n';
            failed = true;
        }
        loader.Unload(*record.Value()); // 借用止于此：比对照着驻留期做完才卸
    }

    // 第③项（Task 12）、第④项（Task 13）在此接入。
    return failed ? kExitCheckFailed : kExitOk;
}

int RunValidate(const std::vector<std::string>& args, std::ostream& out, std::ostream& err)
{
    if (args.size() < 2U)
    {
        err << "usage: VaseCli validate <插件目录> [--host-provides <name>@<version>]...\n";
        return kExitUsage;
    }
    ValidateOptions options;
    options.PluginDirectory = args[1];
    // --host-provides 的解析在 Task 12 补；本任务先只认目录。
    if (args.size() > 2U)
    {
        err << "unexpected extra arguments\n";
        return kExitUsage;
    }
    return ValidateDirectory(options, out, err);
}

} // namespace tools::cli
```

- [ ] **Step 5: 跑测试确认通过（含 T10 那条端到端）**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R '^VaseCli' --output-on-failure
```

Expected: `VaseCliValidate.*` 4 条全过，**并且 T10 的 `GeneratedManifestIsAcceptedByValidateAndByTheLoader` 由红转绿**——这正是「三处一致」的兑现。

- [ ] **Step 6: 提交**

```bash
git add Tools/VaseCli/Validate.h Tools/VaseCli/Validate.cpp \
        Tests/Integration/VaseCliValidateTests.cpp Tests/CMakeLists.txt
git commit -m "M5-T11：validate 第①②项（快照不建成的报法 + 逐插件比对）"
```

---

### Task 12: `validate` 第③项与 `--host-provides`

**Files:**
- Modify: `Tools/VaseCli/Validate.cpp`
- Test: `Tests/Integration/VaseCliValidateTests.cpp`

**Interfaces:**
- Consumes: `vase::PluginCatalog::Solve(const LoadRequest&)`、`vase::LoadRequest::HostProvided`（D60）
- Produces: `--host-provides <name>@<version>`（可重复）的解析；第③项的判定口径

**判定口径（spec §3.2 第③行，D127/D131）**：`Solve` 返 `Err` → 该红并透传原文（环与被阻塞共用一个出口，不另分列）；`kSkip + kMissingDependency` / `kSkip + kVersionMismatch` → **算未过**；`kSkip + kDisabled` → **不算未过**（那是静态跳过，§4.4）；`Notes` 四类逐条打印。

- [ ] **Step 1: 写失败测试**

在 `Tests/Integration/VaseCliValidateTests.cpp` 追加：

```cpp
// 第三项的语料用 DependentPlugin：它 Requires `Vase.Hello.Greeter`（Hello 不在树里即缺失），
// 且它的 provides 是 `Vase.Dependent.Farewell`——**第四项合规**（T7 改名之后），
// 所以「第三项的红」是单变量的。清单由 scan 现场生成，不手写——手写就得逐字猜描述符。
void StageDependentWithGeneratedManifest(const CatalogSandbox& sandbox)
{
    StagePlugin(sandbox, "Dependent", "DependentPlugin", VASE_FIXTURE_DEPENDENT);
    std::ostringstream scanOut;
    std::ostringstream scanErr;
    ASSERT_EQ(tools::cli::RunScan(sandbox.Root.string(), scanOut, scanErr), tools::cli::kExitOk) << scanErr.str();
}

TEST(VaseCliValidate, UnsatisfiedDependencyFailsTheThirdCheck)
{
    const CatalogSandbox sandbox("validate-missing");
    StageDependentWithGeneratedManifest(sandbox);

    std::ostringstream out;
    std::ostringstream err;
    // 不喂 --host-provides：Vase.Hello.Greeter 没人提供 → 第三项未过。
    EXPECT_EQ(tools::cli::RunValidate({"validate", sandbox.Root.string()}, out, err), tools::cli::kExitCheckFailed);
    EXPECT_NE(out.str().find("check 3"), std::string::npos);
    EXPECT_NE(out.str().find("Vase.Dependent"), std::string::npos);
}

TEST(VaseCliValidate, HostProvidedServicesSatisfyTheThirdCheck)
{
    const CatalogSandbox sandbox("validate-hostprov");
    StageDependentWithGeneratedManifest(sandbox);

    std::ostringstream out;
    std::ostringstream err;
    // 宿主声明它会注册这条服务（D60 的既有模型）：第三项不再误报，其余三项本就全过。
    const int rc = tools::cli::RunValidate(
        {"validate", sandbox.Root.string(), "--host-provides", "Vase.Hello.Greeter@1"}, out, err);
    EXPECT_EQ(rc, tools::cli::kExitOk) << out.str() << err.str();
}

TEST(VaseCliValidate, MalformedHostProvidesIsAUsageError)
{
    const CatalogSandbox sandbox("validate-badprov");
    StageDependentWithGeneratedManifest(sandbox);

    std::ostringstream out;
    std::ostringstream err;
    // 版本必须 ≥1（与 D64 的解析期闸同口径）：0 与「没有 @」都是用法错误。
    EXPECT_EQ(tools::cli::RunValidate({"validate", sandbox.Root.string(), "--host-provides", "Vase.Hello.Greeter@0"},
                                      out, err),
              tools::cli::kExitUsage);
    EXPECT_EQ(tools::cli::RunValidate({"validate", sandbox.Root.string(), "--host-provides", "Vase.Hello.Greeter"}, out,
                                      err),
              tools::cli::kExitUsage);
}
```

- [ ] **Step 2: 跑测试确认失败**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R '^VaseCliValidate\.' --output-on-failure
```

Expected: 3 条新用例全红（第③项还没接、`--host-provides` 还没解析）。

- [ ] **Step 3: 解析 `--host-provides`**

`Tools/VaseCli/Validate.cpp` 的匿名命名空间加：

```cpp
// "name@version"：版本必须 ≥1（与 D64 的解析期闸同口径）；名字不得为空。
std::optional<vase::ServiceRef> ParseHostProvided(std::string_view text)
{
    const std::size_t at = text.rfind('@');
    if (at == std::string_view::npos || at == 0U || at + 1U >= text.size())
    {
        return std::nullopt;
    }
    std::uint32_t version = 0;
    const std::string_view digits = text.substr(at + 1U);
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), version);
    if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size() || version < 1U)
    {
        return std::nullopt;
    }
    return vase::ServiceRef{.Name = text.substr(0, at), .Version = version};
}
```

`RunValidate` 换成完整解析：

```cpp
int RunValidate(const std::vector<std::string>& args, std::ostream& out, std::ostream& err)
{
    if (args.size() < 2U)
    {
        err << "usage: VaseCli validate <插件目录> [--host-provides <name>@<version>]...\n";
        return kExitUsage;
    }
    ValidateOptions options;
    options.PluginDirectory = args[1];
    for (std::size_t index = 2U; index < args.size(); ++index)
    {
        if (args[index] != "--host-provides" || index + 1U >= args.size())
        {
            err << "usage: VaseCli validate <插件目录> [--host-provides <name>@<version>]...\n";
            return kExitUsage;
        }
        const std::optional<vase::ServiceRef> provided = ParseHostProvided(args[index + 1U]);
        if (!provided.has_value())
        {
            err << "invalid --host-provides value: " << args[index + 1U] << " (expected <name>@<version>=1)\n";
            return kExitUsage;
        }
        options.HostProvided.push_back(*provided);
        ++index;
    }
    return ValidateDirectory(options, out, err);
}
```

- [ ] **Step 4: 接第③项**

在 `ValidateDirectory` 的比对循环之后（返回之前）加：

```cpp
    // —— 第③项：依赖图 / 环 / 版本（D127/D131）。Solve 无 preset = 目录全集按 enabledByDefault。
    vase::LoadRequest request;
    request.HostProvided = options.HostProvided;
    out << "check 3 (dependency graph):\n";
    const vase::Result<vase::SolveOutcome> solved = catalog.Solve(request);
    if (!solved.IsOk())
    {
        // 环与被阻塞共用一个出口（D127）：原文透传，不另造「环」这个分类。
        out << "  FAIL: " << solved.GetError().Message() << '\n';
        failed = true;
    }
    else
    {
        for (const vase::LoadPlanEntry& entry : solved.Value().Plan.Ordered)
        {
            if (entry.Decision == vase::LoadDecision::kLoad)
            {
                continue;
            }
            if (entry.Reason == vase::SkipReason::kDisabled)
            {
                out << "  " << entry.Id << ": skip (disabled)\n"; // 静态跳过不算未过（§4.4）
                continue;
            }
            out << "  " << entry.Id << ": FAIL ("
                << (entry.Reason == vase::SkipReason::kMissingDependency ? "missing dependency"
                                                                        : "service version mismatch")
                << ")\n";
            failed = true;
        }
        for (const vase::SolveNote& note : solved.Value().Notes)
        {
            out << "  note: " << note.Message << '\n';
        }
    }
```

include 区补 `<charconv>`、`<optional>`、`<cstdint>`。

- [ ] **Step 5: 跑测试确认通过**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R '^VaseCliValidate\.' --output-on-failure
```

Expected: 7 条全过。

- [ ] **Step 6: 提交**

```bash
git add Tools/VaseCli/Validate.cpp Tests/Integration/VaseCliValidateTests.cpp
git commit -m "M5-T12：validate 第③项与 --host-provides（kDisabled 不算未过，环/阻塞如实透传）"
```

---

### Task 13: `validate` 第④项（服务命名前缀 + 宿主越界）

**Files:**
- Modify: `Tools/VaseCli/Validate.cpp`
- Test: `Tests/Integration/VaseCliValidateTests.cpp`

**Interfaces:**
- Consumes: 描述符的 `Meta->Provides`（`MetaArray<ServiceRef, 16>`）、快照的全量 Id、`options.HostProvided`
- Produces: 第④项的两条判据（spec §3.2 末段）

**判据**：① 每条 `Provides[i].Name` 必须以 `<插件Id>.` 为前缀（逐条独立判定，**不遇错即停**）；② `--host-provides` 喂进来的每个名字**不得落在快照里任一插件 Id 的前缀之下**（越界占用）。宿主名字**不做**「符合自家前缀」的判定——宿主没有 Id 可对照（D132）。

- [ ] **Step 1: 写失败测试**

```cpp
// SharedProviderPlugin：Id `Vase.SharedProvider`，provides 写的是 `Vase.Test.Shared`——第四项的
// 单变量反例（事实取证注⑧）。
TEST(VaseCliValidate, NamingViolationIsReportedForSharedProvider)
{
    const CatalogSandbox sandbox("validate-naming");
    StagePlugin(sandbox, "SharedProvider", "SharedProviderPlugin", VASE_FIXTURE_SHAREDPROVIDER);
    sandbox.CopyFile("SharedProvider/plugin.json",
                     std::filesystem::path{VASE_FIXTURE_MANIFESTS} / "shared_provider" / "plugin.json");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::RunValidate({"validate", sandbox.Root.string()}, out, err), tools::cli::kExitCheckFailed);
    EXPECT_NE(out.str().find("check 4"), std::string::npos);
    EXPECT_NE(out.str().find("Vase.Test.Shared"), std::string::npos); // 违规条点名了服务
    // 第①项也跑到了（该 fixture 的清单按 D57 逐字义务与描述符一致，故它应当 ok）——
    // 若这一行红，说明清单与描述符漂了，那是**发现**，不是本用例写错。
    EXPECT_NE(out.str().find("check 1 (manifest vs binary):\n  Vase.SharedProvider: ok"), std::string::npos);
}

TEST(VaseCliValidate, HostProvidedNameUnderAPluginIdIsCaught)
{
    const CatalogSandbox sandbox("validate-squat");
    StagePlugin(sandbox, "SharedProvider", "SharedProviderPlugin", VASE_FIXTURE_SHAREDPROVIDER);
    sandbox.CopyFile("SharedProvider/plugin.json",
                     std::filesystem::path{VASE_FIXTURE_MANIFESTS} / "shared_provider" / "plugin.json");

    std::ostringstream out;
    std::ostringstream err;
    // 宿主占用某插件 Id 之下的名字 = 越界（D132 的第二条判据）。
    const int rc = tools::cli::RunValidate(
        {"validate", sandbox.Root.string(), "--host-provides", "Vase.SharedProvider.Hijacked@1"}, out, err);
    EXPECT_EQ(rc, tools::cli::kExitCheckFailed);
    EXPECT_NE(out.str().find("Hijacked"), std::string::npos);
}
```

- [ ] **Step 2: 跑测试确认失败**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R '^VaseCliValidate\.' --output-on-failure
```

Expected: 2 条新用例全红。

- [ ] **Step 3: 实现**

`Tools/VaseCli/Validate.cpp` 的匿名命名空间加：

```cpp
std::string PrefixOf(std::string_view id) { return std::string(id) + "."; }

bool HasPrefix(std::string_view text, std::string_view prefix)
{
    return text.size() > prefix.size() && text.substr(0, prefix.size()) == prefix;
}

// 第④项判据（spec §3.2 末段）。只覆盖**服务**——事件名在描述符与清单里都没有载体（事实取证注⑨）。
// 逐条判定、不遇错即停：一次报全，人才修得动。
std::vector<std::string> NamingViolations(std::string_view pluginId, const vase::PluginMeta& meta,
                                          const std::vector<std::string>& allPluginIds,
                                          const std::vector<vase::ServiceRef>& hostProvided)
{
    std::vector<std::string> out;
    const std::string prefix = PrefixOf(pluginId);
    for (const vase::ServiceRef& service : meta.Provides)
    {
        if (!HasPrefix(service.Name, prefix))
        {
            out.push_back(std::string(pluginId) + " provides \"" + std::string(service.Name) +
                          "\" which is not under its own id prefix \"" + prefix + "\"");
        }
    }
    // 反向：宿主提供的名字不得落在任何插件 Id 之下（占用他人命名空间是明确的错）。
    for (const vase::ServiceRef& hosted : hostProvided)
    {
        for (const std::string& otherId : allPluginIds)
        {
            if (HasPrefix(hosted.Name, PrefixOf(otherId)))
            {
                out.push_back("host-provided \"" + std::string(hosted.Name) + "\" squats the namespace of plugin \"" +
                              otherId + "\"");
            }
        }
    }
    return out;
}
```

在 `ValidateDirectory` 里，收集全量 Id（循环外一次）：

```cpp
    const std::vector<std::string> allIds = catalog.Ids();
```

并在比对循环内、`CompareDescriptor` 之后加：

```cpp
        for (const std::string& violation : NamingViolations(id, *descriptors.Value()[0]->Meta, allIds,
                                                            options.HostProvided))
        {
            out << "  " << violation << '\n';
            failed = true;
        }
```

再在循环**之前**打出节标题（与其它节同形）：

```cpp
    out << "check 4 (service naming prefix):\n";
```

- [ ] **Step 4: 跑测试确认通过**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R '^VaseCli' --output-on-failure
```

Expected: 全过。

- [ ] **Step 5: 补 `Samples/` 的验收判据（改名后的零违规）**

在 `Tests/Integration/VaseCliValidateTests.cpp` 追加（`Samples/` 的产物路径从既有的 `VASE_FIXTURE_HELLO` / `VASE_FIXTURE_DEPENDENT` 取，清单须先由 `scan` 生成——这正是本波的正常用法）：

```cpp
TEST(VaseCliValidate, SamplesScanThenValidateHasNoNamingViolation)
{
    // D121 的验收：改名后 Samples 的两只合规插件应当零违规。
    const CatalogSandbox sandbox("validate-samples");
    StagePlugin(sandbox, "Hello", "HelloPlugin", VASE_FIXTURE_HELLO);
    StagePlugin(sandbox, "Dependent", "DependentPlugin", VASE_FIXTURE_DEPENDENT);

    std::ostringstream scanOut;
    std::ostringstream scanErr;
    ASSERT_EQ(tools::cli::RunScan(sandbox.Root.string(), scanOut, scanErr), tools::cli::kExitOk) << scanErr.str();

    std::ostringstream out;
    std::ostringstream err;
    // 依赖链：Dependent 要求 Vase.Hello.Greeter（Hello 提供）→ 第三项应满足。
    EXPECT_EQ(tools::cli::RunValidate({"validate", sandbox.Root.string()}, out, err), tools::cli::kExitOk)
        << out.str() << err.str();
}
```

- [ ] **Step 6: 提交**

```bash
git add Tools/VaseCli/Validate.cpp Tests/Integration/VaseCliValidateTests.cpp
git commit -m "M5-T13：validate 第④项（服务前缀自洽 + 宿主越界）与 Samples 零违规验收"
```

---

### Task 14: M4 遗留同域两笔

**Files:**
- Modify: `Tests/HotSwap/AdoptManifestTests.cpp:73`
- Modify: `Tests/HotSwap/EjectTests.cpp`（在 `SemanticDependencyAbsentWithoutProcessStateReset` 之后加一条）

**Interfaces:**
- Consumes: 既有 `EjectTests.cpp` 匿名命名空间的 `Plan(...)` 助手、`VASE_FIXTURE_STATEFUL` / `VASE_FIXTURE_NEIGHBORB`
- Produces: 无（纯测试改动）；六线基数各 +1

- [ ] **Step 1: 删掉那处恒真断言**

`Tests/HotSwap/AdoptManifestTests.cpp:73` 整行删除：

```cpp
    EXPECT_TRUE(adopted.Value().ManifestVerified); // 清单轨比对跑过且通过（D69 新轨成功态）
```

**理由（写进就地注释，替换原行）**：

```cpp
    // 此处曾有 EXPECT_TRUE(ManifestVerified)：恒真——PluginHost.cpp 在成功态那一个分支上置它、
    // 拒绝态不携报告，故任何拿得到的 AdoptReport 都满足。该字段在下游另有真证人
    // （VaseConsoleAdoptManifestSyncedAdoptReason 钉 manifestVerified= 的输出文本）。
```

- [ ] **Step 2: 跑该文件确认没删坏**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R '^AdoptManifest\.' --output-on-failure
```

Expected: 全过。

- [ ] **Step 3: 写跨 Pod falsifier（先写测试）**

在 `Tests/HotSwap/EjectTests.cpp` 的 `TEST(Eject, SemanticDependencyAbsentWithoutProcessStateReset)` 之后追加：

```cpp
TEST(Eject, SemanticDependencyPossibleAcrossPods)
{
    // M4/D110 的跨 Pod falsifier（M5/D139）：既有三条证人的邻居都在**同一局**里，把
    // 「进程内任一 Pod」的计数限制到本 Pod 的变异会让它们全绿。邻居放到另一局，才咬得住。
    vase::PluginHost host;
    const vase::PodHandle first = host.CreatePod(Plan({{"Vase.Stateful", VASE_FIXTURE_STATEFUL}})).Value();
    const vase::PodHandle second = host.CreatePod(Plan({{"Vase.NeighborB", VASE_FIXTURE_NEIGHBORB}})).Value();

    const vase::Result<vase::EjectReport> r = host.EjectPlugin(first, "Vase.Stateful");
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    EXPECT_EQ(r.Value().Status, vase::EjectStatus::kEjected);
    EXPECT_FALSE(r.Value().ProcessStatesReset.empty());
    EXPECT_TRUE(r.Value().SemanticDependencyPossible); // ← 邻居在另一局，仍须置位

    EXPECT_TRUE(host.DestroyPod(second).Clean());
    EXPECT_TRUE(host.DestroyPod(first).Clean());
}
```

- [ ] **Step 4: 跑测试确认通过**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R '^Eject\.' --output-on-failure
```

Expected: 全过（新增 1 条）。

- [ ] **Step 5: 用变异证明这条有牙（本任务的判据）**

临时把 `Source/Host/PluginHost.cpp` 里算 `SemanticDependencyPossible` 的那处「进程内」集合收窄成「本 Pod」的实例集合，`cmake --build` 后跑：

```bash
ctest --preset win-x64-clang-debug -R '^Eject\.SemanticDependency' --output-on-failure
```

Expected: 红落在 `SemanticDependencyPossibleAcrossPods`（其余三条仍绿——**这正是本条存在的理由**）。**变异必须当场撤回**，撤回后复跑一次确认全绿。

- [ ] **Step 6: 提交**

```bash
git add Tests/HotSwap/AdoptManifestTests.cpp Tests/HotSwap/EjectTests.cpp
git commit -m "M5-T14：还 M4 同域两笔（删恒真断言、补 D110 的跨 Pod falsifier）"
```

---

### Task 15: 文书义务

**Files:**
- Modify: `wiki/vase-architecture.md`（spec §7 表的七处）
- Modify: `CLAUDE.md`（项目状态 + 「仓库里有什么」）
- 复核：`.claude/skills/vase-cpp-engineering/references/architecture.md`

**Interfaces:** 无（纯文档）。**规矩 7**：阈值与基数只住 `CLAUDE.md`，技能里出现即违规。

- [ ] **Step 1: 定位全部同口径句**

```bash
grep -n "从不加载\|不加载任何 DLL\|不必执行 DLL" wiki/vase-architecture.md
```

Expected: 命中 §1.1 与 §3.1 两处（spec 事实取证注⑬ 记的是两处；`§10` 是否有同口径以本条实测为准——**若冒出第三处一并处理**，这正是本步存在的理由）。

- [ ] **Step 2: 逐处挂勘误**

按 spec §7 表改七处：§1.1 分层表行、§3.1 的两句（「不必执行 DLL 里的任何代码」与 POD 段落那句）、§3.1 的描述符样例（`Vase.DamageSystem` → `Vase.Combat.DamageSystem`）、§8.1 的唯一一跳、§6.1 的前缀与宿主记账、§11.1 的 `scan` 段、§12.3 的 M5 行、§13.3 的两条新静默面。**每处都带日期与依据指针**（M5/D1xx），与本仓库既有的勘误块同形。

- [ ] **Step 3: 改 `CLAUDE.md`**

「项目状态」加 M5 第一波；「仓库里有什么」补 `Tools/VaseCli` 与三个新公开面（`InspectDescriptors`、提升后的 `CompareDescriptor`、命名的正反两函数）。**基数与 tidy 数字留到 Task 16 落账**（那时才有实测值）。

- [ ] **Step 4: 复核技能**

```bash
grep -rn "从不加载\|不必执行 DLL\|kHeaderVersion" .claude/skills/vase-cpp-engineering/ | grep -v cpp-core-guidelines.md
```

Expected: 只命中「判据与理由」那一类（允许两边各写一份）；**若命中阈值或基数，删掉并改成指向 `CLAUDE.md` 的指针**（规矩 7 的自检）。

- [ ] **Step 5: 跑一遍规矩 7 的自检 grep**

```bash
grep -rn "Total Tests\|[0-9][0-9] TU\|DEBUG:FULL\|build-id=sha1\|EHs-c-\|HAS_EXCEPTIONS\|--cached --others\|WarningsAsErrors\|PRE_TEST\|ctest --preset\|run-clang-tidy -p\|cmake --build --preset" \
  .claude/skills/vase-cpp-engineering/ | grep -v cpp-core-guidelines.md
```

Expected: 命中项逐条属「允许两类」（嵌在论述里的承重 flag / 配置值，且带就地指针；或判据与理由）。**容器的既有口径如此**，本步只做复核、不新增命中。

- [ ] **Step 6: 提交**

```bash
git add wiki/vase-architecture.md CLAUDE.md
git commit -m "M5-T15：文书义务（scan/validate 口径、枚举面与唯一一跳的分工、前缀记账）"
```

---

### Task 16: 收口——六线全量验证、基数落账、折并

**Files:**
- Modify: `CLAUDE.md`（两张表 + 项目状态）
- 可能 Modify: `README.md`（若它引用了 `Tools/VaseCli` 的存在性——本波前它不存在）

**Interfaces:** 无。

- [ ] **Step 1: 六线全量（删树重配）**

```bash
Scripts/win-verify.cmd
```

Linux 侧（脚本落盘后再走登录 shell，避开 `$` 展开）：

```bash
wsl -d Ubuntu -- bash -lc 'bash /mnt/d/Git/Vase/Scripts/linux-verify.sh'
```

Expected: 六线全绿、构建零警告。**若某线红**：先看是不是缺产物（`z-applocal` 拷贝步的文件锁假红——本仓库已撞过四次），按协议**单线删树重跑一次**再判。

- [ ] **Step 2: 读六线 `ctest -N` 基数并逐位对账**

各线 `Total Tests` 相对 M4 收口表（267/266/267/266/267/266）应**六线同幅增长**，两笔来源要分开记：**gtest 那批**（本波新增的用例条数，进 `VaseTests` 的发现结果）**+ ctest 那四条**（`VaseCli*`，Task 6 注册在 `Tools/VaseCli/CMakeLists.txt`，六线各 +4、无平台门）。Console 的 33 条不动。用名集合差集独立核：`debug − release` 仍应**恰为** `EffectScopeDeath.CreateAfterDisposeTerminates` 那一条，`linux − win` 与 `win − linux` 两个方向都应为空。

- [ ] **Step 3: 三条 tidy 线与 format**

```bash
Scripts\win-clang-tidy.cmd
wsl -d Ubuntu -- bash -lc 'bash /mnt/d/Git/Vase/Scripts/linux-clang-tidy.sh'
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
```

Expected: 三线**首跑即三判据全过**（退出 0 + 正文 `error:` 0 + 正文 `warning:` 0）、format 零 violation。**TU 数会从 99 上跳**（新增 TU：`Tools/VaseCli/` 五个源 + `Source/Host/Inspect.cpp` + 三个新测试文件）——上跳本身不是错，**要看它变在哪一类**：单 TU 极值（`998 / 45609` 与 `359 / 11337`）若也动，那才是新诊断。

- [ ] **Step 4: 跑一次规矩 6 的窄选择子（双平台各留证据）**

```bash
ctest --preset win-x64-clang-debug -R 'HotSwap|Eject|Adopt'
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && ctest --preset linux-x64-clang-debug -R "HotSwap|Eject|Adopt"'
```

Expected: 两侧全过。本波动了 **Loader 的版本闸与描述符导出面**，这条是承重的。

- [ ] **Step 5: 基数与 tidy 计数落账**

把 Step 2 与 Step 3 的**实测值**写进 `CLAUDE.md` 的「构建与测试」与「静态检查与格式」两张表，并更新「核这些门禁时……」那节的抑制计数。**只写实测值**，不写推算值；差值若与预期形状不符，先把原因查清再落账（那是发现，不是抄写）。

- [ ] **Step 6: 折并**

按本仓库既有形态把本波的提交折成一笔（M4 的先例是 amend 成一笔）：

```bash
git log --oneline main..HEAD    # 先看清有几笔
git rebase -i main              # 折成一笔：消息按里程碑体例写清三条腿与连带项
git log --oneline -1
```

- [ ] **Step 7: 提交收口**

```bash
git add CLAUDE.md
git commit -m "M5 第一波收口：六线基数与 tidy 计数落账"
```

---

## 偏离登记

执行中发现与本计划或 spec 不符的事实，**逐条记在这里**（计划会说错话，那是正常的；静默改掉不是）。格式：`发现时间 · 现象 · 实测取证 · 裁决`。

> 计划阶段的预登记（执行时若撞上，按这里的裁决走）：
>
> 1. **T10 的端到端用例依赖 T11 才转绿**（Task 10 Step 4）：这是刻意的跨任务判据，**不许**为让它提前绿而放宽断言。
> 2. **Task 9 Step 4 的 `MakeManifestConfigField` 未对 string 型 min/max 设闸**：描述符侧误用 min/max 的形态今天不存在（`VASE_CONFIG` 的元信息由宏生成），若实现中发现可达，回来改判并记此处。
> 3. **本计划未逐一核 `ParseConfig` 的「必填 / 可选」细分**：Task 5 的 round-trip 用例是它的判据——若生成的清单被解析器拒，先看是不是漏了 parser 要求的键，再决定是改写入面还是补用例。
> 4. **T13 的 `NamingViolationIsReportedForSharedProvider` 断言了第①项为 ok**：它依赖 `Tests/Integration/fixtures/manifests/shared_provider/plugin.json` 与描述符逐字一致（D57 的义务）。若这一行红，**先查清单是否漂了**——那是发现，不许改断言绕过。
>
> **计划自查阶段已修的三处缺陷**（记在这里，免得执行者以为它们从没被核过）：① 手写清单漏 `binary` 键会让用例红在「装载失败」而非目标判据（Task 11 两处，已补键并就地注解）；② Task 12 原先拿 `EdgeConsumerPlugin` 手写清单当语料——它的 `provides` 未核过、且第四项合规性未知，改用 `DependentPlugin` + `scan` 现场生成清单；③ Task 9 Step 7 曾留一处 `EXPECT_DOUBLE_EQ` 占位写法，已按 `Storage` 的实持类型改成 `std::get<float>`。

**执行轮登记（T2–T14 + 一笔计划外清账，2026-09-27/28，均 controller 复核后入账）**：

1. **2026-09-27 · T2（a2508fd）· 计划文本的命名空间写错**：Step 6 让写 `detail::Result<void*>`，编译不过——`Result` 住 `vase`，不在 `vase::detail`。**实测取证**：改动前该形式在 `Inspect.h` 直接报错，`vase::` 形式即过。**裁决**：按最小改写 `Result<void*>`，接口契约零动。
2. **2026-09-27 · T5（954aa4c）· 写盘时序缺陷与用例缺口**：Step 写侧 `if (!out)` 检查在 `close` 之前——BUFSIZ 内的内容落盘发生在析构期，disk-full 时错误查不到，截断的 temp 仍会被 rename 上盘（违 D134 的原子语义）。**实测取证**：`out.flush()` 前置后错误才可见；另 brief 的两条 round-trip 用例不覆盖 config/enum 写支。**裁决**：补 `flush()` 前置 + 第三条逐字段证人（用例基数 +1 而非 +2 的账本差异由此来）。
3. **2026-09-28 · T8（d1f92e8）· 抛出型 API 违规矩 4**：`DiscoverPlugins` 用了抛出的 `exists()`（`_HAS_EXCEPTIONS=0` 下前置条件失败即 terminate）。**实测取证**：改 `ec` 重载并把探测移入唯一消费支，ec 置位时点名 Failed；三条钉住文案的用例未动。**裁决**：按不抛形式改写，additive。
4. **2026-09-28 · T9（74b56b1 + 75a2111）· brief 的两处宏上限皆错**：Step 7 说「一个 TU 只能展一次宏（`ManifestJsonTests` 可以展）」——上限实为**每链接镜像一次**（定名 `extern "C"` 入口，同 exe 二次展开实测 lld-link `duplicate symbol`）；且 `DescriptorProbePlugin` 的展开在全局 scope，同 exe 前置声明即可引真探针，手写 40 行镜像没有必要。**实测取证**：duplicate-symbol 原文与真探针链证各一轮。**裁决**：链证改真探针、删手写镜像；`DescriptorTests` 的 per-TU 注释随改为 per-link-image。
5. **2026-09-28 · T10（0bb3f4e + be9c171）· 注释承诺与形状不符**：brief 的 `ManifestMerge` 注释说「读不出由调用方打印」，但返回形状里无路可带这个信息——坏清单会静默覆盖、丢 `enabledByDefault`（违 D134/D122 的保真义务）。**实测取证**：坏清单注入下旧字段消失可复现。**裁决**：补 `ParseError` 第四员 + 工具侧 warning 支 + 证人（rc 法不变）；预登记 #1 的设计红按预期在 T11 转绿。
6. **2026-09-28 · T11（9c7f075）· brief 自相矛盾的退出码**：`ValidateDirectory` 缺目录返 rc1，而它自己的 test4 期望 `kExitUsage`（2）。**实测取证**：两条用例不可能同时满足。**裁决**：按 D135 定 rc2；`is_directory` 的 ec 前置闸置于 `ValidateDirectory` 内以扛 T12 的重写。
7. **2026-09-28 · T12（cb34a5f）· brief 未钉 validate 的零插件树**：按 D135/§3.5 应为 rc2，计划文本没写。**实测取证**：空树旧行为静默 rc0。**裁决**：补 `Ids` 空闸 + 「no plugins found」+ 证人。另 check-3 原因文案由两目三元改穷举 switch——clang 线的前瞻咬得到，MSVC 线未开 C4062，已就地注记。
8. **2026-09-28 · T13（e6eedb4 + eeaa3ec）· 宿主越界判据的位置错了**：按 brief 置于每插件循环内 ⇒ 同一条 squat 逐插件重复打印，且全 `continue` 树被 mask。**实测取证**：修复轮前复现重复行。**裁决**：hoist 出环、每棵树一报；rc 语义与文案逐字不变。
9. **2026-09-28 · T14（9fa319d）· brief 的两枚断言物理不可满足**：用例尾部 `EXPECT_TRUE(DestroyPod(...).Clean())` 在跨 Pod 场景必假——per-Pod 基线快照存进程级计数 + 无符号差（`PluginHost.cpp:144-155` / `:328-329` 机制）。**实测取证**：第二条即红，红在断言本身不在被测行为。**裁决**：按同文件五枚两 Pod 证人的成例删这两枚附随断言，三枚实质断言逐字保留。
10. **2026-09-28 · 计划外清账（7772242）· T1 宏的 tidy 尾巴**：`return kAll` 在三条 tidy 线引 41+1 处 `array-to-pointer-decay`（当时 win 52 / linux 52 条正文 warning）。**实测取证**：宏内 NOLINT 结构性不可行（诊断落在各展开行，不在宏定义行）；改显式 `static_cast`（恒等转换）后抑制桶数字前后逐位相同——该反证确认没有新抑制。**裁决**：走代码级出路（恒等 cast 写明 decay 即 D118 契约的本体）；顺带修 `TimerPlugin` 那条「匿名 ns ⇒ 内部链接」的证伪注释勘误。
11. **2026-09-28 · 终审修复波 · 第④项版式漂移（唯一一处静默 spec drift，补登记）**：plan Task 13 Step 3 规定 `check 4` 节标题先打、其下仅落 hoist 的宿主越界 (b)，而判据 (a)（逐插件 provides 违规）留在 `check 1` 循环里每插件 ok/FAIL 之后——证据错挂到第①项节下，违 spec §3.5「逐项分节」。**实测取证**：终审捕获；此前十一条登记未含它（即本波唯一一处未登记的 spec drift）。**裁决**：(a) 在循环内收集、循环后并入 `check 4` 节，violation 文案与 rc 逐字不变；`NamingViolationIsReportedForSharedProvider` 的 `"check 1 (manifest vs binary):\n  Vase.SharedProvider: ok"` 复合断言不受扰。**同条记名后续账**：wiki 既存裸名服务样例 `Vase.DamageSystem` / `Vase.AttributeSystem`（§6.1 附近 L558/L559，前缀非完整 Id，违本波 D121）与 §11.2 的「CMake 后置步骤自动跑」行内指针（L1524/L1535，plan/doctor 与后置本波未接，D119/D125）= M5 后续波次的记名文书债。
