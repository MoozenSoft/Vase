# Vase M4 热替换全谱 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 兑现 `wiki` §12.3 的 M4 行——换件谱扩到描述符维（含回退方向）、还 Windows 侧档三负例这笔 M1 登记账、给「语义依赖」加一个如实上报不可知的知情位。

**Architecture:** 三条腿互相独立，各占 2–4 个任务：① 换件谱加两个描述符漂移版 fixture 与一条五步阶梯用例；② 把档三负例的换件序列统一成「改名离开 + 落新字节」，使 Windows 与 Linux 共用一条路径，并把缺身份特征的 fixture 跨平台化；③ 在 `EjectReport` 尾追加一个 bool 与三条证人。**不动**插件描述符布局、`kHeaderVersion`、`ProcessStateDesc`、50 轮循环。

**Tech Stack:** C++20 / CMake + vcpkg / GoogleTest / 六 preset（Win clang-cl & cl.exe、Linux clang + libc++）/ clang-tidy 23.1.0 / 全项目关异常。

**Spec:** `docs/superpowers/specs/2026-09-27-vase-m4-hot-replacement-design.md`（决定 D104–D116；执行时以 spec 为准，本计划是它的展开）

## Global Constraints

- **不使用 C++ 异常**：不写 `throw` / `try` / `catch`；测试里**不用** `EXPECT_THROW` 一族（编译期硬失败）。错误一律经 `Result<T>` / `Error` 显式返回。
- **Vase 自己的头一律引号包含**：`#include "Vase/Plugin.h"`，不写尖括号（尖括号会让 `/W4 /WX` 静默失效）。
- **新 target 必须链 `VaseBuildOptions`**；**插件 target 一律经 `vase_add_plugin_fixture`**，不手写 `add_library(SHARED)`。
- **命名规范由 `.clang-tidy` 强制**：类/函数/成员 `CamelCase`，参数/局部 `camelBack`，常量 `k` + `CamelCase`，命名空间 `lower_case`。
- **格式化**：`.clang-format` 是 LLVM 基线 + 多处偏离（Allman、`PointerAlignment: Left`、构造初始化表每式一行且逗号在行首、`template <...>` 与签名分两行）。
- **注释判据**：删掉它读者会不会踩坑；单条 ≤2 行、硬上限 3 行；论证属 spec/plan，代码只留结论 + 指针。
- **提交信息不带任何 AI 署名尾注**（不写 `Co-Authored-By:` / `Generated-with:` / `Signed-off-by:`）。
- **`Tests/HotSwap` 按主干对待**：动 Loader / 账本 / Eject / Adopt / 描述符布局 / `HeaderVersion` 的提交，必须跑 `-R 'HotSwap|Eject|Adopt'`，**Windows 与 Linux 各自留证据**。
- **基数与 tidy 阈值只在 `CLAUDE.md`**，任何任务都不得把数字抄到别处（含技能）。
- **本轮不动**：`kHeaderVersion`、`PluginMeta` 布局、`ProcessStateDesc`、`Tests/HotSwap/HotSwapLoopTests.cpp` 的 `FiftyRoundsBehaveLikeFirstTime`、`LoaderPosix.cpp` 的 Darwin 分支。
- **开发线**：`win-x64-clang-debug`（Windows / Git Bash）；Linux 侧经 `wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && …'`。拼 WSL 命令时**别把 `$` 写进双引号**。

---

## File Structure

| 文件 | 职责 | 动作 |
|---|---|---|
| `Tests/HotSwap/fixtures/BCommon.h` | 热插拔 fixture 的共享接口（Counter / Heart / Pulse / Tick / StateProbe） | 加 `ICounterV2` |
| `Tests/HotSwap/fixtures/VersionedAStampDrift/VersionedAStampDrift.cpp` | 换件谱 rung2：只差 `Version` 串 | 新建 |
| `Tests/HotSwap/fixtures/VersionedAServiceDrift/VersionedAServiceDrift.cpp` | 换件谱 rung3：只差 `Provides` 版本（注册 v2 接口） | 新建 |
| `Tests/HotSwap/fixtures/CMakeLists.txt` | 热插拔 fixture 的 target 与 stage 目录 | 加两个 target |
| `Tests/CMakeLists.txt` | `VaseTests` 的 fixture 路径宏、`NoIdentityPlugin` 的跨平台化 | 加两个宏、拆 `if(NOT WIN32)` |
| `Tests/TestingSupport/AdoptExpectations.h` | 各 fixture 的清单期望工厂 | 加两条、改一条 |
| `Tests/HotSwap/HotSwapLoopTests.cpp` | 热插拔主循环与换件谱 | 加 `SwapWorkspace::Install` + 阶梯用例 |
| `Tests/HotSwap/AdoptTests.cpp` | Adopt 面（含档三负例） | 改 `ProbeSwapGuard`、改一条用例、拆 `#ifndef _WIN32`、改一条用例名 |
| `Tests/HotSwap/fixtures/NoIdentityPlugin/` | 缺身份特征的 fixture（原 `NoBuildIdPlugin/`） | 改名 + 跨平台化 |
| `Include/Vase/Host/Evidence.h` | `EjectReport` / `AdoptReport` 的字段 | 尾追加一个 bool |
| `Source/Host/PluginHost.cpp` | Host 判定流 | 算这个 bool |
| `Tests/HotSwap/EjectTests.cpp` | Eject 面证人 | 加三条用例 |
| `Tools/VaseConsole/Console.cpp` | 前端打点（交互台） | 补一个字段 + 改准自述注释 |
| `Samples/Embedding/main.cpp` | 前端打点（嵌入示例） | 同上 |
| `wiki/vase-architecture.md` | 架构文档 | §12.1 / §12.3 / §9.1 义务 |
| `CLAUDE.md` | 命令、基数、规矩的唯一真值 | 项目状态 + 两张表 + 子串清单 |
| `.claude/skills/vase-cpp-engineering/references/verification.md` | 技能 | 删一行违反规矩 7 的基数真值 |

---

### Task 1: 换件谱 fixture 与期望工厂

**Files:**
- Modify: `Tests/HotSwap/fixtures/BCommon.h`
- Create: `Tests/HotSwap/fixtures/VersionedAStampDrift/VersionedAStampDrift.cpp`
- Create: `Tests/HotSwap/fixtures/VersionedAServiceDrift/VersionedAServiceDrift.cpp`
- Modify: `Tests/HotSwap/fixtures/CMakeLists.txt`
- Modify: `Tests/CMakeLists.txt:96`（宏块内，`VASE_FIXTURE_STATEFUL` 之后）
- Modify: `Tests/TestingSupport/AdoptExpectations.h`

**Interfaces:**
- Produces: `samples_fixture::ICounterV2`（`kName = "Vase.Test.Counter"`，`kVersion = 2`，`virtual int Value() const`）；fixture 宏 `VASE_FIXTURE_VERSIONEDASTAMPDRIFT`、`VASE_FIXTURE_VERSIONEDASERVICEDRIFT`；`testing_support::MakeVersionedAStampDriftExpectation()`、`testing_support::MakeVersionedAServiceDriftExpectation()`。
- Consumes: 既有 `samples_fixture::ICounter`（`BCommon.h`）、既有 `MakeVersionedAExpectation()`。

- [ ] **Step 1: `BCommon.h` 加 `ICounterV2`**

插在 `ICounter` 的类体之后（同一个 `BCommon.h` 里，`IHeart` 之前）：

```cpp
// rung3 的漂移维（M4/D106）：与 ICounter **同名、主版本 2**——§6.1「主版本不同=不同服务」，
// 故两者是两个键、不构成碰撞。存在的理由：`Context::Provide<T>()` 的注册键来自接口常量
// （T::kVersion）而非描述符，所以「声明提供 v2」的 fixture 必须真有一个 v2 接口可注册，
// 否则描述符就是在撒谎。
class ICounterV2
{
public:
    static constexpr std::string_view kName = "Vase.Test.Counter";
    static constexpr std::uint32_t kVersion = 2;

    ICounterV2() = default;
    ICounterV2(const ICounterV2&) = delete;
    ICounterV2& operator=(const ICounterV2&) = delete;
    ICounterV2(ICounterV2&&) = delete;
    ICounterV2& operator=(ICounterV2&&) = delete;
    virtual ~ICounterV2() = default;

    [[nodiscard]] virtual int Value() const = 0;
};
```

