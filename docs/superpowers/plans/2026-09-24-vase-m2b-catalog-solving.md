# Vase M2b 第一波（Catalog 与求解）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 按 spec（D44–D66）落地 M2b 第一波：`VaseCatalog` 库 target、`plugin.json` 解析、`PluginCatalog::Refresh`（扫描/查重/事务快照）、`LoadPreset`、`Solve`（前置/验证/参与/闭包/碰撞/环/拓扑/配置/路径）与端到端证人——全程 `VasePod` / `VaseHost` 生产代码零改动。

**Architecture:** 三层不变，`VaseCatalog` 站 `VaseHost` 之上（D45：`Solve` 产定形 `LoadPlan`，JSON 只进 Catalog 的 PRIVATE 包含面，公开头零 json 类型，D55）。快照构建后只读；`Plan.Id` 与 `Find()` 同借快照字符串（D61，窗口 = 下次 `Refresh`），`HostProvided` 反向借调用方（D60，窗口 = 本次 `Solve`）。求解是纯函数；装配期 Host 预检原样保留（D56）。

**Tech Stack:** C++20（关异常）、CMake + 六 preset、GoogleTest 1.18.0（vcpkg manifest）、`nlohmann-json`（vcpkg，`JSON_NOEXCEPTION`）、`Result<T>`/`Error`、clang-tidy/format LLVM 23.1.0。

**Spec:** [`docs/superpowers/specs/2026-09-24-vase-m2b-catalog-solving-design.md`](../specs/2026-09-24-vase-m2b-catalog-solving-design.md)——本计划逐条对它的 §2–§10 落地；冲突处以 spec 为准并停下问需求方。

**实施前提（spec 定案时已核实的磁盘事实）**：`EdgeConsumer` 描述符 `Requires {Vase.Test.Shared,1},{Vase.Test.HostOnly,1}`（HostOnly 只有宿主经 Stage0 提供）；全仓服务版本无 0；fixture DLL 平铺 `bin/`、经 `VASE_FIXTURE_*` 宏给绝对路径；temp 沙箱范式见 `Tests/HotSwap/HotSwapLoopTests.cpp:29-52`（声明序契约同处）；Hello 配置字段为 `Repeats`（int32，默认 1）；`IHostOnlyService` 住 `Tests/Integration/fixtures/SharedCommon.h`，集成测试以 `#include "fixtures/SharedCommon.h"` 取用。

## Global Constraints

每个任务的隐含要求，值从 spec / CLAUDE.md 原样抄：

- **无异常**：不用 `throw`/`try`/`catch`；可恢复失败 `Result<T>`/`Error`。`std::optional` 用 `*opt`/`has_value`/`if (opt)`，禁 `.value()`；禁 `std::stoi`（用 `from_chars`）、禁 `.at()`；`<filesystem>` 一律 `error_code` 重载。
- **nlohmann 形态（D55）**：`#include <nlohmann/json.hpp>` 只出现在 `Source/Catalog/*.cpp`；`JSON_NOEXCEPTION` 由 `target_compile_definitions(VaseCatalog PRIVATE JSON_NOEXCEPTION)` 提供；解析一律 `json::parse(text, nullptr, false)` + `is_discarded()`；**取类型值之前必须先 `is_*()` 检查**（无异常模式下 `get<T>()` 错型即 abort）；公开头零 json 类型。
- **测试禁用 `EXPECT_THROW` 一族**。测「必须失败」用 `Result` 返回值；`SolveNote`/`CatalogWarning` 断言走 `Kind`/`Cause` 等字段，消息子串匹配只用最小区分集（D62、spec §7）。
- **包含**：Vase 自己的头一律引号 `#include "Vase/..."`（规矩 3）；`Source/Catalog` 内部头以文件相对路径引号包含（如 `#include "Detail/LibraryFileName.h"`）。
- **target**：`VaseCatalog` 是新库 target，**不走** `vase_add_plugin_fixture`（规矩 5 只管插件形态）；必须链 `VaseBuildOptions`（规矩 1）。
- **命名**（tidy 强制）：类型/函数/成员 `CamelCase`；参数/局部 `camelBack`；常量与枚举值 `k`+`CamelCase`；内部命名空间 `catalog_detail`（lower_case）；成员无前后缀。
- **格式**：Allman、`PointerAlignment: Left`、构造初始化表每式一行逗号在行首、`BreakTemplateDeclarations: Yes`；计划内代码若与本配置有出入，以 `clang-format -i` 之后为准（dry-run 门必须绿）。
- **注释**：中文、单条 ≤2 行为宜硬上限 3 行；写「为什么」与指针（D 号/spec 节号）。
- **提交信息**：标题 + 中文正文，**不加任何 AI 署名尾注**。
- **每任务收尾三件套**：新/改文件先 `git add`，再跑 `git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror`；构建 `cmake --build --preset win-x64-clang-debug`；测试 `ctest --preset win-x64-clang-debug`（全量，不只新用例）。**六线删树重配、三条 tidy 线、Linux 证据、基数重测统一在 Task 8 收口**——中间任务不跑。
- 本波**不触发**规矩 6（Loader/账本/Eject/Adopt/描述符/HeaderVersion 零改动）；任何任务若顺手改了这些实体 = 越界，停下问需求方。
- 依赖 `nlohmann-json` 首次 configure 需要网络（vcpkg 按既有 baseline 解析端口，baseline 不许动，D55）。

## File Structure

**新增**（职责一句话）：

| 文件 | 职责 |
|---|---|
| `Include/Vase/Catalog/ManifestView.h` | 快照元素与诊断形状（`ManifestEntry` / `ManifestConfigField` / `CatalogWarning` / `SolveNoteKind` / `SolveNote` / `SolveOutcome`） |
| `Include/Vase/Catalog/Preset.h` | `PluginOverride`（Preset 条目 = 临时覆盖共用形）+ `Preset` 值形态 + `LoadPreset` 声明 |
| `Include/Vase/Catalog/LoadRequest.h` | `LoadRequest`（Preset + Overrides + HostProvided，D46/D60） |
| `Include/Vase/Catalog/PluginCatalog.h` | `PluginCatalog` 类 + `ParseManifestFile` 声明 |
| `Source/Catalog/Detail/LibraryFileName.h` | stem→平台库文件名的纯字符串拼接（D54） |
| `Source/Catalog/ManifestJson.cpp` | plugin.json → `ManifestEntry` 与 `Value` 按需物化访问器（spec §4 + D49/D64 全执法线） |
| `Source/Catalog/PluginCatalog.cpp` | `Refresh` 扫描/查重/事务快照 + `Ids`/`Find`/`Warnings`（spec §3） |
| `Source/Catalog/PresetJson.cpp` | `LoadPreset` 结构/语法解析（D59） |
| `Source/Catalog/Solve.cpp` | `Solve` 全链（spec §6 八格 + D51/D52/D53/D66） |
| `Source/Catalog/CMakeLists.txt` | `VaseCatalog` SHARED target（D45/D55 链接边与 PRIVATE nlohmann） |
| `Tests/TestingSupport/CatalogSandbox.h` | temp 沙箱 + manifest 落盘 + `RefreshOrFail`（T2 起四个测试簇共用） |
| `Tests/Unit/CatalogWiringSmoke.cpp` | T1 链接冒烟 |
| `Tests/Unit/ManifestJsonTests.cpp` | T2 §4 逐执法线 |
| `Tests/Unit/PresetJsonTests.cpp` | T4 §5 结构线 |
| `Tests/Integration/CatalogScanTests.cpp` | T3 扫描/查重/事务/借用窗 |
| `Tests/Integration/SolveTests.cpp` | T5+T6 §6 逐格（两波增补同一文件） |
| `Tests/Integration/AssemblyFromSolveTests.cpp` | T7 端到端证人（D57/D65） |
| `Tests/Integration/fixtures/manifests/{hello,shared_provider,edge_consumer,shared_consumer2}/plugin.json` | T7 沙箱 staging 的字面材料 |

**修改**：`vcpkg.json`（T1 加 `nlohmann-json`）、`Include/Vase/Detail/Export.h`（T1 加 `VASE_CATALOG_API`）、`CMakeLists.txt`（T1 `add_subdirectory(Source/Catalog)`）、`Tests/CMakeLists.txt`（各任务加源文件/链接/宏）、`Tests/TestingSupport/PodTestPeer.{h,cpp}`（T7 `InstanceOrder`）、`CLAUDE.md` 与 `wiki/vase-architecture.md`（T8 文书波）。

**不动**：`Source/Pod/**`、`Source/Host/**`、`Include/Vase/{Pod,Host,Config,Service,Event,Effect,Detail/*}`（除 `Export.h` 追加块）、三工具链文件、两承重 flag、`Cmake/VaseThirdParty.cmake`、`ThirdParty/cli`、既有 fixture 源码与 CMake 落位、`Tools/VaseConsole`（D19 归波 2）。（零改动口径 = 行为面：`Include/Vase/Host/LoadPlan.h` 的注释曾被 456078d 的术语同步触到，零行为变更。）

---

---

## Task 1: `VaseCatalog` target 组装（链接边 + 导出宏 + 冒烟）

**Spec:** §2（D45、D55 的构建立面）。本任务只立 target 与空 Catalog 类（读侧三访问器），`Refresh`/`Solve` 声明在 T3/T5 随实现加入——**不留桩**。

**Files:**
- Modify: `vcpkg.json:5-7`（dependencies 数组）
- Modify: `Include/Vase/Detail/Export.h:27-31`（HOST 块之后追加）
- Modify: `CMakeLists.txt:273-274`（`add_subdirectory` 序列）
- Create: `Include/Vase/Catalog/ManifestView.h`（先只放 `CatalogWarning` + `ManifestEntry` 骨架？否——**整头在 T2 落**；本任务只建 `PluginCatalog.h` 的读侧最小形，T2/T3 增补它）
- Create: `Include/Vase/Catalog/PluginCatalog.h`
- Create: `Source/Catalog/PluginCatalog.cpp`、`Source/Catalog/CMakeLists.txt`
- Create: `Tests/Unit/CatalogWiringSmoke.cpp`
- Modify: `Tests/CMakeLists.txt`（源列表 + 链接库）

**Interfaces:**
- Consumes: `VaseHost`（经其 PUBLIC 的 `Include/` 拿到 `Vase/Detail/Result.h`、`Vase/Host/LoadPlan.h`）。
- Produces（T2/T3 按这些名字用）：target `VaseCatalog`；宏 `VASE_CATALOG_API`；`vase::PluginCatalog`（含 `Ids()`/`Find()`/`Warnings()` 与私有快照状态：`SnapshotDirectory`、`SnapshotEntries`（`std::vector<ManifestEntry>`，Id 序）、`SnapshotWarnings`、`HasScanned`）。

- [ ] **Step 1: `vcpkg.json` 加依赖**

```json
  "dependencies": [
    "gtest",
    "nlohmann-json"
  ],
```

- [ ] **Step 2: `Include/Vase/Detail/Export.h` 追加（HOST 块后）**

```cpp
#ifdef VASE_CATALOG_BUILD
#define VASE_CATALOG_API VASE_EXPORT
#else
#define VASE_CATALOG_API VASE_IMPORT
#endif
```

- [ ] **Step 3: 写 `Include/Vase/Catalog/PluginCatalog.h`（本任务的形状；T2 追加 `ParseManifestFile`，T3 追加 `Refresh`）**

```cpp
#pragma once

// Catalog 层（spec §2/§3）：唯一 JSON 消费者、唯一插件目录扫描者；站 VaseHost 之上
// 产定形 LoadPlan（D45）。宿主/工具自持的普通对象：不在 PluginHost 的线程绑定契约
// （§1.4）范围内，无内部锁，契约随宿主用法（spec §3「线程」段原文）。

#include "Vase/Catalog/ManifestView.h"
#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace vase
{

class VASE_CATALOG_API PluginCatalog
{
public:
    PluginCatalog();            // 定义在 cpp：Windows 类级 dllexport 要求全部声明的成员有出定义
    ~PluginCatalog();

    [[nodiscard]] std::vector<std::string> Ids() const;                     // 值拷贝，字典序
    [[nodiscard]] const ManifestEntry* Find(std::string_view id) const;     // 借用快照，下次 Refresh 失效（D61）
    [[nodiscard]] const std::vector<CatalogWarning>& Warnings() const { return SnapshotWarnings; }
    [[nodiscard]] const std::filesystem::path& Directory() const { return SnapshotDirectory; }

private:
    std::filesystem::path       SnapshotDirectory; // 首次 Refresh 后为绝对路径
    std::vector<ManifestEntry>  SnapshotEntries;   // Id 字典序（Find 二分与 D53 同源）
    std::vector<CatalogWarning> SnapshotWarnings;
    bool                        HasScanned = false; // D63
};

} // namespace vase
```

- [ ] **Step 4: 写 `Source/Catalog/PluginCatalog.cpp`（本任务只实现读侧；快照恒空的默认态）**

```cpp
#include "Vase/Catalog/PluginCatalog.h"

#include <algorithm>

namespace vase
{

PluginCatalog::PluginCatalog() = default;
PluginCatalog::~PluginCatalog() = default;

std::vector<std::string> PluginCatalog::Ids() const
{
    std::vector<std::string> out;
    out.reserve(SnapshotEntries.size());
    for (const ManifestEntry& entry : SnapshotEntries)
    {
        out.push_back(entry.Id);
    }
    return out;
}

const ManifestEntry* PluginCatalog::Find(std::string_view id) const
{
    auto it = std::ranges::lower_bound(SnapshotEntries, id, {}, &ManifestEntry::Id);
    if (it != SnapshotEntries.end() && it->Id == id)
    {
        return &*it;
    }
    return nullptr;
}

} // namespace vase
```

- [ ] **Step 5: `ManifestView.h` 的临时最小形**（T2 会补 config/notes 全量；本任务只需 `ManifestEntry` 带 `Id` 成员让上面的二分编译通过——直接按 T2 Step 1 的**最终版**写整头也可以，两者取其一；推荐直接写最终版，T2 的 Step 1 变成本任务的 Step 5a 引用。）

- [ ] **Step 6: 写 `Source/Catalog/CMakeLists.txt`**

```cmake
find_package(nlohmann_json CONFIG REQUIRED)

add_library(VaseCatalog SHARED
    PluginCatalog.cpp)

target_include_directories(VaseCatalog
    PUBLIC "${PROJECT_SOURCE_DIR}/Include")

target_compile_definitions(VaseCatalog
    PRIVATE VASE_CATALOG_BUILD
            JSON_NOEXCEPTION)

set_target_properties(VaseCatalog PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

# D45：Catalog 站 Host 之上产定形 LoadPlan。nlohmann 只在本 target 的 PRIVATE
# 包含面出现（D55）；它的 imported target 走 system include，tidy 按非用户代码抑制。
target_link_libraries(VaseCatalog
    PUBLIC VaseHost
    PRIVATE VaseBuildOptions
            nlohmann_json::nlohmann_json)
```

`add_library` 的源列表随 T2/T3/T4/T5 各加一个 `.cpp`（每次任务改这一行）。

- [ ] **Step 7: 根 `CMakeLists.txt`：Host 之后插一行（保持「被消费者之前先存在」的既有排序纪律）**

```cmake
add_subdirectory(Source/Pod)
add_subdirectory(Source/Host)
add_subdirectory(Source/Catalog)
```

- [ ] **Step 8: 写 `Tests/Unit/CatalogWiringSmoke.cpp` 并接入 `Tests/CMakeLists.txt`**

```cpp
// VaseCatalog 的链接冒烟（T1）：能构造、默认态读侧三访问器可用。
// 真语义的测试从 T2 起按簇各立文件；这里只钉「target 立起来了、DLL 落位对了」。

#include "Vase/Catalog/PluginCatalog.h"

#include <gtest/gtest.h>

namespace
{

using vase::PluginCatalog;

TEST(CatalogWiring, DefaultCatalogHasEmptyReadSide)
{
    PluginCatalog catalog;
    EXPECT_TRUE(catalog.Ids().empty());
    EXPECT_TRUE(catalog.Warnings().empty());
    EXPECT_EQ(catalog.Find("Vase.Nope"), nullptr);
}

} // namespace
```

`Tests/CMakeLists.txt`：`add_executable(VaseTests` 源列表 `Unit/DetailTests.cpp` 之前插 `Unit/CatalogWiringSmoke.cpp`；`target_link_libraries(VaseTests PRIVATE VaseBuildOptions VaseHost VaseCatalog GTest::gtest_main)`（`VaseCatalog` 插在 `VaseHost` 之后）。

- [ ] **Step 9: configure + build + 跑冒烟（vcpkg 首次拉 nlohmann 需网络）**

```bash
cmake --preset win-x64-clang-debug
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R CatalogWiring
ctest --preset win-x64-clang-debug          # 全量不回退
```

Expected：configure 通过（vcpkg 按 baseline 装 `nlohmann-json`，**不改 baseline 字段**）；`bin/` 下出现 `VaseCatalog.dll`；冒烟 1 绿、存量 141 绿。

- [ ] **Step 10: format 门 + 提交**

