# Vase M6：VaseCli plan / doctor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 兑现 §11.1 的后两条子命令——`VaseCli plan`（零装载的加载计划预览）与 `VaseCli doctor`（收窄四项的环境诊断），把 D135 同一律扩到四命令。

**Architecture:** 全部增量住 `Tools/VaseCli/`（`VaseCliCore` 加 `Plan.{h,cpp}` 与 `Doctor.{h,cpp}`）与测试/文书层；**库与公开面零增量**（不动 `kHeaderVersion`、不新增导出符号）。`plan` = Console `pod new` 已在走的 `Refresh → LoadPreset → Solve` 链搬到批处理前端；`doctor` 四项站在既有读取面（`Loader::FileIdentity` 磁盘读、`InspectDescriptors` 装载读）+ 两个文件内探针。argv 的 `--host-provides` 解析腿提入 `Cli.{h,cpp}` 共用。

**Tech Stack:** C++20 / CMake + vcpkg / GoogleTest / 六 preset（Win clang-cl & cl.exe、Linux clang + libc++）/ clang-tidy 23.1.0 / 全项目关异常。

**Spec:** `docs/superpowers/specs/2026-09-29-vase-m6-vasecli-plan-doctor-design.md`（决定 D141–D155；执行时以 spec 为准，本计划是它的展开。spec 的「事实取证注」九条与 grilling R1 是本文所有代码块引用的事实来源。）

## Global Constraints

- **不使用 C++ 异常**：不写 `throw` / `try` / `catch`；测试里**不用** `EXPECT_THROW` 一族（编译期硬失败）。错误一律经 `Result<T>` / `Error` 显式返回。
- **Vase 自己的头一律引号包含**：`#include "Vase/Plugin.h"`，不写尖括号。
- **新 target 必须链 `VaseBuildOptions`**；**插件 target 一律经 `vase_add_plugin_fixture`**。本波不新建 target，只改 `VaseCliCore` 源表。
- **命名规范由 `.clang-tidy` 强制**：类/函数/成员 `CamelCase`，参数/局部 `camelBack`，常量 `k` + `CamelCase`，命名空间 `lower_case`（Tools 层住单层 `tools::cli::`）。
- **格式化**：`.clang-format` 是 LLVM 基线 + 偏离项（Allman、`PointerAlignment: Left`、初始化表每式一行逗号在行首、`BreakTemplateDeclarations: Yes`）。
- **注释判据**：删掉它读者会不会踩坑；单条 ≤2 行、硬上限 3 行；论证属 spec/plan，代码只留结论 + 指针（D-number 指针形如 `// M6/D152`）。
- **提交信息不带任何 AI 署名尾注**。提交前缀 `M6-T<N>：`。
- **本波不动**：`kHeaderVersion`（仍 4）、`PluginMeta`/`LoadPlan` 布局、`VaseHost`/`VaseCatalog` 任何源文件、`Tools/VaseConsole` 任何文件、`Solve` 判定本体。
- **`Add a source = 三处登记`**：新 `.cpp` 必须手加 `Tools/VaseCli/CMakeLists.txt`（库源表）与 `Tests/CMakeLists.txt:3-40`（`VaseTests` 源清单，无 GLOB）；测试文件不进 VaseCliCore。
- **开发线**：`win-x64-clang-debug`（Windows / Git Bash）；Linux 侧经 `wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && …'`。拼 WSL 命令时**别把 `$` 写进双引号**（外层 Git Bash 先展开，命令照跑、结果假绿）。
- **规矩 6**：本波零 Loader/账本/Eject 路径改动，HotSwap 选择子族的触发条件不适用；但 doctor② 是 Loader 的**新调用方**，收口波六线全量（含该族逐条）即调用面回归证据。
- **基数与 tidy 阈值只在 `CLAUDE.md`**，任何任务不得把数字抄到别处。

---

## File Structure

| 文件 | 职责 | 动作 |
|---|---|---|
| `Tools/VaseCli/Cli.h` | 共用 argv 助手声明（D146） | 改：加 `ParseTrailingHostProvides` |
| `Tools/VaseCli/Cli.cpp` | 子命令分发 + usage + argv 助手实现 | 改（T1 搬家；T2/T3 分发与 usage 扩行） |
| `Tools/VaseCli/Validate.cpp` | validate 编排 | 改（匿名 ns 的 `ParseHostProvided` 删走，`RunValidate` 换接助手；行为逐字不变） |
| `Tools/VaseCli/Plan.h` / `Plan.cpp` | `plan` 编排：链条、输出、退出码映射 | 新建 |
| `Tools/VaseCli/Doctor.h` / `Doctor.cpp` | `doctor` 编排：收窄四项、两枚探针、收集序 ①③④→② | 新建 |
| `Tools/VaseCli/CMakeLists.txt` | 库源表 + ctest 用法腿 | 改 |
| `Tests/Integration/VaseCliPlanTests.cpp` | plan 的 11 条用例 | 新建 |
| `Tests/Integration/VaseCliDoctorTests.cpp` | doctor 的 6+2 条用例（2 条体内 `#ifdef` 平台各形） | 新建 |
| `Tests/CMakeLists.txt` | `VaseTests` 源清单 | 改（加两个测试文件） |
| `docs/superpowers/specs/…m6…design.md` | spec 样例文本随实定文案对齐 | 改（T2 一处） |
| `CLAUDE.md` / `wiki/vase-architecture.md` | 状态、基数、四命令措辞；§11.1/§12.3 现状注 | 改（T5 收口） |

**任务序**：T1 argv 搬家（无新行为）→ T2 `plan` → T3 `doctor`（平台中立支）→ T4 平台证人 + 两条实施期首测 → T5 收口与文书。T2 依赖 T1 的助手；T3 依赖 T1；T4 依赖 T3 的 `Doctor.cpp`；T5 收尾。

---

### Task 1: `--host-provides` 解析腿搬进 Cli（D146，纯搬家零行为变化）

**Files:**
- Modify: `Tools/VaseCli/Cli.h`（`Run` 声明之前加助手）
- Modify: `Tools/VaseCli/Cli.cpp`（匿名 ns 收 `ParseHostProvided`；加助手实现）
- Modify: `Tools/VaseCli/Validate.cpp:27-49`（删搬走的函数）与 `:252-279`（`RunValidate` 换接）

**Interfaces:**
- Consumes: 无新依赖。
- Produces: `bool ParseTrailingHostProvides(const std::vector<std::string>& args, std::size_t firstFlag, std::vector<vase::ServiceRef>& out, std::string_view usageLine, std::ostream& err)` —— T2 的 `RunPlan` 以 `firstFlag = 2 或 3` 调它；`usageLine` 带行尾 `\n` 原样打印。坏值的点名文案（`invalid --host-provides value: …`）在助手内逐字保留。

- [ ] **Step 1: `Cli.h` 加声明**

在 `Run` 声明之上加（include 区加 `#include "Vase/PluginDescriptor.h"`、`#include <cstddef>`、`#include <string>`、`#include <string_view>`、`#include <vector>`——`iosfwd` 已有）：

```cpp
// 尾段成对消费 `--host-provides <name>@<version>`（validate 与 plan 共用，M6/D146）：
// 任一格错即点名并返回 false；args 存活须覆盖调用（D60 的借窗同律）。
bool ParseTrailingHostProvides(const std::vector<std::string>& args, std::size_t firstFlag,
                               std::vector<vase::ServiceRef>& out, std::string_view usageLine, std::ostream& err);
```

- [ ] **Step 2: `Cli.cpp` 收实现**

把 `Validate.cpp:30-49` 的匿名 `ParseHostProvided` **原文搬进** `Cli.cpp` 的匿名 namespace（连它头上的两行注释一起搬）；在其后加实现：

```cpp
bool ParseTrailingHostProvides(const std::vector<std::string>& args, std::size_t firstFlag,
                               std::vector<vase::ServiceRef>& out, std::string_view usageLine, std::ostream& err)
{
    // 一次吃「flag + 值」两格：步进放 header 的 std::advance(it, 2)（双 ++it 触发 -Wfor-loop-analysis）。
    for (auto it = std::next(args.begin(), static_cast<std::ptrdiff_t>(firstFlag)); it != args.end(); std::advance(it, 2))
    {
        if (*it != "--host-provides" || std::next(it) == args.end())
        {
            err << usageLine;
            return false;
        }
        const std::optional<vase::ServiceRef> provided = ParseHostProvided(*std::next(it));
        if (!provided.has_value())
        {
            err << "invalid --host-provides value: " << *std::next(it) << " (expected <name>@<version>=1)\n";
            return false;
        }
        out.push_back(*provided);
    }
    return true;
}
```

`Cli.cpp` include 区补 `#include <charconv>`（`ParseHostProvided` 的 `from_chars` 随函数一起搬）、`#include <cstddef>`、`#include <iterator>`、`#include <optional>`、`#include <string>`、`#include <string_view>`、`#include <vector>`。

