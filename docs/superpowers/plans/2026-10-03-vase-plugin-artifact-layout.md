# 插件产物布局：一插件一目录 · 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把每个插件目标的产物从平铺的 `bin/`(Windows) / `lib/`(POSIX) 改为独占 `<产物根>/<target 名>/`，并把三份描述样本插件的手写清单搬到清单所描述的插件旁边。

**Architecture:** 插件形态的唯一出口 `vase_add_plugin_fixture`（`Cmake/VasePluginHelpers.cmake`）是全仓 44 个插件目标的共同入口，所以布局规则与清单拷贝都只在这一处实现；消费者全部走 `$<TARGET_FILE:...>` 绝对路径，因此除清单路径外零散改无。

**Tech Stack:** CMake 4.x + Ninja + 八个 preset（Win clang-cl/cl.exe、Linux clang、macOS clang）、vcpkg manifest、GoogleTest 1.18 + ctest、clang-tidy/clang-format 23.1.x。

**Spec:** `docs/superpowers/specs/2026-10-03-vase-plugin-artifact-layout-design.md`（下称 spec；决策号 D180–D195 均指该文）

## Global Constraints

以下每一条都是 spec 的承重要求，每个任务默认继承：

- **目录名 = target 名**（D183/D194），不是 `OUTPUT_NAME`、不是插件 Id。
- **产物根随平台**：Windows `${CMAKE_RUNTIME_OUTPUT_DIRECTORY}`（= `build-win/<preset>/bin`），Linux/macOS `${CMAKE_LIBRARY_OUTPUT_DIRECTORY}`（= `build-linux|macos/<preset>/lib`）（D185）。
- **框架库与可执行不成目录**：`VasePod`/`VaseHost`/`VaseCatalog`、`libVaseCliCore.a`、gtest、以及宿主可执行一律留在产物根平铺。
- **`ARCHIVE_OUTPUT_DIRECTORY` 不动**：Windows 的导入库 `.lib` 与 Linux 的 `.a` 继续留 `lib/`。
- **`Versioned` 四兄弟是记名的例外**（D194）：`VersionedA`/`VersionedAPrime`/`VersionedAStampDrift`/`VersionedAServiceDrift` 维持 `Tests/HotSwap/fixtures/stage*` 各自目录，不进产物根，且那四段显式设置必须留在 helper 调用**之后**。
- **清单按需**（D181）：只有 `Samples/{HelloPlugin,DependentPlugin,FailingPlugin}` 三个带 `plugin.json`；`HelloPluginPrime` 与其余 36 个插件目标不带。
- **console 回放树一个字不改**（D190）：`Tools/VaseConsole/replay/<case>/<Plugin>/` 保留副本，不改建到产物根。
- **不为依赖解析加机器守卫**（D195）：那条前提是自证的，加守卫等于给恒真式写断言；本波只把它写进文案。
- **插件 target 一律经 `vase_add_plugin_fixture`**（CLAUDE.md 规矩 5）——本波不改这条。
- **全项目无异常**：不写 `throw`/`try`/`catch`；错误走 `Result<T>`/`Error`（CLAUDE.md 语言约定）。
- **测试里禁用 `EXPECT_THROW`/`ASSERT_THROW`/`EXPECT_ANY_THROW`/`EXPECT_NO_THROW`**（CLAUDE.md 规矩 2，编译期硬失败）。
- **Vase 自己的头一律引号包含**（CLAUDE.md 规矩 3）。
- **提交信息用中文、不加任何 AI 署名尾注**（CLAUDE.md 语言约定）。
- **基数与命令以 `CLAUDE.md` 为准**，计划里出现的数字若与它冲突，以它为准。

**预期基数变化**：gtest **+3**（三条用例各有独立失败信号）、ctest 级 **+1**、tidy TU **+1**。故 `ctest -N` 由 debug 337 / release 336 变为 **debug 341 / release 340**（八线同幅，无平台门、无 `#ifndef NDEBUG` 门）。

> **与 spec §8 的差额**：spec §8 预算的是「gtest **+1**（一条用例内断言多个目标）」。本计划把断言拆成三条用例——布局主断言 / `Versioned` 例外 / 清单就位——理由是每条要有自己的失败信号（仓库既有的 `Eject.SemanticDependency*` 族就是同款拆法）。差额在 Task 9 Step 7 一并把 spec §8 改准，并记进末尾的偏离登记。

---

### Task 1: 把布局不变式立成失败测试

**Files:**
- Create: `Tests/Unit/ArtifactLayoutTests.cpp`
- Modify: `Tests/CMakeLists.txt`（`add_executable(VaseTests ...)` 的 SOURCES 列表）

**Interfaces:**
- Consumes: `Tests/CMakeLists.txt` 已定义的 `VASE_FIXTURE_*` 宏（`$<PATH:CMAKE_PATH,$<TARGET_FILE:X>>` 绝对路径，正斜杠）。
- Produces: 测试套件 `ArtifactLayout`，两条用例 `PluginBinariesLiveInTheirOwnTargetDirectory` 与 `VersionedVariantsKeepTheirStageDirectories`。

- [ ] **Step 1: 写失败测试**

创建 `Tests/Unit/ArtifactLayoutTests.cpp`：