```bash
git add vcpkg.json Include/Vase/Detail/Export.h CMakeLists.txt Include/Vase/Catalog Source/Catalog Tests/Unit/CatalogWiringSmoke.cpp Tests/CMakeLists.txt
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
cmake --build --preset win-x64-clang-debug
git commit -m "M2b 波1-T1：立 VaseCatalog target（链 VaseHost，nlohmann PRIVATE）

D45/D55 的构建立面：vcpkg 加 nlohmann-json（baseline 不动）、Export.h 加
VASE_CATALOG_API、Source/Catalog 起步一个空读侧的 PluginCatalog + 链接冒烟。"
```


---

## Task 2: `plugin.json` 解析（`ManifestJson.cpp` + `ParseManifestFile`）

**Spec:** §4（D48、D49、D64 全部解析期硬闸）。解析函数是公开自由函数：扫描器（T3）与未来的 `VaseCli validate`（M5）共用同一实现（§11.1「同一套规则」）。

**Files:**
- Create: `Include/Vase/Catalog/ManifestView.h`
- Modify: `Include/Vase/Catalog/PluginCatalog.h`（追加 `ParseManifestFile` 声明）
- Create: `Source/Catalog/ManifestJson.cpp`
- Create: `Tests/TestingSupport/CatalogSandbox.h`
- Create: `Tests/Unit/ManifestJsonTests.cpp`
- Modify: `Source/Catalog/CMakeLists.txt`、`Tests/CMakeLists.txt`（各加一行源）

**Interfaces:**
- Consumes: `vase::Value`/`ValueKind`（`Vase/Config/Value.h`）、`Result`/`Error`、`LoadPlan.h`（`SolveOutcome` 引用 `LoadPlan`）。
- Produces（T3–T7 按这些名字用）：`vase::CatalogWarning`、`vase::SolveNoteKind`、`vase::SolveNote`、`vase::SolveOutcome`、`vase::ManifestDependency`、`vase::ManifestConfigField`（`DefaultValue()/MinValue()/MaxValue()`）、`vase::ManifestEntry`、`Result<ManifestEntry> ParseManifestFile(const std::filesystem::path&, std::string_view subdirectoryName)`、`testing_support::CatalogSandbox`（`Root` / `CreateDir` / `WriteFile` / `CopyFile`）。

- [ ] **Step 1: 写 `Include/Vase/Catalog/ManifestView.h`**

```cpp
#pragma once

// 快照与诊断的只读形状（spec §3/§6）。字符串全自持；字符串型配置值不存借用的
// const char*，由访问器按需物化——与 D61 拆掉的 SSO 雷同源：短串搬进容器再搬家即悬垂。

#include "Vase/Config/Value.h"
#include "Vase/Host/LoadPlan.h"

#include <cstdint>
#include <string>
#include <vector>

namespace vase
{

struct CatalogWarning
{
    std::string Subdirectory;
    std::string Message;
};

enum class SolveNoteKind : std::uint8_t
{
    kUnknownPluginId,
    kUnknownConfigKey,
    kProviderSkipped,
    kVersionMismatchProvider,
};

struct SolveNote
{
    SolveNoteKind Kind = SolveNoteKind::kUnknownPluginId;
    std::string   PluginId;
    std::string   Key;     // config 相关 Kind 用
    std::string   Cause;   // 非平凡跳过的责任主体（D62）
    std::string   Message;
};

struct SolveOutcome
{
    LoadPlan               Plan;   // 条目 Id 借用 Catalog 快照（D61）；本结构无被借字段，move 安全
    std::vector<SolveNote> Notes;
};

struct ManifestDependency
{
    std::string   Service;
    std::uint32_t Version = 0; // 解析期保证 ≥1（D64）
};

struct ManifestConfigField
{
    std::string Key;
    ValueKind   Kind = ValueKind::kNone; // 六型（spec §4）；enum 波 2 开闸

    std::uint64_t DefaultBits = 0;
    std::string   DefaultStr;            // kString 专属

    std::uint64_t MinBits = 0;
    std::uint64_t MaxBits = 0;
    bool          HasMin = false;        // min/max 仅数值型可出现（解析期保证），故无 Str
    bool          HasMax = false;

    std::string DisplayName;

    [[nodiscard]] Value DefaultValue() const; // kString 借本条目 DefaultStr（快照内稳定）
    [[nodiscard]] Value MinValue() const;     // !HasMin → kNone
    [[nodiscard]] Value MaxValue() const;
};

struct ManifestEntry
{
    std::string Id;
    std::string DisplayName;   // 缺省 = Id
    std::string Version;       // 纯展示（§3.3），缺省 ""
    std::string Subdirectory;  // 与 DLL 同目录即配对（456078d）
    std::string Binary;        // stem；缺省 = 子目录名（D54）
    bool        EnabledByDefault = true;

    std::vector<ManifestDependency>  Requires;
    std::vector<ManifestDependency>  OptionalRequires;
    std::vector<ManifestDependency>  Provides;
    std::vector<ManifestConfigField> Config;
};

} // namespace vase
```

- [ ] **Step 2: `PluginCatalog.h` 在类之后、`} // namespace vase` 之前追加**

```cpp
// 单文件清单解析（spec §4 全执法线；unknown 字段拒 + D64 硬闸）。subdirectoryName
// 只参与 binary 缺省与 Subdirectory 字段，不校验与 Id 的关系。
VASE_CATALOG_API Result<ManifestEntry> ParseManifestFile(const std::filesystem::path& file,
                                                         std::string_view subdirectoryName);
```

（T1 Step 5 若按推荐直接写了本 Step 的最终版 `ManifestView.h`，此处跳过 Step 1。）

- [ ] **Step 3: 写 `Source/Catalog/ManifestJson.cpp`（上半：匿名命名空间 helpers）**

```cpp
#include "Vase/Catalog/PluginCatalog.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace
{

using nlohmann::json;
using vase::Error;
using vase::ManifestConfigField;
using vase::ManifestDependency;
using vase::ManifestEntry;
using vase::Result;
using vase::Value;
using vase::ValueKind;

Error ManifestError(const std::filesystem::path& file, std::string_view detail)
{
    return Error("plugin manifest \"" + file.string() + "\": " + std::string(detail));
}

Result<json> ReadJsonFile(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    if (!in)
    {
        return Result<json>::Err(ManifestError(file, "cannot open file"));
    }
    const std::string text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    json parsed = json::parse(text, nullptr, false); // D55：无异常形态，坏 JSON 返回 discarded 而非抛
    if (parsed.is_discarded())
    {
        return Result<json>::Err(ManifestError(file, "malformed JSON"));
    }
    return Result<json>::Ok(std::move(parsed));
}

bool AsInt64(const json& v, std::int64_t& out)
{
    if (!v.is_number_integer())
    {
        return false;
    }
    if (v.is_number_unsigned())
    {
        const std::uint64_t u = v.get<std::uint64_t>();
        if (u > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        {
            return false;
        }
        out = static_cast<std::int64_t>(u);
        return true;
    }
    out = v.get<std::int64_t>();
    return true;
}

bool TypeName(std::string_view name, ValueKind& out)
{
    if (name == "bool") { out = ValueKind::kBool; }
    else if (name == "int32") { out = ValueKind::kInt32; }
    else if (name == "int64") { out = ValueKind::kInt64; }
    else if (name == "float") { out = ValueKind::kFloat; }
    else if (name == "double") { out = ValueKind::kDouble; }
    else if (name == "string") { out = ValueKind::kString; }
    else { return false; }
    return true;
}

// 按声明类型装值（spec §4：同型；数值加宽仅 int→float/double）。false = 不匹配。
bool Encode(const json& v, ValueKind kind, std::uint64_t& bits, std::string& str)
{
    std::int64_t i = 0;
    switch (kind)
    {
    case ValueKind::kBool:
        if (!v.is_boolean()) { return false; }
        bits = v.get<bool>() ? 1U : 0U;
        return true;
    case ValueKind::kInt32:
        if (!AsInt64(v, i) || i < std::numeric_limits<std::int32_t>::min() ||
            i > std::numeric_limits<std::int32_t>::max())
        {
            return false;
        }
        bits = static_cast<std::uint64_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(i)));
        return true;
    case ValueKind::kInt64:
        if (!AsInt64(v, i)) { return false; }
        bits = static_cast<std::uint64_t>(i);
        return true;
    case ValueKind::kFloat:
    {
        double d = 0.0;
        if (v.is_number_integer())
        {
            if (!AsInt64(v, i)) { return false; }
            d = static_cast<double>(i);
        }
        else if (v.is_number_float())
        {
            d = v.get<double>();
        }
        else
        {
            return false;
        }
        bits = std::bit_cast<std::uint32_t>(static_cast<float>(d));
        return true;
    }
    case ValueKind::kDouble:
        if (v.is_number_integer())
        {
            if (!AsInt64(v, i)) { return false; }
            bits = std::bit_cast<std::uint64_t>(static_cast<double>(i));
            return true;
        }
        if (!v.is_number_float()) { return false; }
        bits = std::bit_cast<std::uint64_t>(v.get<double>());
        return true;
    case ValueKind::kString:
        if (!v.is_string()) { return false; }
        str = v.get<std::string>();
        return true;
    default:
        return false;
    }
}

// min≤max 用；两侧同 Kind（Encode 保证），数值四型才有意义（bool/string 在闸外）。
bool ValueLess(ValueKind kind, std::uint64_t a, std::uint64_t b)
{
    switch (kind)
    {
    case ValueKind::kInt32:
        return static_cast<std::int32_t>(static_cast<std::uint32_t>(a)) <
               static_cast<std::int32_t>(static_cast<std::uint32_t>(b));
    case ValueKind::kInt64:
        return static_cast<std::int64_t>(a) < static_cast<std::int64_t>(b);
    case ValueKind::kFloat:
        return std::bit_cast<float>(static_cast<std::uint32_t>(a)) <
               std::bit_cast<float>(static_cast<std::uint32_t>(b));
    default:
        return std::bit_cast<double>(a) < std::bit_cast<double>(b);
    }
}

bool OnlyKeys(const json& object, const std::initializer_list<std::string_view>& allowed)
{
    for (auto it = object.begin(); it != object.end(); ++it)
    {
        if (std::ranges::find(allowed, std::string_view(it.key())) == allowed.end())
        {
            return false;
        }
    }
    return true;
}

Result<std::vector<ManifestDependency>> ParseDeps(const json& array, std::string_view fieldName,
                                                  const std::filesystem::path& file)
{
    std::vector<ManifestDependency> out;
    if (!array.is_array())
    {
        return Result<std::vector<ManifestDependency>>::Err(
            ManifestError(file, "field \"" + std::string(fieldName) + "\" must be an array"));
    }
    for (const json& item : array)
    {
        if (!item.is_object() || !OnlyKeys(item, {"service", "version"}))
        {
            return Result<std::vector<ManifestDependency>>::Err(
                ManifestError(file, std::string(fieldName) + " entries must be {service, version} objects"));
        }
        const auto service = item.find("service");
        const auto version = item.find("version");
        if (service == item.end() || !service->is_string() || service->get_ref<const std::string&>().empty())
        {
            return Result<std::vector<ManifestDependency>>::Err(
                ManifestError(file, std::string(fieldName) + ": \"service\" must be a non-empty string"));
        }
        std::int64_t v = 0;
        if (version == item.end() || !AsInt64(*version, v) || v < 1 ||
            v > std::numeric_limits<std::uint32_t>::max())
        {
            return Result<std::vector<ManifestDependency>>::Err(
                ManifestError(file, std::string(fieldName) + ": \"version\" must be an integer >= 1 (D64)"));
        }
        const std::string& serviceName = service->get_ref<const std::string&>();
        for (const ManifestDependency& held : out)
        {
            if (held.Service == serviceName && held.Version == static_cast<std::uint32_t>(v))
            {
                return Result<std::vector<ManifestDependency>>::Err(
                    ManifestError(file, std::string(fieldName) + ": duplicate (service,version) entry (D64)"));
            }
        }
        out.push_back(ManifestDependency{.Service = serviceName, .Version = static_cast<std::uint32_t>(v)});
    }
    return Result<std::vector<ManifestDependency>>::Ok(std::move(out));
}

- [ ] **Step 4: `Source/Catalog/ManifestJson.cpp`（下半：`ParseConfig` 与 `vase::` 实现段）**

```cpp
Result<std::vector<ManifestConfigField>> ParseConfig(const json& array, const std::filesystem::path& file)
{
    std::vector<ManifestConfigField> out;
    if (!array.is_array())
    {
        return Result<std::vector<ManifestConfigField>>::Err(
            ManifestError(file, "field \"config\" must be an array"));
    }
    for (const json& item : array)
    {
        if (!item.is_object() || !OnlyKeys(item, {"key", "type", "default", "min", "max", "displayName"}))
        {
            return Result<std::vector<ManifestConfigField>>::Err(
                ManifestError(file, "config entries accept only {key,type,default,min,max,displayName}"));
        }
        const auto key = item.find("key");
        const auto type = item.find("type");
        const auto def = item.find("default");
        if (key == item.end() || !key->is_string() || key->get_ref<const std::string&>().empty())
        {
            return Result<std::vector<ManifestConfigField>>::Err(
                ManifestError(file, "config: \"key\" must be a non-empty string"));
        }
        ManifestConfigField field;
        field.Key = key->get_ref<const std::string&>();
        if (type == item.end() || !type->is_string() || !TypeName(type->get_ref<const std::string&>(), field.Kind))
        {
            return Result<std::vector<ManifestConfigField>>::Err(
                ManifestError(file, "config \"" + field.Key + "\": unknown type (six kinds; enum opens in wave 2)"));
        }
        if (def == item.end())
        {
            return Result<std::vector<ManifestConfigField>>::Err(
                ManifestError(file, "config \"" + field.Key + "\": \"default\" is required"));
        }
        if (!Encode(*def, field.Kind, field.DefaultBits, field.DefaultStr))
        {
            return Result<std::vector<ManifestConfigField>>::Err(
                ManifestError(file, "config \"" + field.Key + "\": default does not match declared type"));
        }
        std::uint64_t boundBits = 0;
        std::string ignored;
        if (const auto min = item.find("min"); min != item.end())
        {
            if (field.Kind != ValueKind::kInt32 && field.Kind != ValueKind::kInt64 &&
                field.Kind != ValueKind::kFloat && field.Kind != ValueKind::kDouble)
            {
                return Result<std::vector<ManifestConfigField>>::Err(
                    ManifestError(file, "config \"" + field.Key + "\": min/max only for numeric types"));
            }
            if (!Encode(*min, field.Kind, boundBits, ignored))
            {
                return Result<std::vector<ManifestConfigField>>::Err(
                    ManifestError(file, "config \"" + field.Key + "\": min does not match declared type"));
            }
            field.MinBits = boundBits;
            field.HasMin = true;
        }
        if (const auto max = item.find("max"); max != item.end())
        {
            if (!Encode(*max, field.Kind, boundBits, ignored))
            {
                return Result<std::vector<ManifestConfigField>>::Err(
                    ManifestError(file, "config \"" + field.Key + "\": max does not match declared type"));
            }
            field.MaxBits = boundBits;
            field.HasMax = true;
        }
        if (field.HasMin && field.HasMax && ValueLess(field.Kind, field.MaxBits, field.MinBits))
        {
            return Result<std::vector<ManifestConfigField>>::Err(
                ManifestError(file, "config \"" + field.Key + "\": min > max (D64)"));
        }
        if (const auto display = item.find("displayName"); display != item.end())
        {
            if (!display->is_string())
            {
                return Result<std::vector<ManifestConfigField>>::Err(
                    ManifestError(file, "config \"" + field.Key + "\": displayName must be a string"));
            }
            field.DisplayName = display->get_ref<const std::string&>();
        }
        for (const ManifestConfigField& held : out)
        {
            if (held.Key == field.Key)
            {
                return Result<std::vector<ManifestConfigField>>::Err(
                    ManifestError(file, "config: duplicate key \"" + field.Key + "\" (D64)"));
            }
        }
        out.push_back(std::move(field));
    }
    return Result<std::vector<ManifestConfigField>>::Ok(std::move(out));
}

} // namespace

- [ ] **Step 5: `Source/Catalog/ManifestJson.cpp`（`vase::` 实现段：三个访问器 + `ParseManifestFile`）**

