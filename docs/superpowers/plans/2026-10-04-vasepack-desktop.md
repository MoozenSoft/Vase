# VasePack：发布期组合库 · 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 §8.6 的组合库从提案落成机制——`vase_pack_plugins` 把若干以 pack 形编译的插件静态库链成一个 SHARED 库，并生成一张不写 Id 字面量的统一入口分发表。

**Architecture:** 插件源的编译期输入全部经 `vase_add_plugin_fixture` 的调用点参数表达（规矩 5 补上 include 那一维），`PACK` 关键字额外建一个静态库 `${name}.Pack`（带 `VASE_PLUGIN_PACK_OBJECT`，故不发两枚每库一份的入口）；`vase_pack_plugins` 用 `configure_file` 出一份分发表 TU，把各静态库经 `target_link_libraries` 链进一个 SHARED 库——**静态库的按需提取正是硬引用链保活承重的前提**。

**Tech Stack:** CMake 4.x + Ninja + 八个 preset（Win clang-cl/cl.exe、Linux clang、macOS clang）、vcpkg manifest、GoogleTest 1.18 + ctest、clang-tidy/clang-format 23.1.x。

**Spec:** `docs/superpowers/specs/2026-10-04-vasepack-desktop-design.md`（下称 spec；决策号 D196–D215 均指该文）

## Global Constraints

以下每一条都是 spec 的承重要求，每个任务默认继承：

- **开发形逐字不变**（D197）：不开 `VASE_PLUGIN_PACK_OBJECT` 时 `VASE_PLUGIN` 的展开物与今天完全一致——`kHeaderVersion` 不动、描述符布局不动、开发形的导出符号集合不动。
- **插件形态的唯一出口是 `vase_add_plugin_fixture`**（CLAUDE.md 规矩 5），本波给它加 `PACK`；**手搭 `add_library(SHARED)` 视为违规**。
- **编译期输入经调用点参数进**（D210）：插件的源 / include / 编译定义一律在 `vase_add_plugin_fixture` 的调用点表达，**调用之后不得再对同一 target 追加编译期输入**。**不辖**输出落位（`Versioned` 四兄弟）与链接选项（`NoIdentityPlugin` 的 `target_link_options`）——那两处不受影响，本波一个字不改。
- **组合库平铺在产物根**（D201）：`bin/VasePlugins.dll`(Win) / `lib/libVasePlugins.{so,dylib}`(POSIX)，**不进** `<产物根>/<名>/` 子目录。
- **静态库归档落独立输出目录**（D212）：`${name}.Pack` 的 `.lib`/`.a` **不进产物根**——它是构建中间物。
- **组合库不链进 `VaseTests`**（D204）：六条证人一律经 `Loader` 按路径装载后取符号。
- **生成的 TU 落构建树**（D205）：`configure_file` 的输出在 `${CMAKE_CURRENT_BINARY_DIR}`，**绝不落源码树**；它**进 tidy 面**，故必须零正文 warning、零 NOLINT。
- **两枚入口的名字是契约**：`VasePlugin_GetPlugin` / `VasePlugin_Descriptors` 是字面约定，任何改名都是破坏性改动。
- **全项目无异常**：不写 `throw`/`try`/`catch`；错误走 `Result<T>`/`Error`。
- **测试里禁用 `EXPECT_THROW`/`ASSERT_THROW`/`EXPECT_ANY_THROW`/`EXPECT_NO_THROW`**（CLAUDE.md 规矩 2，编译期硬失败）。
- **Vase 自己的头一律引号包含**（CLAUDE.md 规矩 3）。
- **提交信息用中文、不加任何 AI 署名尾注**。
- **基数与命令以 `CLAUDE.md` 为准**，计划里出现的数字若与它冲突，以它为准。

**预期基数变化**：gtest **+7**（`VasePack.*` 六条 + `ArtifactLayout` 一条），无 `#ifndef NDEBUG` 门、无平台门。故 `ctest -N` 由 debug 341 / release 340 变为 **debug 348 / release 347**（八线同幅）。tidy TU **+2**（`Tests/Integration/VasePackTests.cpp` + 生成的 `VasePlugins.dispatch.cpp`），四线 117 → **119**，平台差维持 0。

**探针材料不入库**：本计划有两处需要临时探针（Task 1 的符号集合、Task 4 的目标属性），一律建在仓库外的 `%TEMP%\vasepack-probe\`，跑完即删；输出原文记进本文末尾的偏离登记（spec 取证注的既有做法）。

---

### Task 1: 立失败测试——pack 形开关还不存在

**Files:**
- Create（仓库外，throwaway）：`%TEMP%\vasepack-probe\plugin.cpp`、`%TEMP%\vasepack-probe\probe.sh`

**Interfaces:**
- Consumes: 无（本任务只读仓库的 `Include/Vase/PluginDescriptor.h`，不改它）。
- Produces: 一个可复跑的**符号集合探针**——它将在 Task 2 之后转绿，成为「开发形逐字不变」的机器判据。

- [ ] **Step 1: 写失败测试（探针）**

探针的判据是**两件事一起**：带 `VASE_PLUGIN_PACK_OBJECT` 编出来的对象**只**剩唯一命名的描述符；不带它编出来的对象**与今天逐字相同**（三枚）。

创建 `C:\Users\<你>\AppData\Local\Temp\vasepack-probe\plugin.cpp`：

```cpp
#include "Vase/Plugin.h"

namespace
{
class ProbePlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Ok();
    }
};
} // namespace

VASE_PLUGIN(ProbePlugin){
    .Id = "Probe.Plugin",
    .DisplayName = "Probe",
    .Version = "0.1.0",
    .Requires = {},
    .OptionalRequires = {},
    .Provides = {},
};
```

创建 `%TEMP%\vasepack-probe\probe.sh`：

```bash
#!/usr/bin/env bash
# VasePack 探针（throwaway，spec Task 1/2 的机器判据）：pack 形开关下的符号集合。
set -u
cd "$(dirname "$0")" || exit 1
# Git Bash 会把 /nologo 这类 MSVC 风格 flag 当路径改写，必须关掉路径转换。
export MSYS_NO_PATHCONV=1
export MSYS2_ARG_CONV_EXCL='*'

CC="D:/Developer/LLVM/bin/clang-cl.exe"
NM="D:/Developer/LLVM/bin/llvm-nm.exe"
COMMON=(/nologo -TP -D_HAS_EXCEPTIONS=0 -ID:/Git/Vase/Include /DWIN32 /D_WINDOWS -std:c++20 -MDd /utf-8 /EHs-c-)