- [ ] **Step 2: 建 rung2 fixture**

`Tests/HotSwap/fixtures/VersionedAStampDrift/VersionedAStampDrift.cpp`：

```cpp
// 换件谱 rung2（M4/D106）：与 VersionedAPrime 只差描述符的 `Version` 串（1.0.0 → 1.1.0），
// 行为同为 return 2。它是「描述符变了 ⇒ 期望必须跟着换」那一步的字节。
#include "Vase/Plugin.h"

#include "../BCommon.h"

namespace
{

class CounterImpl final : public samples_fixture::ICounter
{
public:
    [[nodiscard]] int Value() const override { return 2; } // 与 rung1 同行为：本步只漂版本戳
};

class VersionedAStampDriftPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<samples_fixture::ICounter>(Counter);
        return vase::Result<void>::Ok();
    }

private:
    CounterImpl Counter;
};

} // namespace

VASE_PLUGIN(VersionedAStampDriftPlugin){
    .Id = "Vase.VersionedA", // 同一个插件的下一版——Id 不变，换件语义才成立
    .DisplayName = "双版本探针",
    .Version = "1.1.0", // ← 与 rung1 的唯一差异
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.Counter", .Version = 1}},
};
```

- [ ] **Step 3: 建 rung3 fixture**

`Tests/HotSwap/fixtures/VersionedAServiceDrift/VersionedAServiceDrift.cpp`：

```cpp
// 换件谱 rung3（M4/D106）：与 rung2 只差 `Provides` 的服务版本（v1 → v2），行为同为 return 2。
// 注册的是 ICounterV2——**声明与实注册同版本**（理由见 BCommon.h 的注释）。
#include "Vase/Plugin.h"

#include "../BCommon.h"

namespace
{

class CounterV2Impl final : public samples_fixture::ICounterV2
{
public:
    [[nodiscard]] int Value() const override { return 2; }
};

class VersionedAServiceDriftPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<samples_fixture::ICounterV2>(Counter);
        return vase::Result<void>::Ok();
    }

private:
    CounterV2Impl Counter;
};

} // namespace

VASE_PLUGIN(VersionedAServiceDriftPlugin){
    .Id = "Vase.VersionedA",
    .DisplayName = "双版本探针",
    .Version = "1.1.0",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.Counter", .Version = 2}}, // ← 与 rung2 的唯一差异
};
```

- [ ] **Step 4: `fixtures/CMakeLists.txt` 加两个 target**

在 `set_target_properties(VersionedAPrime PROPERTIES …)` 那两块**之后**追加（两个新 target 必须与 `VersionedAPrime` 一样：`OUTPUT_NAME` 同 `VersionedA`、各自独立 stage 目录，否则 Ninja 生成期报 `multiple rules generate …/VersionedA.lib`）：

```cmake
# 换件谱的另两版（M4/D106）：与 A′ 同 OUTPUT_NAME、各自 stage 目录（同 A′ 的既有处理）。
vase_add_plugin_fixture(VersionedAStampDrift
    SOURCES VersionedAStampDrift/VersionedAStampDrift.cpp
    LINK_LIBRARIES VasePod)
vase_add_plugin_fixture(VersionedAServiceDrift
    SOURCES VersionedAServiceDrift/VersionedAServiceDrift.cpp
    LINK_LIBRARIES VasePod)
set_target_properties(VersionedAStampDrift PROPERTIES OUTPUT_NAME VersionedA)
set_target_properties(VersionedAServiceDrift PROPERTIES OUTPUT_NAME VersionedA)
set_target_properties(VersionedAStampDrift PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stageStampDrift"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stageStampDrift"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stageStampDrift")
set_target_properties(VersionedAServiceDrift PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stageServiceDrift"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stageServiceDrift"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stageServiceDrift")
```

- [ ] **Step 5: `Tests/CMakeLists.txt` 加两条路径宏**

在 `VASE_FIXTURE_STATEFUL=…` 那一行之后追加：

```cmake
    VASE_FIXTURE_VERSIONEDASTAMPDRIFT="$<PATH:CMAKE_PATH,$<TARGET_FILE:VersionedAStampDrift>>"
    VASE_FIXTURE_VERSIONEDASERVICEDRIFT="$<PATH:CMAKE_PATH,$<TARGET_FILE:VersionedAServiceDrift>>"
```

- [ ] **Step 6: `AdoptExpectations.h` 加两条工厂**

在 `MakeVersionedAExpectation()` 之后追加：

```cpp
// VersionedAStampDrift.cpp 逐字抄（M4 换件谱 rung2）：与 MakeVersionedAExpectation 只差 Version 串。
inline vase::ManifestExpectation MakeVersionedAStampDriftExpectation()
{
    vase::ManifestExpectation expected;
    expected.Id = "Vase.VersionedA";
    expected.DisplayName = "双版本探针";
    expected.Version = "1.1.0"; // ← 与 rung1 的唯一差异
    expected.Provides = {vase::ExpectedService{.Name = "Vase.Test.Counter", .Version = 1}};
    return expected;
}

// VersionedAServiceDrift.cpp 逐字抄（M4 换件谱 rung3）：与 rung2 只差 Provides 的服务版本。
inline vase::ManifestExpectation MakeVersionedAServiceDriftExpectation()
{
    vase::ManifestExpectation expected;
    expected.Id = "Vase.VersionedA";
    expected.DisplayName = "双版本探针";
    expected.Version = "1.1.0";
    expected.Provides = {vase::ExpectedService{.Name = "Vase.Test.Counter", .Version = 2}}; // ← 与 rung2 的唯一差异
    return expected;
}
```

- [ ] **Step 7: 构建，确认三份产物都在**

```bash
cmake --build --preset win-x64-clang-debug
```

Expected: 构建成功、零警告。然后核产物（**同名、三个目录**）：

```bash
ls build-win/win-x64-clang-debug/Tests/HotSwap/fixtures/stageA/
ls build-win/win-x64-clang-debug/Tests/HotSwap/fixtures/stageStampDrift/
ls build-win/win-x64-clang-debug/Tests/HotSwap/fixtures/stageServiceDrift/
```

Expected: 三个目录里各有 `VersionedA.dll`（Linux 上是 `libVersionedA.so`）。

- [ ] **Step 8: 提交**

```bash
git add Tests/HotSwap/fixtures/BCommon.h Tests/HotSwap/fixtures/VersionedAStampDrift \
        Tests/HotSwap/fixtures/VersionedAServiceDrift Tests/HotSwap/fixtures/CMakeLists.txt \
        Tests/CMakeLists.txt Tests/TestingSupport/AdoptExpectations.h
git commit -m "M4-T1：换件谱 fixture（描述符漂移两版）与期望工厂"
```

---

### Task 2: 换件谱五步阶梯用例

**Files:**
- Modify: `Tests/HotSwap/HotSwapLoopTests.cpp`（`SwapWorkspace` 类体 + 文件末尾新用例）
- Modify: `docs/superpowers/specs/2026-09-27-vase-m4-hot-replacement-design.md`（§6 风险行填入实测耗时，D116）

**Interfaces:**
- Consumes: `VASE_FIXTURE_VERSIONEDAPRIME`、`VASE_FIXTURE_VERSIONEDASTAMPDRIFT`、`VASE_FIXTURE_VERSIONEDASERVICEDRIFT`、`VASE_FIXTURE_VERSIONEDA`；`testing_support::MakeVersionedAExpectation()` / `MakeVersionedAStampDriftExpectation()` / `MakeVersionedAServiceDriftExpectation()`；既有 `SwapWorkspace`、`LoopPlan`、`LoopAdoptRequest`。
- Produces: `SwapWorkspace::Install(const std::filesystem::path& source)`；用例 `HotSwap.DescriptorDriftLadderSwapsBothWays`。

- [ ] **Step 1: 给 `SwapWorkspace` 加参数化落字节的入口**

`InstallPrime()` 保持原样（既有两条用例在用）。在它**之后**追加：

```cpp
    // 与 InstallPrime 同一动作、只是参数化：换件谱的每一级要落不同的字节。
    // 与 InstallPrime 同一条断言——写不动（Windows sharing violation）就是卸载路径坏了。
    void Install(const std::filesystem::path& source) const
    {
        std::error_code ec;
        std::filesystem::copy_file(source, APath, std::filesystem::copy_options::overwrite_existing, ec);
        EXPECT_FALSE(ec) << ec.message();
    }
```