```cpp
namespace vase
{

Value ManifestConfigField::DefaultValue() const
{
    Value out{};
    out.Kind = Kind;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) D24 位形按 Kind 互斥读写（与 Value::From 同机制）
    if (Kind == ValueKind::kString) { out.Str = DefaultStr.c_str(); } else { out.Bits = DefaultBits; }
    return out;
}

Value ManifestConfigField::MinValue() const
{
    Value out{};
    if (!HasMin)
    {
        return out; // kNone
    }
    out.Kind = Kind;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) 同上。
    out.Bits = MinBits;
    return out;
}

Value ManifestConfigField::MaxValue() const
{
    Value out{};
    if (!HasMax)
    {
        return out;
    }
    out.Kind = Kind;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) 同上。
    out.Bits = MaxBits;
    return out;
}

Result<ManifestEntry> ParseManifestFile(const std::filesystem::path& file, std::string_view subdirectoryName)
{
    Result<json> read = ReadJsonFile(file);
    if (!read.IsOk())
    {
        return Result<ManifestEntry>::Err(read.GetError());
    }
    const json& root = read.Value();
    if (!root.is_object())
    {
        return Result<ManifestEntry>::Err(ManifestError(file, "top level must be a JSON object"));
    }
    if (!OnlyKeys(root, {"schemaVersion", "id", "displayName", "version", "binary", "enabledByDefault",
                         "requires", "optionalRequires", "provides", "config"}))
    {
        return Result<ManifestEntry>::Err(ManifestError(file, "unknown top-level field (D49)"));
    }

    ManifestEntry entry;
    entry.Subdirectory = std::string(subdirectoryName);

    const auto schemaVersion = root.find("schemaVersion");
    if (schemaVersion == root.end() || !schemaVersion->is_number_integer())
    {
        return Result<ManifestEntry>::Err(
            ManifestError(file, "schemaVersion required, integer form only (1.0 rejected, D64)"));
    }
    if (schemaVersion->get<std::int64_t>() != 1)
    {
        return Result<ManifestEntry>::Err(ManifestError(file, "unsupported schemaVersion major (expected 1)"));
    }
    const auto id = root.find("id");
    if (id == root.end() || !id->is_string() || id->get_ref<const std::string&>().empty())
    {
        return Result<ManifestEntry>::Err(ManifestError(file, "\"id\" required and non-empty"));
    }
    entry.Id = id->get_ref<const std::string&>();

    if (const auto dn = root.find("displayName"); dn != root.end())
    {
        if (!dn->is_string())
        {
            return Result<ManifestEntry>::Err(ManifestError(file, "displayName must be a string"));
        }
        entry.DisplayName = dn->get_ref<const std::string&>();
    }
    else
    {
        entry.DisplayName = entry.Id; // spec §4 缺省
    }
    if (const auto ver = root.find("version"); ver != root.end())
    {
        if (!ver->is_string())
        {
            return Result<ManifestEntry>::Err(ManifestError(file, "version must be a string"));
        }
        entry.Version = ver->get_ref<const std::string&>();
    }
    if (const auto bin = root.find("binary"); bin != bin.end())
    {
        if (!bin->is_string())
        {
            return Result<ManifestEntry>::Err(ManifestError(file, "binary must be a string"));
        }
        const std::string& stem = bin->get_ref<const std::string&>();
        if (stem.empty() || stem == "." || stem == ".." || stem.find_first_of("/\\") != std::string::npos)
        {
            return Result<ManifestEntry>::Err(
                ManifestError(file, "binary must be a plain file name (no path characters, D64)"));
        }
        entry.Binary = stem;
    }
    else
    {
        entry.Binary = entry.Subdirectory; // 缺省 = 子目录名（D54）
    }
    if (const auto enabled = root.find("enabledByDefault"); enabled != root.end())
    {
        if (!enabled->is_boolean())
        {
            return Result<ManifestEntry>::Err(ManifestError(file, "enabledByDefault must be a bool"));
        }
        entry.EnabledByDefault = enabled->get<bool>();
    }

    if (const auto array = root.find("requires"); array != root.end())
    {
        auto parsed = ParseDeps(*array, "requires", file);
        if (!parsed.IsOk())
        {
            return Result<ManifestEntry>::Err(parsed.GetError());
        }
        entry.Requires = std::move(parsed.Value());
    }
    if (const auto array = root.find("optionalRequires"); array != root.end())
    {
        auto parsed = ParseDeps(*array, "optionalRequires", file);
        if (!parsed.IsOk())
        {
            return Result<ManifestEntry>::Err(parsed.GetError());
        }
        entry.OptionalRequires = std::move(parsed.Value());
    }
    if (const auto array = root.find("provides"); array != root.end())
    {
        auto parsed = ParseDeps(*array, "provides", file);
        if (!parsed.IsOk())
        {
            return Result<ManifestEntry>::Err(parsed.GetError());
        }
        entry.Provides = std::move(parsed.Value());
    }
    if (const auto array = root.find("config"); array != root.end())
    {
        auto parsed = ParseConfig(*array, file);
        if (!parsed.IsOk())
        {
            return Result<ManifestEntry>::Err(parsed.GetError());
        }
        entry.Config = std::move(parsed.Value());
    }

    return Result<ManifestEntry>::Ok(std::move(entry));
}

} // namespace vase
```

- [ ] **Step 6: 写 `Tests/TestingSupport/CatalogSandbox.h`（T3/T5/T6/T7 共用）**

```cpp
#pragma once

// Catalog 测试沙箱：temp 目录下的一次性插件树，析构 remove_all。
// 机制与 Tests/HotSwap/HotSwapLoopTests.cpp:38-43 同型（声明序契约见 T7）。

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "Vase/Detail/Fail.h"

namespace testing_support
{

class CatalogSandbox
{
public:
    explicit CatalogSandbox(std::string_view name)
    {
        std::error_code ec;
        const std::filesystem::path temp = std::filesystem::temp_directory_path(ec);
        if (ec)
        {
            vase::detail::ProgrammerError("CatalogSandbox: no temp directory");
        }
        const auto ns = static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        Root = temp / ("vase-catalog-" + std::string(name) + "-" + std::to_string(ns));
    }

    CatalogSandbox(const CatalogSandbox&) = delete;
    CatalogSandbox& operator=(const CatalogSandbox&) = delete;
    CatalogSandbox(CatalogSandbox&&) = delete;
    CatalogSandbox& operator=(CatalogSandbox&&) = delete;

    ~CatalogSandbox()
    {
        std::error_code ec;
        std::filesystem::remove_all(Root, ec); // 尽力清理；残留只是 temp 垃圾，不判据
    }

    void CreateDir(std::string_view relPath) const
    {
        std::error_code ec;
        std::filesystem::create_directories(Root / std::filesystem::path(relPath), ec);
    }

    void WriteFile(std::string_view relPath, std::string_view content) const
    {
        const std::filesystem::path full = Root / std::filesystem::path(relPath);
        std::error_code ec;
        std::filesystem::create_directories(full.parent_path(), ec);
        std::ofstream out(full, std::ios::binary | std::ios::trunc);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    void CopyFile(std::string_view relPath, const std::filesystem::path& source) const
    {
        const std::filesystem::path full = Root / std::filesystem::path(relPath);
        std::error_code ec;
        std::filesystem::create_directories(full.parent_path(), ec);
        std::filesystem::copy_file(source, full, std::filesystem::copy_options::overwrite_existing, ec);
    }

    std::filesystem::path Root;
};

} // namespace testing_support
```

- [ ] **Step 7: 写 `Tests/Unit/ManifestJsonTests.cpp`（上半：fixture 骨架 + 正面用例）**

```cpp
// spec §4 逐执法线（D48/D49/D64）。断言 = IsOk + 字段值；错误线只验「响且不绿」，
// 消息不逐字钉（最小区分集纪律，spec §7）。

#include "Vase/Catalog/PluginCatalog.h"

#include "CatalogSandbox.h"

#include <bit>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace
{

using testing_support::CatalogSandbox;
using vase::ManifestEntry;
using vase::ParseManifestFile;
using vase::Result;
using vase::ValueKind;

class ManifestJson : public ::testing::Test
{
protected:
    CatalogSandbox Sandbox{"manifest-json"};

    static std::string Wrapped(std::string_view fields)
    {
        std::string out = R"({"schemaVersion":1,"id":"Vase.Probe")";
        if (!fields.empty())
        {
            out += ",";
            out += fields;
        }
        out += "}";
        return out;
    }

    Result<ManifestEntry> Parse(std::string_view fields, std::string_view sub = "probe")
    {
        Sandbox.WriteFile(sub + "/plugin.json", Wrapped(fields));
        return ParseManifestFile(Sandbox.Root / sub / "plugin.json", sub);
    }

    void ExpectReject(std::string_view fields)
    {
        const auto parsed = Parse(fields);
        EXPECT_FALSE(parsed.IsOk()) << "accepted: " << Wrapped(fields);
    }
};

TEST_F(ManifestJson, MinimalFillsDefaults)
{
    const auto parsed = Parse("");
    ASSERT_TRUE(parsed.IsOk()) << parsed.GetError().Message();
    const ManifestEntry& entry = parsed.Value();
    EXPECT_EQ(entry.Id, "Vase.Probe");
    EXPECT_EQ(entry.DisplayName, "Vase.Probe");
    EXPECT_TRUE(entry.Version.empty());
    EXPECT_EQ(entry.Binary, "probe"); // 缺省 = 子目录名（D54）
    EXPECT_TRUE(entry.EnabledByDefault);
    EXPECT_TRUE(entry.Requires.empty());
    EXPECT_TRUE(entry.OptionalRequires.empty());
    EXPECT_TRUE(entry.Provides.empty());
    EXPECT_TRUE(entry.Config.empty());
}

TEST_F(ManifestJson, AcceptsFullShape)
{
    const auto parsed = Parse(
        R"("displayName":"探针","version":"1.2.0","binary":"ProbeBin","enabledByDefault":false)"
        R"(,"requires":[{"service":"Vase.World","version":1}])"
        R"(,"provides":[{"service":"Vase.P","version":2}])"
        R"(,"config":[{"key":"speed","type":"float","default":2,"min":1,"max":10,"displayName":"速度"}])");
    ASSERT_TRUE(parsed.IsOk()) << parsed.GetError().Message();
    const ManifestEntry& entry = parsed.Value();
    EXPECT_EQ(entry.DisplayName, "探针");
    EXPECT_EQ(entry.Version, "1.2.0");
    EXPECT_EQ(entry.Binary, "ProbeBin");
    EXPECT_FALSE(entry.EnabledByDefault);
    ASSERT_EQ(entry.Requires.size(), 1U);
    EXPECT_EQ(entry.Requires[0].Service, "Vase.World");
    EXPECT_EQ(entry.Requires[0].Version, 1U);
    ASSERT_EQ(entry.Config.size(), 1U);
    EXPECT_EQ(entry.Config[0].Kind, ValueKind::kFloat);
    EXPECT_EQ(entry.Config[0].DefaultBits, static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(2.0F))); // int→float 加宽
    EXPECT_TRUE(entry.Config[0].HasMin);
    EXPECT_TRUE(entry.Config[0].HasMax);
    EXPECT_EQ(entry.Config[0].DisplayName, "速度");
}
```
同一文件继续（匿名 ns 内，`AcceptsFullShape` 之后追加；行尾注释放 C++ `//` 形式）：

```cpp
TEST_F(ManifestJson, RejectsMissingOrEmptyId)
{
    Sandbox.WriteFile("pe/plugin.json", R"({"schemaVersion":1,"id":""})");
    EXPECT_FALSE(ParseManifestFile(Sandbox.Root / "pe" / "plugin.json", "pe").IsOk());
    Sandbox.WriteFile("ni/plugin.json", R"({"schemaVersion":1})");
    EXPECT_FALSE(ParseManifestFile(Sandbox.Root / "ni" / "plugin.json", "ni").IsOk()); // 缺 id
}

TEST_F(ManifestJson, RejectsNonIntegerSchemaVersion)
{
    Sandbox.WriteFile("p1/plugin.json", R"({"schemaVersion":1.0,"id":"Vase.Probe"})");
    EXPECT_FALSE(ParseManifestFile(Sandbox.Root / "p1" / "plugin.json", "p1").IsOk()); // 1.0 拒，D64
}

TEST_F(ManifestJson, RejectsMajorTwo)
{
    Sandbox.WriteFile("p2/plugin.json", R"({"schemaVersion":2,"id":"Vase.Probe"})");
    EXPECT_FALSE(ParseManifestFile(Sandbox.Root / "p2" / "plugin.json", "p2").IsOk());
}

TEST_F(ManifestJson, RejectsUnknownTopLevelField) { ExpectReject(R"("author":"someone")"); } // D49

TEST_F(ManifestJson, RejectsMalformedJson)
{
    Sandbox.WriteFile("p3/plugin.json", "{\"schemaVersion\":1,,}");
    EXPECT_FALSE(ParseManifestFile(Sandbox.Root / "p3" / "plugin.json", "p3").IsOk());
}

TEST_F(ManifestJson, RejectsBinaryPathChars) // D64 三条
{
    ExpectReject(R"("binary":"../evil")");
    ExpectReject(R"("binary":"..")");
    ExpectReject(R"("binary":"")");
    ExpectReject(R"("binary":"sub\dir")");
}

TEST_F(ManifestJson, RejectsDepEntryShapes)
{
    ExpectReject(R"("requires":[{"service":"S"}])");                      // 缺 version
    ExpectReject(R"("requires":[{"service":"S","version":0}])");          // version ≥1（D64）
    ExpectReject(R"("requires":[{"service":"S","version":1,"extra":1}])"); // 条目 unknown 字段
    ExpectReject(R"("requires":[{"service":"S","version":1},{"service":"S","version":1}])"); // 重复（D64）
    ExpectReject(R"("provides":"not-an-array")");
}

TEST_F(ManifestJson, RejectsConfigShapes)
{
    ExpectReject(R"("config":[{"key":"a","type":"int32"}])");               // 缺 default
    ExpectReject(R"("config":[{"key":"a","type":"int32","default":"x"}])"); // 型不匹配
    ExpectReject(R"("config":[{"key":"a","type":"enum","default":1}])");    // 波 2 开闸（D49）
    ExpectReject(R"("config":[{"key":"a","type":"bool","default":true,"min":false}])"); // bool 禁 min/max
    ExpectReject(R"("config":[{"key":"a","type":"int32","default":1,"min":5,"max":2}])"); // 倒挂（D64）
    ExpectReject(R"("config":[{"key":"a","type":"int32","default":1},{"key":"a","type":"int64","default":2}])"); // 重复 key
    ExpectReject(R"("config":[{"key":"a","type":"int32","default":1,"nope":2}])"); // 条目 unknown
    ExpectReject(R"("config":[{"key":"a","type":"int32","default":9999999999}])"); // int32 越界
}

TEST_F(ManifestJson, AccessorsMaterializePerKind)
{
    const auto parsed = Parse(
        R"("config":[{"key":"name","type":"string","default":"hello"},)"
        R"({"key":"n","type":"int64","default":7,"min":1,"max":9}])");
    ASSERT_TRUE(parsed.IsOk()) << parsed.GetError().Message();
    const ManifestEntry& entry = parsed.Value();
    EXPECT_EQ(entry.Config[0].DefaultValue().Kind, ValueKind::kString);
    EXPECT_STREQ(entry.Config[0].DefaultValue().GetAs<const char*>(), "hello");
    EXPECT_EQ(entry.Config[1].DefaultValue().GetAs<std::int64_t>(), 7);
    EXPECT_EQ(entry.Config[1].MinValue().GetAs<std::int64_t>(), 1);
    EXPECT_EQ(entry.Config[1].MaxValue().GetAs<std::int64_t>(), 9);
    EXPECT_EQ(entry.Config[0].MinValue().Kind, ValueKind::kNone); // !HasMin
    EXPECT_EQ(entry.Config[0].MaxValue().Kind, ValueKind::kNone);
}

} // namespace
```

- [ ] **Step 8: 接入构建并跑绿**

`Source/Catalog/CMakeLists.txt` 的 `add_library(VaseCatalog SHARED` 源列表追加 `ManifestJson.cpp`；`Tests/CMakeLists.txt` 源列表在 `Unit/ConfigValueTests.cpp` 之后插 `Unit/ManifestJsonTests.cpp`。

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R ManifestJson      # 11 条全绿
ctest --preset win-x64-clang-debug                       # 全量不回退
git add Include/Vase/Catalog Source/Catalog Tests/TestingSupport/CatalogSandbox.h Tests/Unit/ManifestJsonTests.cpp Tests/CMakeLists.txt Source/Catalog/CMakeLists.txt
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
git commit -m "M2b 波1-T2：plugin.json 解析（spec §4 全执法线 + ManifestView + 测试沙箱）

ParseManifestFile 公开自由函数（扫描器与未来 VaseCli validate 共用，§11.1 同一套
规则）。D48/D49/D64 全部解析期硬闸逐条钉用例；字符串配置值走访问器按需物化，
避开 D61 拆掉的 SSO 自借用雷。"
```

---

## Task 3: `Refresh` 扫描（事务快照 / Id 查重 / 缺清单跳过 / 借用窗）

**Spec:** §3 + D50 + D58 + D61 + D63 的「置位」侧。扫描器只读目录与 `plugin.json`，**不碰二进制**（存在性不查归波 2）。

**Files:**
- Modify: `Include/Vase/Catalog/PluginCatalog.h`（类内加 `Refresh` 声明）
- Modify: `Source/Catalog/PluginCatalog.cpp`（实现）
- Create: `Tests/Integration/CatalogScanTests.cpp`
- Modify: `Tests/CMakeLists.txt`（加一行源）

**Interfaces:**
- Consumes: `ParseManifestFile`（T2）、`CatalogSandbox`（T2）。
- Produces: `Result<void> PluginCatalog::Refresh(const std::filesystem::path&)`；`HasScanned=true` 后 T5 的 `Solve` 前置放行；快照 `SnapshotEntries` 按 Id 字典序（T5/T6 的迭代序与 `Ids()` 同源）。

- [ ] **Step 1: `PluginCatalog.h` 类内 `public:` 段追加**

```cpp
    // 事务性（D58+D64a）：成功才整体换快照与 Warnings；失败两者都不换。
    // 目录不存在 → Err；子目录缺 plugin.json → 跳过 + Warnings（D50）；
    // 坏清单 / 重复 Id → Err（D50）。入参绝对化后存为 Directory()。
    Result<void> Refresh(const std::filesystem::path& pluginDirectory);