rm -f ./*.obj
# 注意 -Fo 必须紧贴：分离写时那个词会被当成额外的输入文件（探针期实测）。
"$CC" "${COMMON[@]}" -Fodev.obj -c plugin.cpp || exit 1
"$CC" "${COMMON[@]}" -DVASE_PLUGIN_PACK_OBJECT -Fopack.obj -c plugin.cpp || exit 1

echo "--- 开发形（不带开关）---"
"$NM" --extern-only --defined-only dev.obj 2>/dev/null | grep -i vaseplugin | sort
echo "--- pack 形（带开关）---"
"$NM" --extern-only --defined-only pack.obj 2>/dev/null | grep -i vaseplugin | sort

echo "--- 判据 ---"
dev=$("$NM" --extern-only --defined-only dev.obj 2>/dev/null | grep -ci vaseplugin)
pack=$("$NM" --extern-only --defined-only pack.obj 2>/dev/null | grep -ci vaseplugin)
[ "$dev" = 3 ] && echo "PASS: 开发形三枚（描述符 + 两枚入口）" || echo "FAIL: 开发形应为 3 枚，实得 $dev"
[ "$pack" = 1 ] && echo "PASS: pack 形一枚（只剩唯一命名的描述符）" || echo "FAIL: pack 形应为 1 枚，实得 $pack"
```

- [ ] **Step 2: 跑探针，确认它失败**

Run:
```bash
bash "$TEMP/vasepack-probe/probe.sh"
```
（Git Bash 里 `$TEMP` 可能是反斜杠形式；等价地 `cd "$(cygpath "$TEMP")/vasepack-probe" && bash probe.sh`。）

Expected: **`dev` 判据 PASS、`pack` 判据 FAIL（实得 3 枚）**——`VASE_PLUGIN_PACK_OBJECT` 今天根本不存在，宏体照常发两枚入口。这就是本波的红。

- [ ] **Step 3: 记下红的原因**

这不是「探针写错了」——`PluginDescriptor.h:110-129` 的宏体里没有这个开关，所以带不带它编出来的对象**逐字相同**。Task 2 加的那支分支正是让第二行判据转绿的东西。

*(本任务无提交——探针是 throwaway，不进仓库。)*

---

### Task 2: `PluginDescriptor.h` 的 pack 分支

**Files:**
- Modify: `Include/Vase/PluginDescriptor.h:110-156`（生成物注释块 + 宏体）

**Interfaces:**
- Consumes: Task 1 的探针（作为验收判据）。
- Produces: 宏 `VASE_PLUGIN_ENTRIES(Type)` 与开关 `VASE_PLUGIN_PACK_OBJECT`；`VASE_PLUGIN(Type)` 的展开物在**不开开关时逐字不变**。

- [ ] **Step 1: 把两枚入口抽成 `VASE_PLUGIN_ENTRIES`，并把 112–113 行那句改准**

编辑 `Include/Vase/PluginDescriptor.h`。先把生成物注释块（现 110–129 行）里这段：

```
// 生成物全集（spec 3.3(1) 探针定稿 + M5/D118）：工厂 + 唯一命名描述符 + 统一入口 + 枚举面 `VasePlugin_Descriptors`。
// 符号名由类名派生不是插件 Id（Id 含点号不是合法标识符；类名在镜像内天然唯一，§3.1）。
// 一个库装一个插件才有 GetPlugin；组合库的分发表由 VasePack 生成（M5，§8.6），
// 与本宏无关。
```

改为：

```
// 生成物全集（spec 3.3(1) 探针定稿 + M5/D118 + VasePack）：工厂 + 唯一命名描述符 + 统一入口 + 枚举面。
// 符号名由类名派生不是插件 Id（Id 含点号不是合法标识符；类名在镜像内天然唯一，§3.1）。
// 两枚入口（`VasePlugin_GetPlugin` / `VasePlugin_Descriptors`）是 **extern "C"、每库各一份**——
// 「一个库装一个插件」的前提就写在这里。组合库要装 N 个，故 pack 形（VASE_PLUGIN_PACK_OBJECT）
// 不发它们，由 VasePack 生成的分发表 TU 代发（VasePack spec D197/D200）。**不开这个开关 = 逐字不变。**
```

然后在 `#define VASE_PLUGIN(Type)` 之前插入两支：

```cpp
// pack 形与开发形的唯一分岔点（VasePack spec D197）：pack 形把两枚每库一份的入口让给
// 分发表 TU，自己只留唯一命名的描述符。漏发开关的症状是**重定义、链接失败**（响亮）。
#ifdef VASE_PLUGIN_PACK_OBJECT
#define VASE_PLUGIN_ENTRIES(Type)
#else
#define VASE_PLUGIN_ENTRIES(Type)                                                                                      \
    extern "C" VASE_EXPORT const ::vase::PluginDescriptor* VasePlugin_GetPlugin(const char* id)                        \
    {                                                                                                                  \
        return id != nullptr && std::string_view{id} == kVaseMeta_##Type.Id ? VasePluginDesc_##Type() : nullptr;       \
    }                                                                                                                  \
    /* 枚举面（M5/D118）：一个库里的全部描述符。纯追加——既有生成物一字不变*/                                           \
    /* kHeaderVersion 不动。名字是契约：工具按字面取它；组合库由 VasePack 生成同名（§8.6）。  */                       \
    extern "C" VASE_EXPORT const ::vase::PluginDescriptor* const* VasePlugin_Descriptors(::std::uint32_t* outCount)    \
    {                                                                                                                  \
        static const ::vase::PluginDescriptor* const kAll[] = {VasePluginDesc_##Type()};                               \
        *outCount = 1;                                                                                                 \
        return static_cast<const ::vase::PluginDescriptor* const*>(kAll);                                              \
    }
#endif
```

再在 `VASE_PLUGIN(Type)` 宏体里，把原来那两枚入口的定义整段删除，替换为一行：

```cpp
    VASE_PLUGIN_ENTRIES(Type)                                                                                          \
```

宏体的其余部分（`extern const PluginMeta kVaseMeta_##Type;`、匿名命名空间里的工厂与销毁端、`VasePluginDesc_##Type()`、末行 `const ::vase::PluginMeta kVaseMeta_##Type = ::vase::PluginMeta`）**一字不动**。注意 `VASE_PLUGIN_ENTRIES(Type)` 必须落在 `VasePluginDesc_##Type()` **之后**（它要用那个函数）。

- [ ] **Step 2: 跑探针，确认转绿**

Run:
```bash
bash "$TEMP/vasepack-probe/probe.sh"
```

Expected: **两行判据都 PASS**——`dev` 3 枚、`pack` 1 枚。

- [ ] **Step 3: 确认开发形逐字不变（八线里挑一条重编）**

Run:
```bash
cmake --build --preset win-x64-clang-debug --target HelloPlugin
D:/Developer/LLVM/bin/llvm-readobj.exe --coff-exports build-win/win-x64-clang-debug/bin/HelloPlugin/HelloPlugin.dll 2>/dev/null | grep -E "Name:" | sed 's/^ *//' | sort
```

Expected: 与改动前逐字相同的三枚（`VasePluginDesc_HelloPlugin` / `VasePlugin_GetPlugin` / `VasePlugin_Descriptors`）。**这一条就是「开发形零 ABI 增量」的机器判据**，spec §2.2 靠它成立。

> **勘误（Task 2 落地期，见偏离登记 3）**：本步原写 `llvm-nm --extern-only --defined-only <dll>`。实测 `llvm-nm` 对 **PE 动态库**给 **0 行**（它是符号表工具，DLL 的导出表不在它的读程内）。改用 `llvm-readobj --coff-exports`，判据不变——**`.obj` 仍用 `llvm-nm`**（Task 1/4 的对象级判据是对的）。

- [ ] **Step 4: 提交**

```bash
git add Include/Vase/PluginDescriptor.h
git commit -m "PluginDescriptor：VASE_PLUGIN 抽出 pack 形分支（VasePack spec D197/D200）

两枚每库一份的入口抽成 VASE_PLUGIN_ENTRIES(Type)，由 VASE_PLUGIN_PACK_OBJECT 决定其空否；
不开开关时展开物逐字不变（探针实测：开发形三枚、pack 形一枚）。
同时把「组合库的分发表与本宏无关」那句改准——统一入口的发射范围本来就是宏的职责。"
```

---

### Task 3: `Samples/HelloCommon` 立成 INTERFACE 库，五处消费者全收编

**Files:**
- Create: `Samples/HelloCommon/CMakeLists.txt`
- Modify: `CMakeLists.txt`（`add_subdirectory` 区，`Samples/HelloPlugin` 之前）
- Modify: `Samples/{HelloPlugin,HelloPluginPrime,DependentPlugin}/CMakeLists.txt`（删函数后的 `target_include_directories`，改 `LINK_LIBRARIES`）
- Modify: `Samples/Embedding/CMakeLists.txt:12-14`、`Tools/VaseConsole/CMakeLists.txt:14`

**Interfaces:**
- Consumes: 无。
- Produces: INTERFACE 库 `HelloCommon`，接口 include 目录 = `Samples/HelloCommon`；五个 target 各自 `target_link_libraries(... PRIVATE HelloCommon)`。

- [ ] **Step 1: 建 INTERFACE 库**

创建 `Samples/HelloCommon/CMakeLists.txt`：

```cmake
# HelloCommon——Samples 共用的服务契约头（Greeter.h / Mood.h）。
#
# 立成 INTERFACE 库而非裸头目录（VasePack spec D209）：插件形态的唯一出口
# （vase_add_plugin_fixture，CLAUDE.md 规矩 5）要求插件的**编译期输入全部经参数表达**，
# 因为组合库的 pack 形目标拿不到「函数调用之后」加的 include。形状照 Cmake/VaseThirdParty.cmake
# 的 VaseThirdPartyCli（INTERFACE + 裸路径，本仓库无 install()）。
add_library(HelloCommon INTERFACE)
target_include_directories(HelloCommon INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
```

- [ ] **Step 2: 接进根 CMakeLists，位置在 HelloPlugin 之前**

编辑根 `CMakeLists.txt`，在 `add_subdirectory(Samples/HelloPlugin)`（现 287 行）**之前**插入：

```cmake
# HelloCommon（VasePack spec D209）：Samples 三个插件与两个可执行共用服务契约头，
# 立成 INTERFACE 库。必须在 HelloPlugin 之前——后者经 LINK_LIBRARIES 取它。
add_subdirectory(Samples/HelloCommon)
```

- [ ] **Step 3: 五个消费者改为经 `LINK_LIBRARIES` 取**

`Samples/HelloPlugin/CMakeLists.txt` 全文改为：

```cmake
# 示例插件与 fixture 走同一个形态定义函数（Cmake/VasePluginHelpers.cmake）——
# 「插件 target 长什么样」全仓库只此一处，示例也不例外。
vase_add_plugin_fixture(HelloPlugin
    SOURCES HelloPlugin.cpp
    LINK_LIBRARIES VasePod HelloCommon
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json")

# 注：提供的头（Greeter.h）住在 Samples/HelloCommon，经 LINK_LIBRARIES 取——
# 调用点之后不再追加编译期输入（VasePack spec D210）。
```

`Samples/HelloPluginPrime/CMakeLists.txt`：把 `LINK_LIBRARIES VasePod` 改成 `LINK_LIBRARIES VasePod HelloCommon`，删掉末尾的 `target_include_directories(HelloPluginPrime PRIVATE ...)` 两行。

`Samples/DependentPlugin/CMakeLists.txt`：同样把 `LINK_LIBRARIES VasePod` 改成 `LINK_LIBRARIES VasePod HelloCommon`，删掉末尾的 `target_include_directories(DependentPlugin PRIVATE ...)` 两行。

`Samples/Embedding/CMakeLists.txt`：把

```cmake
target_include_directories(VaseEmbedding PRIVATE
    "${PROJECT_SOURCE_DIR}/Samples/HelloCommon"
    "${PROJECT_SOURCE_DIR}/Tests/TestingSupport")
```

改为

```cmake
target_link_libraries(VaseEmbedding PRIVATE HelloCommon)
# Tests/TestingSupport 是测试支撑头，不属 HelloCommon，留在这里。
target_include_directories(VaseEmbedding PRIVATE
    "${PROJECT_SOURCE_DIR}/Tests/TestingSupport")
```

`Tools/VaseConsole/CMakeLists.txt:14`：把

```cmake
target_include_directories(VaseConsole PRIVATE "${PROJECT_SOURCE_DIR}/Samples/HelloCommon")
```

改为

```cmake
target_link_libraries(VaseConsole PRIVATE HelloCommon)
```

- [ ] **Step 4: 复核纪律零违反**

判据是**功能性**的，不是字面 grep 计数——**别用 grep 的条数当验收**（本步原文曾写「恰好剩 1 条」，那是错的：本步 Step 3 自己就要求往 `Samples/HelloPlugin/CMakeLists.txt` 里逐字写一句含 `Samples/HelloCommon` 的注释，`Tools/VaseConsole/CMakeLists.txt:12` 也有一句既有注释带同字面量，见偏离登记第 6 条）。

Run:
```bash
# 功能判据 1：还有没有 target_include_directories 指着 HelloCommon（应为零）
grep -rn "target_include_directories" --include=CMakeLists.txt . | grep -v "build-" | grep -i hellocommon
# 功能判据 2：经 LINK_LIBRARIES 取它的 target 有几个（应为五）
grep -rn "HelloCommon" --include=CMakeLists.txt . | grep -v "build-" | grep "target_link_libraries"
```

Expected: 判据 1 **零命中**；判据 2 **五条**——`HelloPlugin` / `HelloPluginPrime` / `DependentPlugin` / `VaseEmbedding` / `VaseConsole` 各一条。**字面量出现在注释里是允许的**（那正是「HelloCommon 在哪」的说明文字）。

Run:
```bash
cmake --build --preset win-x64-clang-debug
```

Expected: 构建成功、零警告；`VaseEmbedding` 与 `VaseConsole` 照常编出。**这一条钉住的是「五处全收编」没漏人。**

- [ ] **Step 5: 顺手核一遍「函数之后碰 target」的清单**

Run:
```bash
grep -rn -A 12 "vase_add_plugin_fixture(" --include=CMakeLists.txt Samples/ Tools/
```

Expected: 三个 Sample 的调用点之后**没有** `target_include_directories`（`Tests/HotSwap/fixtures/CMakeLists.txt` 的 `Versioned` 四兄弟与 `Tests/CMakeLists.txt` 的 `NoIdentityPlugin` 仍在，但那是**输出落位**与**链接选项**两个维度，D210 明文不辖——**不要动它们**）。

- [ ] **Step 6: 提交**

```bash
git add Samples/HelloCommon/CMakeLists.txt CMakeLists.txt \
        Samples/HelloPlugin/CMakeLists.txt Samples/HelloPluginPrime/CMakeLists.txt \
        Samples/DependentPlugin/CMakeLists.txt Samples/Embedding/CMakeLists.txt \
        Tools/VaseConsole/CMakeLists.txt
git commit -m "HelloCommon 立成 INTERFACE 库，五处消费者全收编（VasePack spec D209/D210）

插件形态的唯一出口要求编译期输入全部经调用点参数表达——组合库的 pack 形目标拿不到
「函数调用之后」加的 include（Samples/HelloPlugin:9-10 等三处）。把 HelloCommon 变成
可经 LINK_LIBRARIES 传递的 INTERFACE 库，形状照 VaseThirdPartyCli。
收编全部五处（三个插件 fixture + VaseEmbedding + VaseConsole）：只改一半会让
「HelloCommon 在哪」有两个答案。"
```

---

### Task 4: `PACK` 关键字——额外建一个 pack 形静态库

**Files:**
- Modify: `Cmake/VasePluginHelpers.cmake`（函数体 + 头注释）

**Interfaces:**
- Consumes: Task 2 的 `VASE_PLUGIN_PACK_OBJECT`；Task 3 的 `HelloCommon`。
- Produces: `vase_add_plugin_fixture(<name> ... PACK)` 额外建出静态库 `${name}.Pack`（带 `VASE_PLUGIN_PACK_OBJECT`、`POSITION_INDEPENDENT_CODE ON`、归档落 `${CMAKE_CURRENT_BINARY_DIR}/pack-objects`）；每个插件 target 带属性 `VASE_PLUGIN_CLASS`（缺省 = target 名）。

- [ ] **Step 1: 写失败测试**

Run:
```bash
ls build-win/win-x64-clang-debug/Samples/HelloPlugin/pack-objects/
```

Expected: `No such file or directory`——`PACK` 关键字还不存在，`${name}.Pack` 这个静态库（连同它的归档目录）都还没有。这就是本任务的红。

*(探针的落点说明：`cmake -P` 脚本**看不到**构建树里已定义的 target，所以「用 CMake 脚本查目标属性」这条路走不通。本任务的验收改用 Step 3 的两条直接命令——构建出归档 + 查 pack 形对象的符号集合，判据力等价且不引入额外工具。)*

- [ ] **Step 2: 落地 `PACK` 与 `VASE_PLUGIN_CLASS`**

编辑 `Cmake/VasePluginHelpers.cmake`。头注释末尾追加：

```cmake
# PACK（VasePack spec D199）：额外建一个**静态库** ${name}.Pack——同一个源、同一组链接库、
# 同样的可见性收紧，外加 VASE_PLUGIN_PACK_OBJECT（pack 形不发两枚每库一份的入口）与
# POSITION_INDEPENDENT_CODE（它要被链进 SHARED 库的 vase_pack_plugins 产物）。
# 必须显式设 PIC：全仓今天零处 POSITION_INDEPENDENT_CODE，本目标是第一个被链进 SHARED 的静态库。
# 归档落 pack-objects/：它是构建中间物，不进产物根（D212）。
# VASE_PLUGIN_CLASS（D198）：分发表拼 VasePluginDesc_<类名> 时读它，缺省 = target 名。
```

函数体改为（`cmake_parse_arguments` 的 option 槽加 `PACK`，其余原样）：

```cmake
function(vase_add_plugin_fixture name)
    cmake_parse_arguments(FIX "PACK" "MANIFEST" "SOURCES;LINK_LIBRARIES" ${ARGN})
    add_library(${name} SHARED ${FIX_SOURCES})
    target_link_libraries(${name}
        PRIVATE VaseBuildOptions ${FIX_LINK_LIBRARIES})
    set_target_properties(${name} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        VASE_PLUGIN_CLASS "${name}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${name}")

    if(FIX_MANIFEST)
        # 构建后把清单铺到插件产物目录旁（D186 的「拷贝」形态）。本命令无 DEPENDS，
        # 只改源清单而不重链目标时不会重跑——不保证副本随源清单单独改动自动同步。
        add_custom_command(TARGET ${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${FIX_MANIFEST}" "$<TARGET_FILE_DIR:${name}>/plugin.json"
            COMMENT "staging plugin.json for ${name}")
    endif()

    if(FIX_PACK)
        add_library(${name}.Pack STATIC ${FIX_SOURCES})
        target_link_libraries(${name}.Pack
            PRIVATE VaseBuildOptions ${FIX_LINK_LIBRARIES})
        target_compile_definitions(${name}.Pack PRIVATE VASE_PLUGIN_PACK_OBJECT)
        set_target_properties(${name}.Pack PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON
            POSITION_INDEPENDENT_CODE ON
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pack-objects")
    endif()
endfunction()
```

- [ ] **Step 3: 让 HelloPlugin 走 `PACK`，跑探针看它转绿**

`Samples/HelloPlugin/CMakeLists.txt` 的调用加一个词：

```cmake
vase_add_plugin_fixture(HelloPlugin
    SOURCES HelloPlugin.cpp
    LINK_LIBRARIES VasePod HelloCommon
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/plugin.json"
    PACK)
```

把探针改写成**跑在构建树里的形式**——用 CMake 的 `configure_file` 时机不好取，改用最直接的两条命令：

Run:
```bash
cmake --preset win-x64-clang-debug
cmake --build --preset win-x64-clang-debug --target HelloPlugin.Pack
ls build-win/win-x64-clang-debug/Samples/HelloPlugin/pack-objects/
```

Expected: 构建成功，且 `pack-objects/` 下出现 `HelloPlugin.Pack.lib`（Windows）——**归档确实落在独立目录、没有进 `lib/`**（D212 的判据）。用 `ls build-win/win-x64-clang-debug/lib/ | grep -i pack` 复核：**应当零命中**。

再看 pack 形对象的符号集合：

Run:
```bash
D:/Developer/LLVM/bin/llvm-nm.exe --extern-only --defined-only \
  build-win/win-x64-clang-debug/Samples/HelloPlugin/CMakeFiles/HelloPlugin.Pack.dir/HelloPlugin.cpp.obj 2>/dev/null | grep -i vaseplugin
```

Expected: **只有 `VasePluginDesc_HelloPlugin` 一枚**。

- [ ] **Step 4: 确认开发形那三个 target 一字不变**

Run:
```bash
cmake --build --preset win-x64-clang-debug
D:/Developer/LLVM/bin/llvm-readobj.exe --coff-exports build-win/win-x64-clang-debug/bin/HelloPlugin/HelloPlugin.dll 2>/dev/null | grep -E "Name:" | sed 's/^ *//' | sort
```

Expected: 三枚，与 Task 2 Step 3 的输出逐字相同。`HelloPluginPrime` / `DependentPlugin` / `FailingPlugin` 同理（都没加 `PACK`）。

> 同上条勘误：PE 动态库走 `llvm-readobj --coff-exports`，不走 `llvm-nm`。

- [ ] **Step 5: 提交**

```bash
git add Cmake/VasePluginHelpers.cmake Samples/HelloPlugin/CMakeLists.txt
git commit -m "vase_add_plugin_fixture 增 PACK 关键字与 VASE_PLUGIN_CLASS 属性（VasePack spec D198/D199/D212）

PACK 额外建静态库 \${name}.Pack：同源、同链接库、同可见性收紧，外加
VASE_PLUGIN_PACK_OBJECT 与 POSITION_INDEPENDENT_CODE（它是全仓第一个被链进 SHARED 的静态库，
而全仓今天零处 PIC）；归档落 pack-objects/，不进产物根。
VASE_PLUGIN_CLASS 缺省 = target 名，是分发表拼符号名时唯一的读取点。"
```

---

### Task 5: `vase_pack_plugins` 与示范组合库

**Files:**
- Create: `Cmake/VasePackDispatch.cpp.in`（分发表模板）
- Modify: `Cmake/VasePluginHelpers.cmake`（新增 `vase_pack_plugins`）
- Create: `Samples/VasePlugins/CMakeLists.txt`
- Modify: `CMakeLists.txt`（`Samples/FailingPlugin` 之后）
- Modify: `Samples/{DependentPlugin,FailingPlugin}/CMakeLists.txt`（各加 `PACK`）

**Interfaces:**
- Consumes: Task 4 的 `PACK` 与 `VASE_PLUGIN_CLASS`；Task 2 的 pack 形。
- Produces: 命令 `vase_pack_plugins(<target> PLUGINS <name>...)`；产物 `bin/VasePlugins.dll`(Win) / `lib/libVasePlugins.so|.dylib`(POSIX)。

- [ ] **Step 1: 写失败测试**

组合库还不存在。先跑一条会红的判据把它记住：

Run:
```bash
ls build-win/win-x64-clang-debug/bin/VasePlugins.dll
```

Expected: `No such file or directory`。这就是本任务的红。

- [ ] **Step 2: 写分发表模板**

创建 `Cmake/VasePackDispatch.cpp.in`：

> **勘误（Task 5 落地期，评审实测掀出，见偏离登记 7）**：本模板原文写「**不许加 NOLINT**」，那是**基于错误假设**——实测本文件会产生 **5 条 `readability-identifier-naming`**（`VasePlugin_GetPlugin` / `VasePlugin_Descriptors` / 三个 `VasePluginDesc_<类名>`）。根因：插件 TU 里同样的名字出自**宏展开**，clang-tidy 跳过宏展开的声明；**真生成的文件不是宏**，于是这些检查第一次咬上来——也就是说仓库对这些 ABI 契约名的豁免一直是**偶然的**。契约名是 `extern "C"` 的约定名、**改不动**，属「无代码级出路」，按仓库既有惯例（`Include/Vase/Config/ConfigMacros.h:24`、`Include/Vase/Config/Value.h:88` 等五处同款形态）**就地 NOLINT 区域 + 理由一句**。另两条（`misc-include-cleaner` 缺 `VASE_EXPORT` 的直接包含、`avoid-c-arrays`）**有真出路**，按「有出路的就改写法」必须改代码（补包含、`std::array`），不许压。下面的模板是改准后的版本。

```cpp
// 由 vase_pack_plugins 生成（VasePack spec D200）——**不含任何 Id 字面量**：
// 路由靠遍历描述符比对 Meta->Id，与 VASE_PLUGIN 单插件入口的行为逐字同律。
//
// 本文件落**构建树**（D205），且会被 run-clang-tidy 扫到（compile_commands 含它）——
// 故它必须自己写到零正文 warning。下列 NOLINT 区是「无代码级出路」的那种，理由见区内。
#include "Vase/Detail/Export.h"
#include "Vase/PluginDescriptor.h"

#include <array>
#include <cstdint>
#include <string_view>

// NOLINTBEGIN(readability-identifier-naming) 本文件通篇是 ABI 契约名（`VasePlugin_GetPlugin` /
// `VasePlugin_Descriptors` / 生成的 `VasePluginDesc_<类名>` 声明）——extern "C" 的约定名，改不动，
// 没有代码级出路。插件 TU 里同样的名字出自宏展开、被 clang-tidy 跳过；生成的源不是宏，故在此说明
// （形态同 ConfigMacros.h 的 cppcoreguidelines-macro-usage 区）。
@VASE_PACK_DECLS@

namespace
{

const std::array kVasePluginTable{@VASE_PACK_ENTRIES@};

} // namespace

extern "C" VASE_EXPORT const ::vase::PluginDescriptor* VasePlugin_GetPlugin(const char* id)
{
    if (id == nullptr)
    {
        return nullptr;
    }
    const std::string_view wanted{id};
    for (const ::vase::PluginDescriptor* descriptor : kVasePluginTable)
    {
        if (descriptor->Meta->Id == wanted)
        {
            return descriptor;
        }
    }
    return nullptr;
}

extern "C" VASE_EXPORT const ::vase::PluginDescriptor* const* VasePlugin_Descriptors(::std::uint32_t* outCount)
{
    *outCount = static_cast<::std::uint32_t>(kVasePluginTable.size());
    return kVasePluginTable.data();
}
// NOLINTEND(readability-identifier-naming)
```

> **两处细节留给落地实测**：① `std::array<const PluginDescriptor*, N>::data()` 在 const 数组上返回的正是 `const PluginDescriptor* const*`，与返回类型同型 ⇒ 不需要 cast（原文那个 `static_cast` 是给裸数组的 decay 用的）。若实测有检查要点名「隐式」，再加回显式 cast 并就地说明。② 直接包含集以 `clang-tidy` 零正文 warning 为准收敛——`misc-include-cleaner` 对「多包含」与「少包含」都可能开口，以实测为准。

- [ ] **Step 3: 写 `vase_pack_plugins`**

在 `Cmake/VasePluginHelpers.cmake` 末尾追加：

```cmake
# 发布期组合库（VasePack spec D196）：把若干 pack 形静态库链成一个 SHARED 库，
# 并生成一张统一入口的分发表 TU。表单 §8.6 的「硬引用链」保活——分发表显式调用每个
# 插件的描述符符号，而静态库按需提取，那条引用链才把成员拉进库（D199/D213）。
# 主动闸三项（D211）：未 PACK / 重复 / 空——后两项都会**静默**造出坏产物（重复 Id 的
# 枚举面、装零个插件的库），故一律在配置期带名拒绝。
function(vase_pack_plugins target)
    cmake_parse_arguments(PACK "" "" "PLUGINS" ${ARGN})

    if(NOT PACK_PLUGINS)
        message(FATAL_ERROR
            "vase_pack_plugins(${target}): PLUGINS 为空——装零个插件的组合库没有意义（同 VaseCli 的 D135 同一律）")
    endif()
    set(declared ${PACK_PLUGINS})
    list(REMOVE_DUPLICATES declared)
    list(LENGTH PACK_PLUGINS declaredCount)
    list(LENGTH declared uniqueCount)
    if(NOT declaredCount EQUAL uniqueCount)
        message(FATAL_ERROR
            "vase_pack_plugins(${target}): PLUGINS 有重复成员——会生成含重复 Id 的枚举面")
    endif()

    set(VASE_PACK_DECLS "")
    set(VASE_PACK_ENTRIES "")
    foreach(plugin IN LISTS PACK_PLUGINS)
        if(NOT TARGET ${plugin}.Pack)
            message(FATAL_ERROR
                "vase_pack_plugins(${target}): ${plugin} 未声明 PACK——请在它的 vase_add_plugin_fixture 调用点加 PACK")
        endif()
        get_target_property(pluginClass ${plugin} VASE_PLUGIN_CLASS)
        string(APPEND VASE_PACK_DECLS
            "extern \"C\" const ::vase::PluginDescriptor* VasePluginDesc_${pluginClass}();\n")
        string(APPEND VASE_PACK_ENTRIES "    VasePluginDesc_${pluginClass}(),\n")
    endforeach()

    configure_file(
        "${CMAKE_SOURCE_DIR}/Cmake/VasePackDispatch.cpp.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${target}.dispatch.cpp"
        @ONLY)

    add_library(${target} SHARED "${CMAKE_CURRENT_BINARY_DIR}/${target}.dispatch.cpp")
    target_link_libraries(${target} PRIVATE VaseBuildOptions VasePod)
    set_target_properties(${target} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
    foreach(plugin IN LISTS PACK_PLUGINS)
        target_link_libraries(${target} PRIVATE ${plugin}.Pack)
    endforeach()
endfunction()
```

- [ ] **Step 4: 剩下两个插件加 `PACK`，建示范组合库**

`Samples/DependentPlugin/CMakeLists.txt` 与 `Samples/FailingPlugin/CMakeLists.txt` 各在 `vase_add_plugin_fixture` 调用里加一个 `PACK`（`HelloPluginPrime` **不加**——它与 `HelloPlugin` 同 Id，两个 `.Pack` 同库会重定义，spec §6.2 第一条地雷）。

创建 `Samples/VasePlugins/CMakeLists.txt`：

```cmake
# 示范组合库（VasePack spec D202）：把 Samples 三个插件链成一个发布形 SHARED 库。
# 三者的类名恰好等于 target 名，故 PACK 只写一个词、不写 CLASS——默认路径自证。
# DependentPlugin 与 FailingPlugin 的 PACK 在各自的 CMakeLists 里。
vase_pack_plugins(VasePlugins PLUGINS HelloPlugin DependentPlugin FailingPlugin)
```

在根 `CMakeLists.txt` 的 `add_subdirectory(Samples/FailingPlugin)`（现 295 行）**之后**、`add_subdirectory(Tools/VaseConsole)` 之前插入：

```cmake
# 组合库要在三个成员之后（它们要先有 .Pack 静态库），且必须在 add_subdirectory(Tests)
# 之前——VaseTests 要用 $<TARGET_FILE:VasePlugins>。
add_subdirectory(Samples/VasePlugins)
```

- [ ] **Step 5: 跑测试，确认转绿**

Run:
```bash
cmake --preset win-x64-clang-debug
cmake --build --preset win-x64-clang-debug
ls build-win/win-x64-clang-debug/bin/VasePlugins.dll
D:/Developer/LLVM/bin/llvm-readobj.exe --coff-exports build-win/win-x64-clang-debug/bin/VasePlugins.dll 2>/dev/null | grep -E "Name:" | sed 's/^ *//' | sort
```

Expected: 构建成功、零警告；文件存在；导出表恰为 **5 枚**——三个 `VasePluginDesc_<类名>` **+** `VasePlugin_GetPlugin` **+** `VasePlugin_Descriptors`，**没有**任何别的 `VasePlugin*` 符号。对照 spec 取证注②：走 `-D` 重命名那条路的产物在同样规模下会多出 2N 枚死符号，本形态不产生它们。

再核产物根没有被中间物污染：

Run:
```bash
ls build-win/win-x64-clang-debug/lib/ | grep -i "Pack" ; echo "rc=$?"
```

Expected: **零命中、rc=1**（D212）。

- [ ] **Step 6: 核主动闸三项都真的挡得住**

Run:
```bash
# 临时把 Samples/VasePlugins/CMakeLists.txt 改成 PLUGINS HelloPlugin HelloPlugin
# 然后 cmake --preset win-x64-clang-debug
```
Expected: `FATAL_ERROR: vase_pack_plugins(VasePlugins): PLUGINS 有重复成员`。

Run:
```bash
# 临时改成 PLUGINS HelloPlugin HelloPluginPrime
```
Expected: `FATAL_ERROR: vase_pack_plugins(VasePlugins): HelloPluginPrime 未声明 PACK`。

Run:
```bash
# 临时清空 PLUGINS
```
Expected: `FATAL_ERROR: vase_pack_plugins(VasePlugins): PLUGINS 为空`。

**三个都验过之后把文件改回原样**，再跑一次 `cmake --preset win-x64-clang-debug` 确认恢复。

- [ ] **Step 7: 提交**

```bash
git add Cmake/VasePackDispatch.cpp.in Cmake/VasePluginHelpers.cmake \
        Samples/VasePlugins/CMakeLists.txt CMakeLists.txt \
        Samples/DependentPlugin/CMakeLists.txt Samples/FailingPlugin/CMakeLists.txt
git commit -m "vase_pack_plugins：分发表模板与示范组合库（VasePack spec D196/D200/D201/D202/D211）

分发表不写 Id 字面量，路由靠遍历描述符比对 Meta->Id；生成物落构建树、零 NOLINT。
组合库经 target_link_libraries 链各 .Pack 静态库（不用 \$<TARGET_OBJECTS:>——它对
静态库无效，而按需提取正是保活承重的前提）。主动闸三项：未 PACK / 重复 / 空。
示范装 Samples 三个（类名 == target 名，CLASS 不写），产物平铺在产物根。"
```

---

### Task 6: 六条证人

**Files:**
- Create: `Tests/Integration/VasePackTests.cpp`
- Modify: `Tests/CMakeLists.txt`（SOURCES 列表 + `target_compile_definitions`）

**Interfaces:**
- Consumes: Task 5 的 `VasePlugins`；`Tests/CMakeLists.txt:59-64` 已链的 `VaseHost`（`vase::detail::Loader`）；既有 `VASE_FIXTURE_*` 编译定义族的形状。
- Produces: 测试套件 `VasePack`，六条用例。

- [ ] **Step 1: 注册编译定义**

在 `Tests/CMakeLists.txt` 的 `target_compile_definitions(VaseTests PRIVATE ...)` 族里（71–95 行那一族）加一行：

```cmake
    VASE_FIXTURE_VASEPLUGINS="$<PATH:CMAKE_PATH,$<TARGET_FILE:VasePlugins>>"
```

并在 `add_executable(VaseTests ...)` 的 SOURCES 列表里加一行（与 `Integration/FailureSemanticsTests.cpp` 等同形）：

```cmake
    Integration/VasePackTests.cpp
```

- [ ] **Step 2: 写证人**

创建 `Tests/Integration/VasePackTests.cpp`：

```cpp
// VasePack 组合库的契约证人（spec D204）：六条一律**经 Loader 按路径装载**后取符号。
// 组合库不链进 VaseTests——链进去会让整个测试二进制从启动起就驻留它，且导入表直连
// 绕过了 Loader，恰好把「宿主零改动」这一跳挖空（spec §5.1）。
#include "Vase/Detail/Result.h"
#include "Vase/Host/Loader.h"
#include "Vase/PluginDescriptor.h"

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace
{

using GetPluginFn = const vase::PluginDescriptor* (*)(const char*);
using DescriptorsFn = const vase::PluginDescriptor* const* (*)(std::uint32_t*);

std::filesystem::path PackedLibrary() { return std::filesystem::path{VASE_FIXTURE_VASEPLUGINS}; }

// 六条共用：装载组合库，按字面契约名取一枚入口。失败时返回 nullptr，由调用方 ASSERT。
// Loader 是局部对象：EnsureResident 的记录随它析构而释放，但那**不卸货**（§8.1——
// 卸货只发生在 Eject / Shutdown），镜像仍在驻留期，故取到的函数指针与描述符可安全使用。
template <typename Fn>
Fn PackedEntry(const char* symbolName, vase::detail::Loader& loader, const vase::detail::BinaryRecord*& record)
{
    const vase::Result<vase::detail::BinaryRecord*> resident = loader.EnsureResident(PackedLibrary());
    if (!resident.IsOk())
    {
        return nullptr;
    }
    record = resident.Value();
    const vase::Result<void*> symbol = vase::detail::Loader::Symbol(*record, symbolName);
    if (!symbol.IsOk())
    {
        return nullptr;
    }
    return reinterpret_cast<Fn>(symbol.Value());
}

std::set<std::string> PackedIds(vase::detail::Loader& loader, const vase::detail::BinaryRecord*& record)
{
    const DescriptorsFn all = PackedEntry<DescriptorsFn>("VasePlugin_Descriptors", loader, record);
    if (all == nullptr)
    {
        return {};
    }
    std::uint32_t count = 0;
    const vase::PluginDescriptor* const* descriptors = all(&count);
    std::set<std::string> ids;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        ids.insert(std::string{descriptors[i]->Meta->Id});
    }
    return ids;
}

} // namespace

TEST(VasePack, GetPluginRoutesEveryPackedId)
{
    vase::detail::Loader loader;
    const vase::detail::BinaryRecord* record = nullptr;
    const GetPluginFn getPlugin = PackedEntry<GetPluginFn>("VasePlugin_GetPlugin", loader, record);
    ASSERT_NE(getPlugin, nullptr);

    for (const char* id : {"Vase.Hello", "Vase.Dependent", "Vase.Failing"})
    {
        const vase::PluginDescriptor* descriptor = getPlugin(id);
        ASSERT_NE(descriptor, nullptr) << id;
        EXPECT_EQ(descriptor->Meta->Id, id); // 逐字相等，不是「非空即可」
    }
    loader.Unload(*record);
}

TEST(VasePack, DescriptorsEnumerateAllPackedPlugins)
{
    vase::detail::Loader loader;
    const vase::detail::BinaryRecord* record = nullptr;
    const std::set<std::string> ids = PackedIds(loader, record);
    ASSERT_EQ(ids.size(), 3U);
    EXPECT_EQ(ids, (std::set<std::string>{"Vase.Dependent", "Vase.Failing", "Vase.Hello"}));
    loader.Unload(*record);
}

TEST(VasePack, PackedDescriptorsKeepTheirOwnMetadata)
{
    vase::detail::Loader loader;
    const vase::detail::BinaryRecord* record = nullptr;
    const GetPluginFn getPlugin = PackedEntry<GetPluginFn>("VasePlugin_GetPlugin", loader, record);
    ASSERT_NE(getPlugin, nullptr);

    const vase::PluginDescriptor* hello = getPlugin("Vase.Hello");
    const vase::PluginDescriptor* dependent = getPlugin("Vase.Dependent");
    const vase::PluginDescriptor* failing = getPlugin("Vase.Failing");
    ASSERT_NE(hello, nullptr);
    ASSERT_NE(dependent, nullptr);
    ASSERT_NE(failing, nullptr);

    // 打包没把元数据搅平：三个成员各自的 Provides / Requires 仍是自己的。
    // MetaArray 的计数走 Size()（Count 是私有成员，公开面只有 Size()/Empty()）。
    EXPECT_EQ(hello->Meta->Provides.Size(), 1U);       // Vase.Hello.Greeter
    EXPECT_EQ(hello->Meta->Requires.Size(), 0U);
    EXPECT_EQ(dependent->Meta->Requires.Size(), 1U);   // requires Greeter
    EXPECT_EQ(dependent->Meta->Provides.Size(), 1U);   // Vase.Dependent.Farewell
    EXPECT_EQ(failing->Meta->Provides.Size(), 0U);     // 空 Provides 那一员——证人判别力的对照点
    EXPECT_EQ(failing->Meta->Requires.Size(), 0U);
    loader.Unload(*record);
}

TEST(VasePack, UnknownIdAndNullReturnNull)
{
    vase::detail::Loader loader;
    const vase::detail::BinaryRecord* record = nullptr;
    const GetPluginFn getPlugin = PackedEntry<GetPluginFn>("VasePlugin_GetPlugin", loader, record);
    ASSERT_NE(getPlugin, nullptr);

    EXPECT_EQ(getPlugin("Vase.Nope"), nullptr);
    EXPECT_EQ(getPlugin(nullptr), nullptr); // 宏体的守卫语义（spec §5.1）
    loader.Unload(*record);
}

TEST(VasePack, HeaderVersionMatchesPackedBinary)
{
    vase::detail::Loader loader;
    const vase::detail::BinaryRecord* record = nullptr;
    const GetPluginFn getPlugin = PackedEntry<GetPluginFn>("VasePlugin_GetPlugin", loader, record);
    ASSERT_NE(getPlugin, nullptr);

    for (const char* id : {"Vase.Hello", "Vase.Dependent", "Vase.Failing"})
    {
        const vase::PluginDescriptor* descriptor = getPlugin(id);
        ASSERT_NE(descriptor, nullptr) << id;
        EXPECT_EQ(descriptor->HeaderVersion, vase::kHeaderVersion) << id;
    }
    loader.Unload(*record);
}

TEST(VasePack, LoaderFindsEntryByLiteralContractName)
{
    vase::detail::Loader loader;
    vase::Result<vase::detail::BinaryRecord*> resident = loader.EnsureResident(PackedLibrary());
    ASSERT_TRUE(resident.IsOk()) << resident.GetError().Message();

    // 两枚字面契约名都必须解析得到——这是「宿主零改动」的可证伪点：
    // 谁把入口改名或漏发，这两行当场红。
    EXPECT_TRUE(vase::detail::Loader::Symbol(*resident.Value(), "VasePlugin_GetPlugin").IsOk());
    EXPECT_TRUE(vase::detail::Loader::Symbol(*resident.Value(), "VasePlugin_Descriptors").IsOk());

    // 错名负例：换个名字必须失败（否则上面两行可能只是在匹配某种通配）。
    EXPECT_FALSE(vase::detail::Loader::Symbol(*resident.Value(), "VasePlugin_GetPluginX").IsOk());
    EXPECT_FALSE(vase::detail::Loader::Symbol(*resident.Value(), "VasePlugin_Descriptor").IsOk());

    const vase::Result<void*> raw = vase::detail::Loader::Symbol(*resident.Value(), "VasePlugin_GetPlugin");
    ASSERT_TRUE(raw.IsOk());
    const auto getPlugin = reinterpret_cast<GetPluginFn>(raw.Value());
    for (const char* id : {"Vase.Hello", "Vase.Dependent", "Vase.Failing"})
    {
        const vase::PluginDescriptor* descriptor = getPlugin(id);
        ASSERT_NE(descriptor, nullptr) << id;
        EXPECT_EQ(descriptor->Meta->Id, id);
    }
    loader.Unload(*resident.Value());
}
```

- [ ] **Step 3: 跑测试，确认全绿**

Run:
```bash
cmake --preset win-x64-clang-debug
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R "VasePack"
```

Expected: **6/6 PASS**。

- [ ] **Step 4: 核基数**

Run:
```bash
ctest --preset win-x64-clang-debug -N | tail -3
```

Expected: `Total Tests: 347`（= 341 + 6；`ArtifactLayout` 那条在 Task 8 加，届时到 348）。

- [ ] **Step 5: 提交**

```bash
git add Tests/Integration/VasePackTests.cpp Tests/CMakeLists.txt
git commit -m "VasePack 六条证人：装载/路由/枚举/元数据/版号/字面契约名（spec D204）

六条一律经 Loader 按路径装载后取符号——组合库不链进 VaseTests（链进去会让整个测试
二进制从启动起驻留它并绕过 Loader，把要证的「宿主零改动」那一跳挖空）。
第六条承重：两枚字面契约名都必须解析得到，外加两条错名负例。"
```

---

### Task 7: Linux 线先行验证点（失败即停下重裁）

**Files:** 无改动——本任务是**验证**，不是实现。

**Interfaces:**
- Consumes: Task 5 的组合库、Task 6 的六条证人。
- Produces: 「静态库链进 SHARED」这条全仓首次出现的构建面在三件事上的实测结论：PIC 是否足够、静态库成员里的 `VASE_EXPORT` 经归档链进 `.so` 后是否照常导出、组合库在 `-fvisibility=hidden` 下能否被按字面名取到。

- [ ] **Step 1: 在 Linux 线上 configure + build**

Run:
```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --preset linux-x64-clang-debug'
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug'
```

Expected: 构建成功、零警告。**若这里因 PIC 失败**（症状是 `relocation R_X86_64_PC32 against symbol ... can not be used when making a shared object; recompile with -fPIC`），说明 `POSITION_INDEPENDENT_CODE ON` 没生效——**停下、不预置补丁、回报重裁**（D207 与 macOS 腿 D192 同款纪律）。

- [ ] **Step 2: 核导出符号（`.dynsym`）**

Run:
```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && nm -D --defined-only build-linux/linux-x64-clang-debug/lib/libVasePlugins.so | grep -i vaseplugin | sort'
```

Expected: 与 Windows 同形——6 枚（三个描述符 + 两枚入口 + 可能一枚 `VasePluginCreate_` 若可见性没收紧；**若出现多余符号，说明 `CXX_VISIBILITY_PRESET hidden` 没生效**，留下证据）。

- [ ] **Step 3: 跑六条证人**

Run:
```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && ctest --preset linux-x64-clang-debug -R VasePack --output-on-failure'
```

Expected: **6/6 PASS**。

- [ ] **Step 4: 核产物落位**

Run:
```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && ls build-linux/linux-x64-clang-debug/lib/ | grep -i pack; echo "rc=$?"'
```

Expected: **零命中、rc=1**（`.Pack.a` 归档在 `Samples/<名>/pack-objects/`，不在产物根）。

- [ ] **Step 5: 把结论与输出原文记进偏离登记**

编辑本文末尾的「偏离登记」，把上四步的**输出原文**记成一条（照 spec 取证注的先例——这是该构建面的唯一记录）。若三件事全部与预期一致，写「D207 未触发重裁」。

*(本任务无提交，除非偏离登记有内容——那就把计划文件本身提交掉。)*

---

### Task 8: 布局断言与保活变异取证

**Files:**
- Modify: `Tests/Unit/ArtifactLayoutTests.cpp`

**Interfaces:**
- Consumes: Task 6 的 `VASE_FIXTURE_VASEPLUGINS` 编译定义（同一个 `VaseTests` 目标，直接可用）。
- Produces: 一条钉「组合库平铺在产物根」的断言；两枚一次性变异取证的记录（保活 D213、布局 D215）。

- [ ] **Step 1: 写断言**

在 `Tests/Unit/ArtifactLayoutTests.cpp` 末尾追加：

```cpp
// VasePack spec D215：组合库是唯一一条**反向**布局规则——它不是插件 fixture，
// 不进 <产物根>/<名>/，而是平铺在产物根。同一个波里两条相邻规则不该一条有守卫
// 一条没有（spec 取证注⑪：Refresh 只扫子目录故组合库隐形、本文件又是逐目标断言，
// 所以平铺这条今天没有任何东西看得见它）。
TEST(ArtifactLayout, PackedLibraryStaysFlatInTheProductRoot)
{
    const std::filesystem::path packed{VASE_FIXTURE_VASEPLUGINS};
    EXPECT_TRUE(std::filesystem::exists(packed)) << packed.string();

    const std::filesystem::path root{VASE_FIXTURE_HELLO}; // 插件在 <根>/<名>/，故父目录的父目录就是根
    EXPECT_EQ(packed.parent_path(), root.parent_path().parent_path()) << packed.string();
    // 它**不是**自己的一个目录（对照：插件必须是）。
    EXPECT_NE(packed.parent_path().filename().string(), "VasePlugins") << packed.string();
}
```

- [ ] **Step 2: 跑测试，确认绿**

Run:
```bash
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R "ArtifactLayout" --output-on-failure
```

Expected: **4/4 PASS**。

- [ ] **Step 3: 变异取证一——布局（D215）**

把 `Samples/VasePlugins/CMakeLists.txt` 临时加一行把它挪进子目录：

```cmake
set_target_properties(VasePlugins PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/VasePlugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/VasePlugins")
```

Run:
```bash
cmake --preset win-x64-clang-debug && cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R "ArtifactLayout.PackedLibraryStaysFlatInTheProductRoot" --output-on-failure
```

Expected: **FAIL**（`packed.parent_path()` 不再等于产物根）。**记下输出原文**，然后**撤掉那两行**、重新 configure + build，确认恢复绿。

- [ ] **Step 4: 变异取证二——保活（D213）**

这是 D199 选静态库换来的唯一实质收益的兑现。**做法：直接改构建树里那份生成物，且不要重新 configure**（`configure_file` 会在 configure 时把它再生成回来，这正好是撤销手段）。编辑 `build-win/win-x64-clang-debug/Samples/VasePlugins/VasePlugins.dispatch.cpp`：

```cpp
// 把表里代表 FailingPlugin 的那一行**引用**删掉，例如由
const std::array kVasePluginTable{    VasePluginDesc_HelloPlugin(),
    VasePluginDesc_DependentPlugin(),
    VasePluginDesc_FailingPlugin(),
};
// 改成只留前两行（外层的 extern 声明**留着**——声明不产生引用，只删调用那一处即可；
// 留着它恰好把「拉动静态库成员的是**引用**、不是声明」这一点隔离出来）
```

**观察面是导出表，不是枚举计数**（观察面选错的勘误见偏离登记）：`kVasePluginTable` 的字面长度直接驱动 `VasePlugin_Descriptors` 的 `outCount`（`outCount = kVasePluginTable.size()`），删一行 ⇒ 表长变短，**即便换成对象库形态枚举计数也会变小**，故它区分不了两形态。能区分的只有「那个描述符符号在不在库里」。

Run:
```bash
cmake --build --preset win-x64-clang-debug
D:/Developer/LLVM/bin/llvm-readobj.exe --coff-exports build-win/win-x64-clang-debug/bin/VasePlugins.dll | grep -E "Name:" | sed 's/^ *//' | sort
```

Expected: **`VasePluginDesc_FailingPlugin` 从导出表消失**（静态库按需提取，没人引用的 `FailingPlugin.Pack` 成员不进库）。**记下输出原文**（这就是「硬引用链保活承重」的证据），然后**撤销变异**、重新 configure + build，确认导出表恢复 5 枚、6/6 恢复绿。

> **对照意义**：若本波用的是对象库（`$<TARGET_OBJECTS:>`），这个变异**不会**让符号从导出表消失——对象是无条件进库的。这一对「变异 → 符号消失」正是 D199 那一票的兑现。

- [ ] **Step 5: 核基数**

Run:
```bash
ctest --preset win-x64-clang-debug -N | tail -3
```

Expected: `Total Tests: 348`（= 341 + 6 + 1）。

- [ ] **Step 6: 提交**

```bash
git add Tests/Unit/ArtifactLayoutTests.cpp
git commit -m "ArtifactLayout 增组合库平铺断言（VasePack spec D215）

组合库是唯一一条反向布局规则（不进 <产物根>/<名>/）。变异取证：临时把它挪进子目录，
该断言转红。"
```

---

### Task 9: 文书

**Files:**
- Modify: `CLAUDE.md`（项目状态 / 目录布局行 / 规矩 5 / 两张基数表）
- Modify: `README.md:115-117`、`:314-316`
- Modify: `wiki/vase-architecture.md`（§8.6 / §11.3 / §13.2 / §13.4）
- Modify: `Cmake/VasePluginHelpers.cmake` 头注释（若 Task 4 已写则只核一遍）
- Modify: `Include/Vase/PluginDescriptor.h` 宏体注释（若 Task 2 已写则只核一遍）
- 复核 `.claude/skills/vase-cpp-engineering/references/{abi-boundary,architecture}.md`

**Interfaces:** 无代码接口——本任务只改文档与注释。

- [ ] **Step 1: `CLAUDE.md` —— 项目状态一笔**

在「项目状态」段末尾追加一段（形制照「插件产物布局波（2026-10-03…）」那一段），内容须覆盖：本波两段（机制 + 证人）；`Tools/VasePack` 工具形态改口为 CMake 函数（D196）；pack 形开关与开发形逐字不变；静态库保活（D199）；欠账收窄措辞（D208：桌面机制兑现，余项 = 移动产物 iOS `.a` / Android `.so` 与发布形清单/装配链）；明记「组合库的唯一消费者是证人，没有产品」（§1 的定位说明）。

- [ ] **Step 2: `CLAUDE.md` —— 目录布局行与规矩 5**

**目录布局行**：`Samples/{HelloCommon,HelloPlugin,…}` 那串里给 `HelloCommon` 加注「INTERFACE 库」，并补 `Samples/VasePlugins/`。

**规矩 5 段**：在既有三条承载之后追加一条，措辞照 spec §3.2 的引文（窄到编译期输入）：

> **编译期输入经调用点参数表达**（VasePack spec D210）：插件的源 / include / 编译定义一律在 `vase_add_plugin_fixture` 的调用点经参数给出，**调用之后不得再对同一 target 追加编译期输入**——组合库的 pack 形目标拿不到那些后加的设置。**不辖**输出落位与链接选项两个维度。

- [ ] **Step 3: `CLAUDE.md` —— 两张基数表**

按 Task 10 的八线实测值改「构建与测试」的 `ctest -N` 表与「静态检查与格式」的 tidy 表（TU 117 → 119、基数 341/340 → 348/347；Suppressed 与 NOLINT 命中按实测填）。**顺序**：本步先把结构改好、数值留**待 Task 10 实测回填**——**不要写占位数**（`TBD` 之类会被当成没写完），本轮只改「哪一行说什么」，数值改动由 Task 10 Step 6 一次做完。

- [ ] **Step 4: `README.md` 两处**

`115-117` 那段与 `314-316` 那段各补一句：**组合库平铺在产物根**（它既不是可执行/框架库、也不进插件子目录——是现有两句之外的一类）。

- [ ] **Step 5: `wiki/vase-architecture.md`**

- **§8.6** 挂现状注：形态改口为 CMake 函数（`Tools/VasePack` 目录不存在）、分发表不写 Id 字面量、pack 形走静态库使保活承重、桌面三平台先行、移动产物留桩。
- **§11.3** 挂现状注：命令行形态作废，指向 §8.6 的新注。
- **§13.2**「插件形态」行加指针（发布期那一半已落）。
- **§13.4** 补一句组合库与「对外分发不做」的关系（它随产品走，不是把 Vase 当 SDK 发）。

- [ ] **Step 6: D214 三条地雷各落到它该在的地方**

三条都写进 spec §6.2 了，但**代码现场也要各留一句**——越靠近改动现场越不易腐（规矩 7）：

- **同源多 `.Pack` 不得同库** → `Cmake/VasePluginHelpers.cmake` 的 `vase_pack_plugins` 函数头注释（那里正是它的作用域）。一句话 + 一个实例名：`HelloPlugin.cpp` 同时编进 `HelloPlugin` 与 `HelloPluginPrime`，两者都 `PACK` 又同列 `PLUGINS` ⇒ 两个静态库都定义 `VasePluginDesc_HelloPlugin` ⇒ 重定义。
- **手写入口不受开关管辖** → `Include/Vase/PluginDescriptor.h` 的宏体注释那一段。一句话点出 `Tests/Integration/fixtures/StaleHeaderPlugin/StaleHeaderPlugin.cpp:55` 手写了自己的 `VasePlugin_GetPlugin`，**不在宏里**、不受 `VASE_PLUGIN_PACK_OBJECT` 影响；日后别把它当成「开关失灵」。
- **枚举顺序无契约** → 同上的宏体注释或 `vase_pack_plugins` 头注释。一句话：「`VasePlugin_Descriptors` 的返回顺序无契约；生成物里的顺序 = `PLUGINS` 的书写顺序，但不构成对外承诺」。

- [ ] **Step 7: 技能复核**

Run:
```bash
grep -rn "VASE_PLUGIN\|VasePlugin_GetPlugin\|分发表\|VasePack" .claude/skills/vase-cpp-engineering/
```

Expected: 逐条判断是否需要同步。**判据照 CLAUDE.md 规矩 7 的四类表**——「判据与理由」可两边各写一份，「阈值与基数」只住 CLAUDE.md。若有涉及描述符生成物符号集合的段落，补一句 pack 形的存在。

- [ ] **Step 8: 提交**

```bash
git add CLAUDE.md README.md wiki/vase-architecture.md .claude/skills/ \
        Cmake/VasePluginHelpers.cmake Include/Vase/PluginDescriptor.h
git commit -m "VasePack 文书：CLAUDE.md 项目状态与规矩 5 / README / wiki §8.6 §11.3 §13.2 §13.4 / D214 三条地雷落点（spec §9）"
```

---

### Task 10: 八线收口与基数落账

**Files:** 无源码改动（除非收口波内发现缺陷）。

**Interfaces:** 无。

- [ ] **Step 1: 八线删树重配全量**

Run（Windows 四条 + Linux 两条）：
```bash
Scripts/win-verify.cmd      # 四棵 preset 树各自删树 → configure → build → ctest → ctest -N
```
Run（Linux 两条，经登录 shell）：
```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && bash Scripts/linux-verify.sh'
```
Run（macOS 两条，在 macOS 开发机上）：
```bash
ssh -o BatchMode=yes <mac 别名> 'cd ~/WindowsGit/Vase && bash Scripts/macos-verify.sh'
```

Expected: **八线全绿**；`ctest -N` 为 debug **348** / release **347**（八线同幅）；平台差 0；`release − debug` 恰为 M1 T3 那枚 death test 一条；debug 四条线名集合两两全等、release 四条线亦两两全等。

**退出码非 0 时先重跑那一条线再判**（CLAUDE.md 的「这一档有环境性假红」：Windows 的 `v-applocal` 锁、Linux 首跑 `ctest` RC=8 零输出、macOS 的 `Resource busy` 三种形态）。

- [ ] **Step 2: 逐对比差集**

导出八线的用例名集合（`gtest_discover_tests` 用 `PRE_TEST` 模式，故 `ctest -N` 是唯一的枚举面）：

```bash
for p in win-x64-clang-debug win-x64-clang-release win-x64-msvc-debug win-x64-msvc-release; do
  ctest --preset "$p" -N | grep -oP '^\s*Test\s+#\d+:\s+\K.*' | sort > "build-win/$p.names.txt"
done
# Linux 两条经登录 shell 跑同形命令，落 build-linux/；macOS 两条落 build-macos/（在 mac 上跑）
```

Run:
```bash
for a in win-x64-clang-debug win-x64-msvc-debug; do
  for b in win-x64-clang-debug win-x64-msvc-debug; do
    [ "$a" = "$b" ] && continue
    echo "$a vs $b: $(comm -3 <(sort "build-win/$a.names.txt") <(sort "build-win/$b.names.txt") | wc -l)"
  done
done
diff <(sort build-win/win-x64-clang-debug.names.txt) <(sort build-win/win-x64-clang-release.names.txt)
```

Expected: debug 四线任意方向差集**全空**；release 四线同；`debug − release` 恰为 `EffectScopeDeath.CreateAfterDisposeTerminates` 一条。

- [ ] **Step 3: 三平台留规矩 6 族证据**

Run:
```bash
ctest --preset win-x64-clang-debug -R "HotSwap|Eject|Adopt" --output-on-failure
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && ctest --preset linux-x64-clang-debug -R "HotSwap|Eject|Adopt" --output-on-failure'
ssh -o BatchMode=yes <mac 别名> 'cd ~/WindowsGit/Vase && ctest --preset macos-x64-clang-debug -R "HotSwap|Eject|Adopt" --output-on-failure'
```

Expected: 三平台各 **58/58 全绿**（该族选择子把 ctest 自动拉入的 fixture 也算在内）。

- [ ] **Step 4: tidy 四线**

Run:
```bash
Scripts\win-clang-tidy.cmd clangcl
Scripts\win-clang-tidy.cmd msvc
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && bash Scripts/linux-clang-tidy.sh'
ssh -o BatchMode=yes <mac 别名> 'cd ~/WindowsGit/Vase && bash Scripts/macos-clang-tidy.sh'
```

Expected: **四线三判据全过、正文双 0**；TU **119**（117 + 测试文件 + 生成的 TU），四线同值、平台差 0。**首跑若有正文 warning 且落本波新代码**——尤其落在**生成的 TU** 上（`misc-include-cleaner` 是头号嫌疑，它可能认为 `PluginDescriptor.h` 已带 `<cstdint>`/`<string_view>`）——按「有没有代码级出路」处置，**不许 NOLINT**（D205）；改动生成模板后重跑。

- [ ] **Step 5: format 双判**

Run:
```bash
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
```

Expected: **RC=0**。**注意**：生成的 TU 与 `Cmake/*.cpp.in` 都不在 `git ls-files` 的扩展名清单里（`.in` 不被匹配）——**照旧只判已入库的源**，这条口径不改。

- [ ] **Step 6: 回填基数**

把 Task 9 Step 3 留的数值按本任务的实测填回 `CLAUDE.md` 两张表，并把 `ctest -R '^VaseCli'`、console 套件、Embedding 回放三行的现值复核一遍（本波不动它们，应当逐位不变）。

- [ ] **Step 7: 复核 spec §5.5 的措辞与磁盘一致**

若实测与本计划「预期基数变化」有差额（例如 tidy 首跑捕得正文 warning 并修掉、或某条线重跑），**改 spec 的对应行并挂勘误注**（spec 定稿不改史），差额记进本文末尾的偏离登记。

- [ ] **Step 8: 提交**

```bash
git add CLAUDE.md docs/superpowers/specs/2026-10-04-vasepack-desktop-design.md \
        docs/superpowers/plans/2026-10-04-vasepack-desktop.md
git commit -m "VasePack 收口：八线全绿与基数落账（debug 348 / release 347，tidy TU 119）"
```

---

## 附：偏离登记

落地期间任何与 spec / 本计划不一致的地方，按仓库惯例记在这里：「计划原文 / 实际做法 / 判决 / 理由」。spec 的定稿**不改史**，只在需要时挂勘误注。

**计划期自察**

1. **C++ 公开面增量与构建面增量分开表述**：spec 的 header 原写「公开面增量只有一处」，经 grilling 改为「**C++ 公开面**增量只有一处（`VASE_PLUGIN_PACK_OBJECT`）；构建面另有增量（`PACK`、`vase_pack_plugins`、`VASE_PLUGIN_CLASS`、`HelloCommon`）」。判决：**按改了之后的**；本计划不再复述。
2. **Task 1 的探针取代「先写 gtest 看它红」**：本波是构建系统特性，`VasePlugins` 目标在 Task 5 之前不存在，若把 gtest 注册提前，**configure 期即失败**（`Target "VasePlugins" not found`），后续任务全部无法验证。判决：**红由探针承担**，六条证人注册在目标存在之后（Task 6）。这是对 writing-plans 的「先写失败测试」在构建系统特性上的一处**有意偏离**。

**落地期**（执行时逐条填）

3. **`llvm-nm` 读不了 PE 动态库的导出表（Task 2 落地期发现，Task 4 未执行即一并改准）**：计划原文 Task 2 Step 3 与 Task 4 Step 4 都用 `llvm-nm --extern-only --defined-only <dll>` 核「开发形三枚」。实测 `llvm-nm` 对 **PE 动态库**给 **0 行**——它是符号表工具，DLL 的导出表不在它的读程内（Task 1 在 `.obj` 上用它是对的，`.obj` 有符号表）。判决：**两处均改用 `llvm-readobj --coff-exports <dll> | grep Name:`**，判据（导出表是那三枚）不变；`.obj` 级的判据仍用 `llvm-nm`。Task 2 实现者已就地适配并如实报告，Task 4 的指令由控制者改准。

4. **探针脚本的退出码恒为 0（Task 1 落地期记录，指针随 Task 2 派单传递）**：`probe.sh` 末行是 `[ ... ] && echo PASS || echo FAIL` 形态，FAIL 分支照样成功返回——故**判据只在正文文本行**，照抄退出码会把还没转绿的状态读成绿。判决：**读文本行**；计划原文的 Expected 段本来就是文本形态，无需改。

5. **空 pack 分支在宏体里留一行纯空白（Task 2 评审记，Minor）**：`VASE_PLUGIN_ENTRIES(Type)` 展成空时，那一行只剩续行反斜杠前的填充空白。判决：**不改**——无害，且 `clang-format` 认它（format 门 RC=0）；记此一笔免得日后被当成游离空行删掉（删了会断掉宏的续行）。

6. **Task 3 Step 4 的验收判据自相矛盾（控制者预检漏项，实现者如实上报而非凑数）**：本步原文写「`grep -rn "Samples/HelloCommon"` 应**恰好剩 1 条**」。但同一步的 **Step 3 自己要求**往 `Samples/HelloPlugin/CMakeLists.txt` 逐字写一句注释，那句注释含 `Samples/HelloCommon` 字面量；`Tools/VaseConsole/CMakeLists.txt:12` 另有一句既有注释带同字面量。三条口径互斥——**控制者的派单里也照抄了这个错的「恰好 1 条」**（派单口径与 brief 冲突是我的错，不是实现者的）。实现者的处置是对的：遵循更强的内容指令（逐字注释）、保留注释、如实上报 grep 得 3 条。判决：**验收判据改为功能性**——「指 `HelloCommon` 的 `target_include_directories` 消费者 = 0；经 `LINK_LIBRARIES` 取者 = 5」，已就地改准本步。字面量出现在注释里**允许且应当**（那正是「HelloCommon 在哪」的说明文字）。**预检扫描的教训**：我声称核过「每任务自身文本是否自洽」，但只核了「测试 vs 代码」这一面，没核「验收判据 vs 该任务自己要求写入的内容」这一面——后者正是这次漏掉的形态。

7. **计划里的证人代码在 tidy/format 两个硬门禁下会红——Task 6 落地期发现（实现者主动跑门禁后上报，未让评审去抓）**：Task 6 Step 2 的 `Tests/Integration/VasePackTests.cpp` 原文（我写的）实测撞三处，实现者做了**语义等价的修准**并如实登记：
   - **5 处 `modernize-use-auto`**：`const DescriptorsFn/GetPluginFn x = PackedEntry<...>(...)` → `const auto x`（初值已带显式模板实参 ⇒ 类型显然；与 `Source/Host/PluginHost.cpp:74` 的 canonical 同形）。
   - **1 处 `cppcoreguidelines-pro-bounds-pointer-arithmetic`**：`descriptors[i]` → `*std::next(...)`（并补 `<cstddef>` / `<iterator>`）——**与 `Source/Host/Inspect.cpp:64` 同构**，那处本就带着「用 `std::next` 而非指针算式」的既有说明。
   - **4 处 `clang-format`**：行尾对齐注释，`-i` 归一。

   **判决：准。** 依据是 CLAUDE.md 的 NOLINT 口径——「**有代码级出路的就改写法**（改写法 ≠ 改语义）」，照压 NOLINT 反而违规；同类先例见本登记第 8 条（Task 5 的 verbatim 模板同样被 tidy 拒）。修准后：tidy 正文双 0、format RC=0、六条证人 6/6 仍全过、`ctest -N` = 347。

   **教训（记名，本波第二次同类）**：**我写进计划的 C++ 代码块在计划期就该过一遍 tidy 与 format**——Task 5 的生成物模板与 Task 6 的证人文件是同一形态的两次，都是「计划原文照抄 ⇒ 落地撞门禁 ⇒ 实现者改写法」。下次出计划时，对新增的 C++ 代码块先跑一次门禁再写进计划，比让每个实现者各撞一次便宜。

8. **「生成物零 NOLINT」是错的——Task 5 评审实测掀出（本波最重的一处修正）**：计划 Task 5 Step 2 的模板原文写「不许加 NOLINT」，spec §5.2/D205 也写了同一句。**实测**（评审对生成物单跑 `clang-tidy -p build-win/win-x64-clang-debug`）产出 **7 条正文 warning**：
   - `readability-identifier-naming` **5 条**——`VasePlugin_GetPlugin` / `VasePlugin_Descriptors` 的定义（:29,:46）与三个 `VasePluginDesc_<类名>` 的 extern 声明（:13,14,15）；
   - `cppcoreguidelines-avoid-c-arrays` + `modernize-avoid-c-arrays` 1 条——`kAll[]`（:21）；
   - `misc-include-cleaner` 1 条——`no header providing "VASE_EXPORT" is directly included`（:29）。

   **根因（评审给出，我未预料到）**：插件 TU 里同样的契约名出自**宏展开**，clang-tidy **跳过宏展开的声明**；**真生成的文件不是宏**，于是这些检查第一次咬上来——仓库对这些 ABI 名的豁免一直是**偶然的**，从没人显式决定过。
   **判决**：① 5 条命名警告**改不动**（`extern "C"` 契约名即 ABI）⇒ 「无代码级出路」⇒ 按仓库五处既有形态（`ConfigMacros.h:24` / `Value.h:88,126` / `ManifestExpectation.h:25` / `PluginHost.h:42`）**就地 NOLINT 区域 + 理由一句**；**不改 `.clang-tidy`**（那是全局放宽，会把将来任何人写的 `VasePlugin*` 辅助函数一并豁免；而仓库对「无出路」的既定形态是就地说明）。② 另 2 条**有真出路** ⇒ 按「有出路的就改写法」**必须改代码**：补 `#include "Vase/Detail/Export.h"`、`kAll[]` 改 `std::array`（`std::array<const PluginDescriptor*, N>::data()` 在 const 数组上正是返回类型，故原来那个 `static_cast` 也不需要了）。③ 判据改准为「零正文 warning」，**实现路径允许带理由的 NOLINT**，「有出路的不许压」不变。④ spec §5.2/D205 已就地挂勘误注。
   **教训（记名）**：我把「生成物会比手写源干净」当成了默认前提，而事实是**宏一直是这些契约名的免检伞**——**「换个发射方式」会改变门禁覆盖面**这件事，值得进下次 spec 的预检清单。

9. **Linux 线先行验证点（Task 7 全部实测通过——D207 未触发重裁）**：三件「全仓首次出现的构建面」在 `linux-x64-clang-debug` 上逐条取证。**输出原文**（`$?` 陷阱见本条末）——

   **① configure + build**：`cmake --preset linux-x64-clang-debug` → RC=0；`cmake --build --preset …` → RC=0；另做一轮 `--clean-first` 全量重编 **358/358、RC=0、`grep -icE "warning:|error:"` = 0**。无 `relocation R_X86_64_PC32 … recompile with -fPIC` 类错误。三个 `.Pack.a` 归档产在 `Samples/<名>/pack-objects/`、`libVasePlugins.so` 链接成功。

   **② `.dynsym` 导出符号**（**整张表、未过滤**——`grep -i vaseplugin` 只看得见 `VasePlugin*`，而可见性泄漏是 **mangled C++ 名** `_ZN…`，过滤后结构上看不见，故此处贴全表以自足）：`nm -D --defined-only build-linux/linux-x64-clang-debug/lib/libVasePlugins.so` →
   ```
   0000000000005730 T VasePluginDesc_DependentPlugin
   0000000000005bf0 T VasePluginDesc_FailingPlugin
   0000000000002750 T VasePluginDesc_HelloPlugin
   0000000000002390 T VasePlugin_Descriptors
   00000000000022b0 T VasePlugin_GetPlugin
   ```
   同命令 `… | wc -l` → **5**。即**整张动态符号表恰这 5 行、无第 6 行**——既无多余 `VasePlugin*`（brief 预告的「可能一枚 `VasePluginCreate_`」未出现），也**无任何 mangled C++ 名（无一条 `_ZN…`）**。与 Windows 逐名同形。⇒ `CXX_VISIBILITY_PRESET hidden` 生效。（**勘误**：上面那几行的地址序 `5730,5bf0,2750,2390,22b0` **不是**地址序——先升后降，那是 `nm` 打印的 `.dynsym` **原生序**，不是按地址排的；原写「顺序为 `T` 地址序」有误，已改准。）

   **③ 六条证人**：`ctest --preset linux-x64-clang-debug -R VasePack --output-on-failure` → **6/6 Passed（`100% tests passed out of 6`）**，含 `VasePack.LoaderFindsEntryByLiteralContractName`。

   **④ 产物落位**：`ls build-linux/linux-x64-clang-debug/lib/ | grep -i pack` → **零命中、rc=1**（`.Pack.a` 只在 `Samples/<名>/pack-objects/`）。

   三件事判定：**① PIC 足够 / ② 静态库成员 `VASE_EXPORT` 经归档链进 `.so` 后照常导出 / ③ 组合库在 `-fvisibility=hidden` 下按字面名可取到——三者全部「证实」，与预期一致，D207 未触发重裁**（无需改工具链、无需加 `-fPIC`、无需改 CMake）。

   **附记（引号/展开陷阱的新形态）**：brief Step 4 原文的 `ls … | grep -i pack; echo "rc=$?"` 在本机**恒打印 rc=0**——`$?` 在 `wsl -d Ubuntu -- bash -lc '…'` 这个字符串里**不反映被跑命令的真实退出码**（实测 `false; echo "rc=$?"` 也给 0）；**可靠通道是 `wsl …` 这个外层调用的退出码**（`wsl … bash -lc 'false'` 的外层 RC=1）。故本任务 Step 2/3/4 的退出码一律读**外层**，Step 4 的 rc=1 也如此取得。与 CLAUDE.md「WSL 命令的 `$` 展开陷阱」同族、但不同形（此处非外层 Git Bash 提前展开，而是 WSL 侧 `$?` 本身不忠）——后续凡在内层 `echo "rc=$?"` 取判据的派单都会拿到假绿/假红，建议统一改外层退出码形态。

10. **D213 的观察面选错（Task 8 评审掀出，控制者的变异设计之误——实现者执行无误）**：计划 Task 8 Step 4 原文与 spec §5.2 都写「抽掉 `kVasePluginTable` 里一行 ⇒ **枚举面少一条**」为判据，并把「对象库下变异不会让枚举面少一条」当对照。**这对照句不成立**：`Cmake/VasePackDispatch.cpp.in` 的 `VasePlugin_Descriptors` 是 `outCount = kVasePluginTable.size()`——**枚举计数完全由那张表的字面长度驱动**，删一行 ⇒ 表长 2 ⇒ 枚举面 2，这个后果由**改字面量本身**直接给出，与「静态库按需提取」与否无关；换成对象库形态（`$<TARGET_OBJECTS:>`）同一行删除**仍让枚举面变 2**。故那枚 red **区分不了**对象库与静态库，D199 那一票没被真正兑现。
    **判决**：**换观察面重做**——判据改为「`VasePluginDesc_FailingPlugin` 是否还在组合库**导出表**里」（观察「那个符号在不在库里」，不是枚举计数）。重做实测（`llvm-readobj --coff-exports bin/VasePlugins.dll`）：删引用前导出表 **5 枚含 `VasePluginDesc_FailingPlugin`**；删掉表里那一行**引用**（**保留** `extern "C"` 声明——声明不产生引用，留着恰好隔离出「拉动静态库成员的是**引用**不是声明」）后，该符号**从导出表消失**（余 4 枚）；撤销（重新 configure 还原生成物）+ build 后恢复 5 枚、六条证人 6/6 绿。**这才是能区分两形态的证据**。spec §5.2、D213 行与本节 Step 4（含「对照意义」段）已就地改准。
    **教训（记名，本波第三次同类，形态不同）**：前两次（第 7、8 条）是「计划里的 C++ 撞门禁」，这次是「**计划的判据本身不自洽**」——写判据时没问「这个可观察量**真正由什么决定**」。枚举计数看着像「库里有几个插件」，实际读数是「表里写了几行」；一个可观察量与被断言机制之间的因果链断了，变异就成了自我实现。**下次写变异取证，先写清「这个量由谁决定」，再确认变异动的正是那个「谁」**。

11. **八线收口波（Task 10）实测与预期逐位一致；一形假红重跑**：八条 preset 线删树重配全量、`ctest` 与 `ctest -N` 一并跑完。

    **读数**：debug **348 × 4** / release **347 × 4**、平台差 **0**、`release − debug` 恰为 M1 T3 那枚 death test（`EffectScopeDeath.CreateAfterDisposeTerminates`，受 `#ifndef NDEBUG` 门）；八线用例名集合逐对求差——debug 四条两两全等、release 四条两两全等、`release − debug` 空。与 plan「预期基数变化」（gtest **+7** → 348/347）**逐位一致**。tidy 四线**首跑即三判据全过、正文双 0**，TU **119**（+2 = `Tests/Integration/VasePackTests.cpp` + 生成的 `VasePlugins.dispatch.cpp`，四线同幅），Suppressed Windows 802687 / Linux 331259 / macOS 468442、NOLINT 命中 5145 / 5140 / 5141（三线各 **+250**；canonical 位点净增 **3** = `VasePackTests.cpp` 两枚 `reinterpret-cast` + `Cmake/VasePackDispatch.cpp.in` 一枚 naming 区——**首次给生成物就地写 NOLINT 理由**，D205）；单 TU 极值 Windows 998/45609、Linux 359/11337 **未动**，macOS min 278 未动 / max **7278→7284**（落 `Samples/HelloPlugin/HelloPlugin.cpp`，**成因未归因**——`HelloCommon` 改经 INTERFACE 后 include 路径字符串前后未变、原「包含路径形态变化」的推断存疑，未做逐 header 对照；可背书的只有「非新正文诊断」，四线正文仍双 0）。format 三线 **RC=0**（Windows/Linux `clang-format` 23.1.0、macOS `clang-format-mp-23` 23.1.2）。规矩 6 族 `-R 'HotSwap|Eject|Adopt'` 三平台各 **58/58 全绿**（本波动了 `PluginDescriptor.h` 宏而触发，D206）。

    **假红与重跑**：Windows 四条、Linux 两条**首跑即绿、零重跑**；**macOS debug 线首跑 `ctest` 与 `ctest -N` 各返回 RC=8**——正文是 `discover_tests failed to run command … Process terminated due to timeout`（`gtest_discover_tests` 的 `PRE_TEST` 枚举步超时，**新形态**，不是用例回归），按「先重跑再判」单线重跑即 **348/348 全绿、`ctest -N` 348**（其 release 线首跑即绿、347）。macOS `rm -rf` 预清亦撞过一次 `Resource busy`（SMB 目录缓存，第 6 次），未拖倒 configure。

    **两笔文书修正（Task 9 评审路由）**：① `CLAUDE.md`「构建与测试」的产物落位句补上「发布期组合库（`VasePlugins`）平铺在产物根」——`README.md` 副本已有、权威源此前缺，方向反了；② WSL `$?` 坑的**因果句收窄**：由「WSL 侧 `$?` 本身不忠」改为「跨 Windows→WSL 边界的**命令行参数传输**不保真」（payload 用**脚本文件**投递时 `$?` 报得准；操作建议『取外层 rc』不变）。

    **spec 复核**：§5.5 措辞与磁盘一致（无差额，不挂注）；§5.1 的 `+6`（`341/340 → 347/346`）**只指六条证人**，连「另加一条布局断言」（+1）共 **+7 → 348/347**——已就地挂一枚**消歧勘误注**（不改史）。

    **一处顺带订正**：`CLAUDE.md` 的 `ctest -R '^VaseCli'` 现值此行此前记作 **50**，实测 **51**（插件产物布局波 D189 所加的 `VaseCliValidateProductRoot` 当时漏记进此行），已改准为 51 并补注 ctest 级 11 条。