```cpp
// 插件产物布局不变式（spec D183/D189/D194）：每个插件独占 <产物根>/<target 名>/。
// 本波之前没有任何测试枚举构建产物根（spec 取证注⑦），这份是布局规则唯一的自动化证据。

#include <filesystem>
#include <string>

#include <gtest/gtest.h>

namespace
{

// 两个断言一起才有意义：文件真在盘上，且它的父目录名就是 target 名。
void ExpectOwnsDirectory(const char* binaryPath, const std::string& targetName)
{
    const std::filesystem::path binary{binaryPath};
    EXPECT_TRUE(std::filesystem::exists(binary)) << binary.string();
    EXPECT_EQ(binary.parent_path().filename().string(), targetName) << binary.string();
}

} // namespace

TEST(ArtifactLayout, PluginBinariesLiveInTheirOwnTargetDirectory)
{
    ExpectOwnsDirectory(VASE_FIXTURE_HELLO, "HelloPlugin");
    ExpectOwnsDirectory(VASE_FIXTURE_DEPENDENT, "DependentPlugin");
    ExpectOwnsDirectory(VASE_FIXTURE_FAILING, "FailingPlugin");
    ExpectOwnsDirectory(VASE_FIXTURE_LOADPROBE, "LoadProbe");
    ExpectOwnsDirectory(VASE_FIXTURE_NOIDENTITY, "NoIdentityPlugin");
    ExpectOwnsDirectory(VASE_FIXTURE_NEIGHBORC, "NeighborC");
}

// D194：四个同 OUTPUT_NAME 的变体维持各自的 stage 目录，不进产物根——
// 「顺手统一」会当场撞回 Ninja 的 multiple rules generate .../VersionedA.lib。
TEST(ArtifactLayout, VersionedVariantsKeepTheirStageDirectories)
{
    ExpectOwnsDirectory(VASE_FIXTURE_VERSIONEDA, "stageA");
    ExpectOwnsDirectory(VASE_FIXTURE_VERSIONEDAPRIME, "stageAPrime");
    ExpectOwnsDirectory(VASE_FIXTURE_VERSIONEDASTAMPDRIFT, "stageStampDrift");
    ExpectOwnsDirectory(VASE_FIXTURE_VERSIONEDASERVICEDRIFT, "stageServiceDrift");
}
```

在 `Tests/CMakeLists.txt` 的 `add_executable(VaseTests` 列表里，把 `Unit/ArtifactLayoutTests.cpp` 插在 `Unit/CatalogWiringSmoke.cpp` **之后**（紧邻既有 Unit 文件，保持列表局部有序）。

- [ ] **Step 2: 跑测试，确认它按预期的理由失败**

```bash
cmake --preset win-x64-clang-debug
cmake --build --preset win-x64-clang-debug
build-win/win-x64-clang-debug/bin/VaseTests.exe --gtest_filter='ArtifactLayout.*'
```

预期：两条用例都 FAIL。`PluginBinariesLiveInTheirOwnTargetDirectory` 的失败信息形如
`Expected equality of these values: binary.parent_path().filename().string() Which is: "bin" / targetName Which is: "HelloPlugin"`——**失败理由必须是「父目录是 `bin`」**，不是「文件不存在」。若报 `exists` 失败，说明宏名或构建没起来，先修那个。

> Linux 侧等价命令：`build-linux/linux-x64-clang-debug/bin/VaseTests --gtest_filter='ArtifactLayout.*'`（父目录预期是 `"lib"`）。

- [ ] **Step 3: 提交红测试**

```bash
git add Tests/Unit/ArtifactLayoutTests.cpp Tests/CMakeLists.txt
git commit -m "测试：插件产物布局不变式先立成失败测试（红）"
```

---

### Task 2: `vase_add_plugin_fixture` 设输出目录

**Files:**
- Modify: `Cmake/VasePluginHelpers.cmake`（整个 `vase_add_plugin_fixture` 函数体）

**Interfaces:**
- Consumes: 根 `CMakeLists.txt:47-49` 定义的 `CMAKE_RUNTIME_OUTPUT_DIRECTORY` / `CMAKE_LIBRARY_OUTPUT_DIRECTORY`（目录作用域继承，子目录可见）。
- Produces: 全仓插件目标的产物落 `<产物根>/<target 名>/`；函数签名暂不变（`MANIFEST` 参数在 Task 5 加）。

- [ ] **Step 1: 实现**

把 `Cmake/VasePluginHelpers.cmake` 的函数体改成（注释按仓库口径：只写「删掉会踩什么坑」）：

```cmake
# 插件形态的模板：SHARED + 只链 VasePod（D11 的链接期事实）+ 收紧可见性 + 产物落自己的目录。
# M1 后续所有插件 target（Sample 与 fixture）都走这一个函数——插件形态定义只此一处，
# 防「某个 fixture 忘了 hidden visibility / 忘了链 VaseBuildOptions」（CLAUDE.md 规矩 1）。
#
# 一插件一目录（spec D180/D183）：产物落 <产物根>/<target 名>/，宿主可执行与框架库仍平铺。
# Tests/HotSwap/fixtures 的 Versioned 四兄弟是记名的例外——它们**在调用本函数之后**
# 显式设三项输出目录到各自的 stage* 目录（D194），靠的是「后设者胜」，顺序别调换。
function(vase_add_plugin_fixture name)
    cmake_parse_arguments(FIX "" "" "SOURCES;LINK_LIBRARIES" ${ARGN})
    add_library(${name} SHARED ${FIX_SOURCES})
    target_link_libraries(${name}
        PRIVATE VaseBuildOptions ${FIX_LINK_LIBRARIES})
    set_target_properties(${name} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${name}")
endfunction()
```

- [ ] **Step 2: 重建并跑布局测试，确认转绿**

```bash
cmake --preset win-x64-clang-debug
cmake --build --preset win-x64-clang-debug
build-win/win-x64-clang-debug/bin/VaseTests.exe --gtest_filter='ArtifactLayout.*'
```

预期：`[  PASSED  ] 2 tests.`

- [ ] **Step 3: 跑整条线全量，确认没有别的测试因布局搬动作废**

```bash
ctest --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -N
```