```

- [ ] **Step 2: `Source/Catalog/PluginCatalog.cpp` 追加实现（文件顶部补 `#include <system_error>`、`#include <utility>`）**

```cpp
Result<void> PluginCatalog::Refresh(const std::filesystem::path& pluginDirectory)
{
    std::error_code ec;
    std::filesystem::path abs = std::filesystem::absolute(pluginDirectory, ec);
    if (ec)
    {
        return Result<void>::Err(
            Error("catalog refresh: cannot absolutize \"" + pluginDirectory.string() + "\": " + ec.message()));
    }
    if (!std::filesystem::is_directory(abs, ec))
    {
        return Result<void>::Err(Error("catalog refresh: not a directory: \"" + abs.string() + "\""));
    }

    std::vector<std::filesystem::path> subdirectories;
    for (std::filesystem::directory_iterator it(abs, std::filesystem::directory_options::skip_permission_denied, ec),
             end;
         !ec && it != end; it.increment(ec))
    {
        if (it->is_directory(ec))
        {
            subdirectories.push_back(it->path());
        }
    }
    if (ec)
    {
        return Result<void>::Err(
            Error("catalog refresh: cannot enumerate \"" + abs.string() + "\": " + ec.message()));
    }
    std::ranges::sort(subdirectories, [](const std::filesystem::path& a, const std::filesystem::path& b)
                      { return a.filename().string() < b.filename().string(); });

    std::vector<ManifestEntry> nextEntries;
    std::vector<CatalogWarning> nextWarnings;
    for (const std::filesystem::path& dir : subdirectories)
    {
        const std::string name = dir.filename().string();
        const std::filesystem::path manifest = dir / "plugin.json";
        if (!std::filesystem::is_regular_file(manifest, ec))
        {
            nextWarnings.push_back(
                CatalogWarning{.Subdirectory = name, .Message = "no plugin.json — skipped"});
            continue;
        }
        Result<ManifestEntry> parsed = ParseManifestFile(manifest, name);
        if (!parsed.IsOk())
        {
            return Result<void>::Err(parsed.GetError()); // D50：坏清单整体响；失败不换快照/Warnings（D58/D64a）
        }
        ManifestEntry entry = std::move(parsed.Value());
        for (const ManifestEntry& held : nextEntries)
        {
            if (held.Id == entry.Id)
            {
                return Result<void>::Err(Error("catalog refresh: duplicate plugin id \"" + entry.Id + "\" in \"" +
                                               held.Subdirectory + "\" and \"" + entry.Subdirectory + "\""));
            }
        }
        nextEntries.push_back(std::move(entry));
    }

    std::ranges::sort(nextEntries, [](const ManifestEntry& a, const ManifestEntry& b) { return a.Id < b.Id; });

    SnapshotDirectory = std::move(abs);
    SnapshotEntries = std::move(nextEntries);
    SnapshotWarnings = std::move(nextWarnings);
    HasScanned = true;
    return Result<void>::Ok();
}
```

- [ ] **Step 3: 写 `Tests/Integration/CatalogScanTests.cpp`**

```cpp
// spec §3 的扫描面：枚举/跳过/查重/事务/借用窗/目录闸（D50/D58/D61/D64a）。
// 全部走真文件系统（temp 沙箱），不碰二进制。

#include "Vase/Catalog/PluginCatalog.h"

#include "CatalogSandbox.h"

#include <cstdio>
#include <gtest/gtest.h>
#include <string>

namespace
{

using testing_support::CatalogSandbox;
using vase::ManifestEntry;
using vase::PluginCatalog;

void PutManifest(CatalogSandbox& sandbox, const std::string& dir, const std::string& id)
{
    char buf[512];
    const int written = std::snprintf(buf, sizeof(buf), R"({"schemaVersion":1,"id":"%s"})", id.c_str());
    ASSERT_GT(written, 0);
    ASSERT_LT(static_cast<size_t>(written), sizeof(buf));
    sandbox.WriteFile(dir + "/plugin.json", buf);
}

TEST(CatalogScan, EmptyDirIsLegalEmptySnapshot)
{
    CatalogSandbox sandbox{"scan-empty"};
    sandbox.CreateDir("stray-dir"); // 唯一的子目录，且没有清单 → warn + 跳过（D50）
    PluginCatalog catalog;
    const auto refreshed = catalog.Refresh(sandbox.Root);
    ASSERT_TRUE(refreshed.IsOk()) << refreshed.GetError().Message();
    EXPECT_TRUE(catalog.Ids().empty());
    ASSERT_EQ(catalog.Warnings().size(), 1U);
    EXPECT_EQ(catalog.Warnings()[0].Subdirectory, "stray-dir");
}

TEST(CatalogScan, NonexistentDirRejects)
{
    CatalogSandbox sandbox{"scan-nodir"};
    PluginCatalog catalog;
    EXPECT_FALSE(catalog.Refresh(sandbox.Root / "missing").IsOk());
}

TEST(CatalogScan, StrayRootFileIgnored)
{
    CatalogSandbox sandbox{"scan-stray"};
    sandbox.WriteFile("README.md", "not a plugin"); // 根散文件：忽略，连 warn 都不记
    PluginCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    EXPECT_TRUE(catalog.Ids().empty());
    EXPECT_TRUE(catalog.Warnings().empty());
}

TEST(CatalogScan, IdsSortedAndFindWorks)
{
    CatalogSandbox sandbox{"scan-order"};
    PutManifest(sandbox, "zeta", "Vase.Z");
    PutManifest(sandbox, "alpha", "Vase.A");
    PluginCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    const auto ids = catalog.Ids();
    ASSERT_EQ(ids.size(), 2U);
    EXPECT_EQ(ids[0], "Vase.A"); // Id 字典序（D53 同源）
    EXPECT_EQ(ids[1], "Vase.Z");
    const ManifestEntry* found = catalog.Find("Vase.Z");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->Subdirectory, "zeta"); // binary 缺省来自子目录名
    EXPECT_EQ(found->Binary, "zeta");
    EXPECT_EQ(catalog.Find("Vase.Missing"), nullptr);
}

TEST(CatalogScan, DuplicateIdRejectsWholeRefresh)
{
    CatalogSandbox sandbox{"scan-dup"};
    PutManifest(sandbox, "one", "Vase.Same");
    PutManifest(sandbox, "two", "Vase.Same");
    PluginCatalog catalog;
    const auto first = catalog.Refresh(sandbox.Root);
    ASSERT_FALSE(first.IsOk());
    EXPECT_NE(first.GetError().Message().find("duplicate plugin id"), std::string::npos); // 最小区分子串
    EXPECT_TRUE(catalog.Ids().empty()); // D58：失败不留半成品
}

TEST(CatalogScan, BadManifestRejectsAndOldSnapshotAndWarningsStand)
{
    CatalogSandbox sandbox{"scan-bad"};
    PutManifest(sandbox, "good", "Vase.Good");
    PluginCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());

    sandbox.WriteFile("broken/plugin.json", "{\"schemaVersion\":1,,}"); // 坏清单（D50）
    const auto second = catalog.Refresh(sandbox.Root);
    EXPECT_FALSE(second.IsOk());
    const auto ids = catalog.Ids(); // 旧快照与旧 Warnings 都不换（D58+D64a）
    ASSERT_EQ(ids.size(), 1U);
    EXPECT_EQ(ids[0], "Vase.Good");
    EXPECT_TRUE(catalog.Warnings().empty());
}

TEST(CatalogScan, BorrowWindowFollowsRefresh)
{
    CatalogSandbox sandbox{"scan-borrow"};
    PutManifest(sandbox, "a", "Vase.Old");
    PluginCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    ASSERT_NE(catalog.Find("Vase.Old"), nullptr);

    PutManifest(sandbox, "a", "Vase.New"); // 同一文件换 Id 再扫
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    EXPECT_EQ(catalog.Find("Vase.Old"), nullptr); // 旧快照随 Refresh 作废（D61 借用窗的另一半）
    EXPECT_NE(catalog.Find("Vase.New"), nullptr);
}

} // namespace
```

- [ ] **Step 4: 接入并跑绿**

`Tests/CMakeLists.txt` 源列表在 `Integration/ConfigApplyTests.cpp` 之后插 `Integration/CatalogScanTests.cpp`。

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R CatalogScan     # 7 条全绿
ctest --preset win-x64-clang-debug                     # 全量不回退
git add Include/Vase/Catalog/PluginCatalog.h Source/Catalog/PluginCatalog.cpp Tests/Integration/CatalogScanTests.cpp Tests/CMakeLists.txt
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
git commit -m "M2b 波1-T3：Refresh 扫描（事务快照/查重/缺清单跳过/借用窗）

D50 执法不对称（坏清单响、缺清单静默留痕）、D58+D64a 失败时快照与 Warnings
都不换、D61 借用窗由重复扫描钉死。扫描器不碰二进制（§4.4，存在性归波 2）。"
```

---

## Task 4: `LoadPreset`（结构/语法解析，D59 第一段）

**Spec:** §5。类型核对与越界在 T5/T6 的 `Solve` 里做（那里才有清单 schema）——本任务只交付「文件 → `Preset` 值」，数值按 JSON 原样入 `ConfigBlob`（int64/double）。

**Files:**
- Create: `Include/Vase/Catalog/Preset.h`
- Create: `Source/Catalog/PresetJson.cpp`
- Create: `Tests/Unit/PresetJsonTests.cpp`
- Modify: `Source/Catalog/CMakeLists.txt`、`Tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `ConfigBlob`（`Vase/Host/ConfigBlob.h`）、`Result`。
- Produces: `vase::PluginOverride{Id, Enabled, Config}`、`vase::Preset`（`SchemaVersion()/DisplayName()/Entries()`）、`Result<Preset> LoadPreset(const std::filesystem::path&)`。

- [ ] **Step 1: 写 `Include/Vase/Catalog/Preset.h`**

```cpp
#pragma once

// Preset 值形态（spec §5）。Preset 条目与会话临时覆盖共用 PluginOverride——
// §4.1「临时调整与保存为 Preset 之间没有转换代码」的类型兑现。

#include "Vase/Config/ConfigBlob.h"
#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace vase
{

struct PluginOverride
{
    std::string         Id;      // 引用未知 Id → Solve 记 note（warn 族）
    std::optional<bool> Enabled; // nullopt = 未提及
    ConfigBlob          Config;  // 只存增量；数值按 D59 以 JSON 原样入（int64/double）
};

class VASE_CATALOG_API Preset
{
public:
    [[nodiscard]] std::uint32_t SchemaVersion() const { return Version; }
    [[nodiscard]] const std::string& DisplayName() const { return Name; }
    [[nodiscard]] const std::vector<PluginOverride>& Entries() const { return Items; } // Id 字典序（nlohmann 对象 = std::map，非文件序）

private:
    friend Result<Preset> LoadPreset(const std::filesystem::path& file);

    std::uint32_t Version = 1;
    std::string Name;
    std::vector<PluginOverride> Items;
};

// 仅结构/语法（D59）：JSON 合法、形状对、schemaVersion major 闸、config 值域 = 标量
// （嵌套对象/数组即拒，§4.2）。Id 引用与类型核对归 Solve——那里才有清单 schema。
VASE_CATALOG_API Result<Preset> LoadPreset(const std::filesystem::path& file);

} // namespace vase
```

- [ ] **Step 2: 写 `Source/Catalog/PresetJson.cpp`**

```cpp
#include "Vase/Catalog/Preset.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace vase
{

namespace
{

Error PresetError(const std::filesystem::path& file, std::string_view detail)
{
    return Error("preset file \"" + file.string() + "\": " + std::string(detail));
}

} // namespace

Result<Preset> LoadPreset(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    if (!in)
    {
        return Result<Preset>::Err(PresetError(file, "cannot open file"));
    }
    const std::string text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    nlohmann::json root = nlohmann::json::parse(text, nullptr, false); // D55：非抛形
    if (root.is_discarded())
    {
        return Result<Preset>::Err(PresetError(file, "malformed JSON"));
    }
    if (!root.is_object())
    {
        return Result<Preset>::Err(PresetError(file, "top level must be a JSON object"));
    }

    const auto schemaVersion = root.find("schemaVersion");
    if (schemaVersion == root.end() || !schemaVersion->is_number_integer() ||
        schemaVersion->get<std::int64_t>() != 1)
    {
        return Result<Preset>::Err(PresetError(file, "schemaVersion required, integer major 1 (D59)"));
    }
    if (const auto name = root.find("displayName"); name != root.end() && !name->is_string())
    {
        return Result<Preset>::Err(PresetError(file, "displayName must be a string"));
    }
    const auto overrides = root.find("overrides");
    if (overrides == root.end() || !overrides->is_object())
    {
        return Result<Preset>::Err(PresetError(file, "field \"overrides\" must be an object"));
    }

    Preset preset;
    preset.Version = 1;
    if (const auto name = root.find("displayName"); name != root.end())
    {
        preset.Name = name->get_ref<const std::string&>();
    }
    for (auto it = overrides->begin(); it != overrides->end(); ++it)
    {
        const nlohmann::json& entry = it.value();
        if (!entry.is_object())
        {
            return Result<Preset>::Err(PresetError(file, "override for \"" + it.key() + "\" must be an object"));
        }
        PluginOverride item;
        item.Id = it.key();
        for (auto field = entry.begin(); field != entry.end(); ++field)
        {
            if (field.key() == "enabled")
            {
                if (!field.value().is_boolean())
                {
                    return Result<Preset>::Err(
                        PresetError(file, "override for \"" + item.Id + "\": enabled must be a bool"));
                }
                item.Enabled = field.value().get<bool>();
                continue;
            }
            if (field.key() != "config")
            {
                return Result<Preset>::Err(
                    PresetError(file, "override for \"" + item.Id + "\": unknown override field"));
            }
            const nlohmann::json& config = field.value();
            if (!config.is_object())
            {
                return Result<Preset>::Err(
                    PresetError(file, "override for \"" + item.Id + "\": config must be an object"));
            }
            for (auto kv = config.begin(); kv != config.end(); ++kv)
            {
                const nlohmann::json& raw = kv.value();
                if (raw.is_boolean())
                {
                    item.Config.Set(kv.key(), Value::From(raw.get<bool>()));
                }
                else if (raw.is_number_integer())
                {
                    // D59 原样形：整数一律 int64；越 int64 的 unsigned 没有合法窄化，直接拒
                    if (raw.is_number_unsigned() &&
                        raw.get<std::uint64_t>() >
                            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
                    {
                        return Result<Preset>::Err(PresetError(
                            file, "override \"" + item.Id + "\" key \"" + kv.key() + "\": integer too large"));
                    }
                    item.Config.Set(kv.key(), Value::From(raw.get<std::int64_t>()));
                }
                else if (raw.is_number_float())
                {
                    item.Config.Set(kv.key(), Value::From(raw.get<double>()));
                }
                else if (raw.is_string())
                {
                    item.Config.Set(kv.key(), Value::From(raw.get_ref<const std::string&>().c_str())); // Set 深拷入
                }
                else
                {
                    // 嵌套对象/数组与 null：§4.2 禁嵌套在结构段的兑现。
                    return Result<Preset>::Err(PresetError(
                        file, "override \"" + item.Id + "\" key \"" + kv.key() + "\": config values must be scalars"));
                }
            }
        }
        preset.Items.push_back(std::move(item));
    }
    return Result<Preset>::Ok(std::move(preset));
}

} // namespace vase
```

- [ ] **Step 3: 写 `Tests/Unit/PresetJsonTests.cpp`**