- [ ] **Step 3: `Validate.cpp` 删搬走的东西并换接**

删 `:27-49` 的匿名 `ParseHostProvided`（整个函数含注释）与不再用的 include（`<charconv>`、`<cstdint>`、`<optional>`、`<string_view>` 里以**编译报错为准逐个摘**——`:51-97` 还在用 `string_view`）。`RunValidate` 的 `:259-277` 整段换成：

```cpp
    ValidateOptions options;
    options.PluginDirectory = *std::next(args.begin(), 1); // std::next 形态：pro-bounds 检查的既有惯例（同 Cli.cpp）
    // 解析腿已提入 Cli 层与 plan 共用（M6/D146）；文案逐字保留（MalformedHostProvides 用例钉着）。
    if (!ParseTrailingHostProvides(args, 2, options.HostProvided,
                                   "usage: VaseCli validate <插件目录> [--host-provides <name>@<version>]...\n", err))
    {
        return kExitUsage;
    }
```

- [ ] **Step 4: 全量构建 + 家族回归**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R '^VaseCli' --output-on-failure
```

Expected: 零警告；`^VaseCli` 家族 24 条全绿（其中 `VaseCliValidate,MalformedHostProvidesIsAUsageError` 就是这次搬家的行为回归证人）。**搬家不新增用例是刻意的**——同一段逻辑第二处注册只会把「两份实现漂移」伪装成「覆盖翻倍」。

- [ ] **Step 5: 提交**

```bash
git add Tools/VaseCli/Cli.h Tools/VaseCli/Cli.cpp Tools/VaseCli/Validate.cpp
git commit -m "M6-T1：--host-provides 解析腿从 Validate.cpp 提入 Cli.{h,cpp} 共用（D146，纯搬家零行为变化）"
```

---

### Task 2: `VaseCli plan`

**Files:**
- Create: `Tools/VaseCli/Plan.h`、`Tools/VaseCli/Plan.cpp`
- Modify: `Tools/VaseCli/Cli.cpp`（分发 + `PrintUsage` 加 plan 行）、`Tools/VaseCli/CMakeLists.txt:4-8`（源表加 `Plan.cpp`）、`:25-36` 之后（ctest 腿）
- Create: `Tests/Integration/VaseCliPlanTests.cpp`
- Modify: `Tests/CMakeLists.txt:3-40`（源清单）、spec 样例一处（见 Step 5）

**Interfaces:**
- Consumes: T1 的 `ParseTrailingHostProvides`；`vase::PluginCatalog::Refresh/Solve/Ids/Directory`、`vase::LoadPreset`、`LoadRequest{Preset, HostProvided}`、`SolveOutcome{Plan, Notes}`、`LoadPlanEntry{Id, Decision, Reason}`、`SolveNote{Kind, PluginId, Key, Cause, Message}`。
- Produces: `int RunPlan(const std::vector<std::string>& args, std::ostream& out, std::ostream& err)` 与 `int PlanDirectory(const PlanOptions& options, std::ostream& out, std::ostream& err)`；`struct PlanOptions { std::filesystem::path PluginDirectory; std::optional<std::filesystem::path> PresetFile; std::vector<vase::ServiceRef> HostProvided; }`。测试直调 `RunPlan`（argv 形），`PlanDirectory` 留给将来可能的程序化消费者。

- [ ] **Step 1: `Plan.h`**

```cpp
#pragma once

// plan（M6/D141/D145）：Refresh → LoadPreset(可选) → Solve → 打印。零装载的全工具唯一预览腿。

#include "Vase/PluginDescriptor.h" // ServiceRef

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace tools::cli
{

struct PlanOptions
{
    std::filesystem::path PluginDirectory;
    std::optional<std::filesystem::path> PresetFile;
    std::vector<vase::ServiceRef> HostProvided; // D60：Name 借 argv 串
};

// 退出码同一律三档见 Cli.h 与 spec §2.3（D142）。
int RunPlan(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

// 供用例直调：跳过 argv 解析（同 ValidateDirectory 形）。
int PlanDirectory(const PlanOptions& options, std::ostream& out, std::ostream& err);

} // namespace tools::cli
```

- [ ] **Step 2: 写失败测试**

新建 `Tests/Integration/VaseCliPlanTests.cpp`。头注释 + include + 常量 + 11 条用例：

```cpp
// plan（M6/D141–D145/D154）：顺序与静态跳过、preset/HostProvided、三档退出码、零装载事实。
#include "Plan.h"

#include "Cli.h"

#include "CatalogSandbox.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

namespace
{

using testing_support::CatalogSandbox;

// plan 从不碰二进制（D145）——所以这些 manifest 连 binary 键都可以不写，
// 缺它时按子目录名（D54）拼一个 plan 侧永不会解析的路径，恰好是「零装载」的反证材料。
constexpr const char* kAlpha = R"({
    "schemaVersion": 1, "id": "Vase.Alpha", "displayName": "甲", "version": "0.0.1"
})";
constexpr const char* kBetaDisabled = R"({
    "schemaVersion": 1, "id": "Vase.Beta", "displayName": "乙", "version": "0.0.1",
    "enabledByDefault": false
})";
constexpr const char* kCycA = R"({
    "schemaVersion": 1, "id": "Vase.CycA", "displayName": "环A", "version": "0.0.1",
    "requires": [ { "service": "Vase.CycB.Svc", "version": 1 } ],
    "provides": [ { "service": "Vase.CycA.Svc", "version": 1 } ]
})";
constexpr const char* kCycB = R"({
    "schemaVersion": 1, "id": "Vase.CycB", "displayName": "环B", "version": "0.0.1",
    "requires": [ { "service": "Vase.CycA.Svc", "version": 1 } ],
    "provides": [ { "service": "Vase.CycB.Svc", "version": 1 } ]
})";

int Run(const CatalogSandbox& sandbox, std::ostringstream& out, std::ostringstream& err,
        const std::vector<std::string>& extra = {})
{
    std::vector<std::string> args{"plan", sandbox.Root.string()};
    for (const std::string& arg : extra)
    {
        args.push_back(arg);
    }
    return tools::cli::RunPlan(args, out, err);
}

TEST(VaseCliPlan, ListsLoadAndStaticSkipsInPlanOrder)
{
    const CatalogSandbox sandbox("plan-order");
    sandbox.WriteFile("Alpha/plugin.json", kAlpha);
    sandbox.WriteFile("Beta/plugin.json", kBetaDisabled);

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(Run(sandbox, out, err), tools::cli::kExitOk) << out.str() << err.str();
    EXPECT_NE(out.str().find("Vase.Alpha load"), std::string::npos);
    EXPECT_NE(out.str().find("Vase.Beta skip disabled"), std::string::npos);
    EXPECT_NE(out.str().find("plan: ok"), std::string::npos);
}

TEST(VaseCliPlan, PresetOverrideDisablesAPlugin)
{
    const CatalogSandbox sandbox("plan-preset");
    sandbox.WriteFile("Alpha/plugin.json", kAlpha);
    sandbox.WriteFile("Beta/plugin.json", R"({
        "schemaVersion": 1, "id": "Vase.Beta", "displayName": "乙", "version": "0.0.1"
    })");
    sandbox.WriteFile("preset.json", R"({
        "schemaVersion": 1, "displayName": "预览", "overrides": { "Vase.Beta": { "enabled": false } }
    })");

    std::ostringstream out;
    std::ostringstream err;
    const std::string preset = (sandbox.Root / "preset.json").string();
    EXPECT_EQ(Run(sandbox, out, err, {preset}), tools::cli::kExitOk) << out.str() << err.str();
    EXPECT_NE(out.str().find("Vase.Beta skip disabled"), std::string::npos);
    EXPECT_NE(out.str().find("(preset:"), std::string::npos); // 头部回显 preset 来源
}

TEST(VaseCliPlan, MissingDependencyIsAHardSkipAndFail)
{
    const CatalogSandbox sandbox("plan-missing");
    sandbox.CopyFile("Dep/plugin.json", std::filesystem::path{VASE_FIXTURE_MANIFESTS} / "dependent" / "plugin.json");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(Run(sandbox, out, err), tools::cli::kExitCheckFailed) << out.str();
    EXPECT_NE(out.str().find("Vase.Dependent skip missing-dependency"), std::string::npos);
    EXPECT_NE(out.str().find("plan: FAIL"), std::string::npos);
}

TEST(VaseCliPlan, HostProvidedTurnsTheHardSkipIntoLoad)
{
    const CatalogSandbox sandbox("plan-hostprov");
    sandbox.CopyFile("Dep/plugin.json", std::filesystem::path{VASE_FIXTURE_MANIFESTS} / "dependent" / "plugin.json");

    std::ostringstream out;
    std::ostringstream err;
    // 与 validate 同名证人 HostProvidedServicesSatisfyTheThirdCheck 同参——「同判」是 D154
    // 结构论证（同一个 Solve + 共用 argv），本用例只钉 plan 侧行为，不做跨命令断言。
    EXPECT_EQ(Run(sandbox, out, err, {"--host-provides", "Vase.Hello.Greeter@1"}), tools::cli::kExitOk) << out.str();
    EXPECT_NE(out.str().find("Vase.Dependent load"), std::string::npos);
}