预期：全绿；`Total Tests` = **339**（基线 337 + Task 1 的两条 gtest）。

> 这一步是关键回归点：`Tests/HotSwap` 的换件序列、`CatalogSandbox` 的沙箱复制、`VaseConsole` 回放 staging 都吃 `$<TARGET_FILE:...>`，理论上全部自动跟随——**这一步就是验「理论上」**。任何红都先按「缺产物型红」读一遍（见 CLAUDE.md「这一档有环境性假红」），再判真伪。

- [ ] **Step 4: 复跑 spec 取证注① 的探针形态**（D193）

```bash
cd build-win/win-x64-clang-debug/bin
rm -rf ProbeRoot && mkdir -p ProbeRoot/HelloPlugin
cp HelloPlugin/HelloPlugin.dll ProbeRoot/HelloPlugin/
cp ../../../Tests/Integration/fixtures/manifests/hello/plugin.json ProbeRoot/HelloPlugin/
printf 'pod new ProbeRoot\npod list\npod destroy\nexit\n' > probe_script.txt
./VaseConsole.exe --script probe_script.txt   # 期望 clean=true、RC=0
rm -rf ProbeRoot probe_script.txt
```

- [ ] **Step 5: 提交**

```bash
git add Cmake/VasePluginHelpers.cmake
git commit -m "插件产物改为一插件一目录：vase_add_plugin_fixture 设输出目录"
```

---

### Task 3: macOS 依赖解析实测（失败即停下重裁）

**Files:** 无（纯验证；若失败，停下并回到需求方重裁，见 spec D192）

**Interfaces:**
- Consumes: Task 2 的布局变更已在本分支。
- Produces: 一段可写进 spec 取证注④ 的实测记录（`LC_RPATH` 内容 + 装载结果），或一次「停下重裁」。

> 本任务在 **macOS 开发机**上跑（仓库路径是 SMB 挂载的同一份工作树；经 ssh 调用必须带 `-o BatchMode=yes`，主机别名见本机 ssh config——**不入库**，与 spec D179 同取向）。

- [ ] **Step 1: 在 macOS 线上配置并构建**

```bash
cmake --preset macos-x64-clang-debug
cmake --build --preset macos-x64-clang-debug
```

- [ ] **Step 2: 读插件的 `LC_RPATH` 与依赖**

```bash
otool -l build-macos/macos-x64-clang-debug/lib/HelloPlugin/libHelloPlugin.dylib | grep -A2 LC_RPATH
otool -L build-macos/macos-x64-clang-debug/lib/HelloPlugin/libHelloPlugin.dylib
```

预期：`LC_RPATH` 指向 `.../lib`（框架库所在），`LC_LOAD_DYLIB` 里有 `@rpath/libVasePod.dylib`。

- [ ] **Step 3: 跑整条线**

```bash
ctest --preset macos-x64-clang-debug
```

预期：全绿、`Total Tests` = 339（与 Windows 侧同值——本波无平台门）。

- [ ] **Step 4: 判定**

- **通过**：把 Step 2 的输出原文写进 spec 取证注④（把「未实测」改成实测值），并提交该 spec 修改。
- **失败**（插件装载起不来）：**停下**。不要加 `-Wl,-rpath,@loader_path/..` 之类的补丁——按 D192 回到需求方重裁。

```bash
git add docs/superpowers/specs/2026-10-03-vase-plugin-artifact-layout-design.md
git commit -m "spec 取证注④：macOS 插件挪层后的 LC_RPATH 与依赖解析实测"
```

---

### Task 4: 清单搬家 + 消费者改指

**Files:**
- Create: `Samples/HelloPlugin/plugin.json`、`Samples/DependentPlugin/plugin.json`、`Samples/FailingPlugin/plugin.json`
- Delete: `Tests/Integration/fixtures/manifests/hello/plugin.json`、`.../dependent/plugin.json`、`.../failing/plugin.json`
- Modify: `Tests/CMakeLists.txt`（加三个宏）、`Tests/HotSwap/AdoptManifestTests.cpp:51`、`Tests/Integration/AssemblyFromSolveTests.cpp:56-100`、`Tests/Integration/VaseCliPlanTests.cpp:86,98`、`Tools/VaseConsole/CMakeLists.txt:21`

**Interfaces:**
- Consumes: 现状 `VASE_FIXTURE_MANIFESTS`（指向 `Tests/Integration/fixtures/manifests`，**保持不动**——`shared_provider`/`shared_consumer2`/`edge_consumer` 三份仍住那里）。
- Produces: 三个新宏
  `VASE_FIXTURE_HELLO_MANIFEST` / `VASE_FIXTURE_DEPENDENT_MANIFEST` / `VASE_FIXTURE_FAILING_MANIFEST`，
  值与既有宏同形：`"$<PATH:CMAKE_PATH,${PROJECT_SOURCE_DIR}/Samples/<X>/plugin.json>"`。

> **不改的**：`Tests/Integration/VaseCliValidateTests.cpp:160,177` 用的是 `shared_provider`，**原地不动**。

- [ ] **Step 1: 搬文件（内容逐字不动）**

```bash
git mv Tests/Integration/fixtures/manifests/hello/plugin.json     Samples/HelloPlugin/plugin.json
git mv Tests/Integration/fixtures/manifests/dependent/plugin.json Samples/DependentPlugin/plugin.json
git mv Tests/Integration/fixtures/manifests/failing/plugin.json   Samples/FailingPlugin/plugin.json
```

搬完逐字核对一遍内容没变（`git diff --cached -M --stat` 应显示 rename，且 `git show --cached --stat` 无内容改动）。

- [ ] **Step 2: 加三个宏**