```cpp
// spec §5 的结构/语法线（D59 第一段）。类型核对与 warn 族归 SolveTests（T5）。

#include "Vase/Catalog/Preset.h"

#include "CatalogSandbox.h"

#include <gtest/gtest.h>
#include <string>

namespace
{

using testing_support::CatalogSandbox;
using vase::LoadPreset;
using vase::Preset;
using vase::Result;
using vase::ValueKind;

class PresetJson : public ::testing::Test
{
protected:
    CatalogSandbox Sandbox{"preset-json"};

    Result<Preset> Load(std::string_view text)
    {
        Sandbox.WriteFile("Client.preset.json", std::string(text));
        return LoadPreset(Sandbox.Root / "Client.preset.json");
    }
};

TEST_F(PresetJson, AcceptsFullShape)
{
    auto loaded = Load(R"({"schemaVersion":1,"displayName":"客户端","overrides":{)"
                       R"("Vase.Combat":{"config":{"friendlyFire":true,"repeats":3,"speed":1.5,"name":"x"}},)"
                       R"("Vase.Admin":{"enabled":false}}})");
    ASSERT_TRUE(loaded.IsOk()) << loaded.GetError().Message();
    const Preset& preset = loaded.Value();
    EXPECT_EQ(preset.SchemaVersion(), 1U);
    EXPECT_EQ(preset.DisplayName(), "客户端");
    ASSERT_EQ(preset.Entries().size(), 2U); // Id 字典序：Admin < Combat（nlohmann map 语义，非文件序）
    EXPECT_EQ(preset.Entries()[0].Id, "Vase.Admin");
    ASSERT_TRUE(preset.Entries()[0].Enabled.has_value());
    EXPECT_FALSE(*preset.Entries()[0].Enabled);
    EXPECT_EQ(preset.Entries()[1].Id, "Vase.Combat");
    EXPECT_FALSE(preset.Entries()[1].Enabled.has_value());
    const auto speed = preset.Entries()[1].Config.Find("speed"); // D59：原样 int64/double/bool/string
    ASSERT_TRUE(speed.has_value());
    EXPECT_EQ(speed->Kind, ValueKind::kDouble);
    const auto repeats = preset.Entries()[1].Config.Find("repeats");
    ASSERT_TRUE(repeats.has_value());
    EXPECT_EQ(repeats->Kind, ValueKind::kInt64);
    const auto name = preset.Entries()[1].Config.Find("name");
    ASSERT_TRUE(name.has_value());
    EXPECT_STREQ(name->GetAs<const char*>(), "x");
}

TEST_F(PresetJson, RejectsMissingSchemaVersionOrOverrides)
{
    EXPECT_FALSE(Load(R"({"displayName":"x","overrides":{}})").IsOk());
    EXPECT_FALSE(Load(R"({"schemaVersion":1})").IsOk());
}

TEST_F(PresetJson, RejectsMajorTwoAndFloatForm)
{
    EXPECT_FALSE(Load(R"({"schemaVersion":2,"overrides":{}})").IsOk());
    EXPECT_FALSE(Load(R"({"schemaVersion":1.0,"overrides":{}})").IsOk());
}

TEST_F(PresetJson, RejectsNestingInConfig)
{
    EXPECT_FALSE(Load(R"({"schemaVersion":1,"overrides":{"Vase.X":{"config":{"a":[1]}}}})").IsOk());
    EXPECT_FALSE(Load(R"({"schemaVersion":1,"overrides":{"Vase.X":{"config":{"a":{"b":1}}}}})").IsOk());
    EXPECT_FALSE(Load(R"({"schemaVersion":1,"overrides":{"Vase.X":{"config":{"a":null}}}})").IsOk());
}

TEST_F(PresetJson, RejectsUnknownFieldAndBadShapes)
{
    EXPECT_FALSE(Load(R"({"schemaVersion":1,"overrides":{"Vase.X":{"volume":2}}})").IsOk());
    EXPECT_FALSE(Load(R"({"schemaVersion":1,"overrides":{"Vase.X":{"enabled":"yes"}}})").IsOk());
    EXPECT_FALSE(Load(R"({"schemaVersion":1,"overrides":{"Vase.X":"nope"}})").IsOk());
    EXPECT_FALSE(Load(R"({"schemaVersion":1,"overrides":[]})").IsOk());
}

} // namespace
```

- [ ] **Step 4: 接入并跑绿**

`Source/Catalog/CMakeLists.txt` 源列表加 `PresetJson.cpp`；`Tests/CMakeLists.txt` 在 `Unit/ManifestJsonTests.cpp` 之后加 `Unit/PresetJsonTests.cpp`。

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R PresetJson    # 5 条全绿
ctest --preset win-x64-clang-debug                   # 全量
git add Include/Vase/Catalog/Preset.h Source/Catalog/PresetJson.cpp Tests/Unit/PresetJsonTests.cpp Source/Catalog/CMakeLists.txt Tests/CMakeLists.txt
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
git commit -m "M2b 波1-T4：LoadPreset 结构/语法解析（D59 第一段）

PluginOverride = Preset 条目与会话临时覆盖共用形；数值按 JSON 原样入 blob，
类型核对留给 Solve（那里才有清单 schema）。嵌套/null 即拒（§4.2 兑现）。"
```

---

## Task 5: `Solve` 第一段（前置 / 验证 / 参与 / 配置 / 路径）

**Spec:** §6 的前置、①、⑥、⑦、⑧ 格 + D46、D59 第二段、D63、D64（Overrides 重复线）、D66。
**分段实现说明（诚实的中间态）**：本段暂不消费 `requires`/`provides`（参与插件一律 kLoad、顺序 = Id 序）——依赖图归 T6，本段测试不涉及依赖语义。两波合并后才是 spec §6 全量。

**Files:**
- Create: `Include/Vase/Catalog/LoadRequest.h`
- Modify: `Include/Vase/Catalog/PluginCatalog.h`（include `LoadRequest.h` + 类内加 `Solve` 声明）
- Create: `Source/Catalog/Detail/LibraryFileName.h`
- Create: `Source/Catalog/Solve.cpp`
- Modify: `Tests/TestingSupport/CatalogSandbox.h`（加 `RefreshOrFail`）
- Create: `Tests/Integration/SolveTests.cpp`
- Modify: `Source/Catalog/CMakeLists.txt`、`Tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `ParseManifestFile`/`Refresh`（T2/T3）、`Preset`（T4）、`ConfigBlob::Set/Find/Entries`、`ServiceRef`（`Vase/PluginDescriptor.h`）。
- Produces（T6 在同文件继续、T7 消费）：`vase::LoadRequest{Preset, Overrides, HostProvided}`；`Result<SolveOutcome> PluginCatalog::Solve(const LoadRequest&) const`；`catalog_detail::LibraryFileName(std::string_view) -> std::string`；`testing_support::RefreshOrFail(PluginCatalog&, const CatalogSandbox&)`。

- [ ] **Step 1: 写 `Include/Vase/Catalog/LoadRequest.h`**

```cpp
#pragma once

// 求解输入面（spec §3）：目录的属主是快照，不在这里出现（D46）；
// HostProvided 借调用方的串，窗口 = 本次 Solve（D60）。

#include "Vase/Catalog/Preset.h"
#include "Vase/PluginDescriptor.h" // ServiceRef

#include <optional>
#include <span>

namespace vase
{

struct LoadRequest
{
    std::optional<Preset>           Preset;
    std::span<const PluginOverride> Overrides;    // 会话临时覆盖；重复 Id → Err（D64）
    std::span<const ServiceRef>     HostProvided; // 宿主声明本局 Stage0 会注册什么（D60）
};

} // namespace vase
```

- [ ] **Step 2: `PluginCatalog.h` 追加**

```cpp
#include "Vase/Catalog/LoadRequest.h"
// 类内 public: 段：
    // 纯函数，吃当前快照；spec §6 全语义。返回的 Plan 借用快照字符串（D61），
    // 借用窗 = 到下一次 Refresh；首扫前调用 → Err（D63）。
    Result<SolveOutcome> Solve(const LoadRequest& request) const;
```

- [ ] **Step 3: 写 `Source/Catalog/Detail/LibraryFileName.h`**

```cpp
#pragma once

// stem → 平台库文件名（D54）：纯字符串拼接。§13.1「平台差异收口在 Loader」管的是
// 加载行为；命名不属之（spec 勘误第 4 条）。iOS 的 .a 形态是 VasePack/M5 的问题。

#include <string>
#include <string_view>

namespace vase::catalog_detail
{

inline std::string LibraryFileName(std::string_view stem)
{
#ifdef _WIN32
    return std::string(stem) + ".dll";
#else
    return "lib" + std::string(stem) + ".so";
#endif
}

} // namespace vase::catalog_detail
```

- [ ] **Step 4: `CatalogSandbox.h` 的 `} // namespace testing_support` 前追加（需文件顶部补 `#include "Vase/Catalog/PluginCatalog.h"`、`#include <gtest/gtest.h>`、`#include <utility>`、`#include <vector>`）**

```cpp
inline void RefreshOrFail(vase::PluginCatalog& catalog, const CatalogSandbox& sandbox)
{
    const auto refreshed = catalog.Refresh(sandbox.Root);
    ASSERT_TRUE(refreshed.IsOk()) << refreshed.GetError().Message();
}

inline void StageManifests(CatalogSandbox& sandbox,
                           const std::vector<std::pair<std::string, std::string>>& dirToJson)
{
    for (const auto& [dir, json] : dirToJson)
    {
        sandbox.WriteFile(dir + "/plugin.json", json);
    }
}
```

- [ ] **Step 5: 写 `Source/Catalog/Solve.cpp`（上半：helpers）**

```cpp
#include "Vase/Catalog/PluginCatalog.h"

#include "Detail/LibraryFileName.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace vase
{
namespace
{

Error SolveError(std::string_view detail)
{
    return Error("catalog solve: " + std::string(detail));
}

std::string_view KindName(ValueKind kind)
{
    switch (kind)
    {
    case ValueKind::kBool: return "bool";
    case ValueKind::kInt32: return "int32";
    case ValueKind::kInt64: return "int64";
    case ValueKind::kFloat: return "float";
    case ValueKind::kDouble: return "double";
    case ValueKind::kString: return "string";
    default: return "none";
    }
}

// D59 原样形（bool/int64/double/string；也容忍 typed 面的 int32/float）→ 字段声明型。
std::optional<Value> Coerce(const Value& raw, ValueKind kind)
{
    Value widened = raw;
    if (raw.Kind == ValueKind::kInt32)
    {
        widened = Value::From(static_cast<std::int64_t>(raw.GetAs<std::int32_t>()));
    }
    else if (raw.Kind == ValueKind::kFloat)
    {
        widened = Value::From(static_cast<double>(raw.GetAs<float>()));
    }

    auto asInt64 = [&widened](std::int64_t& out)
    {
        if (widened.Kind != ValueKind::kInt64)
        {
            return false;
        }
        out = widened.GetAs<std::int64_t>();
        return true;
    };

    switch (kind)
    {
    case ValueKind::kBool:
        if (widened.Kind == ValueKind::kBool) { return widened; }
        break;
    case ValueKind::kInt32:
    {
        std::int64_t i = 0;
        if (asInt64(i) && i >= std::numeric_limits<std::int32_t>::min() &&
            i <= std::numeric_limits<std::int32_t>::max())
        {
            return Value::From(static_cast<std::int32_t>(i));
        }
        break;
    }
    case ValueKind::kInt64:
        if (widened.Kind == ValueKind::kInt64) { return widened; }
        break;
    case ValueKind::kFloat:
    {
        std::int64_t i = 0;
        if (asInt64(i)) { return Value::From(static_cast<float>(i)); }
        if (widened.Kind == ValueKind::kDouble) { return Value::From(static_cast<float>(widened.GetAs<double>())); }
        break;
    }
    case ValueKind::kDouble:
    {
        std::int64_t i = 0;
        if (asInt64(i)) { return Value::From(static_cast<double>(i)); }
        if (widened.Kind == ValueKind::kDouble) { return widened; }
        break;
    }
    case ValueKind::kString:
        if (widened.Kind == ValueKind::kString) { return widened; } // blob.Set 深拷入
        break;
    default:
        break;
    }
    return std::nullopt;
}

// bool/string 无 min/max（解析期保证）→ 数值四型比较。
bool OutOfRange(const ManifestConfigField& field, const Value& value)
{
    auto less = [&field](std::uint64_t a, std::uint64_t b)
    {
        switch (field.Kind)
        {
        case ValueKind::kInt32:
            return static_cast<std::int32_t>(static_cast<std::uint32_t>(a)) <
                   static_cast<std::int32_t>(static_cast<std::uint32_t>(b));
        case ValueKind::kInt64:
            return static_cast<std::int64_t>(a) < static_cast<std::int64_t>(b);
        case ValueKind::kFloat:
            return std::bit_cast<float>(static_cast<std::uint32_t>(a)) <
                   std::bit_cast<float>(static_cast<std::uint32_t>(b));
        default:
            return std::bit_cast<double>(a) < std::bit_cast<double>(b);
        }
    };
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) 读已 Coerce 值的位形（D24 同机制）
    const std::uint64_t v = value.Bits;
    if (field.HasMin && less(v, field.MinBits)) { return true; }
    if (field.HasMax && less(field.MaxBits, v)) { return true; }
    return false;
}

Value FromStorage(const ConfigBlob::Storage& stored)
{
    return std::visit(
        [](const auto& v) -> Value
        {
            using T = std::remove_cvref_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>)
            {
                return Value::From(v.c_str()); // 借用；调用方在同一次 Solve 内 Set 深拷
            }
            else
            {
                return Value::From(v);
            }
        },
        stored);
}

const ManifestConfigField* FindField(const ManifestEntry& entry, std::string_view key)
{
    for (const ManifestConfigField& field : entry.Config)
    {
        if (field.Key == key)
        {
            return &field;
        }
    }
    return nullptr;
}

} // namespace
```



- [ ] **Step 6: `Source/Catalog/Solve.cpp`（下半：`Solve` 主函数）**

```cpp
Result<SolveOutcome> PluginCatalog::Solve(const LoadRequest& request) const
{
    if (!HasScanned)
    {
        return Result<SolveOutcome>::Err(SolveError("no snapshot — call Refresh first (D63)"));
    }

    for (std::size_t i = 0; i < request.Overrides.size(); ++i)
    {
        for (std::size_t j = 0; j < i; ++j)
        {
            if (request.Overrides[j].Id == request.Overrides[i].Id)
            {
                return Result<SolveOutcome>::Err(
                    SolveError("duplicate override entry for id \"" + request.Overrides[i].Id + "\" (D64)"));
            }
        }
    }

    std::vector<const PluginOverride*> layers;
    if (request.Preset.has_value())
    {
        for (const PluginOverride& entry : request.Preset->Entries())
        {
            layers.push_back(&entry);
        }
    }
    for (const PluginOverride& overrideEntry : request.Overrides)
    {
        layers.push_back(&overrideEntry);
    }

    SolveOutcome outcome;

    // 验证与归因（D66：跑快照全量，与本局参与与否无关）。
    for (const PluginOverride* layer : layers)
    {
        const ManifestEntry* entry = Find(layer->Id);
        if (entry == nullptr)
        {
            outcome.Notes.push_back(SolveNote{.Kind = SolveNoteKind::kUnknownPluginId,
                                              .PluginId = layer->Id,
                                              .Message = "override references unknown plugin id"});
            continue;
        }
        for (const ConfigBlob::Entry& kv : layer->Config.Entries())
        {
            const ManifestConfigField* field = FindField(*entry, kv.Key);
            if (field == nullptr)
            {
                outcome.Notes.push_back(SolveNote{.Kind = SolveNoteKind::kUnknownConfigKey,
                                                  .PluginId = entry->Id,
                                                  .Key = kv.Key,
                                                  .Message = "config key not declared in manifest"});
                continue;
            }
            const std::optional<Value> coerced = Coerce(FromStorage(kv.Stored), field->Kind);
            if (!coerced.has_value())
            {
                return Result<SolveOutcome>::Err(SolveError("override for \"" + entry->Id + "\" key \"" + kv.Key +
                                                            "\": type mismatch, manifest declares " +
                                                            std::string(KindName(field->Kind))));
            }
            if (OutOfRange(*field, *coerced))
            {
                return Result<SolveOutcome>::Err(
                    SolveError("override for \"" + entry->Id + "\" key \"" + kv.Key + "\": value out of manifest range"));
            }
        }
    }

    std::vector<char> participating(SnapshotEntries.size(), 0);
    for (std::size_t index = 0; index < SnapshotEntries.size(); ++index)
    {
        const ManifestEntry& entry = SnapshotEntries[index];
        bool enabled = entry.EnabledByDefault;
        for (const PluginOverride* layer : layers)
        {
            if (layer->Id == entry.Id && layer->Enabled.has_value())
            {
                enabled = *layer->Enabled; // 后层整体替换该键（spec §5）
            }
        }
        participating[index] = enabled ? 1 : 0;
    }

    std::vector<LoadPlanEntry> loads;
    std::vector<LoadPlanEntry> skips;
    for (std::size_t index = 0; index < SnapshotEntries.size(); ++index)
    {
        const ManifestEntry& entry = SnapshotEntries[index];
        LoadPlanEntry planEntry;
        planEntry.Id = entry.Id; // 借快照（D61）
        planEntry.BinaryPath = SnapshotDirectory / entry.Subdirectory / catalog_detail::LibraryFileName(entry.Binary);
        if (participating[index] == 0)
        {
            planEntry.Decision = LoadDecision::kSkip;
            planEntry.Reason = SkipReason::kDisabled; // T6 补：闭包跳过的 missing/version 线
            skips.push_back(std::move(planEntry));
            continue;
        }

        ConfigBlob merged;
        for (const ManifestConfigField& field : entry.Config)
        {
            merged.Set(field.Key, field.DefaultValue()); // ⑥ 第一层：清单默认值入计划（D23 清单侧）
        }
        for (const PluginOverride* layer : layers)
        {
            if (layer->Id != entry.Id)
            {
                continue;
            }
            for (const ConfigBlob::Entry& kv : layer->Config.Entries())
            {
                const ManifestConfigField* field = FindField(entry, kv.Key);
                if (field == nullptr)
                {
                    continue; // kUnknownConfigKey 已记 note，不并入
                }
                if (const std::optional<Value> coerced = Coerce(FromStorage(kv.Stored), field->Kind))
                {
                    merged.Set(kv.Key, *coerced);
                }
            }
        }
        planEntry.ResolvedConfig = std::move(merged);
        loads.push_back(std::move(planEntry));
    }

    outcome.Plan.Ordered = std::move(loads);
    for (LoadPlanEntry& skip : skips)
    {
        outcome.Plan.Ordered.push_back(std::move(skip));
    }
    return Result<SolveOutcome>::Ok(std::move(outcome));
}

} // namespace vase
```