- [ ] **Step 2: 写阶梯用例**

追加在 `FiftyRoundsBehaveLikeFirstTime` **之后**、`} // namespace` 之前。**逐字照抄**：

```cpp
// §12.1 换件谱（M4/D113）：五步阶梯，每步只差**一维**描述符；含**回退方向**（回滚拆两步，
// 否则一次退两维、归因说不清）。正例是主体——描述符变了的版本也能走完 Eject→落字节→换期望→Adopt；
// 负例是附属——把本步期望沿本步那一维 tamper 回上一级的值，比对拒。
//
// 每步三条证人的由来：负例那次 Adopt 在 ③.5 比对处被拒，而 AdoptImpl 的 EnsureResident **先于**
// CompareDescriptor——所以它已经把新字节装载驻留了。于是紧接着换对期望必然走**复用分支**，
// `ReusedResidentImage` 的真值在这一趟里就有证人；再 Eject 一次换回**全新装载**分支。两个分支都断。
//
// 例外：S1（代码维）**没有负例**——rung0 与 rung1 描述符逐字节相同，没有字段可漂；硬造一条只能去
// 篡改一个与本步无关的字段，那测的是比对器不是换件谱。故 S1 只做一条全新装载正例。
TEST(HotSwap, DescriptorDriftLadderSwapsBothWays)
{
    const SwapWorkspace ws; // 先于 host 声明：见 SwapWorkspace 的析构序说明
    vase::PluginHost host;

    const vase::ManifestExpectation rung01 = testing_support::MakeVersionedAExpectation();           // rung0 / rung1
    const vase::ManifestExpectation rung2 = testing_support::MakeVersionedAStampDriftExpectation();  // Version 1.1.0
    const vase::ManifestExpectation rung3 = testing_support::MakeVersionedAServiceDriftExpectation(); // + Provides v2

    struct Step
    {
        const char* Label;
        const char* Source; // 本步落在 A 位置上的字节（宏，运行期解成路径）
        const vase::ManifestExpectation* Current;
        const vase::ManifestExpectation* Stale; // 一维 tamper 后的期望；nullptr = 本步无负例
        bool ReadsV2;                           // rung3 提供的是 v2，行为读数随之换接口
        int ExpectedCounter;
    };

    const std::vector<Step> steps = {
        {"S1 代码", VASE_FIXTURE_VERSIONEDAPRIME, &rung01, nullptr, false, 2},
        {"S2 Version 去", VASE_FIXTURE_VERSIONEDASTAMPDRIFT, &rung2, &rung01, false, 2},
        {"S3 Provides 去", VASE_FIXTURE_VERSIONEDASERVICEDRIFT, &rung3, &rung2, true, 2},
        {"S4 Provides 回", VASE_FIXTURE_VERSIONEDASTAMPDRIFT, &rung2, &rung3, false, 2},
        {"S5 Version 回", VASE_FIXTURE_VERSIONEDA, &rung01, &rung2, false, 1},
    };

    const vase::PodHandle h = host.CreatePod(LoopPlan(ws.APath)).Value();
    EXPECT_EQ(CounterValue(host.Resolve(h), false), 1); // 起点 = rung0

    int ticks = 0;
    for (const Step& step : steps)
    {
        SCOPED_TRACE(step.Label); // 失败定位到步（50 轮循环用 << "round " << round，同一用意）
        ASSERT_TRUE(host.EjectPlugin(h, "Vase.VersionedA").IsOk());
        ws.Install(step.Source);

        if (step.Stale == nullptr)
        {
            // S1：本维无描述符可漂 ⇒ 无负例，也就没有「负例留下驻留」这回事。
            const vase::Result<vase::AdoptReport> only = host.AdoptPlugin(h, LoopAdoptRequest(*step.Current, ws.APath));
            ASSERT_TRUE(only.IsOk()) << only.GetError().Message();
            EXPECT_FALSE(only.Value().ReusedResidentImage); // 全新装载分支
            EXPECT_TRUE(only.Value().ManifestVerified);
        }
        else
        {
            // 负例：期望停在本步之前那一级的取值 → 比对拒。
            // 只钉总 token——D88 口径：一个子串即够，字段细节留给人（不为此扩格式器）。
            const vase::Result<vase::AdoptReport> stale = host.AdoptPlugin(h, LoopAdoptRequest(*step.Stale, ws.APath));
            ASSERT_FALSE(stale.IsOk()); // ASSERT_：下一行取 GetError()，Ok 上取会终止进程
            EXPECT_NE(stale.GetError().Message().find("manifest/binary mismatch"), std::string::npos)
                << stale.GetError().Message();

            // 正例甲：上一次调用已把二进制装载驻留 ⇒ 这一次走**复用分支**。
            const vase::Result<vase::AdoptReport> reused =
                host.AdoptPlugin(h, LoopAdoptRequest(*step.Current, ws.APath));
            ASSERT_TRUE(reused.IsOk()) << reused.GetError().Message();
            EXPECT_TRUE(reused.Value().ReusedResidentImage);
            EXPECT_TRUE(reused.Value().ManifestVerified);
            EXPECT_EQ(CounterValue(host.Resolve(h), step.ReadsV2), step.ExpectedCounter);

            // 正例乙：再卸一次 → **全新装载分支**。
            ASSERT_TRUE(host.EjectPlugin(h, "Vase.VersionedA").IsOk());
            const vase::Result<vase::AdoptReport> fresh =
                host.AdoptPlugin(h, LoopAdoptRequest(*step.Current, ws.APath));
            ASSERT_TRUE(fresh.IsOk()) << fresh.GetError().Message();
            EXPECT_FALSE(fresh.Value().ReusedResidentImage);
        }

        // 每步派两拍，并核两只邻居的**精确**累计数（不派拍则两端皆 0、断言恒真）。
        vase::Pod* const pod = host.Resolve(h);
        pod->Root().Emit(samples_fixture::TickEvent{ticks});
        ++ticks;
        pod->Root().Emit(samples_fixture::TickEvent{ticks});
        ++ticks;
        EXPECT_EQ(pod->Root().Get<samples_fixture::IHeart>().Beats(), ticks) << step.Label;
        EXPECT_EQ(pod->Root().Get<samples_fixture::IPulse>().Pulses(), ticks) << step.Label;
    }

    EXPECT_TRUE(host.DestroyPod(h).Clean());
}
```

同一步里 `CounterValue` 需要接一个 `bool`。把既有的 `CounterValue` 换成这个（**两条既有用例的调用点也要跟着补 `false`**）：

```cpp
// 宿主侧解析（§5.6：宿主的解析不落边）。`readsV2` = 被读的那一版提供的是 v2 接口
// （rung3 声明并注册 v2；其余级都是 v1）——`Provide<T>` 的键来自接口常量，不是描述符。
int CounterValue(vase::Pod* pod, bool readsV2)
{
    if (readsV2)
    {
        return pod->Root().Get<samples_fixture::ICounterV2>().Value();
    }
    return pod->Root().Get<samples_fixture::ICounter>().Value(); // Get<T>() 返回 T&（R112）
}
```

- [ ] **Step 3: 跑阶梯用例（预期红：已存在的断言需要适配）**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R 'HotSwap.DescriptorDriftLadderSwapsBothWays' --output-on-failure
```

Expected: 先是因为 `CounterValue` 签名变了、既有调用点没过编译而红——**四处调用点**（`HotSwapLoopTests.cpp:117`、`:137`、`:170`、`:188`）全部补成 `CounterValue(pod, false)`（`:188` 那处是 `CounterValue(host.Resolve(h), false)`）。改完重跑，本用例应**绿**。

- [ ] **Step 4: 变异验证（证明负例真有牙）**

临时把比对器的服务版本比较关掉，确认 S3/S4 的负例**会红**：

在 `Source/Host/Detail/ManifestCompare.cpp` 里找到 `CompareServiceSet`，把版本参与比对的那一处临时改成恒等（例如把 `Version` 的比较短路掉），重建后跑：

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R 'HotSwap.DescriptorDriftLadderSwapsBothWays' --output-on-failure
```