在 `Tests/CMakeLists.txt` 的 `target_compile_definitions(VaseTests PRIVATE ...)` 块内、`VASE_FIXTURE_MANIFESTS=...` 那一行**之前**插入：

```cmake
    # 这三份描述的是 Samples 的三个插件，故随插件住（spec D182）；其余三份描述
    # fixtures 下的 fixture 插件，仍在 VASE_FIXTURE_MANIFESTS 下。
    VASE_FIXTURE_HELLO_MANIFEST="$<PATH:CMAKE_PATH,${PROJECT_SOURCE_DIR}/Samples/HelloPlugin/plugin.json>"
    VASE_FIXTURE_DEPENDENT_MANIFEST="$<PATH:CMAKE_PATH,${PROJECT_SOURCE_DIR}/Samples/DependentPlugin/plugin.json>"
    VASE_FIXTURE_FAILING_MANIFEST="$<PATH:CMAKE_PATH,${PROJECT_SOURCE_DIR}/Samples/FailingPlugin/plugin.json>"
```

- [ ] **Step 3: 改三个消费者**

`Tests/HotSwap/AdoptManifestTests.cpp:51`：

```cpp
        Sandbox.CopyFile("VaseHello/plugin.json", VASE_FIXTURE_HELLO_MANIFEST);
```

`Tests/Integration/VaseCliPlanTests.cpp:86` 与 `:98`（两处同形）：

```cpp
    sandbox.CopyFile("Dep/plugin.json", VASE_FIXTURE_DEPENDENT_MANIFEST);
```

`Tests/Integration/AssemblyFromSolveTests.cpp`：给两个 roster 各加一个清单字段，删掉 `manifests / item.Dir / "plugin.json"` 的拼法。

```cpp
    void StageAll()
    {
        struct Roster
        {
            const char* Dir;
            const char* BinaryMacro;
            const char* Manifest;
        };
        const std::array<Roster, 4> roster{
            {
                Roster{.Dir = "hello", .BinaryMacro = VASE_FIXTURE_HELLO, .Manifest = VASE_FIXTURE_HELLO_MANIFEST},
                Roster{.Dir = "shared_provider",
                       .BinaryMacro = VASE_FIXTURE_SHAREDPROVIDER,
                       .Manifest = VASE_FIXTURE_SHAREDPROVIDER_MANIFEST},
                Roster{.Dir = "edge_consumer",
                       .BinaryMacro = VASE_FIXTURE_EDGECONSUMER,
                       .Manifest = VASE_FIXTURE_EDGECONSUMER_MANIFEST},
                Roster{.Dir = "shared_consumer2",
                       .BinaryMacro = VASE_FIXTURE_SHAREDCONSUMER2,
                       .Manifest = VASE_FIXTURE_SHAREDCONSUMER2_MANIFEST},
            },
        };
        for (const Roster& item : roster)
        {
            const std::filesystem::path binary{item.BinaryMacro};
            Sandbox.CopyFile(std::string(item.Dir) + "/plugin.json", item.Manifest);
            Sandbox.CopyFile(std::string(item.Dir) + "/" + binary.filename().string(), binary);
        }
        const auto refreshed = Catalog.Refresh(Sandbox.Root);
        ASSERT_TRUE(refreshed.IsOk()) << refreshed.GetError().Message();
    }
```

那三个仍住 fixture 目录的清单宏**同样在 `Tests/CMakeLists.txt` 里加**（与上面三条并排）：

```cmake
    VASE_FIXTURE_SHAREDPROVIDER_MANIFEST="$<PATH:CMAKE_PATH,${CMAKE_CURRENT_SOURCE_DIR}/Integration/fixtures/manifests/shared_provider/plugin.json>"
    VASE_FIXTURE_EDGECONSUMER_MANIFEST="$<PATH:CMAKE_PATH,${CMAKE_CURRENT_SOURCE_DIR}/Integration/fixtures/manifests/edge_consumer/plugin.json>"
    VASE_FIXTURE_SHAREDCONSUMER2_MANIFEST="$<PATH:CMAKE_PATH,${CMAKE_CURRENT_SOURCE_DIR}/Integration/fixtures/manifests/shared_consumer2/plugin.json>"
```

改完之后 `grep -rn VASE_FIXTURE_MANIFESTS Tests/ Tools/` 应只剩 `Tests/CMakeLists.txt:111` 处的**定义**——本步把全部八处消费点都换成了 per-item 宏，所以**同时删掉那一行定义**（留一个无人用的宏等于留一个会腐的指针）。

`Stage()`（同文件 `:89-100`）同样改：`StagedPlugin` 加 `const char* Manifest;`，两处调用点（`:184-188`、`:223-226`）分别填 `VASE_FIXTURE_HELLO_MANIFEST` / `VASE_FIXTURE_DEPENDENT_MANIFEST` / `VASE_FIXTURE_FAILING_MANIFEST`。

`Tools/VaseConsole/CMakeLists.txt:21`：

```cmake
set(VASE_CONSOLE_HELLO_MANIFEST "${PROJECT_SOURCE_DIR}/Samples/HelloPlugin/plugin.json")
```

- [ ] **Step 4: 重建并跑受影响的测试**

```bash
cmake --preset win-x64-clang-debug
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug
```

预期：全绿（本步**不改**基数——只是换文件位置，用例数不变）。

- [ ] **Step 5: 提交**

```bash
git add -A Samples Tests Tools/VaseConsole/CMakeLists.txt
git commit -m "三份样本插件清单搬到 Samples/<插件>/：清单与它描述的插件同址"
```

---

### Task 5: `MANIFEST` 参数 + Samples 传参