- [ ] **Step 7: 写 `Tests/Integration/SolveTests.cpp`（第一段用例集；T6 在同文件续写）**

```cpp
// spec §6 的逐格判据（本段 = 前置/验证/参与/配置/路径；依赖图用例随 T6 增补）。
// Notes 一律按 Kind/Cause 字段断言（D62），不匹配消息子串。

#include "Vase/Catalog/PluginCatalog.h"

#include "CatalogSandbox.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

using testing_support::CatalogSandbox;
using testing_support::RefreshOrFail;
using testing_support::StageManifests;
using vase::LoadPlan;
using vase::LoadPlanEntry;
using vase::LoadRequest;
using vase::PluginCatalog;
using vase::SolveNoteKind;
using vase::SolveOutcome;

std::string ManifestWith(std::string_view id, std::string_view extraFields)
{
    std::string out = R"({"schemaVersion":1,"id":")" + std::string(id) + "\"";
    if (!extraFields.empty())
    {
        out += ",";
        out += extraFields;
    }
    out += "}";
    return out;
}

// 计划的可逐字节比较形态（D53 确定性判据）。
std::string PlanText(const LoadPlan& plan)
{
    std::string text;
    for (const LoadPlanEntry& entry : plan.Ordered)
    {
        text += std::string(entry.Id) + "|" +
                (entry.Decision == vase::LoadDecision::kLoad ? "load" : "skip") + "|" +
                std::to_string(static_cast<int>(entry.Reason)) + "|" + entry.BinaryPath.generic_u8string() + "|";
        for (const auto& kv : entry.ResolvedConfig.Entries())
        {
            text += kv.Key + "=";
            std::visit(
                [&text](const auto& v)
                {
                    using T = std::remove_cvref_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, std::string>) { text += v; }
                    else { text += std::to_string(v); }
                },
                kv.Stored);
            text += ";";
        }
        text += "\n";
    }
    return text;
}

std::filesystem::path ExpectedBinaryPath(const CatalogSandbox& sandbox, const std::string& dir,
                                         const std::string& stem)
{
    std::filesystem::path expected = sandbox.Root / dir;
#ifdef _WIN32
    expected /= stem + ".dll";
#else
    expected /= "lib" + stem + ".so";
#endif
    return expected;
}
```

```cpp
SolveOutcome SolveOrDie(PluginCatalog& catalog, const LoadRequest& request)
{
    auto solved = catalog.Solve(request);
    EXPECT_TRUE(solved.IsOk()) << solved.GetError().Message();
    return solved.IsOk() ? std::move(solved.Value()) : SolveOutcome{};
}

vase::Preset LoadPresetOrDie(CatalogSandbox& sandbox, const std::string& name, std::string_view text)
{
    sandbox.WriteFile(name, std::string(text));
    auto loaded = vase::LoadPreset(sandbox.Root / name);
    EXPECT_TRUE(loaded.IsOk()) << loaded.GetError().Message();
    return loaded.IsOk() ? std::move(loaded.Value()) : vase::Preset{};
}

class SolveA : public ::testing::Test
{
protected:
    CatalogSandbox Sandbox{"solve-a"};
    PluginCatalog Catalog;

    void Stage(std::vector<std::pair<std::string, std::string>> dirToJson)
    {
        StageManifests(Sandbox, dirToJson);
        RefreshOrFail(Catalog, Sandbox);
    }
};

TEST_F(SolveA, BeforeRefreshRejects) // D63
{
    LoadRequest request;
    const auto solved = Catalog.Solve(request);
    ASSERT_FALSE(solved.IsOk());
    EXPECT_NE(solved.GetError().Message().find("Refresh"), std::string::npos); // 最小区分子串
}

TEST_F(SolveA, SinglePluginPlanShape)
{
    Stage({{"solo", ManifestWith("Vase.Solo", R"("config":[{"key":"n","type":"int32","default":5}])")}});
    LoadRequest request;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    ASSERT_EQ(outcome.Plan.Ordered.size(), 1U);
    const LoadPlanEntry& entry = outcome.Plan.Ordered[0];
    EXPECT_EQ(entry.Id, "Vase.Solo");
    EXPECT_EQ(entry.Decision, vase::LoadDecision::kLoad);
    EXPECT_EQ(entry.BinaryPath, ExpectedBinaryPath(Sandbox, "solo", "solo")); // binary 缺省=子目录名（D54）
    const auto n = entry.ResolvedConfig.Find("n"); // ⑥ 清单默认值已并入计划（D23 清单侧兑现）
    ASSERT_TRUE(n.has_value());
    EXPECT_EQ(n->GetAs<std::int32_t>(), 5);
    EXPECT_TRUE(outcome.Notes.empty());
}

TEST_F(SolveA, EnabledThreeLayers)
{
    Stage({{"a", ManifestWith("Vase.A", R"("enabledByDefault":false)")},
           {"b", ManifestWith("Vase.B", "")},
           {"c", ManifestWith("Vase.C", "")}});
    // 清单 false → preset true（A 参与）；preset 未提 B（默认 true）；override false 压过 B/C 之上的层。
    vase::Preset preset = LoadPresetOrDie(Sandbox, "Client.preset.json",
                                          R"({"schemaVersion":1,"overrides":{"Vase.A":{"enabled":true},"Vase.B":{"enabled":true}}})");
    const std::vector<vase::PluginOverride> overrides{vase::PluginOverride{.Id = "Vase.B", .Enabled = false}};
    LoadRequest request;
    request.Preset = std::move(preset);
    request.Overrides = overrides;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    ASSERT_EQ(outcome.Plan.Ordered.size(), 3U); // kLoad 段在前（Id 序），skip 段尾随
    EXPECT_EQ(outcome.Plan.Ordered[0].Id, "Vase.A"); // A: preset 层 true 压过清单 false → load
    EXPECT_EQ(outcome.Plan.Ordered[0].Decision, vase::LoadDecision::kLoad);
    EXPECT_EQ(outcome.Plan.Ordered[1].Id, "Vase.C"); // C: 三层均未提及 → enabledByDefault → load
    EXPECT_EQ(outcome.Plan.Ordered[1].Decision, vase::LoadDecision::kLoad);
    EXPECT_EQ(outcome.Plan.Ordered[2].Id, "Vase.B"); // B: override 层压过 preset 层 → skip
    EXPECT_EQ(outcome.Plan.Ordered[2].Decision, vase::LoadDecision::kSkip);
    EXPECT_EQ(outcome.Plan.Ordered[2].Reason, vase::SkipReason::kDisabled);
}

TEST_F(SolveA, NoteKindsAreFields)
{
    Stage({{"a", ManifestWith("Vase.A", R"("config":[{"key":"known","type":"int32","default":1}])")}});
    vase::Preset preset = LoadPresetOrDie(
        Sandbox, "Client.preset.json",
        R"({"schemaVersion":1,"overrides":{"Vase.Ghost":{"enabled":true},"Vase.A":{"config":{"nope":1}}}})");
    LoadRequest request;
    request.Preset = std::move(preset);
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    ASSERT_EQ(outcome.Notes.size(), 2U); // 层内按 Entries() 的 Id 字典序：A 在 Ghost 前
    EXPECT_EQ(outcome.Notes[0].Kind, SolveNoteKind::kUnknownConfigKey);
    EXPECT_EQ(outcome.Notes[0].Key, "nope");
    EXPECT_EQ(outcome.Notes[1].Kind, SolveNoteKind::kUnknownPluginId);
    EXPECT_EQ(outcome.Notes[1].PluginId, "Vase.Ghost");
}
```

```cpp
TEST_F(SolveA, TypeAndRangeErrors)
{
    Stage({{"a", ManifestWith("Vase.A", R"("config":[{"key":"n","type":"int32","default":5,"min":1,"max":9}])")}});
    {
        vase::Preset preset = LoadPresetOrDie(Sandbox, "bad-type.json",
                                              R"({"schemaVersion":1,"overrides":{"Vase.A":{"config":{"n":"five"}}}})");
        LoadRequest request;
        request.Preset = std::move(preset);
        EXPECT_FALSE(Catalog.Solve(request).IsOk()); // 类型不符 = error（D51）
    }
    {
        vase::Preset preset = LoadPresetOrDie(Sandbox, "bad-range.json",
                                              R"({"schemaVersion":1,"overrides":{"Vase.A":{"config":{"n":42}}}})");
        LoadRequest request;
        request.Preset = std::move(preset);
        const auto solved = Catalog.Solve(request);
        ASSERT_FALSE(solved.IsOk()); // 越界 = error（D35 执法点）
        EXPECT_NE(solved.GetError().Message().find("out of manifest range"), std::string::npos);
    }
}

TEST_F(SolveA, WidenInt64ToFloatField)
{
    Stage({{"a", ManifestWith("Vase.A",
                              R"("config":[{"key":"f","type":"float","default":2.0,"min":1.0,"max":10.0}])")}});
    vase::Preset preset = LoadPresetOrDie(Sandbox, "Client.preset.json",
                                          R"({"schemaVersion":1,"overrides":{"Vase.A":{"config":{"f":7}}}})");
    LoadRequest request;
    request.Preset = std::move(preset);
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    const auto f = outcome.Plan.Ordered[0].ResolvedConfig.Find("f"); // int→float 加宽（D51）
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->Kind, vase::ValueKind::kFloat);
    EXPECT_FLOAT_EQ(f->GetAs<float>(), 7.0F);
}

TEST_F(SolveA, DuplicateOverrideIdRejects) // D64
{
    Stage({{"a", ManifestWith("Vase.A", "")}});
    const std::vector<vase::PluginOverride> overrides{
        vase::PluginOverride{.Id = "Vase.A", .Enabled = true},
        vase::PluginOverride{.Id = "Vase.A", .Enabled = false}};
    LoadRequest request;
    request.Overrides = overrides;
    EXPECT_FALSE(Catalog.Solve(request).IsOk());
}

TEST_F(SolveA, AllDisabledIsPlanNotError)
{
    Stage({{"a", ManifestWith("Vase.A", R"("enabledByDefault":false)")}});
    LoadRequest request;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    ASSERT_EQ(outcome.Plan.Ordered.size(), 1U);
    EXPECT_EQ(outcome.Plan.Ordered[0].Decision, vase::LoadDecision::kSkip);
    EXPECT_TRUE(outcome.Plan.Ordered[0].ResolvedConfig.Entries().empty()); // skip 空 blob（D53）
}

TEST_F(SolveA, DoubleRunIsByteIdentical) // D53
{
    Stage({{"b", ManifestWith("Vase.B", "")}, {"a", ManifestWith("Vase.A", "")}});
    LoadRequest request;
    const std::string first = PlanText(SolveOrDie(Catalog, request).Plan);
    const std::string second = PlanText(SolveOrDie(Catalog, request).Plan);
    EXPECT_EQ(first, second);
    EXPECT_EQ(first.find("Vase.A|load"), 0U); // 本段无依赖图：kLoad 段 = Id 序（T6 后此判据移交拓扑用例）
}

} // namespace
```

- [ ] **Step 8: 接入并跑绿**

`Source/Catalog/CMakeLists.txt` 源列表加 `Solve.cpp`；`Tests/CMakeLists.txt` 在 `Integration/CatalogScanTests.cpp` 之后加 `Integration/SolveTests.cpp`。

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R SolveA          # 9 条全绿
ctest --preset win-x64-clang-debug                     # 全量
git add Include/Vase/Catalog/LoadRequest.h Include/Vase/Catalog/PluginCatalog.h Source/Catalog/Solve.cpp Source/Catalog/Detail Tests/TestingSupport/CatalogSandbox.h Tests/Integration/SolveTests.cpp Source/Catalog/CMakeLists.txt Tests/CMakeLists.txt docs/superpowers/specs/2026-09-24-vase-m2b-catalog-solving-design.md
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
git commit -m "M2b 波1-T5：Solve 第一段（前置/验证/参与/配置/路径）+ LoadRequest + LibraryFileName

spec §6 的 ①⑥⑦⑧ 与前置线（D63 首扫 Err、D64 Overrides 重复 Err、D66 全量执法）；
依赖图格随 T6。nlohmann 对象是 std::map——preset 条目迭代 = Id 字典序而非文件序，
spec §3 的该处注释同步纠偏。"
```

---

## Task 6: `Solve` 第二段（依赖闭包 / 碰撞 / 环 / 拓扑序）

**Spec:** §6 的 ②③④⑤ 格 + D52、D60、D62。交付方式 = 把 T5 主函数中「从 `std::vector<char> participating` 声明起到函数尾 return」的整段替换为下面的完整实现（前半段：前置 / Overrides 查重 / layers 收集 / 验证归因——不动）。

**Files:**
- Modify: `Source/Catalog/Solve.cpp`（替换段 + 顶部补 `#include <algorithm>`、`#include <array>`、`#include <map>`、`#include <set>`）
- Modify: `Tests/Integration/SolveTests.cpp`（匿名 ns 尾部、`} // namespace` 之前追加 `SolveB` 簇）

**Interfaces:**
- Consumes: T5 的全部（layers、验证段、`FindField`、`Coerce`、`OutOfRange`、`SolveError`）。
- Produces: `Solve` 的完整语义（T7 消费）：kLoad 段 = Kahn 拓扑序、kSkip 段 = 尾随 Id 序；`participating`/`reasons` 为其内部机器。

- [ ] **Step 1: 替换 `Solve.cpp` 后半段（完整新文，含 ①–⑧）**

```cpp
    std::vector<char> participating(SnapshotEntries.size(), 0);
    std::vector<SkipReason> reasons(SnapshotEntries.size(), SkipReason::kDisabled);

    // ① 参与判定：清单默认 → preset → override，后层整体替换该键。
    for (std::size_t index = 0; index < SnapshotEntries.size(); ++index)
    {
        const ManifestEntry& entry = SnapshotEntries[index];
        bool enabled = entry.EnabledByDefault;
        for (const PluginOverride* layer : layers)
        {
            if (layer->Id == entry.Id && layer->Enabled.has_value())
            {
                enabled = *layer->Enabled;
            }
        }
        participating[index] = enabled ? 1 : 0;
    }

    using ServiceKey = std::pair<std::string, std::uint32_t>;
    std::map<ServiceKey, std::vector<std::size_t>> exactProviders;
    std::map<std::string, std::vector<std::pair<std::uint32_t, std::size_t>>> nameProviders;
    for (std::size_t index = 0; index < SnapshotEntries.size(); ++index)
    {
        for (const ManifestDependency& provided : SnapshotEntries[index].Provides)
        {
            exactProviders[{provided.Service, provided.Version}].push_back(index);
            nameProviders[provided.Service].push_back({provided.Version, index});
        }
    }

    auto hostProvides = [&request](std::string_view name, std::uint32_t version)
    {
        for (const ServiceRef& ref : request.HostProvided)
        {
            if (ref.Name == name && ref.Version == version)
            {
                return true;
            }
        }
        return false;
    };

    // ② 硬需求闭包：不动点迭代，每轮按快照 Id 序扫 → 归因确定性（D53/D62）。
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (std::size_t index = 0; index < SnapshotEntries.size(); ++index)
        {
            if (participating[index] == 0)
            {
                continue;
            }
            const ManifestEntry& entry = SnapshotEntries[index];
            for (const ManifestDependency& need : entry.Requires)
            {
                if (hostProvides(need.Service, need.Version))
                {
                    continue; // D60：宿主声明恒满足、不入环、不定序
                }
                const auto exactIt = exactProviders.find({need.Service, need.Version});
                bool satisfied = false;
                if (exactIt != exactProviders.end())
                {
                    for (const std::size_t provider : exactIt->second)
                    {
                        if (participating[provider] != 0)
                        {
                            satisfied = true;
                            break;
                        }
                    }
                }
                if (satisfied)
                {
                    continue;
                }

                participating[index] = 0;
                changed = true;
                reasons[index] = SkipReason::kMissingDependency;

                bool versionNamed = false;
                const auto nameIt = nameProviders.find(need.Service);
                if (nameIt != nameProviders.end())
                {
                    for (const auto& [version, provider] : nameIt->second)
                    {
                        if (participating[provider] != 0 && version != need.Version)
                        {
                            reasons[index] = SkipReason::kVersionMismatch;
                            outcome.Notes.push_back(SolveNote{.Kind = SolveNoteKind::kVersionMismatchProvider,
                                                              .PluginId = entry.Id,
                                                              .Cause = SnapshotEntries[provider].Id,
                                                              .Message = "only major " + std::to_string(version) +
                                                                         " of " + need.Service + " participates"});
                            versionNamed = true;
                            break;
                        }
                    }
                }
                if (!versionNamed && exactIt != exactProviders.end() && !exactIt->second.empty())
                {
                    outcome.Notes.push_back(
                        SolveNote{.Kind = SolveNoteKind::kProviderSkipped,
                                  .PluginId = entry.Id,
                                  .Cause = SnapshotEntries[exactIt->second.front()].Id,
                                  .Message = "provider disabled or skipped: " + need.Service});
                }
                break; // 首个不满足的声明即归因（§6② 注）
            }
        }
    }
```