TEST(VaseCliPlan, DependencyCycleIsFailWithSolveMessageVerbatim)
{
    const CatalogSandbox sandbox("plan-cycle");
    sandbox.WriteFile("A/plugin.json", kCycA);
    sandbox.WriteFile("B/plugin.json", kCycB);

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(Run(sandbox, out, err), tools::cli::kExitCheckFailed) << out.str();
    EXPECT_NE(out.str().find("cycle or blocked"), std::string::npos); // D52/D127：原文透传，不另造分类
}

TEST(VaseCliPlan, UnknownPresetIdIsNoteOnlyNotFailure)
{
    const CatalogSandbox sandbox("plan-note");
    sandbox.WriteFile("Alpha/plugin.json", kAlpha);
    sandbox.WriteFile("preset.json", R"({
        "schemaVersion": 1, "overrides": { "Vase.Ghost": { "enabled": true } }
    })");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(Run(sandbox, out, err, {(sandbox.Root / "preset.json").string()}), tools::cli::kExitOk) << out.str();
    EXPECT_NE(out.str().find("note: unknownPluginId Vase.Ghost"), std::string::npos);
}

TEST(VaseCliPlan, PlanNeverTouchesBinaries)
{
    const CatalogSandbox sandbox("plan-noload");
    sandbox.WriteFile("Ghost/plugin.json", R"({
        "schemaVersion": 1, "id": "Vase.Ghost", "displayName": "无中生有", "version": "0.0.1",
        "binary": "NoSuchBinary"
    })");

    std::ostringstream out;
    std::ostringstream err;
    // binary 指向磁盘上不存在的文件而 plan 照出计划——「零装载」(D145) 的直接证人。
    EXPECT_EQ(Run(sandbox, out, err), tools::cli::kExitOk) << out.str() << err.str();
    EXPECT_NE(out.str().find("Vase.Ghost load"), std::string::npos);
}

TEST(VaseCliPlan, GarbagePresetFileIsAUsageError)
{
    const CatalogSandbox sandbox("plan-badpreset");
    sandbox.WriteFile("Alpha/plugin.json", kAlpha);
    sandbox.WriteFile("preset.json", "not json at all");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(Run(sandbox, out, err, {(sandbox.Root / "preset.json").string()}), tools::cli::kExitUsage) << err.str();
}

TEST(VaseCliPlan, MissingDirectoryIsAUsageError)
{
    const CatalogSandbox sandbox("plan-nodir");
    std::ostringstream out;
    std::ostringstream err;
    const std::vector<std::string> args{"plan", (sandbox.Root / "absent").string()};
    EXPECT_EQ(tools::cli::RunPlan(args, out, err), tools::cli::kExitUsage) << err.str();
}

TEST(VaseCliPlan, EmptyTreeIsNotASilentSuccess)
{
    const CatalogSandbox sandbox("plan-empty");
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(Run(sandbox, out, err), tools::cli::kExitUsage) << out.str(); // D135 同一律
}

TEST(VaseCliPlan, DuplicateIdIsSnapshotFailNotUsage)
{
    const CatalogSandbox sandbox("plan-dup");
    sandbox.WriteFile("One/plugin.json", kAlpha);
    sandbox.WriteFile("Two/plugin.json", kAlpha); // 同 Id 两份 ⇒ Refresh Err（D50）

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(Run(sandbox, out, err), tools::cli::kExitCheckFailed) << out.str(); // D142：1 不是 2
    EXPECT_NE(out.str().find("snapshot not built"), std::string::npos);
}

} // namespace
```

- [ ] **Step 3: 跑测试确认编译失败**

```bash
cmake --build --preset win-x64-clang-debug --target VaseTests
```

Expected: `Plan.h` 不存在 / `RunPlan` 未声明。

- [ ] **Step 4: `Plan.cpp` 实现**

```cpp
#include "Plan.h"

#include "Cli.h"
#include "Vase/Catalog/LoadRequest.h"
#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Catalog/Preset.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/LoadPlan.h"

#include <cstddef>
#include <iterator>
#include <ostream>
#include <span>
#include <string>
#include <system_error>

namespace
{

// 与 Console 各持一份的第二副本（M6/D149）：字符串逐字同 Console.cpp:197-225——
// 「同一套规则」钉的是 Solve 判定，不是输出形状；改判定的义务在源侧，不在这两份文案。
const char* SkipReasonText(vase::SkipReason reason)
{
    switch (reason)
    {
    case vase::SkipReason::kDisabled:
        return "disabled";
    case vase::SkipReason::kMissingDependency:
        return "missing-dependency";
    case vase::SkipReason::kVersionMismatch:
        return "version-mismatch";
    }
    return "?";
}

const char* SolveNoteKindText(vase::SolveNoteKind kind)
{
    switch (kind)
    {
    case vase::SolveNoteKind::kUnknownPluginId:
        return "unknownPluginId";
    case vase::SolveNoteKind::kUnknownConfigKey:
        return "unknownConfigKey";
    case vase::SolveNoteKind::kProviderSkipped:
        return "providerSkipped";
    case vase::SolveNoteKind::kVersionMismatchProvider:
        return "versionMismatchProvider";
    }
    return "?";
}

} // namespace

namespace tools::cli
{

int PlanDirectory(const PlanOptions& options, std::ostream& out, std::ostream& err)
{
    // 环境档先于判定档（spec §2.1）：目录不存在 ⇒ 快照根本无从谈起，2。
    std::error_code ec;
    if (!std::filesystem::is_directory(options.PluginDirectory, ec))
    {
        err << "plan: not a directory: \"" << options.PluginDirectory.string() << "\"\n";
        return kExitUsage;
    }

    vase::LoadRequest request;
    if (options.PresetFile.has_value())
    {
        const vase::Result<vase::Preset> loaded = vase::LoadPreset(*options.PresetFile);
        if (!loaded.IsOk())
        {
            // preset 读不成 = 环境档（D142）；错误 Message 已带文件路径。
            err << "plan: " << loaded.GetError().Message() << '\n';
            return kExitUsage;
        }
        request.Preset = std::move(loaded.Value());
    }

    vase::PluginCatalog catalog;
    const vase::Result<void> refreshed = catalog.Refresh(options.PluginDirectory);
    if (!refreshed.IsOk())
    {
        // 快照未建成 = FAIL 档 1 不是环境档 2（D142，D130 同律）；没跑到的 Solve 明说没跑到。
        out << "plan: snapshot not built: " << refreshed.GetError().Message() << '\n';
        out << "snapshot not built: plan was not solved\n";
        return kExitCheckFailed;
    }

    if (catalog.Ids().empty())
    {
        out << "no plugins found in the snapshot\n"; // D135 第二档补角，validate 同位文案
        return kExitUsage;
    }

    request.HostProvided = std::span<const vase::ServiceRef>(options.HostProvided);
    const vase::Result<vase::SolveOutcome> solved = catalog.Solve(request);
    if (!solved.IsOk())
    {
        out << "plan: FAIL: " << solved.GetError().Message() << '\n'; // D127 原文透传同律
        return kExitCheckFailed;
    }

    const std::vector<vase::LoadPlanEntry>& ordered = solved.Value().Plan.Ordered;
    out << "plan: " << ordered.size() << " entries";
    if (options.PresetFile.has_value())
    {
        out << " (preset: \"" << options.PresetFile->string() << "\")";
    }
    out << '\n';

    std::size_t hardSkips = 0;
    std::size_t index = 1;
    for (const vase::LoadPlanEntry& entry : ordered)
    {
        if (entry.Decision == vase::LoadDecision::kLoad)
        {
            out << "  " << index << ". " << entry.Id << " load\n";
        }
        else
        {
            // 穷举 switch（无 default）：加枚举值时 -Wswitch 顶出来（Validate.cpp:227 先例）。
            switch (entry.Reason)
            {
            case vase::SkipReason::kDisabled:
                break; // 静态跳过不算未过（§4.4/D131）
            case vase::SkipReason::kMissingDependency:
                ++hardSkips;
                break;
            case vase::SkipReason::kVersionMismatch:
                ++hardSkips;
                break;
            }
            out << "  " << index << ". " << entry.Id << " skip " << SkipReasonText(entry.Reason) << '\n';
        }
        ++index;
    }
    for (const vase::SolveNote& note : solved.Value().Notes)
    {
        out << "  note: " << SolveNoteKindText(note.Kind) << " " << note.PluginId;
        if (!note.Key.empty())
        {
            out << " " << note.Key;
        }
        out << '\n';
    }

    if (hardSkips != 0U)
    {
        out << "plan: FAIL (" << hardSkips << (hardSkips == 1U ? " entry" : " entries")
            << " skipped with a hard reason)\n";
        return kExitCheckFailed;
    }
    out << "plan: ok\n";
    return kExitOk;
}

int RunPlan(const std::vector<std::string>& args, std::ostream& out, std::ostream& err)
{
    if (args.size() < 2U)
    {
        err << "usage: VaseCli plan <插件目录> [presetFile] [--host-provides <name>@<version>]...\n";
        return kExitUsage;
    }
    PlanOptions options;
    options.PluginDirectory = *std::next(args.begin(), 1);
    std::size_t firstFlag = 2;
    // spec §6 风险 5：arg[2] 以 `--` 起 ⇒ flag 段开始、presetFile 缺省。
    if (args.size() >= 3U && !std::next(args.begin(), 2)->starts_with("--"))
    {
        options.PresetFile = std::filesystem::path{*std::next(args.begin(), 2)};
        firstFlag = 3;
    }
    if (!ParseTrailingHostProvides(args, firstFlag, options.HostProvided,
                                   "usage: VaseCli plan <插件目录> [presetFile] [--host-provides <name>@<version>]...\n",
                                   err))
    {
        return kExitUsage;
    }
    return PlanDirectory(options, out, err);
}

} // namespace tools::cli
```

（Step 4 的 switch 即终形：穷举无 default，两个硬值各数一次——`[[fallthrough]]`+`default` 的混写会让 clang 不再顶新增枚举值，此处不留那条弯路。）

- [ ] **Step 5: 分发、usage、源表、spec 样例对齐**

`Tools/VaseCli/CMakeLists.txt` 源表变 `Cli.cpp Scan.cpp Validate.cpp ManifestMerge.cpp Plan.cpp`。`Cli.cpp`：`#include "Plan.h"`；`PrintUsage` 追加行 `       VaseCli plan <插件目录> [presetFile] [--host-provides <name>@<version>]...\n`；`Run` 里 `validate` 分支之后加：