Expected: **红**，且失败点落在 S3 或 S4（`SCOPED_TRACE` 的 Label 会打出来）。**看到红之后把 ManifestCompare 改回原样**，重建、重跑，回绿。

> 这一步不是可选的：本仓库的规矩是「没有证人的那一步要明说」，而**有证人但证人不会红**是更坏的形态（M3 终审就抓到过一次——删掉 `state.Reset()` 全仓仍绿）。做完在提交信息里记一句结论。

- [ ] **Step 5: 实测耗时并写进 spec**

```bash
ctest --preset win-x64-clang-debug -R 'HotSwap.DescriptorDriftLadderSwapsBothWays' --output-on-failure
```

记下 `Total Test time`。把它填进 spec 的 §6 风险表「阶梯用例耗时」那一行的处置格（D116 要求的实测数字），并同时记 50 轮循环的耗时作对照：

```bash
ctest --preset win-x64-clang-debug -R 'HotSwap.FiftyRoundsBehaveLikeFirstTime' --output-on-failure
```

- [ ] **Step 6: Linux 侧跑同一条（规矩 6：双平台各自留证据）**

```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug -R "HotSwap.DescriptorDriftLadderSwapsBothWays" --output-on-failure'
```

Expected: 绿。Linux 上负例走 `dlopen` 同路径复用分支这条路，与 Windows 的路径不同——**红了不要改断言**，先查是哪一环。

- [ ] **Step 7: 提交**

```bash
git add Tests/HotSwap/HotSwapLoopTests.cpp docs/superpowers/specs/2026-09-27-vase-m4-hot-replacement-design.md
git commit -m "M4-T2：换件谱五步阶梯（描述符维含回退方向，两个装载分支各留证人）"
```

---

### Task 3: Windows 档三负例——统一换件序列

**Files:**
- Modify: `Tests/HotSwap/AdoptTests.cpp:194-288`（`ProbeSwapGuard` 类体 + `RenameReplacementCaughtByTierThree` 用例 + 删 `#ifndef _WIN32` / `#endif`）

**Interfaces:**
- Consumes: `VASE_FIXTURE_LOADPROBE`、`VASE_FIXTURE_UNLOADPROBE`、既有 `Plan()`、`RequestFor()`、`testing_support::MakeLoadProbeExpectation()`。
- Produces: `ProbeSwapGuard(std::filesystem::path target)`（**单参构造**，去掉了 `staging`）与 `guard.DisplacedPath()`。

- [ ] **Step 1: 改 `ProbeSwapGuard`**

把整个类体替换成（改动三处：构造函数去掉 `staging` 参数、多一个 `Displaced` 成员与取用口、析构改成「删 `.old` + 把备份写回」）：

```cpp
// 构建树现场保护：本文件**唯一**会改构建树的用例先把原文件备份出来，析构时放回原位。
// 用 RAII 而不是写在测试末行——中途 ASSERT_* 早退会把构建树留在「该路径指向另一个
// 二进制」的状态，此后每一条装载 LoadProbe 的用例都会拿到错的东西。
// M4/D108：换件序列改成「先改名离开、再落新字节」（Windows 允许改名映射中的文件、
// 不允许删除或覆盖它，P1 实测），于是多出一个要收的临时名 `.old`。
class ProbeSwapGuard final
{
public:
    explicit ProbeSwapGuard(std::filesystem::path target)
        : Target(std::move(target))
        , Backup(Target.string() + ".orig")
        , Displaced(Target.string() + ".old")
    {
        std::error_code ec;
        std::filesystem::remove(Backup, ec);
        ec.clear();
        std::filesystem::copy_file(Target, Backup, std::filesystem::copy_options::overwrite_existing, ec);
        Armed = !ec;
    }

    ~ProbeSwapGuard()
    {
        std::error_code ec;
        // 改名后旧字节的落点。**本守卫必须先于 host 销毁**（用例里的声明序），
        // 否则 Windows 上这个文件仍被映射、删不掉。
        std::filesystem::remove(Displaced, ec);
        if (!Armed)
        {
            return;
        }
        ec.clear();
        std::filesystem::copy_file(Backup, Target, std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(Backup, ec);
    }

    ProbeSwapGuard(const ProbeSwapGuard&) = delete;
    ProbeSwapGuard& operator=(const ProbeSwapGuard&) = delete;
    ProbeSwapGuard(ProbeSwapGuard&&) = delete;
    ProbeSwapGuard& operator=(ProbeSwapGuard&&) = delete;

    [[nodiscard]] bool IsArmed() const { return Armed; }
    [[nodiscard]] const std::filesystem::path& DisplacedPath() const { return Displaced; }

private:
    std::filesystem::path Target;
    std::filesystem::path Backup;
    std::filesystem::path Displaced;
    bool Armed = false;
};
```

- [ ] **Step 2: 改用例体与声明序**

把 `TEST(Adopt, RenameReplacementCaughtByTierThree)` 的**函数体**替换成：

```cpp
TEST(Adopt, RenameReplacementCaughtByTierThree)
{
    // §8.2：改名替换骗过档二，只有特征比对分得出新旧。两平台走**同一条**序列——先改名把路径
    // 腾空、再落新字节（Windows 允许改名映射中的文件、不允许删除或覆盖它；Linux 两者皆可，
    // 统一到那条两边都成立的路）。这条序列在 Windows 上必然造出档二会判「已卸载」而镜像其实
    // 还在的局面——正是 §8.2「档二会被改名替换骗过」那段论证的活体实例。本用例走 Adopt 轨、
    // 不产出 Eject 报告，故不为此加断言（要断档二就得另配一次 Eject，会把用例变成两件事拼的）。
    //
    // **声明序承重**：guard 在 host **之前**——host 先析构卸下镜像，之后守卫才删得动 `.old`。
    const std::filesystem::path probe{VASE_FIXTURE_LOADPROBE};
    const ProbeSwapGuard guard{probe};
    ASSERT_TRUE(guard.IsArmed());

    vase::PluginHost host;
    host.DestroyPod(host.CreatePod(Plan({{"Vase.LoadProbe", probe}})).Value()); // 驻留（拆局不卸货，§8.1）

    std::error_code ec;
    std::filesystem::rename(probe, guard.DisplacedPath(), ec);
    ASSERT_FALSE(ec) << ec.message();
    std::filesystem::copy_file(VASE_FIXTURE_UNLOADPROBE, probe, std::filesystem::copy_options::overwrite_existing, ec);
    ASSERT_FALSE(ec) << ec.message();

    const vase::ManifestExpectation probeExpected = testing_support::MakeLoadProbeExpectation();
    const vase::PodHandle h = host.CreatePod(vase::LoadPlan{}).Value();
    const vase::Result<vase::AdoptReport> r = host.AdoptPlugin(h, RequestFor(probeExpected, probe));
    ASSERT_FALSE(r.IsOk()); // ASSERT_：下面立刻取 GetError()，Ok 上取会终止进程
    EXPECT_NE(r.GetError().Message().find("differ"), std::string::npos);
    EXPECT_NE(r.GetError().Message().find("rebuild"), std::string::npos); // 逃生门写明（§8.2 政策）
    host.DestroyPod(h);
    // 复原由 guard 负责——本行确实不是唯一的复原点。
}
```

- [ ] **Step 3: 拆掉 `#ifndef _WIN32`**

删掉 `Tests/HotSwap/AdoptTests.cpp:194` 的 `#ifndef _WIN32` 行与 `:288` 的 `#endif` 行——两条用例自此都跨平台（后一条在 Task 4 里改名）。**本步只删这两行**：原块内 `ProbeSwapGuard` 上方的注释讲的是构建树保护，与平台无关，留给它。

- [ ] **Step 4: Windows 跑（预期红过一次再绿）**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R 'Adopt.RenameReplacementCaughtByTierThree' --output-on-failure
```

Expected: 绿。若红在 `rename` 或 `copy_file` 那两行——说明构建树里还有别的东西开着那个文件，**照实报告**，不要放宽断言。

- [ ] **Step 5: Linux 跑（**本轮最大的一处未验证事实**）**

```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug -R "Adopt.RenameReplacementCaughtByTierThree" --output-on-failure'
```

Expected: 绿。P1 只证了 Windows 可行、**没证 Linux 换序后仍绿**。若红：按 spec §3.2 的退路**退回两平台各自序列**，并在 `#ifdef` 的注释里写明为什么不能统一——**不是**删用例、也不是放宽断言。退回也要如实告诉需求方。