**Files:**
- Modify: `Cmake/VasePluginHelpers.cmake`、`Samples/HelloPlugin/CMakeLists.txt:3`、`Samples/DependentPlugin/CMakeLists.txt:2`、`Samples/FailingPlugin/CMakeLists.txt:2`
- Test: `Tests/Unit/ArtifactLayoutTests.cpp`（加第三条用例）

**Interfaces:**
- Consumes: Task 2 的 `RUNTIME/LIBRARY_OUTPUT_DIRECTORY` 设置；Task 4 落位的三个 `Samples/<X>/plugin.json`。
- Produces: `vase_add_plugin_fixture(<name> SOURCES ... LINK_LIBRARIES ... [MANIFEST <path>])`——给了 `MANIFEST` 就把该文件拷成 `$<TARGET_FILE_DIR:<name>>/plugin.json`。

- [ ] **Step 1: 写失败测试**

在 `Tests/Unit/ArtifactLayoutTests.cpp` 末尾追加：

```cpp
// D181/D182：清单随插件住进产物目录，只有 Samples 三个带。
TEST(ArtifactLayout, HandWrittenManifestsAreStagedBesideTheirBinary)
{
    const std::filesystem::path hello{VASE_FIXTURE_HELLO};
    EXPECT_TRUE(std::filesystem::exists(hello.parent_path() / "plugin.json")) << hello.parent_path().string();

    // prime 是换件材料、不是独立插件（D184）：它没有清单。
    const std::filesystem::path neighbor{VASE_FIXTURE_NEIGHBORC};
    EXPECT_FALSE(std::filesystem::exists(neighbor.parent_path() / "plugin.json")) << neighbor.parent_path().string();
}
```

- [ ] **Step 2: 跑测试确认失败**

```bash
cmake --build --preset win-x64-clang-debug
build-win/win-x64-clang-debug/bin/VaseTests.exe --gtest_filter='ArtifactLayout.HandWrittenManifestsAreStagedBesideTheirBinary'
```

预期：FAIL（`bin/HelloPlugin/plugin.json` 还不存在）。

- [ ] **Step 3: 实现**

`Cmake/VasePluginHelpers.cmake`：`cmake_parse_arguments` 的关键字列表加 `MANIFEST`，并在函数末尾追加：

```cmake
    if(FIX_MANIFEST)
        # POST_BUILD 而非 configure 期拷贝：源清单改了要能自动跟上（D186 的「拷贝」形态）。
        add_custom_command(TARGET ${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${FIX_MANIFEST}" "$<TARGET_FILE_DIR:${name}>/plugin.json"
            COMMENT "staging plugin.json for ${name}")
    endif()
```

三个 Samples CMakeLists 各加一行（示例为 HelloPlugin；另两个同形，只换名字）：

```cmake
vase_add_plugin_fixture(HelloPlugin
    SOURCES HelloPlugin.cpp
    LINK_LIBRARIES VasePod
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json")
```

**不要**给 `HelloPluginPrime` 加（D184）。

- [ ] **Step 4: 重建并确认转绿**

```bash
cmake --preset win-x64-clang-debug
cmake --build --preset win-x64-clang-debug
build-win/win-x64-clang-debug/bin/VaseTests.exe --gtest_filter='ArtifactLayout.*'
ls build-win/win-x64-clang-debug/bin/HelloPlugin/    # 期望 HelloPlugin.dll + plugin.json
ls build-win/win-x64-clang-debug/bin/HelloPluginPrime/  # 期望只有 dll
```

预期：`[  PASSED  ] 3 tests.`

- [ ] **Step 5: 提交**

```bash
git add Cmake/VasePluginHelpers.cmake Samples Tests/Unit/ArtifactLayoutTests.cpp
git commit -m "vase_add_plugin_fixture 增 MANIFEST 参数：清单随插件拷进产物目录"
```

---

### Task 6: 产物根的 ctest 证人

**Files:**
- Modify: `Tools/VaseCli/CMakeLists.txt`（末尾追加）

**Interfaces:**
- Consumes: Task 5 的产物目录形态（3 个带清单 + 37 个不带）。
- Produces: ctest 用例 `VaseCliValidateProductRoot`（期望 rc=0）。

- [ ] **Step 1: 加测试**

在 `Tools/VaseCli/CMakeLists.txt` 末尾追加：

```cmake
# 产物根的布局＋清单一致性证人（spec D189）：指着整棵产物根跑 validate。
# D128：缺清单只 warn、不计退出码，故根里 37 个无清单插件不拖 rc——
# 本用例钉的是「根可刷新」+「三个手写清单与其二进制逐字段一致」。
if(WIN32)
    set(VASE_PLUGIN_ROOT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
else()
    set(VASE_PLUGIN_ROOT "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif()
add_test(NAME VaseCliValidateProductRoot COMMAND VaseCli validate "${VASE_PLUGIN_ROOT}")
```

- [ ] **Step 2: 跑它，确认它真的在测东西**

```bash
cmake --preset win-x64-clang-debug
cmake --build --preset win-x64-clang-debug
build-win/win-x64-clang-debug/bin/VaseCli.exe validate build-win/win-x64-clang-debug/bin
```

预期：末行 `validate: ok` 一类结论、退出码 0，输出里能看到 37 条 `warning: <名字>: no plugin.json — skipped` 与三条逐插件 `ok`。

- [ ] **Step 3: 变异取证——确认这条用例有判据力**

临时把 `Samples/FailingPlugin/plugin.json` 的 `"displayName"` 改一个字符，重建后重跑：

```bash
cmake --build --preset win-x64-clang-debug
build-win/win-x64-clang-debug/bin/VaseCli.exe validate build-win/win-x64-clang-debug/bin; echo "RC=$?"
```