```cpp
    if (command == "plan")
    {
        return RunPlan(args, out, err);
    }
```

`Tests/CMakeLists.txt` 在 `Integration/VaseCliValidateTests.cpp` 之后加 `Integration/VaseCliPlanTests.cpp`。

spec 样例随实定文案对齐（D149 抄 Console 的 camelCase，spec §2.2/§5.1 的示例行是示意文本）：`docs/superpowers/specs/2026-09-29-vase-m6-vasecli-plan-doctor-design.md` 中 `note: unknown-plugin-id Vase.Ghost`（两处）改 `note: unknownPluginId Vase.Ghost`。

- [ ] **Step 6: ctest 用法腿**

`Tools/VaseCli/CMakeLists.txt` 既有两对之后（WILL_FAIL 与 PASS_REGULAR_EXPRESSION 分设的坑注释在位，照形即得）：

```cmake
add_test(NAME VaseCliPlanNoArgsIsUsageError COMMAND VaseCli plan)
set_tests_properties(VaseCliPlanNoArgsIsUsageError PROPERTIES WILL_FAIL TRUE)
add_test(NAME VaseCliPlanNoArgsReason COMMAND VaseCli plan)
set_tests_properties(VaseCliPlanNoArgsReason PROPERTIES PASS_REGULAR_EXPRESSION "usage: VaseCli plan")
```

- [ ] **Step 7: 全量构建 + 测试**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R '^VaseCli' --output-on-failure
ctest --preset win-x64-clang-debug
```

Expected: `^VaseCli` 家族 24 + 11（gtest）+ 2（ctest 腿）= 37 全绿；全量零警告、全绿。

- [ ] **Step 8: 提交**

```bash
git add Tools/VaseCli Tests/CMakeLists.txt Tests/Integration/VaseCliPlanTests.cpp docs/superpowers/specs/2026-09-29-vase-m6-vasecli-plan-doctor-design.md
git commit -m "M6-T2：VaseCli plan 落地——零装载 Solve 预览 / 同一律三档退出码 / argv 与 validate 共用（D141–D145、D149、D154）"
```

---

### Task 3: `VaseCli doctor`（平台中立支）

**Files:**
- Create: `Tools/VaseCli/Doctor.h`、`Tools/VaseCli/Doctor.cpp`
- Modify: `Tools/VaseCli/Cli.cpp`（分发 + usage 行）、`Tools/VaseCli/CMakeLists.txt`（源表 + doctor 腿）
- Create: `Tests/Integration/VaseCliDoctorTests.cpp`
- Modify: `Tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `Loader::FileIdentity(path)`（static、磁盘读）、`Loader::EnsureResident/Symbol/Unload` + `InspectDescriptors`（装载读）、`PluginCatalog::Refresh/Ids/Find/Directory/Warnings`、`catalog_detail::LibraryFileName`、`vase::kHeaderVersion`。
- Produces: `int RunDoctor(const std::vector<std::string>& args, std::ostream& out, std::ostream& err)`；`int DoctorDirectory(const std::filesystem::path& pluginDirectory, std::ostream& out, std::ostream& err)`。

- [ ] **Step 1: `Doctor.h`**

```cpp
#pragma once

// doctor（M6/D143/D144/D152）：收窄四项环境诊断。收集序 ①③④→②（② 唯一装载步置后），
// 输出按 check 1–4 固定序。

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace tools::cli
{

int RunDoctor(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

// 供用例直调：跳过 argv 解析。退出码同一律三档（spec §3.5）。
int DoctorDirectory(const std::filesystem::path& pluginDirectory, std::ostream& out, std::ostream& err);

} // namespace tools::cli
```

- [ ] **Step 2: 写失败测试（平台中立 6 条）**

新建 `Tests/Integration/VaseCliDoctorTests.cpp`：

```cpp
// doctor 收窄四项（M6/D143–D155）：①磁盘读身份、②装载读版号（D152 置后）、③写探针、④平台锁。
// doctor 只判环境事实，不比对清单↔描述符（D155）——manifest 的 Id 与描述符不同不报错，是特性不是漏。
#include "Doctor.h"

#include "Cli.h"
#include "Vase/Catalog/LibraryFileName.h"

#include "CatalogSandbox.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

namespace
{

using testing_support::CatalogSandbox;

void Stage(const CatalogSandbox& sandbox, const std::string& subdirectory, const std::string& stem,
           const char* fixturePath, const std::string& id)
{
    sandbox.CopyFile(subdirectory + "/" + vase::catalog_detail::LibraryFileName(stem), fixturePath);
    sandbox.WriteFile(subdirectory + "/plugin.json", std::string{"{\n    \"schemaVersion\": 1, \"id\": \"" + id +
                                                                 "\", \"displayName\": \"诊断材料\", \"version\": "
                                                                 "\"0.0.1\", \"binary\": \"" + stem + "\"\n}"});
}

TEST(VaseCliDoctor, HealthyTreePassesAllChecks)
{
    const CatalogSandbox sandbox("doctor-ok");
    Stage(sandbox, "Probe", "LoadProbe", VASE_FIXTURE_LOADPROBE, "Vase.Probe");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitOk) << out.str() << err.str();
    EXPECT_NE(out.str().find("Vase.Probe: identity ok"), std::string::npos);
    EXPECT_NE(out.str().find("host header version"), std::string::npos); // D153 的节头 info
    EXPECT_NE(out.str().find("HeaderVersion matches (1 descriptor(s))"), std::string::npos);
    EXPECT_NE(out.str().find("doctor: ok"), std::string::npos);
}

TEST(VaseCliDoctor, MissingIdentityFeatureFailsCheckOne)
{
    const CatalogSandbox sandbox("doctor-noident");
    // NoIdentityPlugin 两平台各摘各的身份特征（M4/D108）——①的负例因此两侧同构成立。
    Stage(sandbox, "NoId", "NoIdentityPlugin", VASE_FIXTURE_NOIDENTITY, "Vase.NoId");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitCheckFailed) << out.str();
    EXPECT_NE(out.str().find("check 1"), std::string::npos);
    EXPECT_NE(out.str().find("Vase.NoId"), std::string::npos);
    EXPECT_NE(out.str().find("tier-3 adopt would reject"), std::string::npos); // 后果指针（D143①）
    EXPECT_NE(out.str().find("Vase.NoId: HeaderVersion matches"), std::string::npos); // ②照常——两项独立
}

TEST(VaseCliDoctor, MissingEnumerationEntryIsNamedByCheckTwo)
{
    const CatalogSandbox sandbox("doctor-noenum");
    Stage(sandbox, "NoEnum", "NoEnumeration", VASE_FIXTURE_NOENUMERATION, "Vase.NoEnum");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitCheckFailed) << out.str();
    EXPECT_NE(out.str().find("no VasePlugin_Descriptors entry"), std::string::npos); // D118 文本零新造
}

TEST(VaseCliDoctor, NewerHeaderVersionIsNamedByCheckTwo)
{
    const CatalogSandbox sandbox("doctor-stale");
    Stage(sandbox, "Stale", "StaleEnum", VASE_FIXTURE_STALEENUM, "Vase.Stale"); // 唯一变量 k+1（D123 探针）

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitCheckFailed) << out.str();
    EXPECT_NE(out.str().find("HeaderVersion mismatch: binary 5, host 4"), std::string::npos);
}

TEST(VaseCliDoctor, UnloadableBinaryIsNamedByCheckTwo)
{
    const CatalogSandbox sandbox("doctor-badload");
    Stage(sandbox, "BadLoad", "FailingLoadPlugin", VASE_FIXTURE_FAILINGLOAD, "Vase.BadLoad");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitCheckFailed) << out.str();
    EXPECT_NE(out.str().find("failed to load binary"), std::string::npos); // D153：可装载性归 ②
    EXPECT_NE(out.str().find("Vase.BadLoad: identity ok"), std::string::npos); // ①独立：文件在、特征在
}

TEST(VaseCliDoctor, TwoFailuresAreReportedIndependently)
{
    const CatalogSandbox sandbox("doctor-two");
    Stage(sandbox, "NoId", "NoIdentityPlugin", VASE_FIXTURE_NOIDENTITY, "Vase.NoId");
    Stage(sandbox, "Stale", "StaleEnum", VASE_FIXTURE_STALEENUM, "Vase.Stale");

    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitCheckFailed) << out.str();
    EXPECT_NE(out.str().find("doctor: FAIL (checks 1 and 2 reported failures)"), std::string::npos); // 不遇错即停
}

} // namespace
```