- [ ] **Step 6: 顺手核一个副作用**

```bash
git status --short build-win/ build-linux/ 2>/dev/null; ls build-win/win-x64-clang-debug/Tests/Unit/fixtures/LoadProbe/ 2>/dev/null
```

Expected: 构建树里**不留** `LoadProbe.dll.old` / `.orig`（守卫收干净了）。`build-*/` 本就在 `.gitignore` 里，这里是看文件不是看 git。

- [ ] **Step 7: 提交**

```bash
git add Tests/HotSwap/AdoptTests.cpp
git commit -m "M4-T3：档三负例换件序列统一为改名离开+落新字节（Windows 侧登记账还清）"
```

---

### Task 4: `NoIdentityPlugin` 跨平台化与改名

**Files:**
- Rename: `Tests/HotSwap/fixtures/NoBuildIdPlugin/NoBuildIdPlugin.cpp` → `Tests/HotSwap/fixtures/NoIdentityPlugin/NoIdentityPlugin.cpp`（内容一并改）
- Modify: `Tests/CMakeLists.txt:100-117`（`if(NOT WIN32)` 块）
- Modify: `Tests/TestingSupport/AdoptExpectations.h:123-129`
- Modify: `Tests/HotSwap/AdoptTests.cpp`（`MissingIdentityFeatureRejectedWithPointer`）

**Interfaces:**
- Consumes: `vase_add_plugin_fixture`、`VASE_FIXTURE_NOBUILDID`（旧名，待删）。
- Produces: fixture target `NoIdentityPlugin`、宏 `VASE_FIXTURE_NOIDENTITY`、插件 Id `Vase.NoIdentity`、事件名 `Vase.NoIdentity.Loaded`、工厂 `testing_support::MakeNoIdentityExpectation()`、用例 `Adopt.MissingIdentityFeatureRejectedWithPointer`。

- [ ] **Step 1: 改名并改内容**

```bash
git mv Tests/HotSwap/fixtures/NoBuildIdPlugin Tests/HotSwap/fixtures/NoIdentityPlugin
git mv Tests/HotSwap/fixtures/NoIdentityPlugin/NoBuildIdPlugin.cpp Tests/HotSwap/fixtures/NoIdentityPlugin/NoIdentityPlugin.cpp
```

然后在该文件里把四处名字改掉（**保留其余注释与结构**）：
- 头部注释的「旧头探针」那句之后补一句：`M4/D109：跨平台化并改名——它的存在理由是平台中性的「身份特征缺失」（Linux 无 .note.gnu.build-id / Windows 无 CodeView），旧名 NoBuildId 对 Windows 读者是谎话。`
- `struct NoBuildIdEvent` → `struct NoIdentityEvent`，`kName = "Vase.NoBuildId.Loaded"` → `"Vase.NoIdentity.Loaded"`
- `class NoBuildIdPlugin` → `class NoIdentityPlugin`
- `VASE_PLUGIN(NoBuildIdPlugin)` → `VASE_PLUGIN(NoIdentityPlugin)`，`.Id = "Vase.NoBuildId"` → `"Vase.NoIdentity"`

- [ ] **Step 2: `Tests/CMakeLists.txt` 换成跨平台块**

把 `:100-117` 整块（含上面的长注释）替换成：

```cmake
# 档三「特征缺失」fixture（T11；M4/D109 跨平台化）：两个平台各摘各的身份特征——
# Linux 是 .note.gnu.build-id，Windows 是 PE 调试目录里的 CodeView(RSDS)。
#
# 链接顺序是**实测**钉住的：`target_link_options` 排在工具链 `CMAKE_SHARED_LINKER_FLAGS_INIT`
# 之后，后者胜——Linux 侧 M1 已用 GNU ld 证过（实验记录见 task-11-report.md），Windows 侧
# `/DEBUG:NONE` 压过 `/DEBUG:FULL` 也已实测（M4 探针 P2；lld-link 调试目录整个空，link.exe
# 剩一条 POGO，而解析器逐条找 CodeView、会跳过它）。
#
# **这个 fixture 自守**：标志一旦失效，产物就长出身份特征，Adopt 会走到装配并返回 Ok，
# `EXPECT_FALSE(r.IsOk())` 当场变红——静默失效不可能产生假绿。
vase_add_plugin_fixture(NoIdentityPlugin
    SOURCES HotSwap/fixtures/NoIdentityPlugin/NoIdentityPlugin.cpp
    LINK_LIBRARIES VasePod)
if(WIN32)
    target_link_options(NoIdentityPlugin PRIVATE "/DEBUG:NONE")
else()
    target_link_options(NoIdentityPlugin PRIVATE "-Wl,--build-id=none")
endif()
target_compile_definitions(VaseTests PRIVATE
    VASE_FIXTURE_NOIDENTITY="$<PATH:CMAKE_PATH,$<TARGET_FILE:NoIdentityPlugin>>")
```

> 注意：`/DEBUG:NONE` 写在 CMake 里，**不经 Git Bash**，所以不受那条「裸 `/flags` 被当路径改写」的影响（那条只坑命令行直调 clang-cl，需要 `export MSYS2_ARG_CONV_EXCL='*'`）。

- [ ] **Step 3: 改期望工厂**

`Tests/TestingSupport/AdoptExpectations.h` 里把 `MakeNoBuildIdExpectation` 整块换成：

```cpp
// NoIdentityPlugin.cpp 逐字抄（M4/D109 起两平台都在）。
inline vase::ManifestExpectation MakeNoIdentityExpectation()
{
    vase::ManifestExpectation expected;
    expected.Id = "Vase.NoIdentity";
    expected.DisplayName = "无身份特征探针";
    expected.Version = "0.0.1";
    return expected;
}
```

- [ ] **Step 4: 改用例（含按平台取指路 token）**

`Tests/HotSwap/AdoptTests.cpp` 里把 `MissingIdentityFeatureRejectedWithPointer` 整块换成：

```cpp
TEST(Adopt, MissingIdentityFeatureRejectedWithPointer)
{
    // §8.2 的「特征缺失 = 直接拒绝」，两平台各有自己的指路 token：Linux 指 --build-id、
    // Windows 指 /DEBUG:FULL（错误臂单源，见 ImageInspectCommon.cpp 的两个 Missing*Error）。
    vase::PluginHost host;
    const vase::LoadPlan plan = Plan({{"Vase.NoIdentity", VASE_FIXTURE_NOIDENTITY}});
    host.DestroyPod(host.CreatePod(plan).Value()); // CreatePod 不设身份闸（v2 语义）——先驻留一回
    const vase::ManifestExpectation expected = testing_support::MakeNoIdentityExpectation();
    const vase::PodHandle h = host.CreatePod(vase::LoadPlan{}).Value();
    const vase::Result<vase::AdoptReport> r = host.AdoptPlugin(h, RequestFor(expected, VASE_FIXTURE_NOIDENTITY));
    ASSERT_FALSE(r.IsOk()); // ASSERT_：下面立刻取 GetError()，Ok 上取会终止进程
#ifdef _WIN32
    EXPECT_NE(r.GetError().Message().find("/DEBUG:FULL"), std::string::npos); // 报告指路补链接标志
#else
    EXPECT_NE(r.GetError().Message().find("--build-id"), std::string::npos);
#endif
    host.DestroyPod(h);
}
```

> 这个 `#ifdef` 是**断言**按平台取 token，与 `EjectTests.cpp:51` 的既有处置同形；D108 禁的是**换件序列**写 `#ifdef`，不是断言。

- [ ] **Step 5: 全仓确认旧名无残留**

```bash
grep -rn "NoBuildId\|NOBUILDID" --include="*.cpp" --include="*.h" --include="*.txt" --include="*.cmake" . | grep -v "^./build-win\|^./build-linux\|^./.git/\|^./docs/\|^./.superpowers/"
```

Expected: **零命中**。`docs/` 与 `.superpowers/` 下的历史记录**不改史**，故排除在外。