（同文件继续，接上段。）

```cpp
    // ③ 碰撞：参与集内 provide 同 {name,ver} 即拒（§6.3「不静默择一」）；撞 HostProvided 同判（D60）。
    std::map<ServiceKey, std::size_t> claimedBy;
    for (std::size_t index = 0; index < SnapshotEntries.size(); ++index)
    {
        if (participating[index] == 0)
        {
            continue;
        }
        const ManifestEntry& entry = SnapshotEntries[index];
        for (const ManifestDependency& provided : entry.Provides)
        {
            const ServiceKey key{provided.Service, provided.Version};
            if (hostProvides(key.first, key.second))
            {
                return Result<SolveOutcome>::Err(SolveError("service \"" + key.first + "\"@" +
                                                            std::to_string(key.second) + "\" provided by plugin \"" +
                                                            entry.Id + "\" collides with HostProvided (D60)"));
            }
            const auto [it, inserted] = claimedBy.emplace(key, index);
            if (!inserted)
            {
                return Result<SolveOutcome>::Err(
                    SolveError("service \"" + key.first + "\"@" + std::to_string(key.second) + "\" provided by both \"" +
                               SnapshotEntries[it->second].Id + "\" and \"" + entry.Id + "\""));
            }
        }
    }

    // ④⑤ 拓扑 = Kahn（边 = 硬 ∪ optional 的精确提供方对，D52）；ready 集按 Id 字典序弹出（D53）。
    std::vector<std::vector<std::size_t>> adjacency(SnapshotEntries.size());
    std::vector<std::size_t> inDegree(SnapshotEntries.size(), 0);
    std::size_t remaining = 0;
    for (std::size_t index = 0; index < SnapshotEntries.size(); ++index)
    {
        if (participating[index] == 0)
        {
            continue;
        }
        ++remaining;
        const ManifestEntry& entry = SnapshotEntries[index];
        std::vector<std::size_t> providers;
        const auto collect = [&](const std::vector<ManifestDependency>& deps)
        {
            for (const ManifestDependency& need : deps)
            {
                if (hostProvides(need.Service, need.Version))
                {
                    continue;
                }
                const auto exactIt = exactProviders.find({need.Service, need.Version});
                if (exactIt == exactProviders.end())
                {
                    continue; // optional 缺席不拦；hard 缺席者已在 ② 被跳出局
                }
                for (const std::size_t provider : exactIt->second)
                {
                    if (participating[provider] != 0)
                    {
                        providers.push_back(provider);
                    }
                }
            }
        };
        collect(entry.Requires);
        collect(entry.OptionalRequires);
        std::ranges::sort(providers);
        providers.erase(std::ranges::unique(providers).begin(), providers.end());
        for (const std::size_t provider : providers)
        {
            adjacency[provider].push_back(index);
            ++inDegree[index];
        }
    }

    std::set<std::pair<std::string_view, std::size_t>> ready;
    for (std::size_t index = 0; index < SnapshotEntries.size(); ++index)
    {
        if (participating[index] != 0 && inDegree[index] == 0)
        {
            ready.insert({SnapshotEntries[index].Id, index});
        }
    }
    std::vector<std::size_t> loadOrder;
    loadOrder.reserve(remaining);
    std::vector<char> emitted(SnapshotEntries.size(), 0);
    while (!ready.empty())
    {
        const std::size_t index = ready.begin()->second;
        ready.erase(ready.begin());
        loadOrder.push_back(index);
        emitted[index] = 1;
        for (const std::size_t consumer : adjacency[index])
        {
            if (--inDegree[consumer] == 0)
            {
                ready.insert({SnapshotEntries[consumer].Id, consumer});
            }
        }
    }
    if (loadOrder.size() != remaining)
    {
        std::string listed;
        for (std::size_t index = 0; index < SnapshotEntries.size(); ++index)
        {
            if (participating[index] != 0 && emitted[index] == 0)
            {
                listed += "\"" + SnapshotEntries[index].Id + "\" ";
            }
        }
        return Result<SolveOutcome>::Err(
            SolveError("dependency cycle or blocked plugins: " + listed + "(hard & pure-optional edges bite, D52)"));
    }

    // ⑥⑦⑧ 条目装配：kLoad 段 = 拓扑序；kSkip 段尾随、按快照 Id 序。
    auto buildLoadEntry = [&](std::size_t index)
    {
        const ManifestEntry& entry = SnapshotEntries[index];
        LoadPlanEntry planEntry;
        planEntry.Id = entry.Id; // 借快照（D61）
        planEntry.BinaryPath = SnapshotDirectory / entry.Subdirectory / catalog_detail::LibraryFileName(entry.Binary);
        ConfigBlob merged;
        for (const ManifestConfigField& field : entry.Config)
        {
            merged.Set(field.Key, field.DefaultValue());
        }
        for (const PluginOverride* layer : layers)
        {
            if (layer->Id != entry.Id)
            {
                continue;
            }
            for (const ConfigBlob::Entry& kv : layer->Config.Entries())
            {
                const ManifestConfigField* field = FindField(entry, kv.Key);
                if (field == nullptr)
                {
                    continue;
                }
                if (const std::optional<Value> coerced = Coerce(FromStorage(kv.Stored), field->Kind))
                {
                    merged.Set(kv.Key, *coerced);
                }
            }
        }
        planEntry.ResolvedConfig = std::move(merged);
        return planEntry;
    };

    for (const std::size_t index : loadOrder)
    {
        outcome.Plan.Ordered.push_back(buildLoadEntry(index));
    }
    for (std::size_t index = 0; index < SnapshotEntries.size(); ++index)
    {
        if (participating[index] != 0)
        {
            continue;
        }
        LoadPlanEntry skip;
        skip.Id = SnapshotEntries[index].Id;
        skip.BinaryPath = SnapshotDirectory / SnapshotEntries[index].Subdirectory /
                          catalog_detail::LibraryFileName(SnapshotEntries[index].Binary);
        skip.Decision = LoadDecision::kSkip;
        skip.Reason = reasons[index]; // kDisabled / kMissingDependency / kVersionMismatch
        outcome.Plan.Ordered.push_back(std::move(skip));
    }
    return Result<SolveOutcome>::Ok(std::move(outcome));
}
```

（替换后 `Solve` 全文即 spec §6。T5 的 `DoubleRunIsByteIdentical` 与 `SinglePluginPlanShape` 无需改动——无依赖图上 Kahn 退化为 Id 字典序，旧断言仍真。）

- [ ] **Step 2: `Tests/Integration/SolveTests.cpp` 追加 `SolveB` 簇（匿名 ns 尾、`} // namespace` 前）**

```cpp
class SolveB : public ::testing::Test
{
protected:
    CatalogSandbox Sandbox{"solve-b"};
    PluginCatalog Catalog;

    void Stage(std::vector<std::pair<std::string, std::string>> dirToJson)
    {
        StageManifests(Sandbox, dirToJson);
        RefreshOrFail(Catalog, Sandbox);
    }

    std::vector<std::string> LoadIds(const LoadPlan& plan)
    {
        std::vector<std::string> ids;
        for (const LoadPlanEntry& entry : plan.Ordered)
        {
            if (entry.Decision == vase::LoadDecision::kLoad)
            {
                ids.emplace_back(entry.Id);
            }
        }
        return ids;
    }

    std::vector<std::string> SkipIds(const LoadPlan& plan)
    {
        std::vector<std::string> ids;
        for (const LoadPlanEntry& entry : plan.Ordered)
        {
            if (entry.Decision == vase::LoadDecision::kSkip)
            {
                ids.emplace_back(entry.Id);
            }
        }
        return ids;
    }
};

constexpr const char* kProvS = R"("provides":[{"service":"S","version":1}])";
constexpr const char* kReqS = R"("requires":[{"service":"S","version":1}])";

TEST_F(SolveB, TopoRespectsHardEdges)
{
    Stage({{"c", ManifestWith("Vase.C", kReqS)}, {"p", ManifestWith("Vase.P", kProvS)}});
    LoadRequest request;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    // 层0 = {P}（C 有入边），弹出 P 后 C 就绪 → 拓扑序 [P, C]，**与 Id 序相反**——这条才是拓扑判据。
    EXPECT_EQ(LoadIds(outcome.Plan), (std::vector<std::string>{"Vase.P", "Vase.C"}));
    EXPECT_TRUE(SkipIds(outcome.Plan).empty());
}

TEST_F(SolveB, SameLayerIdLex)
{
    Stage({{"z", ManifestWith("Vase.Z", kReqS)},
           {"m", ManifestWith("Vase.M", "")},
           {"a", ManifestWith("Vase.A", "")},
           {"p", ManifestWith("Vase.P", kProvS)}});
    LoadRequest request;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    EXPECT_EQ(LoadIds(outcome.Plan),
              (std::vector<std::string>{"Vase.A", "Vase.M", "Vase.P", "Vase.Z"})); // 同层 Id 字典序（D53）
}

TEST_F(SolveB, CascadeSkipNamesProvider) // D62：B 因 A 被禁而跳，C 因 B 被跳而跳
{
    Stage({{"a", ManifestWith("Vase.A", std::string(kProvS) + R"(,"enabledByDefault":false)")},
           {"b", ManifestWith("Vase.B", std::string(kReqS) + R"(,"provides":[{"service":"T","version":1}])")},
           {"c", ManifestWith("Vase.C", R"("requires":[{"service":"T","version":1}])")}});
    LoadRequest request;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    EXPECT_TRUE(LoadIds(outcome.Plan).empty());
    const auto skipIds = SkipIds(outcome.Plan);
    ASSERT_EQ(skipIds.size(), 3U); // A(kDisabled)+B+C 全部示众，尾部 Id 序
    EXPECT_EQ(skipIds, (std::vector<std::string>{"Vase.A", "Vase.B", "Vase.C"}));
    ASSERT_EQ(outcome.Notes.size(), 2U);
    EXPECT_EQ(outcome.Notes[0].Kind, SolveNoteKind::kProviderSkipped);
    EXPECT_EQ(outcome.Notes[0].PluginId, "Vase.B");
    EXPECT_EQ(outcome.Notes[0].Cause, "Vase.A");
    EXPECT_EQ(outcome.Notes[1].Kind, SolveNoteKind::kProviderSkipped);
    EXPECT_EQ(outcome.Notes[1].PluginId, "Vase.C");
    EXPECT_EQ(outcome.Notes[1].Cause, "Vase.B");
}
```

```cpp
TEST_F(SolveB, VersionMismatchSkipsWithNamedCause) // §6②
{
    Stage({{"p", ManifestWith("Vase.P", kProvS)},
           {"c", ManifestWith("Vase.C", R"("requires":[{"service":"S","version":2}])")}});
    LoadRequest request;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    EXPECT_EQ(LoadIds(outcome.Plan), (std::vector<std::string>{"Vase.P"}));
    ASSERT_EQ(outcome.Plan.Ordered.back().Reason, vase::SkipReason::kVersionMismatch);
    ASSERT_EQ(outcome.Notes.size(), 1U);
    EXPECT_EQ(outcome.Notes[0].Kind, SolveNoteKind::kVersionMismatchProvider);
    EXPECT_EQ(outcome.Notes[0].Cause, "Vase.P");
}

TEST_F(SolveB, AbsentProviderSkipsWithoutNote) // §6② 沉默线
{
    Stage({{"c", ManifestWith("Vase.C", R"("requires":[{"service":"Nowhere","version":1}])")}});
    LoadRequest request;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    EXPECT_EQ(outcome.Plan.Ordered[0].Reason, vase::SkipReason::kMissingDependency);
    EXPECT_TRUE(outcome.Notes.empty());
}

TEST_F(SolveB, HostProvidedSatisfiesRequires) // D60
{
    Stage({{"c", ManifestWith("Vase.C", R"("requires":[{"service":"HostThing","version":1}])")}});
    const std::array<vase::ServiceRef, 1> host{
        {vase::ServiceRef{.Name = "HostThing", .Version = 1}}};
    LoadRequest request;
    request.HostProvided = host;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    EXPECT_EQ(LoadIds(outcome.Plan), (std::vector<std::string>{"Vase.C"}));
}

TEST_F(SolveB, HostProvidedCollisionRejects) // D60
{
    Stage({{"p", ManifestWith("Vase.P", R"("provides":[{"service":"HostThing","version":1}])")}});
    const std::array<vase::ServiceRef, 1> host{
        {vase::ServiceRef{.Name = "HostThing", .Version = 1}}};
    LoadRequest request;
    request.HostProvided = host;
    const auto solved = Catalog.Solve(request);
    ASSERT_FALSE(solved.IsOk());
    EXPECT_NE(solved.GetError().Message().find("collides with HostProvided"), std::string::npos);
}

TEST_F(SolveB, PluginCollisionRejects) // §6③
{
    Stage({{"a", ManifestWith("Vase.A", kProvS)}, {"b", ManifestWith("Vase.B", kProvS)}});
    LoadRequest request;
    const auto solved = Catalog.Solve(request);
    ASSERT_FALSE(solved.IsOk());
    EXPECT_NE(solved.GetError().Message().find("provided by both"), std::string::npos);
}

TEST_F(SolveB, CycleRejectsNamingMembers) // §6④
{
    Stage({{"a", ManifestWith("Vase.A",
                              R"("requires":[{"service":"TB","version":1}],"provides":[{"service":"TA","version":1}])")},
           {"b", ManifestWith("Vase.B",
                              R"("requires":[{"service":"TA","version":1}],"provides":[{"service":"TB","version":1}])")}});
    LoadRequest request;
    const auto solved = Catalog.Solve(request);
    ASSERT_FALSE(solved.IsOk());
    EXPECT_NE(solved.GetError().Message().find("Vase.A"), std::string::npos);
    EXPECT_NE(solved.GetError().Message().find("Vase.B"), std::string::npos);
}

TEST_F(SolveB, OptionalCycleAlsoRejects) // D52
{
    Stage({{"a", ManifestWith("Vase.A",
                              R"("optionalRequires":[{"service":"TB","version":1}],"provides":[{"service":"TA","version":1}])")},
           {"b", ManifestWith("Vase.B",
                              R"("optionalRequires":[{"service":"TA","version":1}],"provides":[{"service":"TB","version":1}])")}});
    LoadRequest request;
    EXPECT_FALSE(Catalog.Solve(request).IsOk());
}

TEST_F(SolveB, OptionalEdgeOrdersWhenPresent) // D52：在场则先行
{
    Stage({{"a", ManifestWith("Vase.A", R"("optionalRequires":[{"service":"S","version":1}])")},
           {"z", ManifestWith("Vase.Z", kProvS)}});
    LoadRequest request;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    EXPECT_EQ(LoadIds(outcome.Plan), (std::vector<std::string>{"Vase.Z", "Vase.A"})); // 非 Id 序 → 是 optional 边在起作用
}

TEST_F(SolveB, OptionalAbsentDoesNotSkip) // §6②：optional 缺失不跳（D52）
{
    Stage({{"a", ManifestWith("Vase.A", R"("optionalRequires":[{"service":"Ghost","version":1}])")}});
    LoadRequest request;
    const SolveOutcome outcome = SolveOrDie(Catalog, request);
    EXPECT_EQ(LoadIds(outcome.Plan), (std::vector<std::string>{"Vase.A"}));
    EXPECT_TRUE(outcome.Notes.empty());
}

TEST_F(SolveB, DoubleRunIsByteIdenticalWithGraph) // D53（带图版）
{
    Stage({{"c", ManifestWith("Vase.C", std::string(kReqS) +
                                         R"(,"provides":[{"service":"T2","version":1}])")},
           {"d", ManifestWith("Vase.D", R"("requires":[{"service":"T2","version":1}])")},
           {"p", ManifestWith("Vase.P", kProvS)}});
    LoadRequest request;
    const std::string first = PlanText(SolveOrDie(Catalog, request).Plan);
    const std::string second = PlanText(SolveOrDie(Catalog, request).Plan);
    EXPECT_EQ(first, second);
    EXPECT_EQ(first.find("Vase.P|"), 0U); // 拓扑序在文本判据里同样成立：P 第一
}

} // namespace
```

- [ ] **Step 3: 跑绿并收口本任务**

`SolveTests.cpp` 顶部补 `#include <array>`。

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R "SolveA|SolveB"   # 9+13 全绿
ctest --preset win-x64-clang-debug                       # 全量
git add Source/Catalog/Solve.cpp Tests/Integration/SolveTests.cpp
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
git commit -m "M2b 波1-T6：Solve 第二段（闭包/碰撞/环/拓扑）补全 spec §6