（include 面以文件实际用到为准：这里不需要 `Scan.h`——validate 测试带它是另一段故事的遗留形，include-cleaner 会顶。）

- [ ] **Step 3: 跑测试确认编译失败**

```bash
cmake --build --preset win-x64-clang-debug --target VaseTests
```

Expected: `Doctor.h` 不存在。

- [ ] **Step 4: `Doctor.cpp` 实现**

```cpp
#include "Doctor.h"

#include "Cli.h"
#include "Vase/Catalog/LibraryFileName.h"
#include "Vase/Catalog/ManifestView.h"
#include "Vase/Catalog/PluginCatalog.h"
#include "Vase/Detail/Result.h"
#include "Vase/Host/Inspect.h"
#include "Vase/Host/Loader.h"
#include "Vase/PluginDescriptor.h"

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <ostream>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{

struct Finding
{
    std::vector<std::string> Lines;
    std::size_t Failures = 0;
};

std::filesystem::path BinaryOf(const vase::PluginCatalog& catalog, const vase::ManifestEntry& entry)
{
    return catalog.Directory() / entry.Subdirectory / vase::catalog_detail::LibraryFileName(entry.Binary);
}

// ③ 写探针：创建→关→删；删除失败也算 FAIL（D150——探测不得留下不可见副作用）。
bool ProbeWrite(const std::filesystem::path& dir, std::string& detail)
{
    const auto ns = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::filesystem::path probe = dir / (".vase-probe-" + std::to_string(ns) + ".tmp");
    std::error_code ec;
    {
        std::ofstream stream(probe, std::ios::binary | std::ios::trunc);
        if (!stream.is_open())
        {
            // ofstream 失败不带 ec；errno 是唯一现场线索（两平台底层都是 open 族调用）。
            const std::error_code probeEc(errno, std::generic_category());
            detail = "cannot create \"" + probe.string() + "\": " + probeEc.message();
            return false;
        }
        stream.put('x');
    }
    std::filesystem::remove(probe, ec);
    if (ec)
    {
        detail = "probe file left behind: \"" + probe.string() + "\": " + ec.message();
        return false;
    }
    return true;
}

// ④ Windows 支：独占打开探针（形状照 Loader 的 PlatformReopenWritable，住工具层——D148）。
// nullopt = 未被锁（或文件不在——归①执法，不双报）；字符串 = 锁/探测发现。
std::optional<std::string> ProbeExclusiveOpen(const std::filesystem::path& binary)
{
#ifdef _WIN32
    const HANDLE handle = ::CreateFileW(binary.c_str(), GENERIC_READ, /*dwShareMode=*/0, nullptr, OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(handle);
        return std::nullopt;
    }
    const DWORD lastError = ::GetLastError();
    if (lastError == ERROR_FILE_NOT_FOUND || lastError == ERROR_PATH_NOT_FOUND)
    {
        return std::nullopt;
    }
    if (lastError == ERROR_SHARING_VIOLATION)
    {
        return std::string{"locked — sharing violation (a previous unload may have kept the image resident)"};
    }
    return std::string{"probe failed (Win32 error "} + std::to_string(static_cast<unsigned long>(lastError)) + ")";
#else
    static_cast<void>(binary);
    return std::nullopt; // posix：不逐文件测（④整项一行 info，见调用点）
#endif
}

void JoinVerdict(const std::vector<Finding>& checks, std::ostream& out)
{
    std::vector<std::size_t> failed;
    for (std::size_t index = 0; index < checks.size(); ++index)
    {
        if (checks[index].Failures != 0U)
        {
            failed.push_back(index + 1U);
        }
    }
    if (failed.empty())
    {
        out << "doctor: ok\n";
        return;
    }
    out << "doctor: FAIL (checks ";
    for (std::size_t pos = 0; pos < failed.size(); ++pos)
    {
        out << (pos == 0U ? "" : (pos + 1U == failed.size() ? " and " : ", ")) << failed[pos];
    }
    out << " reported failures)\n"; // 文本与 spec §3.5 样例、D6 用例的 find() 三处逐字同形
}

} // namespace

namespace tools::cli
{

int DoctorDirectory(const std::filesystem::path& pluginDirectory, std::ostream& out, std::ostream& err)
{
    std::error_code ec;
    if (!std::filesystem::is_directory(pluginDirectory, ec))
    {
        err << "doctor: not a directory: \"" << pluginDirectory.string() << "\"\n";
        return kExitUsage;
    }

    vase::PluginCatalog catalog;
    const vase::Result<void> refreshed = catalog.Refresh(pluginDirectory);
    if (!refreshed.IsOk())
    {
        // 快照未建成 = FAIL 档（D144，D130 同律）；四项明说没跑。
        out << "doctor: snapshot not built: " << refreshed.GetError().Message() << '\n';
        out << "snapshot not built: checks 1-4 were not run\n";
        return kExitCheckFailed;
    }
    for (const vase::CatalogWarning& warning : catalog.Warnings())
    {
        out << "  warning: " << warning.Subdirectory << ": " << warning.Message << '\n'; // D128 info
    }
    if (catalog.Ids().empty())
    {
        out << "no plugins found in the snapshot\n";
        return kExitUsage; // D144：环境档补角，D135 同一律
    }

    out << "doctor: " << catalog.Directory().string() << '\n';
    std::vector<Finding> checks(4U);

    // —— ① 身份特征在场（磁盘读，零装载）。
    for (const std::string& id : catalog.Ids())
    {
        const vase::ManifestEntry* entry = catalog.Find(id);
        const vase::Result<vase::detail::ImageIdentity> identity = vase::detail::Loader::FileIdentity(BinaryOf(catalog, *entry));
        if (identity.IsOk())
        {
            checks[0].Lines.push_back("  " + id + ": identity ok");
        }
        else
        {
            checks[0].Lines.push_back("  " + id + ": FAIL — " + identity.GetError().Message() +
                                      " (tier-3 adopt would reject this binary)");
            ++checks[0].Failures;
        }
    }

    // —— ③ 目录可写（根 + 各插件子目录）。
    {
        std::string detail;
        if (ProbeWrite(catalog.Directory(), detail))
        {
            checks[2].Lines.push_back("  .: ok");
        }
        else
        {
            checks[2].Lines.push_back("  .: FAIL — " + detail);
            ++checks[2].Failures;
        }
    }
    for (const std::string& id : catalog.Ids())
    {
        const vase::ManifestEntry* entry = catalog.Find(id);
        std::string detail;
        const std::filesystem::path sub = catalog.Directory() / entry->Subdirectory;
        if (ProbeWrite(sub, detail))
        {
            checks[2].Lines.push_back("  " + entry->Subdirectory + ": ok");
        }
        else
        {
            checks[2].Lines.push_back("  " + entry->Subdirectory + ": FAIL — " + detail);
            ++checks[2].Failures;
        }
    }

    // —— ④ 残留文件锁（在 ② 装载之前跑——D152 的免疫构造）。
#ifdef _WIN32
    for (const std::string& id : catalog.Ids())
    {
        const vase::ManifestEntry* entry = catalog.Find(id);
        const std::optional<std::string> locked = ProbeExclusiveOpen(BinaryOf(catalog, *entry));
        if (locked.has_value())
        {
            checks[3].Lines.push_back("  " + id + ": FAIL — " + *locked);
            ++checks[3].Failures;
        }
        else
        {
            checks[3].Lines.push_back("  " + id + ": unlocked");
        }
    }
#else
    checks[3].Lines.push_back(
        "  lock probe: not observable on this platform — a resident mapping does not prevent overwrites here");
#endif

    // —— ② 装载读（全工具唯一装载步，置后——D152；D117 的代价由它单独承担）。
    checks[1].Lines.push_back("  host header version: " + std::to_string(vase::kHeaderVersion));
    vase::detail::Loader loader;
    for (const std::string& id : catalog.Ids())
    {
        const vase::ManifestEntry* entry = catalog.Find(id);
        const vase::Result<vase::detail::BinaryRecord*> record = loader.EnsureResident(BinaryOf(catalog, *entry));
        if (!record.IsOk())
        {
            checks[1].Lines.push_back("  " + id + ": FAIL (" + record.GetError().Message() + ")");
            ++checks[1].Failures;
            continue;
        }
        const vase::Result<std::vector<const vase::PluginDescriptor*>> descriptors =
            vase::InspectDescriptors(*record.Value());
        if (descriptors.IsOk())
        {
            checks[1].Lines.push_back("  " + id + ": HeaderVersion matches (" + std::to_string(descriptors.Value().size()) +
                                      " descriptor(s))"); // D151：计数是 info，不假设 1
        }
        else
        {
            checks[1].Lines.push_back("  " + id + ": FAIL (" + descriptors.GetError().Message() + ")");
            ++checks[1].Failures;
        }
        loader.Unload(*record.Value()); // 借用止于此（Validate.cpp 同款）
    }

    // —— 固定序输出（收集序 ≠ 输出序，D152）。
    const char* titles[4] = {"check 1 (binary identity features):",
                             "check 2 (load & header version):",
                             "check 3 (directory writability):",
                             "check 4 (file locks):"};
    for (std::size_t index = 0; index < 4U; ++index)
    {
        out << titles[index] << '\n';
        for (const std::string& line : checks[index].Lines)
        {
            out << line << '\n';
        }
    }
    JoinVerdict(checks, out);
    return checks[0].Failures + checks[1].Failures + checks[2].Failures + checks[3].Failures != 0U ? kExitCheckFailed
                                                                                                    : kExitOk;
}

int RunDoctor(const std::vector<std::string>& args, std::ostream& out, std::ostream& err)
{
    if (args.size() != 2U)
    {
        err << "usage: VaseCli doctor <插件目录>\n";
        return kExitUsage;
    }
    return DoctorDirectory(std::filesystem::path{*std::next(args.begin(), 1)}, out, err);
}

} // namespace tools::cli
```