- [ ] **Step 6: 两平台各跑一次**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R 'Adopt.MissingIdentityFeatureRejectedWithPointer' --output-on-failure
```

```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug -R "Adopt.MissingIdentityFeatureRejectedWithPointer" --output-on-failure'
```

Expected: 两侧皆绿。**Windows 侧如果红在「找不到 /DEBUG:FULL」**——说明 `/DEBUG:NONE` 没压住或产物仍带 RSDS，照实报告（探针 P2 证的是链接器语义，仓库内的 flag 次序是 CMake 生成器保证，两者合起来才成立）。

- [ ] **Step 7: 提交**

```bash
git add -A Tests/
git commit -m "M4-T4：NoIdentityPlugin 跨平台化并改名（消掉档三负例的平台不对称）"
```

---

### Task 5: 语义依赖知情位（字段 + 三条证人）

**Files:**
- Modify: `Include/Vase/Host/Evidence.h:62-66`（`ProcessStatesReset` 与 `HotSwapNote` 之间）
- Modify: `Source/Host/PluginHost.cpp`（③ 闸的计数循环 + Reset 块之后）
- Modify: `Tests/HotSwap/EjectTests.cpp`（文件末尾追加三条用例）

**Interfaces:**
- Produces: `vase::EjectReport::SemanticDependencyPossible`（`bool`，默认 `false`）。
- Consumes: 既有 `report.ProcessStatesReset`、`Slots`、`PodSlot::Inner->Instances`。

- [ ] **Step 1: 写三条证人（先写测试）**

追加在 `Tests/HotSwap/EjectTests.cpp` 末尾：

```cpp
TEST(Eject, SemanticDependencyPossibleWhenResetAndOthersLive)
{
    // M4/D110 正例：重置发生 ∧ 进程内仍有活实例 → 置位。B 还活着——它可能攥着从
    // StateProbe 派生的值，而那条边账本看不见（§9.1）。
    vase::PluginHost host;
    const vase::PodHandle h = host
                                  .CreatePod(Plan({{"Vase.Stateful", VASE_FIXTURE_STATEFUL},
                                                   {"Vase.NeighborB", VASE_FIXTURE_NEIGHBORB}}))
                                  .Value();
    const vase::Result<vase::EjectReport> r = host.EjectPlugin(h, "Vase.Stateful");
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    EXPECT_FALSE(r.Value().ProcessStatesReset.empty());
    EXPECT_TRUE(r.Value().SemanticDependencyPossible);
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(Eject, SemanticDependencyAbsentWhenNothingElseLive)
{
    // M4/D110 反例 a：重置确实跑了，但全进程再无活实例 → 不置位（没人可能拿着派生值）。
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(Plan({{"Vase.Stateful", VASE_FIXTURE_STATEFUL}})).Value();
    const vase::Result<vase::EjectReport> r = host.EjectPlugin(h, "Vase.Stateful");
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    EXPECT_FALSE(r.Value().ProcessStatesReset.empty());
    EXPECT_FALSE(r.Value().SemanticDependencyPossible);
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(Eject, SemanticDependencyAbsentWithoutProcessStateReset)
{
    // M4/D110 反例 b：有人活着，但被卸者根本没有进程级状态 → 不置位。
    vase::PluginHost host;
    const vase::PodHandle h = host
                                  .CreatePod(Plan({{"Vase.NeighborB", VASE_FIXTURE_NEIGHBORB},
                                                   {"Vase.NeighborC", VASE_FIXTURE_NEIGHBORC}}))
                                  .Value();
    const vase::Result<vase::EjectReport> r = host.EjectPlugin(h, "Vase.NeighborB");
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    EXPECT_TRUE(r.Value().ProcessStatesReset.empty());
    EXPECT_FALSE(r.Value().SemanticDependencyPossible);
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}
```

- [ ] **Step 2: 跑，确认三条全红**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R 'Eject.SemanticDependency' --output-on-failure
```

Expected: **红**——`EjectReport` 还没有那个成员，先编译失败；这正是要的 TDD 起点。

- [ ] **Step 3: 加字段**

`Include/Vase/Host/Evidence.h`，在 `ProcessStatesReset` 之后、`HotSwapNote` 之前插入：

```cpp
    // §9.1 提示面 / M4-D110：**不是检出**——经进程级状态中转的共享值账本与导入表都看不见，
    // 本字段只如实上报「本次重置了进程态 ∧ 进程内仍有活插件实例 ⇒ 持派生值者可能已陈旧」。
    // 只数**活实例**（Failed 记录与空壳攥不住值，③ 闸另计它们、问的不是同一件事）；
    // Status == kRejectedConsumers 时恒 false（无重置发生）。
    bool SemanticDependencyPossible = false;
```

- [ ] **Step 4: 在 Host 里算它**

`Source/Host/PluginHost.cpp` 的 ③ 闸计数循环。把计数块改成（**新增 `liveInstancesElsewhere`，并把原来的一条 `else if` 拆成嵌套**）：

```cpp
    std::size_t crossPodInstances = 0;
    std::size_t crossPodFailedRecords = 0;
    std::size_t crossPodShellRefs = 0;
    // M4/D110：知情位要问的是「进程内还有没有别的活插件」——与 crossPodInstances（问的是
    // 「这份 binary 还有没有人指着」）不是同一个问题，故另计一个总数。被卸者此刻已从活集合摘除。
    std::size_t liveInstancesElsewhere = 0;
    for (const std::unique_ptr<PodSlot>& other : Slots)
    {
        if (!other->Alive || other->Inner == nullptr)
        {
            continue;
        }
        for (const std::unique_ptr<Pod::LiveInstance>& instance : other->Inner->Instances)
        {
            if (instance->Instance != nullptr)
            {
                ++liveInstancesElsewhere;
                if (instance->Binary == binary)
                {
                    ++crossPodInstances;
                }
            }
            else if (instance->Binary == binary)
            {
                ++crossPodShellRefs; // D94 起空壳可被 Eject——它同样读 Binary，不计即悬垂（M3/C1）
            }
        }
        for (const auto& entry : other->Inner->FailedBinaries)
        {
            if (entry.second == binary)
            {
                ++crossPodFailedRecords;
                break; // 同一局里两条不同 Id 指着同一记录只算一个持有者（只判零与非零）
            }
        }
    }
    report.CrossPodInstancesZeroed = crossPodInstances == 0;
```

然后在 Reset 块（`if (binary != nullptr && crossPodInstances == 0) { … }`）**之后**、kept-resident 提前返回**之前**插入：

```cpp
    // M4/D110：语义依赖知情位。**不假装查过**——经进程级状态中转的共享在图上看不见（§9.1），
    // 这里只如实说「重置发生了、而且进程内还有别人」。范围取进程内而非本局：进程级状态是
    // 进程级的（跨 Pod 存活），pod2 里的另一个插件照样可能攥着派生值。
    report.SemanticDependencyPossible = !report.ProcessStatesReset.empty() && liveInstancesElsewhere > 0;
```

- [ ] **Step 5: 跑，确认三条全绿 + 既有 Eject 用例不红**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R 'Eject' --output-on-failure
```

Expected: 全绿。

- [ ] **Step 6: 变异验证（证明正例有牙）**

临时把 Step 4 那行改成 `… = !report.ProcessStatesReset.empty();`（去掉后半句），重建、跑——正例 `SemanticDependencyPossibleWhenResetAndOthersLive` 与反例 a `…WhenNothingElseLive` 中**必有一条红**（两条件被拆开后，恒真或恒假必与某条证人冲突）。

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R 'Eject.SemanticDependency' --output-on-failure
```

Expected: **红**。看到红之后改回原样、重建、回绿。结论记进提交信息。

- [ ] **Step 7: Linux 侧跑同族**

```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug -R "Eject" --output-on-failure'
```

- [ ] **Step 8: 提交**

```bash
git add Include/Vase/Host/Evidence.h Source/Host/PluginHost.cpp Tests/HotSwap/EjectTests.cpp
git commit -m "M4-T5：语义依赖知情位（EjectReport 尾追加字段 + 三条证人 + 变异验证）"
```

---

### Task 6: 前端两份打点

**Files:**
- Modify: `Tools/VaseConsole/Console.cpp:150-173`（`PrintEjectReport`）
- Modify: `Samples/Embedding/main.cpp:131-165`（`PrintEjectReport`）

**Interfaces:**
- Consumes: `vase::EjectReport::SemanticDependencyPossible`、`vase::EjectStatus::kEjected`。

- [ ] **Step 1: Console 侧**

在 `Console.cpp` 的 `PrintEjectReport` 里，`ProcessStatesReset` 那段之后、`HotSwapNote` 之前插入：

```cpp
    // M4/D111：只在成功态打——bool 没有「空」，靠「拒绝态整行不打」把「判定无风险」与
    // 「根本没走到这一步」分开（字段语义见 Evidence.h）。
    if (report.Status == vase::EjectStatus::kEjected)
    {
        out << "  semanticDependencyPossible=" << BoolText(report.SemanticDependencyPossible) << '\n';
    }
```

- [ ] **Step 2: Embedding 侧**

`Samples/Embedding/main.cpp` 的 `PrintEjectReport` 里，对应的 `ProcessStatesReset` 块之后、`HotSwapNote` 之前插入同形的一段：

```cpp
    // M4/D111：只在成功态打——bool 没有「空」，靠「拒绝态整行不打」把「判定无风险」与
    // 「根本没走到这一步」分开（字段语义见 Evidence.h）。
    if (report.Status == vase::EjectStatus::kEjected)
    {
        Out("  semanticDependencyPossible=" + std::string{BoolText(report.SemanticDependencyPossible)} + "\n");
    }
```

- [ ] **Step 3: 两份自述注释改准**

两个 `PrintEjectReport` 上方的注释都写着「打 EjectReport **14** 字段中的 **8** 个」。加了字段之后是 **15 字段中的 9 个**——两份都改，且把新字段列进正向清单：

```
// §9.2「每次进出必须有账可查」的演示形态：打 EjectReport 15 字段中的 9 个——PluginId、档二×5（两个
// Is… 是 §8.2「哪个字段有判据力」的声明）、processStatesReset、semanticDependencyPossible 与注记。
// **不打 6 个**：Status/Consumers 由调用方 …（两文件各自的原有措辞保持不变）
```

- [ ] **Step 4: 构建并跑 console 族**

```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R 'VaseConsole' --output-on-failure
```

Expected: 绿（该族钉的是文本，**本步不该有红**——没有脚本断言这一行）。

- [ ] **Step 5: 肉眼核一次真输出**

```bash
./build-win/win-x64-clang-debug/bin/VaseEmbedding play
```

Expected: 输出里出现 `eject Vase.Hello …` 那行，其下有 `  semanticDependencyPossible=false`（Hello 没有进程级状态，故为 false 且**仍然打出**——这正是 D111 要的形态）。把实际输出粘进提交信息。

- [ ] **Step 6: 提交**

```bash
git add Tools/VaseConsole/Console.cpp Samples/Embedding/main.cpp
git commit -m "M4-T6：前端两份 PrintEjectReport 补知情位打点并改准自述字段数"
```

---

### Task 7: 文书义务

**Files:**
- Modify: `wiki/vase-architecture.md`（§12.1 判据 3 与新增 3f、§12.3 M4 行、§9.1）
- Modify: `CLAUDE.md`（项目状态、两张表、规矩 6 子串清单）
- Modify: `.claude/skills/vase-cpp-engineering/references/verification.md:27`

**Interfaces:**
- Consumes: 前六个任务的全部事实。

- [ ] **Step 1: `wiki` §12.1 加判据 3f**

在判据 3e 那一行之后插入一行（表格列：`#` / 承诺 / 验证方式 / 承重）：

```
| 3f | Eject 如实上报「语义依赖可能已陈旧」而**不宣称查过**（§9.1 提示面） | `Tests/HotSwap`——**已落**（M4/D110）：`Eject.SemanticDependencyPossibleWhenResetAndOthersLive`（重置发生 ∧ 进程内仍有活实例 → 置位）、`Eject.SemanticDependencyAbsentWhenNothingElseLive`（重置了但没人活着 → 不置位）、`Eject.SemanticDependencyAbsentWithoutProcessStateReset`（有人活着但无重置 → 不置位）。**空壳局一支无证人可写**（空壳攥不住值、条件本就不该置位），如实标注 | 否 |
```

- [ ] **Step 2: `wiki` §12.1 判据 3 补 Windows 侧证人**

在判据 3 的「验证方式」格末尾追加一句：

```
 Windows 侧的档三负例自 M4/D108 起有证人（`Adopt.RenameReplacementCaughtByTierThree` 两平台同序列，M1 的登记账已还）。
```

- [ ] **Step 3: `wiki` §12.3 M4 行标已落**

把 M4 行改成：

```
| M4 热替换 | **已落（Win / Linux，2026-09-27）**：换件谱扩到**描述符维含回退方向**（五步阶梯，每步只差一维；D105/D113）、**Windows 侧档三负例**（换件序列两平台统一为「改名离开+落新字节」；D108）、**语义依赖知情位**（`EjectReport::SemanticDependencyPossible`，如实上报不可知；D110/D115）。**macOS 腿顺延 M5**（与 M0–M3 平台口径一致） |
```

- [ ] **Step 4: `wiki` §9.1 把提示面写成实形**

在 §9.1 「`EjectReport` 提示面兜底」那句之后追加一段：

```
> **[M4 落地（2026-09-27，D110）]** 提示面的实形是 `EjectReport::SemanticDependencyPossible`：置位条件 = **`ProcessStatesReset` 非空 ∧ 进程内任一 Pod 仍有活插件实例**（只数活实例——Failed 记录与空壳攥不住派生值）。它**不是检出**：经进程级状态中转的共享值账本与导入表都看不见，这个字段只把「重置发生了、而且还有别人」如实写进报告，不宣称「查过了、没有语义依赖」。`Status == kRejectedConsumers` 时恒 `false`。
```

- [ ] **Step 5: 删技能里那行违反规矩 7 的基数真值**

`.claude/skills/vase-cpp-engineering/references/verification.md:27` 那一行（「Linux-only fixture 一族包在 `if(NOT WIN32)` 里（T11 的 `NoBuildIdPlugin`）：所以 Linux 的基数高于 Windows。」）——**整行删掉**。它 M4 之后是假话，而且**本来就违反规矩 7**（基数真值只准住 `CLAUDE.md`）。删完跑一次规矩 7 的自检 grep 确认没留孤儿指针：

```bash
grep -rn "Total Tests\|[0-9][0-9] TU\|DEBUG:FULL\|build-id=sha1\|EHs-c-\|HAS_EXCEPTIONS\|--cached --others\|WarningsAsErrors\|PRE_TEST\|ctest --preset\|run-clang-tidy -p\|cmake --build --preset" \
  .claude/skills/vase-cpp-engineering/ | grep -v cpp-core-guidelines.md
```

- [ ] **Step 6: `CLAUDE.md` 的其余部分留给 Task 8**

项目状态、两张表、规矩 6 子串清单要等收口实测读数出来才能写——**本任务不碰它们**（数字的唯一真值来源是 Task 8 的实测）。

- [ ] **Step 7: 提交**

```bash
git add wiki/vase-architecture.md .claude/skills/vase-cpp-engineering/references/verification.md
git commit -m "M4-T7：文书义务——判据 3f 入表、M4 行标已落、§9.1 提示面写实、清技能里一处违反规矩 7 的基数真值"
```

---

### Task 8: 收口全量验证、基数落账、折并

**Files:**
- Modify: `CLAUDE.md`（项目状态、六线基数表、tidy 表、规矩 6 子串清单）

**Interfaces:**
- Consumes: T1–T7 的全部事实。
- Produces: 六线实测读数与它的唯一真值处。

- [ ] **Step 1: 六线全量（删树重配）**

```bash
Scripts/win-verify.cmd
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && bash Scripts/linux-verify.sh'
```

Expected: 六线全绿、构建零警告。**Windows 侧首次删树重配可能撞 `z-applocal` 文件锁假红**——退出码非 0 时先重跑那一条线再判（CLAUDE.md 有记）。

- [ ] **Step 2: 读六线基数并逐位核差值**

```bash
ctest --preset win-x64-clang-debug -N
ctest --preset win-x64-clang-release -N
ctest --preset win-x64-msvc-debug -N
ctest --preset win-x64-msvc-release -N
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && ctest --preset linux-x64-clang-debug -N'
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && ctest --preset linux-x64-clang-release -N'
```

**预期形状（spec §5）**：新增用例六线同幅（阶梯 1 + 知情位 3 = 4）；`RenameReplacementCaughtByTierThree` 与 `MissingIdentityFeatureRejectedWithPointer` 由 Linux-only 转六线同跑（**Windows +2，Linux +0**——那两条本来就在 Linux 的计数里）。净结果 = **`linux − win` 的 +2 差值消失、六线同值**：

| 线 | M3 收口 | 本波 | 现值预期 |
|---|---|---|---|
| win 两线 debug | 261 | +4 +2 | **267** |
| linux debug | 263 | +4 +0 | **267** |
| 四条 release | 260 / 262 | 同上各自 | **266** |

`debug − release` 的 −1（T3 的 death test，`#ifndef NDEBUG`）不变。
**逐位核，不是照抄**——对不上就说出来，别把读数往预期上圆。

- [ ] **Step 3: tidy 三线**

```bash
Scripts/win-clang-tidy.cmd clangcl
Scripts/win-clang-tidy.cmd msvc
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && bash Scripts/linux-clang-tidy.sh'
```

Expected: 三判据（退出码 0 + 正文 `error:` 0 + 正文 `warning:` 0）。**TU 预期两侧同为 99**（Windows 96 → +2 新 fixture +1 `NoIdentityPlugin`；Linux 97 → +2 新 fixture）。新增代码若带 NOLINT，逐处确保就地写了理由。

- [ ] **Step 4: 格式门**

```bash
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
```

Expected: 无输出、exit 0。

- [ ] **Step 5: 规矩 6 选择子双平台**

```bash
ctest --preset win-x64-clang-debug -R 'HotSwap|Eject|Adopt' --output-on-failure
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && ctest --preset linux-x64-clang-debug -R "HotSwap|Eject|Adopt" --output-on-failure'
```

Expected: 两侧全绿；重数较 M3 终审（win 50 / linux 52）**两侧都上升**（新增用例名含 HotSwap / Eject / Adopt），且**两侧之差消失**（那两条不再 Linux-only）。

- [ ] **Step 6: 把读数写进 `CLAUDE.md`**

1. **项目状态**节首行标 M4 完成，把本波三笔写进去（换件谱描述符维含回退 / Windows 档三负例还清 M1 登记账 / 知情位）。
2. **基数表**：填六个整数，并把「差值」列改写成实情——`linux − win` **不再有差值**，六线同值；**同时保留一句「差值为什么变小」的说明**（那一节记的不只是数字）。
3. **tidy 表**：填两侧 TU 数与抑制合计，说明 TU +3（Windows）/ +2（Linux）的来源。
4. **规矩 6 的子串契约清单**：核 `Adopt` 面新增的拒绝子串有没有变动（本波不应新增契约 token——`manifest/binary mismatch` 是既有 token）。若确无变动，**不要动那一段**。
5. 记 `Tests/HotSwap/AdoptTests.cpp:249` 那笔登记账已还——**找到那行注释本身**并把「登记到 M4/M5」改准（代码侧改准属 T3 的收尾，若 T3 漏了就补在这里）。

- [ ] **Step 7: memory**

更新 `vase-v3-hotswap-status`（M4 三笔已落、D108 的序列统一、D110 的知情位口径、下一步 M5）与 `MEMORY.md` 索引行。**别在这里复制基数**——它们住 `CLAUDE.md`。

- [ ] **Step 8: 提交并折并**

```bash
git add CLAUDE.md
git commit -m "M4-T8：六线基数与 tidy 计数落账（收口复测）"
git log --oneline main..HEAD   # 核对本波全部提交
git rebase -i main             # 折成一笔（保留 message 一字不动），force-push 等需求方发话
```

---

## 偏离登记

执行中发现与本计划或 spec 不符的事实，**逐条记在这里**（计划会说错话，那是正常的；静默改掉不是）。格式：`发现时间 · 现象 · 实测取证 · 裁决`。

1. **（预登记，来自写计划阶段的两个发现，已回写 spec）**
   - **rung3 必须注册与声明同版本的接口**：`Context::Provide<T>()` 的键取自 `T::kVersion` 而非描述符；全仓**没有**「声明 vs 实注册」一致性检查（`PluginHost` 读的一律是 `Meta->Provides`）。故 `VersionedAServiceDrift` 注册 `ICounterV2`、该步行为读数经 v2 读。已写进 spec §2.1。
   - **S1 没有负例**：代码维没有描述符字段可漂，硬造只能去篡改一个与本步无关的字段。故 S1 只做一条全新装载正例。已写进 spec §2.3（D113 的「每步三条证人」以那条注为准）。
2. **（已填，Task 3 Step 5）Linux 侧换序结论：绿 ⇒ 统一成立，未走退路。** 换序后
   （`rename(P → P.old)` 再 `copy 新 → P`）Linux 侧 `Adopt.RenameReplacementCaughtByTierThree` 全绿，
   `rename` / `copy_file` 两行没有报错（构建树里没有别的东西开着该文件）。**这个绿不是白绿**：用例断言
   拒绝消息里的 `differ` / `rebuild` 两个子串，而该文本只由档三比对失败这条路发出——若驻留分支没被取到，
   Adopt 会返回 Ok、`ASSERT_FALSE(r.IsOk())` 当场红。故**没有**退回两平台各自序列，也**没有**任何为
   换件序列而设的 `#ifdef`（D108 因此是干净的）。取证见 task-3-report.md §4/§5。
3. **（已填，Task 2 / Task 5 两处变异验证）**
   - **Task 2（换件谱阶梯）**：变异打在 `Source/Host/Detail/ManifestCompare.cpp` 的 `CompareServiceSet`
     两行——把 `std::pair` 字典序比较改成只比 `.first`（服务名），**版本维即被短路**。**红落在 S3
     「Provides 去」**（`HotSwapLoopTests.cpp:306` 的 `stale.IsOk()`，`SCOPED_TRACE` 的步号 Label 如期
     打出），即该步负例真有牙；顺带证了五步的维**可分**——S2/S4/S5 的 `Version` 维仍被 Version 串比对
     兜住，不是一次全红。在**冻结尾稿**上复跑一轮，红点与 Label 完全相同（红线不是挂在中间态上）。
   - **Task 5（知情位）**：变异把置位条件弱化成 `report.SemanticDependencyPossible =
     !report.ProcessStatesReset.empty();`（去掉「∧ 进程内仍有活实例」那一合取项）。**红落在反例 a
     `Eject.SemanticDependencyAbsentWhenNothingElseLive`**（`EjectTests.cpp:361`，Actual true /
     Expected false）；正例与反例 b 在变异下仍绿——可解释：弱化后那两条的取值都不变。**该变异按字面
     落盘编不过**（去掉合取项后 `liveInstancesElsewhere` 变成 set-but-not-used，撞 `-Werror`），按字面跑的
     那一轮 ctest 用的是旧二进制、是**假绿**；加一处 `[[maybe_unused]]` 让变异真编出后才拿到上面那个红。
     两处临时改动已全部撤回。取证见 task-5-report.md §3。
4. **（已填，Task 8 Step 2）六线读数与预期形状逐位对上，无一例外。** 删树重配全量实测：
   `win-x64-clang-debug` **267**、`win-x64-clang-release` **266**、`win-x64-msvc-debug` **267**、
   `win-x64-msvc-release` **266**、`linux-x64-clang-debug` **267**、`linux-x64-clang-release` **266**，
   六线 `ctest` 均 100% 通过、构建零警告——与 §5 预期表（267/267 与四条 266）逐位相同。
   用例名集合的差集独立核过：debug − release 在三条 debug 线上**恰为** `EffectScopeDeath.CreateAfterDisposeTerminates`
   （T3 的 `#ifndef NDEBUG` 门），`linux − win` 与 `win − linux` **两个方向都为空**（D109 消掉的 +2 差值确已归零）。
   **唯一一次非绿是环境性假红**：`win-x64-msvc-release` 的 `z-applocal` 拷贝步撞文件锁（目标 `VaseEmbedding.exe`，
   链接已成功），build 步 RC=1、连带该树 ctest 80 条红；按 CLAUDE.md 记的协议单线删树重跑即 266/266 全绿零警告。
   这是该类抖动第四次出现、第一次落在 clang 线之外，已回写 CLAUDE.md。