预期：**RC≠0** 且输出点名 `Vase.Failing` 的清单↔二进制不一致。确认后**改回原值**并重建。

- [ ] **Step 4: 跑基数**

```bash
ctest --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -N
```

预期：全绿；`Total Tests` = **341**（基线 337 + Task 1 的两条 gtest + Task 5 的一条 gtest + 本任务 ctest 级 +1）。

- [ ] **Step 5: 提交**

```bash
git add Tools/VaseCli/CMakeLists.txt
git commit -m "ctest：指着整棵产物根跑 validate，钉布局与手写清单一致性"
```

---

### Task 7: 文书（一）——CLAUDE.md 与根 CMakeLists 注释

**Files:**
- Modify: `CLAUDE.md`（产物落位句、「工具链 flag 是承重的」段、规矩 6、项目状态、欠账清单）、`CMakeLists.txt:45-46`（注释）

- [ ] **Step 1: 根 `CMakeLists.txt:45-46` 的注释改准**

原文里「可执行与动态库放同一目录 …… 同处 `bin/` 才能直接跑起来」这句是承重句本体，改成：

```cmake
# 产物落位：可执行在 bin/；POSIX 侧框架库与静态库在 lib/。
# **每个插件独占 <产物根>/<target 名>/**（Windows bin/、Linux/macOS lib/）——
# 规则与理由（三平台的依赖解析依据）见 Cmake/VasePluginHelpers.cmake 与 CLAUDE.md。
# 注意：下面的 *_OUTPUT_DIRECTORY 仍是产品根的默认值，插件目标的子目录由
# vase_add_plugin_fixture 在 target 级覆盖，不要在这里改。
```

- [ ] **Step 2: `CLAUDE.md` 产物落位句改准**

在「构建与测试」一节的 Windows 小节里，把「构建产物落 `build-win/<presetName>/`，可执行与 DLL 同处 `bin/`——这是 Windows 能找到 DLL 的前提，不要改 `CMAKE_RUNTIME_OUTPUT_DIRECTORY`。」整段换成 spec §4 的那段引文（逐字照抄，含「改回平铺……必须按规矩 6 重跑三平台」那一句）。

- [ ] **Step 3: `CLAUDE.md` 承重段补条款**（D195）

在「工具链 flag 是承重的」一节里，与 `/DEBUG:FULL`、`--build-id=sha1` 并列加一条：

> **`LOAD_WITH_ALTERED_SEARCH_PATH`（`Source/Host/LoaderWindows.cpp`）**：插件住 `bin/<名>/` 而框架库住 `bin/`，Windows 上这两者能对上，靠的是「**加载器与被依赖的框架库同进程**」——该 flag 把依赖搜索基改成被加载模块自己的目录，而 `VasePod.dll` 早已被宿主静态导入、按已加载模块名命中，根本没走文件系统搜索。摘掉这个 flag、或把 `Loader` 挪出 `VaseHost`，症状都是**插件加载全线失败**，而两个动作看起来都无害。

- [ ] **Step 4: `CLAUDE.md` 规矩 6 增第六类触发**（D191）

规矩 6 的触发清单（现为「Loader、依赖账本、Eject / Adopt 路径、描述符布局或 `HeaderVersion`」）末尾加 **「产物布局」**，并在同段补一句：本波（插件产物布局）就是活证——一次布局改动同时影响所有 Loader 调用方与三平台依赖解析。

- [ ] **Step 5: `CLAUDE.md` 项目状态与欠账清单**

在「项目状态」段追加本波一笔（改了什么、基数从 337/336 到 339/338 → Task 6 后 340/339、tidy TU +1），并把 M5 记名欠账从「余 D125、VasePack 两件」改为「**余 VasePack 一件**」——D125 以「拷贝」形态兑现并结案（spec §5）。

- [ ] **Step 6: 跑自检 grep**

```bash
grep -rn "Total Tests\|[0-9][0-9] TU\|DEBUG:FULL\|build-id=sha1\|EHs-c-\|HAS_EXCEPTIONS\|--cached --others\|WarningsAsErrors\|PRE_TEST\|ctest --preset\|run-clang-tidy -p\|cmake --build --preset" \
  .claude/skills/vase-cpp-engineering/ | grep -v cpp-core-guidelines.md
```

预期：命中项逐条判类别（CLAUDE.md 规矩 7 的两类允许项），本波**不新增**技能侧字面值。

- [ ] **Step 7: 提交**

```bash
git add CLAUDE.md CMakeLists.txt
git commit -m "文书：CLAUDE.md 产物落位句改准 / 承重段补 LOAD_WITH_ALTERED_SEARCH_PATH / 规矩 6 增产物布局 / D125 结案"
```

---

### Task 8: 文书（二）——README、wiki、Embedding 注释、技能

**Files:**
- Modify: `README.md:115-116` 与 `:312-321`、`wiki/vase-architecture.md`（§10、§11.1 现状注、§11.2 第 3 步）、`wiki/vase-console-use.md:232`、`Samples/Embedding/CMakeLists.txt:22`、`.claude/skills/vase-cpp-engineering/references/portability.md:75`

- [ ] **Step 1: `README.md` 两处**

`:115-116` 的「可执行与 DLL 同处 bin/」改成「可执行在 `bin/`，**每个插件在 `bin/<插件名>/`（Linux/macOS 是 `lib/<插件名>/`）**」；`:312-321` 的 VaseEmbedding 段落里「各 DLL 同处 bin/」同改。两处都补一句「宿主可执行与框架库仍在产物根平铺」。

- [ ] **Step 2: `wiki/vase-architecture.md` 三处**