实现期三处自查点（写在任务里，不留口头）：`FileIdentity` 返回类型的确切拼写以 `Include/Vase/Host/Loader.h` 为准（`ImageIdentity` 在 `vase::detail::` 还是 `vase::` 下——照 `Loader.cpp:97-102` 的签名抄）；`CatalogWarning`/`ManifestEntry` 字段名照磁盘；**四个遍历里的 `catalog.Find(id)` 各补一枚防御性 nullptr 跳过**（`Validate.cpp:148-152` 同款一行注释「Ids() 与 Find() 同源，取不到是不可能的」——照抄，别自创第二形态）。

- [ ] **Step 5: 接线**

`CMakeLists.txt` 源表加 `Doctor.cpp`；`Cli.cpp` 加 `#include "Doctor.h"`、usage 行 `       VaseCli doctor <插件目录>\n`、分发：

```cpp
    if (command == "doctor")
    {
        return RunDoctor(args, out, err);
    }
```

`Tests/CMakeLists.txt` 加 `Integration/VaseCliDoctorTests.cpp`。ctest 腿（Tools 的 CMakeLists，plan 腿之后）：

```cmake
add_test(NAME VaseCliDoctorNoArgsIsUsageError COMMAND VaseCli doctor)
set_tests_properties(VaseCliDoctorNoArgsIsUsageError PROPERTIES WILL_FAIL TRUE)
add_test(NAME VaseCliDoctorNoArgsReason COMMAND VaseCli doctor)
set_tests_properties(VaseCliDoctorNoArgsReason PROPERTIES PASS_REGULAR_EXPRESSION "usage: VaseCli doctor")
add_test(NAME VaseCliDoctorTooManyArgsIsUsageError COMMAND VaseCli doctor a b)
set_tests_properties(VaseCliDoctorTooManyArgsIsUsageError PROPERTIES WILL_FAIL TRUE)
add_test(NAME VaseCliDoctorTooManyArgsReason COMMAND VaseCli doctor a b)
set_tests_properties(VaseCliDoctorTooManyArgsReason PROPERTIES PASS_REGULAR_EXPRESSION "usage: VaseCli doctor")
```

- [ ] **Step 6: 全量构建 + 测试**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R '^VaseCli' --output-on-failure
ctest --preset win-x64-clang-debug
```

Expected: Windows 线上 `^VaseCli` = 37 + 6（doctor gtest）+ 4（腿）= 47 全绿；全量零警告全绿。

- [ ] **Step 7: 提交**

```bash
git add Tools/VaseCli Tests/CMakeLists.txt Tests/Integration/VaseCliDoctorTests.cpp
git commit -m "M6-T3：VaseCli doctor 落地——收窄四项 ①磁盘读身份 ②装载读版号置后 ③写探针 ④平台锁（D143/D144/D152/D153）"
```

---

### Task 4: 平台证人 + 两条实施期首测（spec §6 风险 1/2）

**Files:**
- Modify: `Tests/Integration/VaseCliDoctorTests.cpp`（加 2 条体内 `#ifdef` 的用例 + 探针 include）

**Interfaces:**
- Consumes: T3 的 `DoctorDirectory`。
- Produces: 无（测试收口）。

**首测协议**：下面两条用例本身就是风险 1/2 的探针。**先跑、失败即按处置走，不静默改判据**。若 Windows 行为与假设不符（自持句柄不触发冲突 / delete-pending 目录仍可写）：把该用例的 Windows 支改成 `GTEST_SKIP() << "platform probe negative: D152/D143④ witness degraded to contract-only"`，并在 plan 偏离登记记一笔 + 随 T5 把实情写进 CLAUDE.md（不碰 `Doctor.cpp` 的实现——判据本身两平台都真，缺的只是自动化证人）。

- [ ] **Step 1: 加两条用例**

include 区补（平台守卫）：

```cpp
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h> // geteuid：root 跑测试时 DAC 探针不咬，如实跳过（风险 2 处置）
#endif
```

用例（接在 `TwoFailuresAreReportedIndependently` 之后）：

```cpp
TEST(VaseCliDoctor, ResidentMappingProbeIsPlatformAsymmetric)
{
    const CatalogSandbox sandbox("doctor-lock");
    Stage(sandbox, "Probe", "LoadProbe", VASE_FIXTURE_LOADPROBE, "Vase.Probe");
#ifdef _WIN32
    // 现象保真：④ 要抓的就是「镜像仍映射着」（上一轮卸载没干净）。若改以 share=0 自持句柄，
    // ① 的磁盘读与 ② 的装载也会被同一句柄锁住——三项齐红、归因糊掉。LoadLibraryW 才是对位现象：
    // 映射允许 share-read（①② 照常），只有 ④ 的独占打开撞 sharing-violation。
    // 首测（spec §6 风险 1）：同进程驻留映射是否触发 sharing-violation——预期为「是」。
    // 若不成立 → 按任务头协议 GTEST_SKIP + 偏离登记，不改成永真。
    const std::filesystem::path binary = sandbox.Root / "Probe" / vase::catalog_detail::LibraryFileName("LoadProbe");
    const HMODULE held = ::LoadLibraryW(binary.c_str());
    ASSERT_NE(held, nullptr) << "LoadLibrary setup failed — environment surprise, not a verdict";
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitCheckFailed) << out.str();
    EXPECT_NE(out.str().find("doctor: FAIL (checks 4 reported failures)"), std::string::npos); // 单点归因：只有 ④
    EXPECT_NE(out.str().find("locked — sharing violation"), std::string::npos);
    ::FreeLibrary(held); // 尽力还原；Windows 下 kept-resident 时 temp 残留归 sandbox 的尽力清理
#else
    // 正半句：Linux 上锁不可探测——info 行逐字钉、且**不拖退出码**（D144④）。
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitOk) << out.str() << err.str();
    EXPECT_NE(out.str().find("lock probe: not observable on this platform"), std::string::npos);
#endif
}

TEST(VaseCliDoctor, UnwritableSubdirectoryFailsCheckThree)
{
    const CatalogSandbox sandbox("doctor-writable");
    Stage(sandbox, "Probe", "LoadProbe", VASE_FIXTURE_LOADPROBE, "Vase.Probe");
    const std::filesystem::path sub = sandbox.Root / "Probe";
#ifdef _WIN32
    // 首测（spec §6 风险 2）：DELETE_ON_CLOSE + BACKUP_SEMANTICS 持目录句柄 ⇒ delete-pending ⇒
    // 其中创建新文件失败。若不成立 → 协议处理（Linux 支仍是证人，账面如实记）。
    const HANDLE held = ::CreateFileW(sub.c_str(), DELETE, /*dwShareMode=*/0, nullptr, OPEN_EXISTING,
                                      FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    ASSERT_NE(held, INVALID_HANDLE_VALUE) << "delete-pending setup failed — environment surprise, not a verdict";
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitCheckFailed) << out.str();
    EXPECT_NE(out.str().find("check 3"), std::string::npos);
    EXPECT_NE(out.str().find("Probe: FAIL"), std::string::npos);
    ::CloseHandle(held); // delete-pending 随句柄关闭真删——沙箱析构的 remove_all 扑空是常态（尽力清理）
#else
    if (geteuid() == 0)
    {
        GTEST_SKIP() << "running as root: DAC permissions would not bind the probe";
    }
    std::error_code ec;
    std::filesystem::permissions(sub,
                                 std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec |
                                     std::filesystem::perms::group_read | std::filesystem::perms::group_exec |
                                     std::filesystem::perms::others_read | std::filesystem::perms::others_exec,
                                 std::filesystem::perm_options::replace, ec);
    ASSERT_FALSE(ec) << "chmod setup failed: " << ec.message();
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(tools::cli::DoctorDirectory(sandbox.Root, out, err), tools::cli::kExitCheckFailed) << out.str();
    EXPECT_NE(out.str().find("check 3"), std::string::npos);
    EXPECT_NE(out.str().find("Probe: FAIL"), std::string::npos);
    std::filesystem::permissions(sub, std::filesystem::perms::owner_write, std::filesystem::perm_options::add, ec);
    ASSERT_FALSE(ec) << "permission restore failed: " << ec.message(); // 不恢复会绊住沙箱析构与后续跑
#endif
}
```