不动点闭包 + 级联归因 Note（D62，Cause 字段）、HostProvided 满足与碰撞（D60）、
optional 边定序与纯 optional 环拒（D52）、Kahn + 同层 Id 字典序（D53）。
Host 零改动，规矩 6 不触发。"
```

---

## Task 7: 端到端证人（`Solve` → `CreatePod`，Host 零改动）

**Spec:** §8 的 `AssemblyFromSolveTests` + D57、D65。给 4 个既有 fixture 落「目录形态」清单，temp 沙箱复制 staging，`Solve` 产计划喂 `CreatePod`，装载序断言 = 计划拓扑序。本任务不改任何库代码（若逼出改动 = 越界，停下问需求方）。

**Files:**
- Create: `Tests/Integration/fixtures/manifests/hello/plugin.json`、`.../shared_provider/plugin.json`、`.../edge_consumer/plugin.json`、`.../shared_consumer2/plugin.json`
- Modify: `Tests/TestingSupport/PodTestPeer.h` / `PodTestPeer.cpp`（加 `InstanceOrder`）
- Create: `Tests/Integration/AssemblyFromSolveTests.cpp`
- Modify: `Tests/CMakeLists.txt`（源 + `VASE_FIXTURE_MANIFESTS` 宏）

**Interfaces:**
- Consumes: `PluginCatalog`、`T6 的 Solve`、既有 `VASE_FIXTURE_{HELLO,SHAREDPROVIDER,EDGECONSUMER,SHAREDCONSUMER2}`、`samples_fixture::IHostOnlyService`（`fixtures/SharedCommon.h`）、`PodTestPeer`。
- Produces: `PodTestPeer::InstanceOrder(const Pod&) -> std::vector<std::string>`。

- [ ] **Step 1: 写四份清单（内容 = 各 fixture 描述符的真账，`binary` 显式给 target 输出名）**

`Tests/Integration/fixtures/manifests/hello/plugin.json`：

```json
{
    "schemaVersion": 1,
    "id": "Vase.Hello",
    "binary": "HelloPlugin",
    "provides": [ { "service": "Vase.Hello.Greeter", "version": 1 } ],
    "config": [ { "key": "Repeats", "type": "int32", "default": 1 } ]
}
```

`.../shared_provider/plugin.json`：

```json
{
    "schemaVersion": 1,
    "id": "Vase.SharedProvider",
    "binary": "SharedProviderPlugin",
    "provides": [ { "service": "Vase.Test.Shared", "version": 1 } ]
}
```

`.../edge_consumer/plugin.json`：

```json
{
    "schemaVersion": 1,
    "id": "Vase.EdgeConsumer",
    "binary": "EdgeConsumerPlugin",
    "requires": [ { "service": "Vase.Test.Shared", "version": 1 },
                  { "service": "Vase.Test.HostOnly", "version": 1 } ]
}
```

`.../shared_consumer2/plugin.json`：

```json
{
    "schemaVersion": 1,
    "id": "Vase.SharedConsumer2",
    "binary": "SharedConsumer2Plugin",
    "requires": [ { "service": "Vase.Test.Shared", "version": 1 } ]
}
```

- [ ] **Step 2: `PodTestPeer.h` 追加（`InjectLeakedScope` 之后）；`PodTestPeer.cpp` 追加实现**

```cpp
// .h：
    // 证人用例（M2b 波1 T7）读取装载序：数组序 = 计划序（Pod.h:162 注释即依据）。
    static std::vector<std::string> InstanceOrder(const Pod& pod);

// .cpp（顶部补 #include <string>、#include <vector>）：
std::vector<std::string> PodTestPeer::InstanceOrder(const Pod& pod)
{
    std::vector<std::string> order;
    for (const auto& live : pod.Instances)
    {
        order.push_back(std::string(live->OwnerLabel));
    }
    return order;
}
```

- [ ] **Step 3: 写 `Tests/Integration/AssemblyFromSolveTests.cpp`**

```cpp
// D57 端到端证人 + D65 staging：Solve 产的计划喂 CreatePod，Host 一行未改。
// 「Solve 产同形计划、Host 无感」= D12 零返工的直接证据（spec §8）。

#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Context.h"
#include "Vase/Pod/Pod.h"

#include "CatalogSandbox.h"
#include "PodTestPeer.h"
#include "fixtures/SharedCommon.h"

#include <array>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace
{

using testing_support::CatalogSandbox;
using vase::Context;
using vase::LoadRequest;
using vase::PluginCatalog;
using vase::PluginHost;
using vase::PodOptions;
using vase::ServiceRef;

struct HostOnly final : samples_fixture::IHostOnlyService
{
    [[nodiscard]] int Marker() const override { return 1; }
};

void ProvideHostOnly(Context& ctx)
{
    static HostOnly instance; // Stage0 借用注册：实例须活过整局（AdoptTests 同法）
    ctx.Provide<samples_fixture::IHostOnlyService>(instance);
}

class AssemblyFromSolve : public ::testing::Test
{
protected:
    // 沙箱声明最先 → 析构最后：DLL 文件要活到 Host 拆完局（HotSwapLoop 的声明序契约同因）。
    CatalogSandbox Sandbox{"assembly-solve"};
    PluginCatalog Catalog;

    void StageAll()
    {
        const std::filesystem::path manifests = VASE_FIXTURE_MANIFESTS;
        struct Roster
        {
            const char* dir;
            const char* binaryMacro;
        };
        const std::array<Roster, 4> roster{{
            {"hello", VASE_FIXTURE_HELLO},
            {"shared_provider", VASE_FIXTURE_SHAREDPROVIDER},
            {"edge_consumer", VASE_FIXTURE_EDGECONSUMER},
            {"shared_consumer2", VASE_FIXTURE_SHAREDCONSUMER2},
        }};
        for (const Roster& item : roster)
        {
            const std::filesystem::path binary{item.binaryMacro};
            Sandbox.CopyFile(std::string(item.dir) + "/plugin.json", manifests / item.dir / "plugin.json");
            Sandbox.CopyFile(std::string(item.dir) + "/" + binary.filename().string(), binary);
        }
        auto refreshed = Catalog.Refresh(Sandbox.Root);
        ASSERT_TRUE(refreshed.IsOk()) << refreshed.GetError().Message();
    }

    LoadRequest WithHost()
    {
        static const std::array<ServiceRef, 1> host{
            {ServiceRef{.Name = samples_fixture::IHostOnlyService::kName,
                        .Version = samples_fixture::IHostOnlyService::kVersion}}};
        LoadRequest request;
        request.HostProvided = host;
        return request;
    }
};

TEST_F(AssemblyFromSolve, SolvePlanAssemblesInTopoOrder)
{
    StageAll();
    const auto solved = Catalog.Solve(WithHost());
    ASSERT_TRUE(solved.IsOk()) << solved.GetError().Message();
    const auto& plan = solved.Value().Plan;
    ASSERT_EQ(plan.Ordered.size(), 4U);
    const std::vector<std::string> expected{"Vase.Hello", "Vase.SharedProvider", "Vase.EdgeConsumer",
                                            "Vase.SharedConsumer2"}; // 拓扑 + 同层 Id 序（D53）
    std::vector<std::string> planIds;
    for (const auto& entry : plan.Ordered)
    {
        planIds.emplace_back(entry.Id);
    }
    EXPECT_EQ(planIds, expected);

    PluginHost host;
    PodOptions options;
    options.Strict = true; // 本局不该有任何 Failed（§5.5）
    options.Stage0 = &ProvideHostOnly;
    const auto created = host.CreatePod(plan, options);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message();
    auto* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(vase::PodTestPeer::InstanceOrder(*pod), expected); // 实际装载序 == 计划序

    const auto report = host.DestroyPod(created.Value());
    EXPECT_TRUE(report.Clean());
    EXPECT_EQ(host.ForTestCounters().PluginInstances, 0U);
}

TEST_F(AssemblyFromSolve, SkippedEntriesPassThroughHost)
{
    StageAll();
    Sandbox.WriteFile("Client.preset.json",
                      R"({"schemaVersion":1,"overrides":{"Vase.SharedProvider":{"enabled":false}}})");
    auto loaded = vase::LoadPreset(Sandbox.Root / "Client.preset.json");
    ASSERT_TRUE(loaded.IsOk()) << loaded.GetError().Message();
    LoadRequest request = WithHost();
    request.Preset = std::move(loaded.Value());
    const auto solved = Catalog.Solve(request);
    ASSERT_TRUE(solved.IsOk()) << solved.GetError().Message();
    ASSERT_EQ(solved.Value().Plan.Ordered.size(), 4U); // 1 load + 3 skip 全示众（D53）

    PluginHost host;
    PodOptions options;
    options.Stage0 = &ProvideHostOnly;
    const auto created = host.CreatePod(solved.Value().Plan, options);
    ASSERT_TRUE(created.IsOk()) << created.GetError().Message(); // 跳过条目照进局（D30 KnownBinaries 语义不变）
    auto* pod = host.Resolve(created.Value());
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(vase::PodTestPeer::InstanceOrder(*pod), std::vector<std::string>{"Vase.Hello"});
    const auto report = host.DestroyPod(created.Value());
    EXPECT_TRUE(report.Clean());
}

} // namespace
```

- [ ] **Step 4: 接入并跑绿**

`Tests/CMakeLists.txt`：源列表在 `Integration/SolveTests.cpp` 之后加 `Integration/AssemblyFromSolveTests.cpp`；`target_compile_definitions(VaseTests ...)` 块内追加一行：

```cmake
    VASE_FIXTURE_MANIFESTS="$<PATH:CMAKE_PATH,${CMAKE_CURRENT_SOURCE_DIR}/Integration/fixtures/manifests>"
```

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R AssemblyFromSolve   # 2 条全绿
ctest --preset win-x64-clang-debug                         # 全量（141 + 本波新增全绿）
git add Tests/Integration/fixtures/manifests Tests/TestingSupport/PodTestPeer.h Tests/TestingSupport/PodTestPeer.cpp Tests/Integration/AssemblyFromSolveTests.cpp Tests/CMakeLists.txt
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
git commit -m "M2b 波1-T7：端到端证人——Solve 产计划喂 CreatePod，Host 零改动

D57/D65：temp 沙箱复制 staging（HotSwapLoop 范式）；四份 fixture 清单按描述符真账
手写；PodTestPeer 开 InstanceOrder 只读缝（数组序 = 计划序）。第二条钉「kSkip 条目
照单全收不加载」——M2a 装配语义在清单来源下原样成立。规矩 6 不触发（未动 Adopt 面）。"
```

---

## Task 8: 收口波（六线全矩阵 / tidy 三线 / 基数重测 / 文书同步）

**Spec:** §8 环境义务 + §9 文书义务。本任务不写新代码（修门禁暴露的问题除外）。

**Files:**
- Modify: `CLAUDE.md`（项目状态行、目录清单、依赖入口段、六线基数表、tidy 基数表）
- Modify: `wiki/vase-architecture.md`（spec §9 的四条勘误，实施后回挂）

**Interfaces:** 无新接口；输出 = 门禁证据与文书对账。

- [ ] **Step 1: 全矩阵构建与测试（一次网络可用的窗口）**

```bash
Scripts/win-verify.cmd                                   # 四棵 Windows 树删树重配
wsl -d Ubuntu -- bash -lc 'bash /mnt/d/Git/Vase/Scripts/linux-verify.sh'   # 两棵 Linux 树
```

Expected：六线 configure/build/ctest/-N 全绿。**`z-applocal` 文件锁假红**（CLAUDE.md 已录两案）出现时单线删树重跑再判，不改仓库。记录每线 `Total Tests`：新基数 = 旧表（141/140/143/142）+ 本波 T1–T7 新增用例数（T8 实测填表，不预定）。Linux 若 +1 TU（78→+Catalog 4+tests…）以 `run-clang-tidy` 首行实测为准。

- [ ] **Step 2: tidy 三线各自跑、读正文**

```bash
run-clang-tidy -p build-win/win-x64-clang-debug
Scripts\win-clang-tidy.cmd msvc
Scripts/linux-clang-tidy.sh
```

Expected：三处退出码 0 **且正文 `error:` 0 条、`warning:` 0 条**。`Suppressed` 合计会因 nlohmann 模板量明显上跳（与 gtest 同族形态，摘要行读法见 CLAUDE.md）；TU 数三线各 +（4 源 TU；测试源不加 TU 数——`run-clang-tidy` 按路径去重计 VaseTests 的既有方式不变，实测记录）。正文若有新告警：代码级出路优先，确无出路才 NOLINT + 一句「为什么没有出路」（口径见 CLAUDE.md「NOLINT 的口径」）。

- [ ] **Step 3: format 全门**

```bash
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
```

Expected：exit 0（新文件早已 `git add`，否则这一步就是静默漏门）。

- [ ] **Step 4: `CLAUDE.md` 同步（规矩 7：字面真值只住这一份）**

逐项改：

1. 「项目状态」段：`M2a 完成（M2 第一波）` 之后补 `M2b 第一波完成（清单解析/Catalog/Solve/Preset）`；`M2b 未起` 改为 `M2b 第二波未起（加载期比对/Adopt 改接/enum+bump/前端）`。
2. 「磁盘上是这些」清单：`Source/{Pod,Host}` 改 `Source/{Pod,Host,Catalog}`；两动态库 target 段补第三库 `VaseCatalog`（链 `VaseHost`，D45 链接方向）；`Include/Vase/` 枚举补 `Catalog/`。
3. 「第三方依赖有两条入口」段：vcpkg 那条的「现在只有 gtest」改为「gtest 与 nlohmann-json」（并注：nlohmann 虽是 header-only 库，仍走 vcpkg 的 CMake config 包路径，与 submodule+INTERFACE 那条 `ThirdParty/cli` 的区别在它是 `find_package` 消费者）。
4. 六线基数表与 tidy 基数表：用 Step 1/2 实测值更新，差值成因列同步（本波新增用例无 death 门、无平台门 → 六线同幅增加；Linux TU 数 +4，Windows 也 +4，差保持 1 不变——以实测复核这段话）。
5. 自检 grep（技能目录不获知新基数）：

```bash
grep -rn "Total Tests\|[0-9][0-9] TU\|DEBUG:FULL\|build-id=sha1\|EHs-c-\|HAS_EXCEPTIONS\|--cached --others\|WarningsAsErrors\|PRE_TEST\|ctest --preset\|run-clang-tidy -p\|cmake --build --preset" \
  .claude/skills/vase-cpp-engineering/ | grep -v cpp-core-guidelines.md
```

Expected：命中仍限于规矩 7 允许的两类（带指针的字面值、判据与理由）；新文档没有把基数抄进技能。

- [ ] **Step 5: `wiki/vase-architecture.md` 四条勘误（spec §9 原文，逐条挂 `[M2b波1 勘误（2026-09-24, spec D46/D47/D49/D54/D60）]` 标注，不改史）**

1. §4.1 末「`Solve` 同时接受一个路径和一个已解析 Preset 值」→ 注：路径消化在 `Refresh`/`LoadPreset`；`LoadRequest` 携 `HostProvided` 声明面（D46、D60）。
2. §4.4/§5.1 的 `Result<LoadPlan> Solve(const LoadRequest&)` → 注：实形 `Result<SolveOutcome>`（Plan + Notes，D47/D62）；§5.1 `LoadRequest` 提议形无 `PluginDirectory`（D46）。
3. §3.3 末「次要差异（新增可选字段）容忍」→ 注：指格式世代方向，parser 随库版本独一、unknown 一律拒（D49）。
4. §13.1「平台差异全部收口在 Loader 层」→ 注：范围 = 加载行为；库文件命名（`LibraryFileName`）在 Catalog 层立私有实现（D54）。

- [ ] **Step 6: 提交收口**

```bash
git add CLAUDE.md wiki/vase-architecture.md
git commit -m "M2b 波1-T8：收口——六线全矩阵、tidy 三线、基数重测与文书同步

CLAUDE.md 五处（项目状态/磁盘清单/依赖入口/基数两表/技能自检 grep）；
wiki 挂四条 M2b波1 勘误（D46/D47/D49/D54/D60，不改史）。规矩 6 全程未触发：
Loader/账本/Eject/Adopt/描述符/HeaderVersion 零改动，HotSwap 选择子义务不适用；
六线全绿本身覆盖存量回归。"
git log --oneline -9
```

Expected：提交后 `git status` 干净；`git log` 上本波共 8 笔任务提交 + spec/更名在前。

---

## 验收清单（整个计划的完成判据）

- [ ] 六线删树重配全绿（`win-verify.cmd` + `linux-verify.sh` 退出码 0，逐步打印无失败步）。
- [ ] `ctest --preset <六线> -N` 的 `Total Tests` 与新基数表逐位对上，差值成因可复述（无新 death 门/平台门 → 四 Windows 线同幅）。
- [ ] tidy 三线：退出 0 + 正文 `error:`/`warning:` 双 0，摘要行数变化能归因（nlohmann 模板量）。
- [ ] format 门 exit 0（含全部新文件，已 `git add`）。
- [ ] `AssemblyFromSolve` 两条证人在 Windows 与 Linux 各自绿（Linux 侧由 `linux-verify.sh` 覆盖，证据进 T8 汇报）。
- [ ] `grep -rn "nlohmann" Include/` 零命中（D55 公开头零 json）；`grep -rnE '(EXPECT|ASSERT)_(ANY_)?(NO_)?THROW\(' Tests/` 零命中（规矩 2——宏调用形；散文提及不算，`Tests/CMakeLists.txt` 与 `DetailTests.cpp` 里 4 处规约散文注释在新式下预期零命中）。
- [ ] CLAUDE.md 五处 + wiki 四勘误落地；技能目录零字面基数（规矩 7 自检 grep 过）。
- [ ] 提交信息无 AI 署名尾注；`VasePod`/`VaseHost`/三工具链文件/承重 flag/既有 fixture 源码的 `git diff` 为空（波界纪律，D44/D56/D65）。