- §10 目录布局：`Samples/` 子树补 `plugin.json`（三个插件各一份），并说明 `bin/`(win) / `lib/`(posix) 下插件按 target 名成子目录；
- §11.1 现状注：把「CMake 后置步骤本波未接（D125）」改为**结案注**——以「拷贝」形态兑现（`vase_add_plugin_fixture` 的 `MANIFEST` POST_BUILD 步），`scan` 不接后置步骤；
- §11.2 第 3 步：「CMake 后置步骤自动跑（11.1）」改成「构建期由 `MANIFEST` 步骤拷进产物目录（见 11.1 现状注）」，并去掉那行【现状：后置步骤未接（D125）】标注。

- [ ] **Step 3: `wiki/vase-console-use.md:232` 的相对路径**

把示例里的 `file stage Vase.Hello ../../bin/HelloPluginPrime.dll` 改成新布局下的路径 `../../bin/HelloPluginPrime/HelloPluginPrime.dll`（macOS 侧是 `lib/`）。改完按文档的实跑纪律复核一遍该示例仍可照抄执行。

- [ ] **Step 4: Embedding 注释与 portability**

`Samples/Embedding/CMakeLists.txt:22` 的「DLL 由可执行文件同目录（bin/）找到」改成「框架库由宿主静态导入（见 CLAUDE.md 承重段）；插件路径由 `$<TARGET_FILE>` 直接给出，本程序不猜也不扫目录」。`portability.md:75` 那条 pin 同口径改准。

- [ ] **Step 5: 提交**

```bash
git add README.md wiki/ Samples/Embedding/CMakeLists.txt .claude/skills/
git commit -m "文书：README / wiki §10·§11.1·§11.2 / console 示例相对路径 / Embedding 注释 / 技能 portability 同口径改准"
```

---

### Task 9: 八线收口

**Files:** 无（纯验证）

- [ ] **Step 1: Windows 四条线删树重配全量**

```bash
Scripts/win-verify.cmd
```

预期：四线全绿、零警告、零重跑；`Total Tests` 为 debug **341** × 2 / release **340** × 2。
（Windows 有已知的 `z-applocal` 文件锁假红：撞到时**单线删树重跑一次**，见 CLAUDE.md。）

- [ ] **Step 2: Linux 两条线**

```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && bash Scripts/linux-verify.sh'
```

- [ ] **Step 3: macOS 两条线**（在 macOS 开发机上）

```bash
bash Scripts/macos-verify.sh
```

- [ ] **Step 4: 差集复测**

三平台各导出 `ctest -N` 的用例名集合，逐对比过：

- `debug − release` 在四条 debug 线上**恰为 M1 T3 那条 death test**（`EffectScopeDeath.CreateAfterDisposeTerminates`）一条；
- 四条 debug 线名集合两两全等、四条 release 线亦两两全等；
- `linux − win`、`macos − win` 两个方向都应为空。

- [ ] **Step 5: tidy 四线**

```bash
run-clang-tidy -p build-win/win-x64-clang-debug
run-clang-tidy -p build-win/win-x64-msvc-debug -extra-arg=-Wno-unused-command-line-argument
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && bash Scripts/linux-clang-tidy.sh'
# macOS 侧：bash Scripts/macos-clang-tidy.sh
```

预期：**TU 由 116 → 117**（新增 `Tests/Unit/ArtifactLayoutTests.cpp`，四线同幅），三判据（退出码 + 正文 `error:` 0 + 正文 `warning:` 0）全过；本波**不新增** NOLINT 位点。

- [ ] **Step 6: format 双判**

```bash
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' \
  | xargs -0 clang-format --dry-run --Werror
```

- [ ] **Step 7: 归因落账**

把八线读数（`Total Tests`、tidy 的 TU 数与三判据、format RC）写进 `CLAUDE.md` 的「构建与测试」与「静态检查与格式」两张表；同时把 spec §8 的基数预期由「gtest +1」改准为「gtest +3、ctest 级 +1、TU +1」（理由见本文开头的差额注），并在本文末尾的偏离登记记名。

```bash
git add CLAUDE.md
git commit -m "收口：八线 340/339 全绿 / tidy 四线 117 TU 三判据全过 / format 双判"
```

---

## 附：偏离登记

落地期间任何与 spec 不一致的地方，按仓库惯例记在这里：「计划原文 / 实际做法 / 判决 / 理由」。spec 的定稿**不改史**，只在需要时挂勘误注。

**已登记（计划期自察）**

1. **gtest 增量**：spec §8 预算「+1（一条用例内断言多个目标）」；计划实际为 **+3**（布局主断言 / `Versioned` 例外 / 清单就位各一条，理由是要各有独立失败信号，与 `Eject.SemanticDependency*` 族的拆法同款）。判决：**按计划的 +3**；Task 9 Step 7 已把 spec §8 改准并挂勘误注（本波净 **+4/线**，八线实测 341/340）。
2. **`VASE_FIXTURE_MANIFESTS` 保留（裁定方向与计划期自察相反）**：spec §2 只写「保持」；计划期一度判「各消费点改完 per-item 宏后该宏再无消费者，故删」。**收口核实后裁定改为保留**——它仍有 2 个活消费者：`Tests/Integration/VaseCliValidateTests.cpp:160,177` 的 `VASE_FIXTURE_MANIFESTS / "shared_provider" / "plugin.json"`（那两份 fixture 清单本波**不搬**）。判决：**保留**；`Tests/CMakeLists.txt:114` 已就地注明「VaseCliValidateTests 仍消费它」。
3. **`VaseCliValidateTests.cpp:160,177` 不改**：子代理初报把它列为 hello/dependent/failing 的消费者，实读源码是 `shared_provider`（**不搬**）。判决：**不改**，只作事实更正，不进偏离登记以外的动作。

**已登记（落地期）**