- [ ] **Step 2: Windows 线跑**

```bash
cmake --build --preset win-x64-clang-debug --target VaseTests
ctest --preset win-x64-clang-debug -R '^VaseCliDoctor' --output-on-failure
```

Expected: **12 注册**（T3 的 6 gtest + T3 的 4 条 argv 腿 + 本任务的 2 gtest——原文「8 条」为 T3 前旧口径，T5 以实读校正）；Windows 侧终形 11 过 + 1 SKIP（风险 2 的 P2 支，见偏离登记）。**红了先按协议判「平台事实」还是「实现缺陷」**——前者走 SKIP+登记，后者改实现。

- [ ] **Step 3: Linux 线跑（WSL）**

```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --preset linux-x64-clang-debug && cmake --build --preset linux-x64-clang-debug'
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && ctest --preset linux-x64-clang-debug -R "^VaseCliDoctor" --output-on-failure'
```

（`-R "^VaseCliDoctor"` 里没有 `$`，双引号安全；仍建议先落脚本再跑的仓库教训照旧。）Expected: **12 条全过**（原文「8 条」为 T3 前旧口径，T5 以实读校正；两平台同数——体内 `#ifdef` 的用例保持计数对称，规矩 6 的平台差只在判据内容不在计数——Linux 侧两条新用例走真实证人支、无 SKIP）。

- [ ] **Step 4: 提交**

```bash
git add Tests/Integration/VaseCliDoctorTests.cpp
git commit -m "M6-T4：doctor ③④ 的平台证人（体内 #ifdef 计数对称）+ 风险 1/2 实施期首测协议入账"
```

---

### Task 5: 收口波——六线全量、tidy、format、基数落账与文书

**Files:**
- Modify: `CLAUDE.md`（项目状态 M6 段、两张表、四命令措辞、清单工具条目四条腿）
- Modify: `wiki/vase-architecture.md`（§11.1 M6 现状注、§12.3 M5 行指针）

**Interfaces:**
- Consumes: T1–T4 的全部产物。
- Produces: 账面与文档；本波收官。

- [ ] **Step 1: 六线删树重配全量**

```bash
Scripts/win-verify.cmd                                   # 四条 Windows 线
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && bash Scripts/linux-verify.sh'
```

并行后台跑，各自出退出码汇总。**撞 `z-applocal` 文件锁假红**（历史五波撞四次）：按协议单线删树重跑一次，不查代码。
预期：六线全绿零警告。新增 = plan gtest 11 + doctor gtest 8（共享 6 + 平台体内 2）+ ctest 腿 6（plan 2 + doctor 4）= **25 条、六线同幅**（无 `#ifndef NDEBUG` 门、无平台门——体内 `#ifdef` 的两条同名各注册，计数对称）。debug 线 305 → **预期 330**、release **329**（T3 death-test 那道 −1 门不动）。**读实数与预期对不上先归因再落账**。
族计数：`-R '^VaseCli'` = gtest 39（scan 9 + validate 11 + plan 11 + doctor 8）+ ctest 10（既有 4 + 新 6）= **预期 49**；`-R 'HotSwap|Eject|Adopt'` **预期维持 58/58**（新命令名不含选择子串）。以实测为准。

- [ ] **Step 2: 三条 debug 线 tidy（全量重跑）**

```bash
Scripts/win-clang-tidy.cmd clangcl
Scripts/win-clang-tidy.cmd msvc
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && bash Scripts/linux-clang-tidy.sh'
```

Expected: 三线各 115 TU（111 + `Plan.cpp`/`Doctor.cpp`/两个测试文件；`windows.h`/`unistd.h` 的平台分支各归各线）；三判据全过、正文双 0（首跑捕到正文 warning 就代码级修掉复跑）。抑制合计与单 TU 极值读数入账；新代码若需 NOLINT，就地写理由并数 canonical 位点（预期零位点——探针函数无平台违规形态；`errno` 与 `<windows.h>` 若触发 include-cleaner 属**新代码位点**，如实记）。

- [ ] **Step 3: format 门禁 + 技能自检**

```bash
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' \
  | xargs -0 clang-format --dry-run --Werror
grep -rn "Total Tests\|[0-9][0-9] TU\|DEBUG:FULL\|build-id=sha1\|EHs-c-\|HAS_EXCEPTIONS\|--cached --others\|WarningsAsErrors\|PRE_TEST\|ctest --preset\|run-clang-tidy -p\|cmake --build --preset" \
  .claude/skills/vase-cpp-engineering/ | grep -v cpp-core-guidelines.md
```

Expected: format 零 violation（新文件全在 `git ls-files` 内——T4 前确认已 add）；grep 命中逐条判规矩 7 的类别（本波预期无新增违规面）。

- [ ] **Step 4: CLAUDE.md 落账**

1. 「项目状态」标题行追加 **M6 完成**，其下加 M6 段（形如 M5 段：spec/plan 路径、D141–D155 一句话清单、四命令同一律、`plan` 零装载、`doctor` 收窄四项 + D147 记名新账、六线/tidy 实测与日期）。
2. 「清单工具」条目：「两条腿」→「四条腿」，补 `plan <插件目录> [presetFile] [--host-provides …]` 与 `doctor <插件目录>` 一行各自；「两命令共享同一套退出码」改「四命令共享同一套」（全文该措辞两处，均随改）。
3. 基数表四行：debug 线 Total Tests 新值、`与 Win debug 的差` 列不动（形状不变）；族计数句（`^VaseCli`、`HotSwap|Eject|Adopt`、console 33）按实读数写。
4. tidy 表与抑制/极值读数按实写；「本波新增代码 NOLINT 位点」如实记（预期 0）。
5. 假红协议段：追加本波是否撞 `z-applocal`（撞/没撞都记名）。

- [ ] **Step 5: wiki 文书**

§11.1 命令块下方（M5 波 1 现状注之后）挂 M6 注：`plan`/`doctor` 已落（spec `docs/superpowers/specs/2026-09-29-vase-m6-vasecli-plan-doctor-design.md`，D141–D155）；`plan` 采 `<插件目录> [presetFile] [--host-provides]` 形（提案原文 `plan <preset>` 缺目录，勘误入账）；`doctor` 四项收窄定形 + 编译器/版本/CRT 子项记账（D147）；**§11.1 的四条子命令至此全部落地**；D125 后置步骤指针不动。§12.3 M5 行的「`plan` / `doctor` 归后续波次」句补「（已于 M6 落地，2026-09-29）」。

- [ ] **Step 6: 提交并宣告收官**

```bash
git add CLAUDE.md wiki/vase-architecture.md
git commit -m "M6-T5：收口——六线/tidy/format 复测与基数落账，四命令同一律入账，wiki §11.1 四条子命令全部落地"
```

宣告完成前的最后一眼（本仓惯例）：全量 `ctest` 六线绿 + `ctest -N` 六线对上落账表 + tidy 三线三判据 + format 零违规 + 上面 grep 的命中逐条归类。**任何一项没跑到，如实说没跑到。**