4. **Task 1 Step 2 的「两条用例都 FAIL」是笔误**：该步原文预期两条用例都失败。实际第二条是钉 D194 例外的**守卫**用例（`Versioned` 四兄弟维持各自 stage 目录），**当天即绿**——只有布局主断言那条是红的。判决：**文档不改史、按实际记录**；红的那条正是该步要的失败信号，笔误不动判据力。
5. **Task 2 Step 4 的探针路径已过时（历史步骤，只作注记）**：该步原文 `cp ../../../Tests/Integration/fixtures/manifests/hello/plugin.json …`；Task 4 把这份清单搬去了 `Samples/HelloPlugin/plugin.json`。该步**在 Task 4 之前已执行过**（当时路径有效），此处只作注记——**照收口时的工作树复跑，要改用 `Samples/HelloPlugin/plugin.json`**。判决：**历史步骤不改**。
6. **tidy 四线首跑出 3 条 `readability-trailing-comma`（本波引入 → 收口波内已修 → 复跑归零）**：首跑四线**正文各 3 条 warning**，全部落本波 01115a8 改写的 `Tests/Integration/AssemblyFromSolveTests.cpp:69,72,75`——`StageAll()` 的 `Roster` 三处多行指定初始化，折行后尾缺逗号（单行那条不触发该检查）；TU **117**（四线同幅，符合 spec 的 TU +1 预期）与抑制基数、单 TU 极值则都对得上。判决：**属本波引入的真回归**（四线同幅、无 debug 门无平台门即证，不是预期偏差）；按「门禁不能红着收口」**收口波内补上三处尾逗号**（`clang-format` 顺带把三处 `Roster{` 折成独立一行，纯格式、语义不变），**四线复跑即三判据全过、正文双 0**——`Suppressed` 776999 / 319226 / 449889、单 TU 极值（998/45609、359/11337、278/7278）与 NOLINT 命中（4895 / 4890 / 4891）**逐数不变**，即只掉 warning、抑制面未动。spec §8 的「四线三判据全过」由首轮的未兑现变为**复跑兑现**。同轮另记：`win-x64-clang-debug` 重建后 `ctest` 仍 **341/341**（纯格式改动，不重跑八线全量）。
7. **Linux debug 线首跑 `ctest` RC=8、正文零输出（环境抖动，非回归）**：八线删树重配全量**首跑**时该线 `ctest` 只打了 `Test project …` 一行就返回 8（连一条 `Test #N` 都没有），同线 build 与 `ctest -N` 都 RC=0；按「退出码非 0 先重跑再判」单线删树重跑一次即 **341/341** 全绿，其余七线首跑即绿。判决：**环境抖动**（与 Windows 的 `z-applocal` 锁同类），已记进 `CLAUDE.md` 的「这一档有环境性假红」段。

**已闭环 / 无待登记**

- macOS 实测（Task 3）：**通过**，spec 取证注④ 的「未实测」已改成实测值（**填证据、不改决定**；D192 的「失败即停下重裁」未触发）。
- 八线实际读数与预期的差额：**只有第 6 条那一笔**（tidy 首跑正文 3 条 warning，收口波内已修、复跑归零）；其余逐位对上（八线 341/340、TU 117、单 TU 极值、NOLINT 命中、差集全空、format 三线 RC=0）。
- macOS tidy 的 RC 口径（第 4 条 concern）：**已核**——该线正文有 warning 时 `run-clang-tidy-mp-23` 退出码仍为 0，「正文有 warning 即 RC=1」不成立，出处是 T10 首跑的包装器缺口（本腿 plan 偏离登记第 9 条）；`CLAUDE.md` 的两处形态句已改准。
- 落地中未再发现本文未列出的消费者或文书点。

**未修、记名（终审判另立跟踪）**

终审把下列三条 Minor 判为**不修、另立跟踪**——记在此处，逐条写清「是什么 / 为什么本波不修 / 何时该修」。

1. **`Cmake/VasePluginHelpers.cmake` 的文件头能力清单未列 `MANIFEST`**：该文件是插件形态的唯一出口（规矩 5），
   头注释枚举了自身能力（只链 VasePod、可见性收紧等），本波新增的 `MANIFEST <path>` 参数（D186：POST_BUILD
   把清单拷进 `$<TARGET_FILE_DIR>/plugin.json`）没进那份清单。**本波不修**：属注释与能力的对齐、非功能缺陷，
   不在本波改动范围内。**何时该修**：下次动 `Cmake/VasePluginHelpers.cmake` 时顺手补。
2. **「清单缺失」这一情形无机器证人**：`Tests/Unit/ArtifactLayoutTests.cpp` 只钉了 `HelloPlugin` **有**清单
   （存在性断言），`VaseCliValidateProductRoot`（ctest 级）因 D128（缺清单只 warn、不计退出码）也抓不到
   `DependentPlugin`/`FailingPlugin` 的 `plugin.json` 缺失——即那两份清单若被误删，门禁仍绿。**本波不修**：
   D128 的「缺清单只 warn」是既定裁定，改它超出本波；补证人属新增用例。**何时该修**：将来若要补，是在 gtest
   里加一句存在性断言（钉 `Samples/{DependentPlugin,FailingPlugin}/plugin.json` 就位）。
3. **`wiki/vase-console-use.md:261` 的输出块缺 `semanticDependencyPossible=false` 一行**：该块系 2026-09-26
   截取，而 `EjectReport::SemanticDependencyPossible` 是 M4 才加的字段（M4 三条证人在测的正是它）。
   **本波不修**：**M4 期既有漂移**、非本波引入，与本波无关。**何时该修**：随下一次动
   `wiki/vase-console-use.md` 时重取该块。