---

## 偏离登记

（执行期回填：首测协议若触发降级、实现与本计划代码块的任何实质出入、新增 NOLINT 位点，都记在此处并随收口波同步进 CLAUDE.md/skill 的口径。）

- **T3 · ②「可装载性」证人素材换**：spec §5.1 证人表 `FailingLoadPlugin → load-failure 点名` 不成立——
  它坏在 OnLoad，工具层装载读（EnsureResident+InspectDescriptors）走不到，其 DLL 平台装载正常（实测 doctor 全绿）。
  `UnloadableBinaryIsNamedByCheckTwo` 素材改为真实 LoadProbe 字节翻 machine 字段（PE→I386 / ELF→EM_386，唯一变量；
  两侧身份解析器不读该字段，`ImageInspectCommon` 源码实证），三条断言逐字保留；win/linux 各自实测过
  EnsureResident-Err 分支。**T5 须随收口勘误 spec §5.1 该行**（素材与用例名不变，素材来源变）。
- **T3 · 新增 NOLINT 位点 2 处**：`Doctor.cpp` 两个 `misc-include-cleaner` 区（windows.h 伞形头 include 区与
  ProbeExclusiveOpen 函数体），同 `Source/Host/LoaderWindows.cpp` 已定形处理——canonical 推导留在那份头部注释，
  此处一句带过。
- **T4 · 风险 1 首测证伪 → 裁定改探针请求形态，证人复活（T4b 收口）**：Windows 11 24H2 实测——同进程
  `LoadLibraryW` 驻留映射（句柄有效）期间，④ 原形 `CreateFileW(GENERIC_READ, share=0)` **成功**（gle=0），
  同文件 `GENERIC_WRITE+share0` 才失败（gle=32 ERROR_SHARING_VIOLATION，与规矩 6 里 `copy_file(overwrite)`
  的 Windows 判据同机制）。**证伪的对象是「读开能看见映射」这一请求形态假设，不是平台可探测性本身**——
  T4a 首轮按任务头协议 SKIP 降级并登记；需求方裁定（2026-09-30）：④ 的契约语义=「换件链下一步能否落新字节」，
  是写问题，与 `PlatformReopenWritable`/`copy_file` 同律——**T4b 已落地**：`ProbeExclusiveOpen` 的
  `GENERIC_READ` 改 `GENERIC_WRITE`（share=0、`OPEN_EXISTING`、锁文案、NOT_FOUND/PATH 支均不变，
  就地注释一行记 gle=0/32 对照），`ResidentMappingProbeIsPlatformAsymmetric` 的 Windows 支撤 SKIP、
  恢复真断言（驻留映射 ⇒ `kExitCheckFailed` + 单点归因 `doctor: FAIL (checks 4 reported failures)` +
  `locked — sharing violation`，FreeLibrary 收尾）——**win 线实测真绿，④ 的头号场景自此有自动化证人**；
  Linux 支（info 行逐字钉 + `kExitOk`，D144④）不变。原「候选改形 reopen-writable 形」悬案就此收掉。
  **FAIL 面自写开后变宽（T5 收口补记）**：sharing-violation 之外的 open 失败——只读属性文件、ACL 拒写等——现落
  `probe failed (Win32 error 5)` 这类 FAIL（旧读开形判 unlocked）；该 generic-error 分支**无自动化证人**（P1 只钉
  sharing-violation 支）。**裁定明记接受的假阴性类**：④ 写开看不见「允许写但拒绝读」型持有者（映射文件自身
  读开 gle=0 已实证此类持有者存在）——与 install 语义一致（该类文件 `copy_file` 照样落字节，不构成换件障碍），
  收口叙述随 CLAUDE.md M6 段带此限。
- **T4b · 测试文件新增 NOLINT 位点 2 处**：`VaseCliDoctorTests.cpp` 两个 `misc-include-cleaner` 区
  （windows.h include 区含尾行 `#undef CopyFile`——拆 winbase.h 宏与 `CatalogSandbox::CopyFile` 的撞名；
  与 P1 支 LoadLibraryW/FreeLibrary 使用区），同 `Doctor.cpp`/`LoaderWindows.cpp` 已定形的伞形头处理，
  canonical 推导在 Doctor.cpp 区注释，此处一句带过。
- **T4 · 风险 2 首测 Windows 支证伪（delete-pending 目录仍可写）**：同机实测——`CreateFileW(dir, DELETE,
  share=0, FILE_FLAG_DELETE_ON_CLOSE|FILE_FLAG_BACKUP_SEMANTICS)` 句柄有效且关闭后目录真删（DELETE_ON_CLOSE
  确已生效）期间，③ 的 `ofstream` 建探针文件**成功**、探针文件删除**成功**（doctor 报 "Probe: ok"）——
  delete-pending 不挡目录内新建，假设的「目录不可写 ⇒ ③ FAIL」在本机不成立，属**平台事实**（`Doctor.cpp` 未动）。
  `UnwritableSubdirectoryFailsCheckThree` 的 Windows 支按协议改
  `GTEST_SKIP("platform probe negative: D143③ witness degraded to contract-only")`；Linux 支（chmod 555 DAC 证人，
  root 跳过）实测为真证人。**③ 的 Windows 形状存疑交需求方（T5 账）**：delete-pending 不是可用的 Windows 不可写构造，
  候选构造待裁（如 ACL deny-Write——非本机免工具可证，本轮不擅动）。③ 对 Windows 上真实只读目录（如清掉写位的
  文件属性）是否仍报 FAIL 未在本轮取证——只如实记「DELETE_ON_CLOSE 构造不咬」这一点。
- **T2 · 计划代码块两处文本缺陷（执行期修正，收口补记）**：其一，测试 helper 名 `Run` 与 `testing::Test::Run`
  （私有成员）在 TEST 体内名遮蔽，两平台编译期硬失败——改名 `RunPlanIn`，用例名与断言零改动。其二，
  `EmptyTreeIsNotASilentSuccess` 按计划原文不给沙箱建目录，而 `CatalogSandbox` ctor **并不创建 Root**——该用例实际
  走的是「not a directory」分支，D135「零插件树」映射无证；裁定镜像 validate 同名证人补 `CreateDir("docs")`
  并钉 `no plugins found in the snapshot` 支路（修复轮 amend 865d0c4→73f8e7e，`^VaseCli` 37/37）。
- **T5 · tidy 三线全量首跑捕得正文 warning 10 条（三线同一集，全部落本波新代码），均走代码级出路修复后复跑归零**：
  `Plan.cpp` 九条（7 条 `misc-include-cleaner` 补直接包含——`ManifestView.h` 一条即解 3 处符号、
  `PluginDescriptor.h`、`<filesystem>`、`<utility>`、`<vector>`；1 条 `performance-move-const-arg`——
  `const Result<Preset>` 上 `std::move(loaded.Value())` 空转，照 Console.cpp 的 `auto loaded` 消费形转真实移动；
  1 条 `bugprone-branch-clone`——两硬跳过 case 合并，穷举 switch 保真）与 `VaseCliPlanTests.cpp` 一条（`<vector>`）。修复后六线删树重配全量与三线 tidy 重跑，
  账面见 CLAUDE.md「构建与测试」（M6 收口 2026-09-30）。
- **T6 · 终审修复波（2026-09-30）· 计划代码块第三处文本缺陷：`Plan.cpp` notes 环缺 Console 的 Cause 支**——计划给出的
  notes 循环只有 `Key` 一支，实盘照抄后 `providerSkipped`/`versionMismatchProvider` 两 kind（Key 恒空、归因在 Cause，
  `Solve.cpp:477-505`）在 `plan` 输出里丢归因、与 Console 侧不再同形（D149 形状忠实副本承诺破口）。终审批评级 Important，
  per-task 评审盲区（比对的是「两文本映射同源」而非「两 notes 环同形」）。修复：补 `else if (!note.Cause.empty())` 支，
  逐字镜像 `Console.cpp:417-420`；证人 `VaseCliPlan.ProviderSkippedNoteCarriesCauseAttribution`（Alpha provides、Beta
  requires、preset 禁 Alpha ⇒ Beta 硬跳 missing-dependency + note 行 `providerSkipped Vase.Beta Vase.Alpha` 两侧点名；
  变异取证：抽掉 Cause 支该枚转红）。同波 Minor：`Doctor.cpp` ④ 缺件文件由 `unlocked` 改 `absent (check 1 owns it)`
  info 支（spec §3.4「文件不存在 → info 跳过」的兑现，不计 Failures，探针本体不动）。
- **T6 · 下一波队列（终审判账，不入本波账面）**：ACL deny ③ 构造 / ④ 只读属性证人 / plan argv 组合 pin /
  spec §2.2 Message 措辞勘误（勘误注已随 T6 落在 spec §2.2 原句处、原文 strikethrough 保留——队列项指措辞的后续复核）。
