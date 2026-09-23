# VaseConsole 验证台 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建 `Tools/VaseConsole` —— 一个交互式命令行验证台，同一套命令既能人肉敲、也能用 `--script` 回放，退出码即"全程是否 Clean"的判据。

**Architecture:** 四层各管一件事：`Shell` 只做命令表 ↔ `ThirdParty/cli`（我们 Own 的无异常 fork）的桥，不认识 Vase；`PlanFile` 把 plan 文本解析成 `std::vector<Entry>`；`Bench` 拥有 `PluginHost`、活局表、暂存字节与**累计判据**；`main.cpp` 把"循环为什么结束"与"累计判据"合成退出码。测试沿用本仓库既有的 sample 纪律：**不给 sample 内部写单测，把 sample 当被测物用 `add_test` 跑端到端**（`Samples/Embedding/CMakeLists.txt:20` 就是先例）。

**Tech Stack:** C++20 · CMake 4.3 + presets · vcpkg（只有 gtest）· GoogleTest 1.18（不用于本 sample）· `ThirdParty/cli`（submodule，BSL-1.0）

**Spec:** `docs/superpowers/specs/2026-09-21-vase-console-design.md` —— 本计划实现它，且**只**实现 §3–§7；§5（fork 改造）已完成并提交（`e799549`）。执行者必须同时读 spec：里面每一条"为什么"都不在本计划的代码里重复。

> **更名注记（2026-09-23）**：本计划落地时该验证台叫 `VaseCli`、建在 `Samples/VaseCli/`；此后因「CLI」不体现它是**交互式**验证台而更名为 `VaseConsole`，并移入 `Tools/VaseConsole/`。**本文已按新名同步全文**（含 ctest 用例名 `VaseConsole*`、CMake 变量 `VASE_CONSOLE_*`、`add_subdirectory(Tools/VaseConsole)`）。按本文回放历史提交时，当时那个路径是 `Samples/VaseCli`。

## Global Constraints

每一个任务的隐含要求。**违反其中任何一条都算任务未完成**：

- **C++20**；新 target 一律链 `VaseBuildOptions`（根 `CLAUDE.md` 规矩 1），**不新增任何异常开关、不新增 target property、不改 `VaseBuildOptions`**（设计文档 §9 已把这条路否掉）。
- **不写 `throw` / `try` / `catch`**（规矩 4）。可恢复失败用 `vase::Result<T>` / 返回值表达。
- **`_HAS_EXCEPTIONS=0` 下 STL 前置条件失败 = terminate，不是可捕获的异常**：`<filesystem>` 只用 `std::error_code` overload；**禁 `.at()`**；禁 `std::stoi/stof/stod/stold`；数值解析只用 `std::from_chars`（`Samples/Embedding/main.cpp:112` 的 `ParseCount` 是同写法先例）。
- **包含 `ThirdParty/cli` 的头必须用引号**：`#include "cli/cli.h"`，**不是** `<cli/cli.h>`。这是反直觉的一条，机制与被否写法见 `Cmake/VaseThirdParty.cmake` 与设计文档 §5.4（尖括号会让 cl.exe 线的异常守卫静音）。Vase 自己的头同样一律引号（规矩 3）。
- 只用 cli 的 `Cli` / `Menu` / `CliSession` 三个公开面；**不包含 `clifilesession.h`**（它把 history 大小钉成 1，且拿不到"脚本读完却没走 exit"这个停止原因——见 spec §5.5），**也不包含 `clilocalsession.h`**（v1）。
- 不新增 Vase 机制：只调 `CreatePod` / `DestroyPod` / `Resolve` / `EjectPlugin` / `AdoptPlugin` / `Pod::{Root,PluginCount,HasPlugin,PluginIds,Failures}` / `detail::Loader::FileIdentity`。**不提供 `load <path>`**（spec §3.2 推论 3）。
- **宿主侧解析服务只用 `Context::TryGet<T>()`**（`Include/Vase/Pod/Context.h:87`，缺失返回 `nullptr`）。`Get<T>()` 走的是 `required=true`（同文件 `:82`），缺失即 `ProgrammerError` 终止——验证台在 `eject` 之后调它会当场打死进程，把"注册已被撤掉"这条最重要的证据变成一次崩溃。
- 不缓存 `vase::Pod*` / `Pod&`：每次现 `Resolve`（§1.4 句柄语义 + 9.3 宿主纪律）。`Bench` 里只存 `PodHandle`（两个 `uint32_t`）、字符串与字节，**装不下实例级对象**（铁律 §1.2 靠类型承载，不靠约定）。
- 新插件 target 经 `vase_add_plugin_fixture` 立（规矩 5），不手搭 `add_library(SHARED)`。
- 格式：Allman 大括号、`PointerAlignment: Left`、构造初始化表**每个初始化式一行且逗号在行首**（`PackConstructorInitializers: Never` + `BreakConstructorInitializers: BeforeComma`）、`BreakTemplateDeclarations: Yes`。命名：函数/类/成员 `CamelCase`（成员**不带**前后缀）、参数与局部 `camelBack`、常量与 constexpr `k` 前缀、枚举常量 `k` 前缀。
- 注释中文、一到三行，判据是"删掉它读者会不会踩坑"。出处用 `§x.y` 指针。
- 提交信息：标题 + 中文说明体，**不加任何 AI 工具/模型署名尾注**（无 `Co-Authored-By`、无 `Generated-with`）。
- **ctest 判据形状只有一种**（spec §2.19 实测）：`PASS_REGULAR_EXPRESSION` 命中即 Passed、**完全无视退出码**；与 `WILL_FAIL` 同设时命中反而判 Failed。所以：每条用例默认判据只钉**退出码**；要钉文本就**另起一条兄弟用例**用裸 `PASS_REGULAR_EXPRESSION`，并在其旁注释"此条无视退出码，码由兄弟用例钉"。**两者绝不同时设**。且只在退出码表达不了的信息上配兄弟用例（换件串、`no provider`、`hotswap log (N)`、拒因文本）；`VaseConsolePodCycle` **不配**——`rc=0` 已经蕴含 `clean=true`。
- **`exit` 的职责是拆净所有活局**（spec §3.3）：逐个 `DestroyPod` + 打完整报告，Clean 判定走规则 ②。**不留活局给 `~PluginHost`** —— Debug 下它断言 `!slot->Alive`（`Source/Host/PluginHost.cpp:155-158`），我们的退出码会被 assert 吃掉（实测 `pod new; exit` → rc=3）。
- **未匹配的输入算失败**（spec §3.3 规则 ①）：打错的命令名走 cli 的 `WrongCommandHandler`、不进任何 Vase 处理器，实测 `bogus; exit` → rc=0。须经 `cli.h:169` 那个 setter 接进失败状态，否则"脚本敲错一条、其余全成功"会假绿。
- 基数（`ctest -N` 的 Total Tests、tidy 的 TU 数与 Suppressed 合计）的真值只住根 `CLAUDE.md`（规矩 7）。本计划里出现的"预计 +N"是**待测项**，不是要抄进别处的数；只有 Task 7 实测之后才更新那张表。
- 本 sample **不碰** Loader / 依赖账本 / Eject / Adopt 路径 / 描述符布局 / `kHeaderVersion` → 按规矩 6 不触发"HotSwap 全量"那一档；Task 7 的触发器来自"改了根 CMakeLists 与新增构建树"。

---

## 文件结构

| 文件 | 职责（一句话） | 何时创建 |
|---|---|---|
| `Tools/VaseConsole/Shell.h` | `CommandSpec` + `ShellStop` + `RunShell` 的对外面，不含任何 cli 类型 | Task 2 |
| `Tools/VaseConsole/Shell.cpp` | 全仓库唯一包含 `cli/*.h` 的 TU；把命令表变成 cli 菜单并驱动读循环 | Task 2 |
| `Tools/VaseConsole/PlanFile.h` | `Entry{Id, BinaryPath}` + `ParsePlanFile` 返回 `Result<vector<Entry>>` | Task 3 |
| `Tools/VaseConsole/PlanFile.cpp` | plan 文本解析：注释、空行、"首个空白前是 Id、其余整段是路径" | Task 3 |
| `Tools/VaseConsole/Commands.h` | `Bench`：活局表 + 暂存字节 + 累计判据 + 命令表出口 | Task 3 |
| `Tools/VaseConsole/Commands.cpp` | 每条命令的处理器与三种报告的打印 | Task 3–6 |
| `Tools/VaseConsole/main.cpp` | `--script` / 交互两条入口 + 退出码合成 | Task 2 |
| `Tools/VaseConsole/CMakeLists.txt` | 可执行 target、`file(GENERATE)` 出回放资产、`add_test` | Task 2–6 |
| `Samples/HelloPluginPrime/HelloPlugin.cpp` + `CMakeLists.txt` | 与 `HelloPlugin` 同 Id、不同返回串的换件材料 | Task 5 |
| `CMakeLists.txt`（根，改） | `add_subdirectory(Tools/VaseConsole)` 与 `Samples/HelloPluginPrime` | Task 2 / 5 |
| `CLAUDE.md`（改） | 基数表、Samples 行、规矩 6 补一个"判据按平台不对称"的第二实例 | Task 7 |

`Bench` 与 `Shell` 之间只隔一个 `std::function`，`Commands.cpp` 与 cli 之间**零接触**——评审时"谁认识谁"这条线一眼可见。

---

## Task 1: 把 fork 的改动落到可被别人取到的地方

**这是人类动作，不是编码动作。** `ThirdParty/cli` 的 gitlink 现在钉在 `769c5fa`（上游、**不含**无异常改造），而工作树里有那些改动——也就是说别人 clone 之后 `Tools/VaseConsole` 会以 20 条编译错误失败（方向是响亮的，但只有这台机器是绿的）。设计文档 §9 拒绝"裸 clone 进子目录"的理由在这里同样成立。

**Interfaces:**
- Produces: 一个推进后的 `ThirdParty/cli` gitlink，指向含 §5.2 改造的 commit。

- [ ] **Step 1: 确认待提交的内容就是设计文档 §5.2 那一张表**

```bash
cd ThirdParty/cli && git status --short && git --no-pager diff --stat HEAD | tail -3
```
Expected: 24 files changed、约 +222 / −2067；16 个 asio 头在 `git diff --cached --diff-filter=D` 里。

- [ ] **Step 2: 处理 fork 侧留下的两件事**

**(a) examples / test 链不过。** fork 里 `examples/{filesession,simplelocalsession,complete,pluginmanager}.cpp` 与 1 个 test 用了 `int` 参数，现在会在**链接期**失败（`from_string<int>` 没有定义）。三选一，**问需求方要一个答案，不要自己定**：改这些 example 用 string + `from_chars`；或删掉它们；或留着（那 fork 侧 CI 一开 examples 就红，本身是一种哨兵）。Vase 这边碰不到它们（不 `add_subdirectory`）。

**(b) `split.h` 那 5 处 `vector::back()`**（`:138,152,163,171,172`）。空 vector 上调 `back()` 是 UB，而 `_HAS_EXCEPTIONS=0` 下 MSVC STL 的前置条件失败是 terminate —— 这是**上游既有、今天就在跑**的代码，不是 fork 引入的，但 fork 之后归我们审。逐处读上下文确认容器在那几条路径上非空（`split` 一进来就 `push_back` 一个空串，所以大概率恒非空），把结论写进 fork 的 README 分歧一节：

```bash
cd ThirdParty/cli && sed -n '125,175p' include/cli/detail/split.h
```
Expected 结论有两种，都要如实记下：确认恒非空 → 加一行注释说明依据；找得到空容器路径 → 改成先判再取（这属于 fork 该修的 bug，不是清洁癖）。

- [ ] **Step 3:（需授权）在 submodule 内提交并推送**

```bash
cd ThirdParty/cli
git add -A && git commit -m "移除异常用法与 asio 支路：core 路径不使用异常"
git push origin HEAD:master
```
Expected: push 成功。**没有写权限或未获授权时停在这里**，把 Step 4 推迟到 Task 7 之后统一处理，并在最终汇报里明说"gitlink 仍指向上游 commit"。

- [ ] **Step 4: 推进父仓库的 gitlink**

```bash
cd /d/Git/Vase && git add ThirdParty/cli && git commit -m "推进 ThirdParty/cli 到含无异常改造的 commit"
```
Expected: `git status` 里 `ThirdParty/cli` 不再带 ` m`（dirty）标记。

---

## Task 2: 能跑起来的最小壳 —— Shell 桥 + `exit` + 第一条 ctest

结束时 `VaseConsole --script <只含 exit 的脚本>` 退 0，且它是 ctest 里的一条真用例。本任务把最难搭的两件事一次做完：**cli 的菜单接线**与**回放资产的路径注入**。

**Files:**
- Create: `Tools/VaseConsole/Shell.h`, `Tools/VaseConsole/Shell.cpp`, `Tools/VaseConsole/main.cpp`, `Tools/VaseConsole/CMakeLists.txt`
- Modify: `CMakeLists.txt`（根，在 `add_subdirectory(Samples/Embedding)` 之后加一行）
- Test: `ctest --preset win-x64-clang-debug -R VaseConsoleExitOnly`

**Interfaces:**
- Produces:
  - `samples::shell::CommandSpec{ std::string Group; std::string Name; std::vector<std::string> Params; std::string Help; std::function<void(std::ostream&, const std::vector<std::string>&)> Handler; }` —— `Group` 空则挂在根菜单，非空则挂在同名子菜单下（实测 `pod new a b` 路由到组 `pod` 内的 `new`，args = `["a","b"]`）
  - `enum class samples::shell::ShellStop : std::uint8_t { kByExitCommand, kEndOfInput, kInputStreamError };`
  - `samples::shell::ShellStop samples::shell::RunShell(const std::vector<CommandSpec>& commands, std::istream& in, std::ostream& out);`
  - `main.cpp` 的退出码 = `Bench::Verdict(stop)`（Task 3 之前 `Verdict` 是临时的"只看 stop"版本）

- [ ] **Step 1: 写 `Shell.h`**

```cpp
#pragma once

// 命令表 ↔ ThirdParty/cli 的桥。本头文件**不出现任何 cli 类型**：认识 cli 的只有
// Shell.cpp，Commands 那一侧只看见 CommandSpec。

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

namespace samples::shell
{

struct CommandSpec
{
    std::string Group;   // 空 = 挂在根菜单上；非空 = 挂在同名子菜单下（如 "pod" / "file"）
    std::string Name;    // 子命令名，单 token（"new" 而不是 "pod new"）
    std::vector<std::string> Params; // 参数名，只为 usage/help 服务；值一律 string
    std::string Help;
    std::function<void(std::ostream&, const std::vector<std::string>&)> Handler;
};

// 循环为什么结束。**不是**「跑得好不好」——累计判据在命令层（spec §5.2）。
enum class ShellStop : std::uint8_t
{
    kByExitCommand,
    kEndOfInput,
    kInputStreamError,
};

ShellStop RunShell(const std::vector<CommandSpec>& commands, std::istream& in, std::ostream& out);

} // namespace samples::shell
```

- [ ] **Step 2: 写 `Shell.cpp`，然后立刻编译它**

```cpp
#include "Shell.h"

// 引号包含是**承重的**：cl.exe 线的 /external:anglebrackets 会把尖括号包含的整棵头树标为
// 外部头，连 /we4530 升出的 C4530 一起静音——那正是本 fork 「不使用异常」这条不变式
// 唯一的编译期把守。理由与被否写法见 Cmake/VaseThirdParty.cmake（spec §5.4）。
#include "cli/cli.h"

#include <algorithm>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace samples::shell
{
namespace
{
// 不用 cli::CliFileSession：它把 history 大小钉成 1（clifilesession.h:46），而且它的
// Start() 只看 eof、拿不到「脚本读完却没走 exit」这个停止原因——那是退出码规则 ④ 的输入。
constexpr std::size_t kHistorySize = 100;
} // namespace

ShellStop RunShell(const std::vector<CommandSpec>& commands, std::istream& in, std::ostream& out)
{
    auto root = std::make_unique<cli::Menu>("vase");
    // 子菜单按 Group 惰性建立。实测：`pod new a b` 会路由到子菜单 new 且 args=["a","b"]，
    // 所以两段名不需要把空格塞进命令名（cli 先按空白切 token，名字里带空格永远匹配不上）。
    std::vector<std::pair<std::string, std::unique_ptr<cli::Menu>>> groups;
    for (const CommandSpec& command : commands)
    {
        cli::Menu* parent = root.get();
        if (!command.Group.empty())
        {
            const auto found = std::find_if(groups.begin(), groups.end(),
                                            [&command](const auto& entry)
                                            { return entry.first == command.Group; });
            if (found != groups.end())
            {
                parent = found->second.get();
            }
            else
            {
                auto menu = std::make_unique<cli::Menu>(command.Group);
                cli::Menu* raw = menu.get();
                groups.emplace_back(command.Group, std::move(menu));
                parent = raw;
            }
        }
        // 拷贝一份 handler：Insert 把它移进菜单持有的命令对象，而 command 是调用方的。
        auto handler = command.Handler;
        parent->Insert(
            command.Name, command.Params,
            [handler](std::ostream& sessionOut, const std::vector<std::string>& args)
            {
                if (handler)
                {
                    handler(sessionOut, args);
                }
            },
            command.Help);
    }
    for (auto& [name, menu] : groups)
    {
        root->Insert(std::move(menu));
    }

    cli::Cli cli(std::move(root));

    bool exitRequested = false;
    cli::CliSession session(cli, out, kHistorySize);
    session.ExitAction([&exitRequested](std::ostream&)
                       {
                           exitRequested = true;
                       });
    session.Enter();

    ShellStop stop = ShellStop::kEndOfInput;
    while (!exitRequested)
    {
        session.Prompt();
        std::string line;
        if (!in.good())
        {
            stop = ShellStop::kInputStreamError;
            break;
        }
        // 先 Feed 再判 eof：上游 CliFileSession 在 eof 时丢掉最后一行，
        // 脚本若不以换行结尾就会把 exit 吞掉——那会让规则 ④ 误判成失败。
        const bool gotLine = static_cast<bool>(std::getline(in, line));
        if (gotLine)
        {
            session.Feed(line);
        }
        else
        {
            stop = in.eof() ? ShellStop::kEndOfInput : ShellStop::kInputStreamError;
            break;
        }
    }

    if (exitRequested)
    {
        stop = ShellStop::kByExitCommand;
    }
    out << std::flush;
    return stop;
}

} // namespace samples::shell
```

验证它能编过（**注意 `MSYS2_ARG_CONV_EXCL`**：不设的话 `/EHs-c-` 会被 Git Bash 改写成路径，你会拿到一个"看起来过了其实没编"的结果——本轮已撞到四次，spec §2 引言块）：

```bash
export MSYS2_ARG_CONV_EXCL='*'
WIN=$(cygpath -w /c/Users/xingxing/AppData/Local/Temp)
clang-cl -std:c++20 /EHs-c- /W4 /WX /utf-8 /D_HAS_EXCEPTIONS=0 \
  -I ThirdParty/cli/include -Fo"$WIN/shell.obj" -c Tools/VaseConsole/Shell.cpp
```
Expected: 无输出、rc=0。若 `Insert` 的四参重载推导失败，改成显式 `std::function` 变量再传（`auto fn = std::function<void(std::ostream&, const std::vector<std::string>&)>(...); root->Insert(command.Name, command.Params, fn, command.Help);`）——本计划写它之前已实测该四参形式在 `/EHs-c-` 下编译并通过运行（`eject 1` / `wrong command: bogus`）。

- [ ] **Step 3: 写 `main.cpp`（此任务的临时版：判据只看 stop）**

```cpp
// VaseConsole —— 交互式热插拔验证台（spec §1）。同一套命令既能敲也能 --script 回放，
// 退出码即「全程是否 Clean」。
#include "Commands.h"
#include "Shell.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string_view>

int main(int argc, char** argv)
{
    std::istream* input = &std::cin;
    std::ifstream script;

    if (argc > 1)
    {
        const std::string_view flag{*std::next(argv, 1)};
        if (flag != "--script" || argc < 3)
        {
            std::cerr << "usage: VaseConsole [--script <file>]\n";
            return 2;
        }
        script.open(std::string{*std::next(argv, 2)}.c_str(), std::ios::binary);
        if (!script.is_open())
        {
            // 打不开脚本是「这次判定没发生」，不是干净运行。
            std::cerr << "cannot open script: " << *std::next(argv, 2) << "\n";
            return 2;
        }
        input = &script;
    }

    samples::bench::Bench bench;
    const samples::shell::ShellStop stop = samples::shell::RunShell(bench.Commands(), *input, std::cout);
    return bench.Verdict(stop);
}
```

- [ ] **Step 4: 写 `Commands.h` / `Commands.cpp` 的**最小可编译版本

本任务只需要它存在且能让 `exit` 走通；完整 `Bench` 在 Task 3。

```cpp
// Commands.h
#pragma once

#include "Shell.h"
#include <vector>

namespace samples::bench
{
class Bench
{
public:
    Bench() = default;
    Bench(const Bench&) = delete;
    Bench& operator=(const Bench&) = delete;
    Bench(Bench&&) = delete;
    Bench& operator=(Bench&&) = delete;
    ~Bench() = default;

    [[nodiscard]] std::vector<shell::CommandSpec> Commands();
    [[nodiscard]] int Verdict(shell::ShellStop stop) const;
};
} // namespace samples::bench
```

```cpp
// Commands.cpp
#include "Commands.h"

namespace samples::bench
{
std::vector<shell::CommandSpec> Bench::Commands()
{
    // Task 3 起填 pod 那一组。这里空表即可：exit / help / history / ! 由 cli 自带。
    return {};
}

int Bench::Verdict(shell::ShellStop stop) const
{
    // 临时版。规则 ①–⑤ 在 Task 3 落地。
    return stop == shell::ShellStop::kByExitCommand ? 0 : 1;
}
} // namespace samples::bench
```

- [ ] **Step 5: 写 `Tools/VaseConsole/CMakeLists.txt`，含第一个回放资产**

```cmake
add_executable(VaseConsole main.cpp Shell.cpp Commands.cpp)

# 宿主侧只链 VaseHost（它 PUBLIC 带 VasePod）。VaseThirdPartyCli 的接入方式与
# 「为什么不是 add_subdirectory / vcpkg」写在 Cmake/VaseThirdParty.cmake。
target_link_libraries(VaseConsole
    PRIVATE VaseBuildOptions
            VaseHost
            VaseThirdPartyCli)

# 回放资产用 file(GENERATE) 而不是 configure_file：产物路径要到 generate 期才求值，
# configure_file 拿不到 $<TARGET_FILE:...>（spec §7.2 的待实测项，此处即答案）。
set(VASE_CONSOLE_REPLAY_DIR "${CMAKE_CURRENT_BINARY_DIR}/replay")

file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/exit_only.txt"
     CONTENT "exit\n")

add_test(NAME VaseConsoleExitOnly
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/exit_only.txt")
```

根 `CMakeLists.txt` 里在 `add_subdirectory(Samples/Embedding)` 之后加：

```cmake
add_subdirectory(Tools/VaseConsole)
```

- [ ] **Step 6: 构建并跑到红的（这条用例应当因"可执行文件不存在"而失败之前，先确认构建本身过）**

```bash
cmake --preset win-x64-clang-debug
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R VaseConsoleExitOnly
```
Expected: `100% tests passed out of 1`。若失败，先看 `--output-on-failure` 里的 stderr 是不是 `usage:`（说明 `--script` 解析没接上）。

- [ ] **Step 7: 确认基数与既有测试没被碰坏，然后提交**

```bash
ctest --preset win-x64-clang-debug -N | grep "Total Tests"
ctest --preset win-x64-clang-debug | tail -3
```
Expected: `Total Tests: 76`（原 75 + 本条）；全量 76/76 passed。

```bash
git add Tools/VaseConsole CMakeLists.txt
git commit -m "VaseConsole 骨架：cli 桥、--script 入口与第一条回放用例"
```

---

## Task 3: plan 文件 + 会话状态 + `pod new` / `pod destroy` + 退出码五条规则

> **Task 2 评审留下的三条交接义务，本任务必须一起做完**（前两条是"现在无症状、本任务落地即成真 bug"）：
> 1. **`Shell.cpp` 的停止原因分类要收窄。** 现在循环顶部是 `if (!in.good()) → kInputStreamError`；而末行不带换行时 `getline` 只置 **eofbit**、不置 failbit，下一轮就会把"脚本自然读完"错报成 `kInputStreamError`。Task 2 的临时 `Verdict` 把两者同归 1 所以看不出来，**本任务的规则 ④/⑤ 要区分它们**：判据改成 `in.bad() || in.fail()` 才算 `kInputStreamError`，仅 eofbit 归 `kEndOfInput`。
> 2. **同处注释的机制要说对。** 现有注释把"末行不被吞"归给"先 Feed 再判 eof"；真正的机制是 `std::getline` 在末行无换行时**仍抽得出字符**（只置 eofbit）。改完第 1 条后重写这段注释。
> 3. **`Commands.cpp` 里那 2 处 `NOLINTNEXTLINE(readability-convert-member-functions-to-static)` 必须消失。** 它们成立的理由是"签名被跨任务契约规定、static 与 const 不能共存"；本任务给 `Bench` 落了会话状态之后 `Commands()`/`Verdict()` 真的会读成员，检查自然不报。**若落地后 NOLINT 还在，就是留着一条已不成立的抑制**——删掉它。
>
> **评审后追加（Task 3 修复轮）**：
> 4. **`exit` 要拆净活局**：`exit` 之前先把所有 `Pods` 逐个 `DestroyPod` 并打报告（Clean 判定照常走规则 ②），再让 `Verdict` 收口。删掉规则 ③（`!Pods.empty()` 那一支）——它与"`exit` 拆净"自相矛盾，且实测在 Debug 下**根本轮不到我们返回 1**：`~PluginHost` 断言 `!slot->Alive`（`Source/Host/PluginHost.cpp:155-158`），`pod new; exit` 实得 rc=3。规则集从五条改为四条（spec §3.3 已改）。
> 5. **未匹配的输入要算失败**（新规则 ① 的后半）：`RunShell` 增设一个"未匹配"回调参数，经 cli 现成的 setter（`ThirdParty/cli/include/cli/cli.h:169` 的 `WrongCommandHandler`）接进来，`Bench` 在回调里 `MarkFailed()`。实测对照必须做：加线前 `bogus; exit` → rc=0，加线后 → rc≠0。
> 6. **`Tools/VaseConsole/CMakeLists.txt:31` 删掉 `VaseConsolePodCycle` 的 `PASS_REGULAR_EXPRESSION`**，让它回到默认判据（退出码）。理由与 ctest 语义见 Global Constraints：正则命中会**吞掉退出码判定**，使"早先命令被拒、最后仍打 clean=true"的脚本假绿。
> 7. 报告的实测矩阵两行与实机不符（未知命令 rc、活局 exit rc），随第 4、5 条修完后**重测并重写那两行**；另注意 `--output-on-failure` 只在用例 **FAILED** 时打输出，绿的 `WILL_FAIL` 用例看不到正文，别把它当"文本已核对"的证据。


**Files:**
- Create: `Tools/VaseConsole/PlanFile.h`, `Tools/VaseConsole/PlanFile.cpp`
- Modify: `Tools/VaseConsole/Commands.h`, `Tools/VaseConsole/Commands.cpp`, `Tools/VaseConsole/CMakeLists.txt`
- Test: `ctest --preset win-x64-clang-debug -R "VaseConsole"`（新增 6 条：`VaseConsolePodCycle`、`VaseConsoleNoExitIsFailure`、`VaseConsoleBadPlan`、`VaseConsoleBadPlanReason`、`VaseConsoleUnknownCommandIsFailure`、`VaseConsoleUnknownCommandReason`；连同 Task 2 的 `VaseConsoleExitOnly` 共 7 条）

**Interfaces:**
- Consumes: Task 2 的 `shell::CommandSpec` / `ShellStop` / `RunShell`
- Produces:
  - `struct samples::plan::Entry { std::string Id; std::filesystem::path BinaryPath; };`
  - `vase::Result<std::vector<Entry>> samples::plan::ParsePlanFile(const std::filesystem::path& file);`
  - `Bench` 的完整私有状态与 `MarkFailed()` / `Active()` / `ResolveActivePod()`；命令 `pod new <planFile>`、`pod use <index>`、`pod list`、`pod destroy`

- [ ] **Step 1: 写 `PlanFile.h`**

```cpp
#pragma once

// plan 文件：CLI 私有的「Id ↔ 二进制路径」文本，M1 用来代替还不存在的 Catalog。
// **不是**架构文档 §5.1 的清单格式，M2 起与 KnownBinaries 一起退场（spec §8.2）。

#include "Vase/Detail/Result.h"

#include <filesystem>
#include <string>
#include <vector>

namespace samples::plan
{

struct Entry
{
    std::string Id;
    std::filesystem::path BinaryPath;
};

// 一行一条：首个空白之前是 Id，其余整段（含其中的空白）都是路径。
// `#` 起始行与空行忽略。数组序 = 文件序 = LoadPlan::Ordered 序。
vase::Result<std::vector<Entry>> ParsePlanFile(const std::filesystem::path& file);

} // namespace samples::plan
```

- [ ] **Step 2: 写 `PlanFile.cpp`，然后单独编它**

```cpp
#include "PlanFile.h"

#include "Vase/Detail/Result.h"

#include <fstream>
#include <string>
#include <system_error>

namespace samples::plan
{
namespace
{

std::string_view Trim(std::string_view text)
{
    constexpr std::string_view blanks = " \t\r\n";
    const std::size_t begin = text.find_first_not_of(blanks);
    if (begin == std::string_view::npos)
    {
        return {};
    }
    const std::size_t end = text.find_last_not_of(blanks);
    return text.substr(begin, end - begin + 1);
}

using Outcome = vase::Result<std::vector<Entry>>;

} // namespace

vase::Result<std::vector<Entry>> ParsePlanFile(const std::filesystem::path& file)
{
    std::ifstream input(file, std::ios::binary);
    if (!input.is_open())
    {
        return Outcome::Err(vase::Error("cannot open plan file: " + file.string()));
    }

    std::vector<Entry> entries;
    std::string line;
    std::size_t lineNo = 0;
    while (std::getline(input, line))
    {
        ++lineNo;
        const std::string_view body = Trim(line);
        if (body.empty() || body.front() == '#')
        {
            continue;
        }
        const std::size_t splitAt = body.find_first_of(" \t");
        if (splitAt == std::string_view::npos)
        {
            return Outcome::Err(vase::Error("plan line " + std::to_string(lineNo) + ": missing binary path"));
        }

        Entry entry;
        entry.Id = std::string(body.substr(0, splitAt));
        entry.BinaryPath = std::filesystem::path{std::string{Trim(body.substr(splitAt + 1))}};
        if (entry.Id.empty() || entry.BinaryPath.empty())
        {
            return Outcome::Err(vase::Error("plan line " + std::to_string(lineNo) + ": empty id or path"));
        }

        std::error_code ec;
        if (!std::filesystem::exists(entry.BinaryPath, ec) || ec)
        {
            return Outcome::Err(vase::Error("plan line " + std::to_string(lineNo) + ": binary not found: " +
                                            entry.BinaryPath.string()));
        }
        entries.push_back(std::move(entry));
    }

    if (entries.empty())
    {
        return Outcome::Err(vase::Error("plan file has no entries: " + file.string()));
    }
    return Outcome::Ok(std::move(entries));
}

} // namespace samples::plan
```

`exists` 走的是 `error_code` overload —— `_HAS_EXCEPTIONS=0` 下不带 `ec` 的那一个失败即 terminate，没有可接住的东西（根 `CLAUDE.md` 规矩 4）。

- [ ] **Step 3: 把 `Bench` 扩成真正的会话状态**

`Commands.h` 替换为：

```cpp
#pragma once

#include "PlanFile.h"
#include "Shell.h"
#include "Vase/Host/PluginHost.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace samples::bench
{

class Bench
{
public:
    Bench() = default;
    Bench(const Bench&) = delete;
    Bench& operator=(const Bench&) = delete;
    Bench(Bench&&) = delete;
    Bench& operator=(Bench&&) = delete;
    ~Bench() = default;

    [[nodiscard]] std::vector<shell::CommandSpec> Commands();
    [[nodiscard]] int Verdict(shell::ShellStop stop) const;

private:
    // 只存 PodHandle（两个 uint32）、字符串与字节：**装不下实例级对象**（铁律 §1.2）。
    // Pod* 一律每次现 Resolve，不缓存（§1.4 句柄语义 / 9.3 宿主纪律）。
    struct LivePod
    {
        vase::PodHandle Handle;
        std::filesystem::path PlanPath;
        std::vector<plan::Entry> Entries;
    };

    void CmdPodNew(std::ostream& out, const std::vector<std::string>& args);
    void CmdPodUse(std::ostream& out, const std::vector<std::string>& args);
    void CmdPodList(std::ostream& out, const std::vector<std::string>& args);
    void CmdPodDestroy(std::ostream& out, const std::vector<std::string>& args);

    [[nodiscard]] LivePod* Active();
    void MarkFailed();
    [[nodiscard]] static bool ParseIndex(std::string_view text, std::uint32_t& out);

    vase::PluginHost Host;
    std::unordered_map<std::uint32_t, LivePod> Pods; // key = PodHandle::Index（销毁时擦除）
    std::uint32_t ActiveIndex = 0;
    bool HasActive = false;
    bool Failed = false;
};

} // namespace samples::bench
```

- [ ] **Step 4: 实现五条退出规则**

`Verdict` 是判据的唯一出口，五条规则一条都不留默认语义（spec §3.3）：

```cpp
int Bench::Verdict(shell::ShellStop stop) const
{
    // ① 任何命令收到 Err  ② 任何一次 pod destroy 不 Clean   → 都记在 Failed 上
    // ③ exit 时仍有活局   ④ 脚本结尾没走 exit   ⑤ 输入流出错
    if (Failed)
    {
        return 1;
    }
    if (stop != shell::ShellStop::kByExitCommand)
    {
        return 1; // ④ 与 ⑤：判定从未发生，或这次运行本身不完整
    }
    if (!Pods.empty())
    {
        return 1; // ③
    }
    return 0;
}
```

- [ ] **Step 5: 实现 `pod new` / `pod destroy` / `pod use` / `pod list` 与报告打印**

报告打印的字段集合照抄 `Samples/Embedding/main.cpp:88-109`（`PrintReport`）——那份已经过验收，别再发明一遍。`Commands.cpp` 里加匿名 namespace 的三个 helper（`Out` 不需要：cli 给的是 `std::ostream&`，直接 `<<`）与 `PhaseText`：

```cpp
namespace
{

const char* BoolText(bool value) { return value ? "true" : "false"; }

const char* PhaseText(vase::Phase phase)
{
    switch (phase)
    {
    case vase::Phase::kLoad:
        return "load";
    case vase::Phase::kStart:
        return "start";
    case vase::Phase::kAdopt:
        return "adopt";
    case vase::Phase::kEject:
        return "eject";
    case vase::Phase::kUnload:
        return "unload";
    }
    return "?";
}

bool CheckArity(std::ostream& out, const std::vector<std::string>& args, std::size_t need, std::string_view usage)
{
    if (args.size() != need)
    {
        out << "usage: " << usage << "  (got " << args.size() << " argument(s))\n";
        return false;
    }
    return true;
}

void PrintPodReport(std::ostream& out, const vase::PodReport& report)
{
    out << "clean=" << BoolText(report.Clean())
        << " handleWasStale=" << BoolText(report.HandleWasStale) << '\n';
    out << "counters diff (baseline = pod creation):"
        << " effects=" << report.CountersDiff.Effects << " services=" << report.CountersDiff.Services
        << " subscriptions=" << report.CountersDiff.Subscriptions
        << " pluginInstances=" << report.CountersDiff.PluginInstances << " scopes=" << report.CountersDiff.Scopes
        << '\n';
    for (const vase::FailedPluginRecord& failure : report.Failures)
    {
        out << "failed: " << failure.Id << " [" << PhaseText(failure.Stage) << "] " << failure.Message << '\n';
    }
    for (const vase::ResidualEntry& residual : report.Residuals)
    {
        out << "residual: " << residual.OwnerLabel << " x" << residual.Count << '\n';
    }
    out << "hotswap log (" << report.HotSwapLog.size() << "):\n";
    for (const std::string& entry : report.HotSwapLog)
    {
        out << "  " << entry << '\n';
    }
}

} // namespace
```

`pod new`：

```cpp
void Bench::CmdPodNew(std::ostream& out, const std::vector<std::string>& args)
{
    if (!CheckArity(out, args, 1, "pod new <planFile>"))
    {
        MarkFailed();
        return;
    }
    auto parsed = plan::ParsePlanFile(std::filesystem::path{args[0]});
    if (!parsed.IsOk())
    {
        out << "pod new: " << parsed.GetError().Message() << '\n';
        MarkFailed();
        return;
    }
    std::vector<plan::Entry> entries = std::move(parsed.Value());

    vase::LoadPlan plan2;
    for (const plan::Entry& entry : entries)
    {
        // LoadPlanEntry::Id 是 string_view，**只在这次 CreatePod 调用期间有效**：
        // entries 之后会被移进 Pods，届时地址变（SSO），那些 view 就都不能再用了。
        plan2.Ordered.push_back(vase::LoadPlanEntry{.Id = std::string_view{entry.Id}, .BinaryPath = entry.BinaryPath});
    }

    vase::PodOptions options;
    options.Strict = true; // 验证台不接受"半成的局"：一局要么完整要么失败
    auto created = Host.CreatePod(plan2, options);
    if (!created.IsOk())
    {
        out << "pod new: " << created.GetError().Message() << '\n';
        MarkFailed();
        return;
    }

    const vase::PodHandle handle = created.Value();
    LivePod slot;
    slot.Handle = handle;
    slot.PlanPath = std::filesystem::path{args[0]};
    slot.Entries = std::move(entries);
    Pods.insert_or_assign(handle.Index, std::move(slot));
    ActiveIndex = handle.Index;
    HasActive = true;

    out << "pod created index=" << handle.Index << " generation=" << handle.Generation << '\n';
    vase::Pod* pod = Host.Resolve(handle);
    if (pod == nullptr)
    {
        out << "pod new: handle did not resolve immediately\n"; // 不该发生，但报告比崩溃有用
        MarkFailed();
        return;
    }
    for (const std::string& id : pod->PluginIds())
    {
        out << "  plugin: " << id << '\n';
    }
    for (const vase::FailedPluginRecord& failure : pod->Failures())
    {
        out << "  failed: " << failure.Id << " [" << PhaseText(failure.Stage) << "] " << failure.Message << '\n';
        MarkFailed(); // 宽容语义下建局仍可能带失败记录：它必须反映到退出码
    }
}

void Bench::CmdPodDestroy(std::ostream& out, const std::vector<std::string>& args)
{
    if (!CheckArity(out, args, 0, "pod destroy") || !HasActive)
    {
        MarkFailed();
        return;
    }
    LivePod* slot = Active();
    vase::PodReport report = Host.DestroyPod(slot->Handle);
    out << "destroy index=" << report.PodIndex << '\n';
    PrintPodReport(out, report);
    if (!report.Clean())
    {
        MarkFailed(); // 规则 ②
    }
    Pods.erase(slot->Handle.Index);
    HasActive = false;
}
```

`pod list` / `pod use`（含 `ParseIndex`，它自己解析数字、不交给 cli）：

```cpp
bool Bench::ParseIndex(std::string_view text, std::uint32_t& out)
{
    std::uint32_t value = 0;
    const char* first = text.data();
    const char* last = first + text.size();
    const std::from_chars_result parsed = std::from_chars(first, last, value);
    if (parsed.ec != std::errc{} || parsed.ptr != last)
    {
        return false;
    }
    out = value;
    return true;
}

void Bench::CmdPodList(std::ostream& out, const std::vector<std::string>&)
{
    out << "pods: " << Pods.size() << '\n';
    for (const auto& [index, pod] : Pods)
    {
        vase::Pod* resolved = Host.Resolve(pod.Handle);
        out << "  [" << index << "] gen=" << pod.Handle.Generation << " plan=" << pod.PlanPath.string()
            << " plugins=" << (resolved == nullptr ? 0 : static_cast<int>(resolved->PluginCount()))
            << (index == ActiveIndex && HasActive ? "  <- active" : "") << '\n';
    }
}

void Bench::CmdPodUse(std::ostream& out, const std::vector<std::string>& args)
{
    if (!CheckArity(out, args, 1, "pod use <index>"))
    {
        MarkFailed();
        return;
    }
    std::uint32_t index = 0;
    const auto found = Pods.find(ParseIndex(args[0], index) ? index : UINT32_MAX);
    if (found == Pods.end())
    {
        out << "pod use: no live pod with index " << args[0] << '\n';
        MarkFailed();
        return;
    }
    ActiveIndex = found->first;
    HasActive = true;
    out << "active pod: index=" << ActiveIndex << " generation=" << found->second.Handle.Generation << '\n';
}
```

`Commands()` 与两个访问器：

```cpp
std::vector<shell::CommandSpec> Bench::Commands()
{
    std::vector<shell::CommandSpec> commands;
    const auto add = [&commands](std::string group, std::string name, std::vector<std::string> params,
                                 std::string help, std::function<void(std::ostream&, const std::vector<std::string>&)> handler)
    {
        commands.push_back(shell::CommandSpec{.Group = std::move(group),
                                              .Name = std::move(name),
                                              .Params = std::move(params),
                                              .Help = std::move(help),
                                              .Handler = std::move(handler)});
    };

    add("pod", "new", {"planFile"}, "Create a pod from a plan file",
        [this](std::ostream& out, const std::vector<std::string>& args)
        { CmdPodNew(out, args); });
    add("pod", "use", {"index"}, "Select the target pod",
        [this](std::ostream& out, const std::vector<std::string>& args)
        { CmdPodUse(out, args); });
    add("pod", "list", {}, "List live pods",
        [this](std::ostream& out, const std::vector<std::string>& args)
        { CmdPodList(out, args); });
    add("pod", "destroy", {}, "Destroy the active pod and print its report",
        [this](std::ostream& out, const std::vector<std::string>& args)
        { CmdPodDestroy(out, args); });
    return commands;
}

Bench::LivePod* Bench::Active()
{
    if (!HasActive)
    {
        return nullptr;
    }
    const auto found = Pods.find(ActiveIndex);
    return found == Pods.end() ? nullptr : &found->second;
}

void Bench::MarkFailed() { Failed = true; }
```

> **命令的两段名字（`pod new`）由 cli 的子菜单承载，不由命令名承载**——`detail::split` 先按空白切 token，`ScanCmds` 拿 `cmdLine[0]` 比名字，所以名字里塞空格永远匹配不上。Task 2 的 `RunShell` 已按 `Group` 惰性建子菜单（实测：`pod new a b` → 组 `pod` 内的 `new`，args = `["a","b"]`），本节只是把 `Commands()` 的每条登记写进对应的组。

- [ ] **Step 6: 加两条 ctest —— 一条正向、一条负向**

`Tools/VaseConsole/CMakeLists.txt` 追加（`VASE_CONSOLE_REPLAY_DIR` 沿用 Task 2 的定义）：

```cmake
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/plan.txt"
     CONTENT "# 由 CMake 生成，不要手改：路径要到 generate 期才知道\nVase.Hello $<PATH:CMAKE_PATH,$<TARGET_FILE:HelloPlugin>>\n")

file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/pod_cycle.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/plan.txt
pod list
pod destroy
exit
")
add_test(NAME VaseConsolePodCycle
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/pod_cycle.txt")
# 默认判据（退出码 0）即足够：rc=0 已由规则 ② 蕴含 "clean=true"，
# 故此处**不配** PASS_REGULAR_EXPRESSION 兄弟用例（配了反而会吞掉退出码判定）。

# 规则 ③：脚本没走 exit → 非零。
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/no_exit.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/plan.txt
pod destroy
")
add_test(NAME VaseConsoleNoExitIsFailure
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/no_exit.txt")
set_tests_properties(VaseConsoleNoExitIsFailure PROPERTIES WILL_FAIL TRUE)

# plan 里那条路径不存在 → 建局前就报错 → 非零
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/bad_plan.txt"
     CONTENT "Vase.Nope /definitely/not/here.dll
")
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/bad_plan_script.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/bad_plan.txt
exit
")
add_test(NAME VaseConsoleBadPlan
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/bad_plan_script.txt")
set_tests_properties(VaseConsoleBadPlan PROPERTIES WILL_FAIL TRUE)
# 兄弟用例：钉拒因文本（实测报错是消息契约，漂移要响）。
# ⚠ 它只看正则、**无视退出码**；码由上面 VaseConsoleBadPlan 钉。两者不可合并——
#    ctest 4.4.3 实测：同设 WILL_FAIL + PASS_REGULAR_EXPRESSION 时，命中反而判 Failed。
add_test(NAME VaseConsoleBadPlanReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/bad_plan_script.txt")
set_tests_properties(VaseConsoleBadPlanReason PROPERTIES
    PASS_REGULAR_EXPRESSION "binary not found")

# 规则 ① 后半：敲错的命令名必须算失败（实测修复前 rc=0）
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/unknown_command.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/plan.txt
pod destroy
ejct Vase.Hello
exit
")
add_test(NAME VaseConsoleUnknownCommandIsFailure
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/unknown_command.txt")
set_tests_properties(VaseConsoleUnknownCommandIsFailure PROPERTIES WILL_FAIL TRUE)
add_test(NAME VaseConsoleUnknownCommandReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/unknown_command.txt")
set_tests_properties(VaseConsoleUnknownCommandReason PROPERTIES
    PASS_REGULAR_EXPRESSION "wrong command")
```

- [ ] **Step 7: 跑到红，再实现到绿**

```bash
cmake --preset win-x64-clang-debug && cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R VaseConsole --output-on-failure
```
Expected: 全部 passed（`VaseConsolePodCycle` 只判 rc=0；`*IsFailure` 三条判非零；两条 `*Reason` 判拒因文本命中）。

- [ ] **Step 8: 格式与提交**

```bash
git add Tools/VaseConsole
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' | xargs -0 clang-format --dry-run --Werror
git commit -m "VaseConsole：plan 文件解析、会话状态与退出码五条规则"
```
Expected: clang-format 无输出。若报违规，`clang-format -i` 那些新文件后重跑一遍再看 diff。

---

## Task 4: `eject` / `adopt` / `swap` / `swaploop` —— 进出与三档证据

**Files:**
- Modify: `Tools/VaseConsole/Commands.h`, `Tools/VaseConsole/Commands.cpp`, `Tools/VaseConsole/CMakeLists.txt`
- Test: `ctest --preset win-x64-clang-debug -R VaseConsoleSwapLoop`

**Interfaces:**
- Consumes: `Bench::ResolveActivePod()` / `MarkFailed()` / `CheckArity` / `BoolText`（Task 3）
- Produces: 命令 `eject <id>`、`adopt <id>`、`swap <id>`、`swap <id> <N>`；`PrintEjectReport` / `PrintAdoptReport`

- [ ] **Step 1: 两个报告打印器（字段集合照 spec §3.3 那条"全字段"要求）**

```cpp
void PrintEjectReport(std::ostream& out, const vase::EjectReport& report)
{
    out << "eject " << report.PluginId
        << " binaryUnloaded=" << BoolText(report.BinaryActuallyUnloaded)
        << " mappingRemoved=" << BoolText(report.MappingRemoved)
        << " reopenWritable=" << BoolText(report.ReopenWritable)
        << " mappingRemovalIsObservable=" << BoolText(report.MappingRemovalIsObservable)
        << " reopenWritableIsMeaningful=" << BoolText(report.ReopenWritableIsMeaningful) << '\n';
    if (!report.HotSwapNote.empty())
    {
        out << "  note: " << report.HotSwapNote << '\n';
    }
}

void PrintAdoptReport(std::ostream& out, const vase::AdoptReport& report)
{
    out << "adopt " << report.PluginId
        << " reusedResidentImage=" << BoolText(report.ReusedResidentImage)
        << " identityVerified=" << BoolText(report.IdentityVerified)
        << " importEnforcementPassed=" << BoolText(report.ImportEnforcementPassed)
        << " outgoingEdges=" << report.OutgoingEdges << '\n';
}
```

两个 `Is…` 声明位**必须打**：不打出来，Windows 上那个 `mappingRemoved=false` 就成了没解释的噪声（spec §4）。

- [ ] **Step 2: `ResolveActivePod` 与 `eject` / `adopt`**

先在 `Commands.h` 的 `private:` 段补四行声明：

```cpp
    // 命令处理器的公共前置：没有活动局 / 句柄已失效，就把话说清并返回 nullptr。
    // handleOut 只在返回非空时被写——所有 Eject/Adopt 调用用的都是它，**不从 Pod* 反推句柄**
    // （Pod 没有公开的 Handle() 访问器，也不该有：句柄是宿主的账）。
    [[nodiscard]] vase::Pod* ResolveActivePod(std::ostream& out, vase::PodHandle& handleOut);
    void CmdEject(std::ostream& out, const std::vector<std::string>& args);
    void CmdAdopt(std::ostream& out, const std::vector<std::string>& args);
    const plan::Entry* FindEntry(std::string_view id); // 在 Active()->Entries 里按 Id 找
```

`ResolveActivePod` 的实现。**不缓存 `Pod*`**：句柄存在 `LivePod` 里，每次现解，解不开就报错——`Pod` 没有也不该有公开的 `Handle()` 访问器，所以处理器一律从这里拿句柄，不从 `Pod*` 反推。

```cpp
vase::Pod* Bench::ResolveActivePod(std::ostream& out, vase::PodHandle& handleOut)
{
    LivePod* slot = Active();
    if (slot == nullptr)
    {
        out << "no active pod (use: pod new <planFile>)\n";
        return nullptr;
    }
    vase::Pod* pod = Host.Resolve(slot->Handle);
    if (pod == nullptr)
    {
        out << "active handle is stale\n"; // §5.1：句柄失效是预期内，不是错误——但命令确实没做成
        return nullptr;
    }
    handleOut = slot->Handle;
    return pod;
}

void Bench::CmdEject(std::ostream& out, const std::vector<std::string>& args)
{
    vase::PodHandle handle;
    vase::Pod* pod = ResolveActivePod(out, handle);
    if (pod == nullptr || !CheckArity(out, args, 1, "eject <id>"))
    {
        MarkFailed();
        return;
    }
    auto result = Host.EjectPlugin(handle, std::string_view{args[0]});
    if (!result.IsOk())
    {
        out << "eject refused: " << result.GetError().Message() << '\n';
        MarkFailed();
        return;
    }
    PrintEjectReport(out, result.Value());
}

void Bench::CmdAdopt(std::ostream& out, const std::vector<std::string>& args)
{
    vase::PodHandle handle;
    vase::Pod* pod = ResolveActivePod(out, handle);
    if (pod == nullptr || !CheckArity(out, args, 1, "adopt <id>"))
    {
        MarkFailed();
        return;
    }
    // 先把「这个 Id 登记自哪份 plan」打出来：M1 的 KnownBinaries 是 Host 级、不按局分账
    // （spec §8.1），多局并存时它是用户最容易撞到的东西，报告里必须看得见来源。
    const plan::Entry* entry = FindEntry(args[0]);
    out << "adopt " << args[0] << " registeredBy="
        << (entry == nullptr ? "not-in-active-plan" : Active()->PlanPath.string()) << '\n';

    auto result = Host.AdoptPlugin(handle, std::string_view{args[0]});
    if (!result.IsOk())
    {
        out << "adopt refused: " << result.GetError().Message() << '\n';
        MarkFailed();
        return;
    }
    PrintAdoptReport(out, result.Value());
}
```

`FindEntry` 本任务就要用（`adopt` 打来源），定义一并放这里：

```cpp
const plan::Entry* Bench::FindEntry(std::string_view id)
{
    LivePod* slot = Active();
    if (slot == nullptr)
    {
        return nullptr;
    }
    for (const plan::Entry& entry : slot->Entries)
    {
        if (entry.Id == id)
        {
            return &entry;
        }
    }
    return nullptr;
}
```

- [ ] **Step 3: `swap` 与 `swaploop`（N 自己解析）**

`Commands.h` 的 `private:` 段补一行声明：

```cpp
    void CmdSwap(std::ostream& out, const std::vector<std::string>& args);
```

```cpp
void Bench::CmdSwap(std::ostream& out, const std::vector<std::string>& args)
{
    vase::PodHandle handle;
    if (!CheckArity(out, args, 2, "swap <id> <rounds>") || ResolveActivePod(out, handle) == nullptr)
    {
        MarkFailed();
        return;
    }
    int rounds = 0;
    const std::string& text = args[1];
    const char* first = text.data();
    const std::from_chars_result parsed = std::from_chars(first, first + text.size(), rounds);
    if (parsed.ec != std::errc{} || parsed.ptr != first + text.size() || rounds <= 0)
    {
        out << "swap: <rounds> must be a positive integer, got " << text << '\n';
        MarkFailed();
        return;
    }
    for (int round = 0; round < rounds; ++round)
    {
        out << "--- round " << round << " ---\n";
        auto ejected = Host.EjectPlugin(handle, std::string_view{args[0]});
        if (!ejected.IsOk()) { out << "eject refused: " << ejected.GetError().Message() << '\n'; MarkFailed(); return; }
        PrintEjectReport(out, ejected.Value());
        auto adopted = Host.AdoptPlugin(handle, std::string_view{args[0]});
        if (!adopted.IsOk()) { out << "adopt refused: " << adopted.GetError().Message() << '\n'; MarkFailed(); return; }
        PrintAdoptReport(out, adopted.Value());
    }
}
```

`swap <id>`（不来回换、跑一次）与 `swaploop <id> <N>` 合并成一条 `swap <id> <rounds>`，默认 `rounds=1` 时参数就是 1 个——**为免两条命令两套 arity，本计划把它定成一条：`swap <id> <rounds>`，rounds 必填**（spec §3.3 那两条合并为一条，Task 7 更新 spec 时记这一笔）。

- [ ] **Step 4: 注册进 `Commands()`，加 ctest，跑到红**

```cmake
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/swaploop.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/plan.txt
swap Vase.Hello 3
pod destroy
exit
")
add_test(NAME VaseConsoleSwapLoop
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/swaploop.txt")
# 兄弟用例：`hotswap log (6)` 是 3 轮 eject+adopt 的确切条数，退出码表达不了"报告真的在动"
add_test(NAME VaseConsoleSwapLoopReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/swaploop.txt")
set_tests_properties(VaseConsoleSwapLoopReason PROPERTIES
    PASS_REGULAR_EXPRESSION "hotswap log \\(6\\)")
```
Expected（实现前）：`swap` 是未知命令 → 退出码非 0 → 用例 FAILED。**看到这条红再做 Step 1–3**，做完后它必须绿，且 `VaseConsoleExitOnly` / `VaseConsolePodCycle` 仍绿。

`hotswap log (6)` 是 3 轮 eject+adopt 的确切条数（`PluginHost.cpp` 每次进出一条）——它同时验证了"报告字段真的在动"，不是只看了退出码。

- [ ] **Step 5: 提交**

```bash
git add Tools/VaseConsole
git commit -m "VaseConsole：eject / adopt / swap 与三档证据的全字段报告"
```

---

## Task 5: 换件 —— `HelloPluginPrime` + `file stage` / `install` / `show`

这是 VaseConsole 相对 gtest 的**唯一增量**：人可以在两步之间把磁盘上的文件换掉。

**Files:**
- Create: `Samples/HelloPluginPrime/HelloPlugin.cpp`, `Samples/HelloPluginPrime/CMakeLists.txt`
- Modify: `CMakeLists.txt`（根）、`Tools/VaseConsole/{Commands.h,Commands.cpp,CMakeLists.txt}`
- Test: `ctest --preset win-x64-clang-debug -R VaseConsoleHotSwap`

**Interfaces:**
- Consumes: `Bench::Active()`（拿登记路径）、`plan::Entry`、`detail::Loader::FileIdentity`
- Produces: 命令 `file stage <id> <srcPath>`、`file install <id>`、`file show <id>`；`Bench::Staged`

- [ ] **Step 1: 换件材料 `HelloPluginPrime`**

`Samples/HelloPluginPrime/HelloPlugin.cpp` —— 与 `HelloPlugin.cpp` 同 Id、`Provides` 不变，**只有返回串不同**（串里没有正则元字符，断言好写）：

```cpp
// HelloPluginPrime.cpp —— 与 HelloPlugin 同一个 Id、同一个服务标识，只有行为不同：
// 它就是 spec §12.1 里那个「A′」。file install 把它覆盖到 Vase.Hello 的登记路径上，
// adopt 之后 get 必须打出不同的串——这就是「行为真的换了」的判据。
#include "Greeter.h"
#include "Vase/Plugin.h"

#include <string_view>

namespace
{

class GreeterImpl final : public samples::IGreeter
{
public:
    [[nodiscard]] std::string_view Greet() const override { return "hello from Vase.Hello prime v2"; }
};

class HelloPluginPrime final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<samples::IGreeter>(Greeter);
        ctx.On<samples::GreetEvent>(&HelloPluginPrime::OnGreet, this);
        return vase::Result<void>::Ok();
    }

    vase::Result<void> OnStart(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Ok();
    }

private:
    void OnGreet(const samples::GreetEvent& event)
    {
        static_cast<void>(event);
        ++GreetCount;
    }

    GreeterImpl Greeter;
    int GreetCount = 0;
};

} // namespace

VASE_PLUGIN(HelloPluginPrime){
    .Id = "Vase.Hello",
    .DisplayName = "示例插件（prime 版）",
    .Version = "0.1.0",
    .Requires = {},
    .Provides = {{.Name = "Vase.Hello.Greeter", .Version = 1}},
};
```

`Samples/HelloPluginPrime/CMakeLists.txt`：

```cmake
vase_add_plugin_fixture(HelloPluginPrime
    SOURCES HelloPlugin.cpp
    LINK_LIBRARIES VasePod)
target_include_directories(HelloPluginPrime PRIVATE "${PROJECT_SOURCE_DIR}/Samples/HelloCommon")
```

根 `CMakeLists.txt` 里紧跟 `add_subdirectory(Samples/HelloPlugin)` 之后：

```cmake
add_subdirectory(Samples/HelloPluginPrime)
```

- [ ] **Step 2: `Bench` 补上暂存字段**

`FindEntry` 的声明与实现已按 Task 4 Step 2 的要求提前，这里不再重复。`Commands.h` 的 `private:` 段补四行声明与一样字段：

```cpp
    void CmdFileStage(std::ostream& out, const std::vector<std::string>& args);
    void CmdFileInstall(std::ostream& out, const std::vector<std::string>& args);
    void CmdFileShow(std::ostream& out, const std::vector<std::string>& args);
    void CmdGet(std::ostream& out, const std::vector<std::string>& args);

    // 「为哪个 Id 暂了哪些字节」，绑成一个值以免两者错配。
    std::optional<std::pair<std::string, std::vector<std::uint8_t>>> Staged;
```

同时 `Commands.cpp` 的 include 段补：`#include "Vase/Host/Loader.h"`（`detail::Loader::FileIdentity`）、`#include <chrono>`、`#include <fstream>`、`#include <iomanip>`、`#include <memory>`、`#include <system_error>`。

- [ ] **Step 3: 实现三个 `file` 命令**

```cpp
void Bench::CmdFileStage(std::ostream& out, const std::vector<std::string>& args)
{
    if (!CheckArity(out, args, 2, "file stage <id> <srcPath>") || Active() == nullptr)
    {
        MarkFailed();
        return;
    }
    std::ifstream source{args[1], std::ios::binary | std::ios::ate};
    if (!source.is_open())
    {
        out << "file stage: cannot open source: " << args[1] << '\n';
        MarkFailed();
        return;
    }
    const std::streampos size = source.tellg();
    source.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    source.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!source)
    {
        out << "file stage: short read from " << args[1] << '\n';
        MarkFailed();
        return;
    }
    // 「为哪个 Id 暂的存」与「暂了哪些字节」绑成一个值，两者不可能错配。
    Staged = std::make_pair(args[0], std::move(bytes));
    out << "staged " << Staged->second.size() << " bytes for " << Staged->first << " (not written yet)\n";
}
```

`file install` 的关键是**把覆盖本身当结论报**，并自带平台声明（spec §4）：

```cpp
void Bench::CmdFileInstall(std::ostream& out, const std::vector<std::string>& args)
{
    if (!CheckArity(out, args, 1, "file install <id>") || Active() == nullptr)
    {
        MarkFailed();
        return;
    }
    if (!Staged || Staged->first != args[0])
    {
        out << "file install: nothing staged for " << args[0] << '\n';
        MarkFailed();
        return;
    }
    const plan::Entry* entry = FindEntry(args[0]);
    if (entry == nullptr)
    {
        out << "file install: " << args[0] << " is not in the active pod's plan\n";
        MarkFailed();
        return;
    }

    std::error_code ec;
    {
        std::ofstream target{entry->BinaryPath, std::ios::binary | std::ios::trunc};
        if (!target.is_open())
        {
            // Windows 上这一支就是 T12 那个 sharing-violation 探针：镜像还映射着就写不开。
            out << "install " << args[0] << " written=false path=" << entry->BinaryPath.string() << '\n';
            out << "  判据力：Windows → 写不开即「Eject 没真卸」，本条有判据力\n";
            MarkFailed();
            return;
        }
        target.write(reinterpret_cast<const char*>(Staged->second.data()),
                     static_cast<std::streamsize>(Staged->second.size()));
        if (!target)
        {
            out << "install " << args[0] << " written=false (short write)\n";
            MarkFailed();
            return;
        }
    }
    out << "install " << args[0] << " written=true path=" << entry->BinaryPath.string() << '\n';
#if defined(_WIN32)
    out << "  判据力：Windows → 覆盖写不开即「Eject 没真卸」（sharing violation）。本条**有**判据力\n";
#else
    out << "  判据力：Linux → 覆盖是 truncate-in-place（同 inode），映射着也写得开。本条**为空转**\n";
#endif
}
```

上面那两行 `判据力：` 不是装饰，是这条命令存在的全部理由：同一个动作为什么在两平台证明的不是同一件事，读输出的人必须不看文档就知道（spec §4）。

`file show` 用 `Loader::FileIdentity` 打磁盘侧指纹：

```cpp
void Bench::CmdFileShow(std::ostream& out, const std::vector<std::string>& args)
{
    if (!CheckArity(out, args, 1, "file show <id>"))
    {
        MarkFailed();
        return;
    }
    const plan::Entry* entry = FindEntry(args[0]);
    if (entry == nullptr)
    {
        out << "file show: " << args[0] << " is not in the active pod's plan\n";
        MarkFailed();
        return;
    }
    std::error_code ec;
    const auto size = std::filesystem::file_size(entry->BinaryPath, ec);
    const auto stamp = std::filesystem::last_write_time(entry->BinaryPath, ec);
    out << "show " << args[0] << " path=" << entry->BinaryPath.string() << " size=" << size << " mtimeNs="
        << std::chrono::duration_cast<std::chrono::nanoseconds>(stamp.time_since_epoch()).count() << '\n';
    auto identity = vase::detail::Loader::FileIdentity(entry->BinaryPath);
    if (!identity.IsOk())
    {
        out << "  identity: unavailable — " << identity.GetError().Message() << '\n';
        return; // 指纹缺失是诊断信息，不是命令失败
    }
    const vase::detail::ImageIdentity& value = identity.Value();
    out << "  identity kind=" << (value.Kind == vase::detail::IdentityKind::kPdbCodeView ? "pdbCodeView"
                                                                                          : "elfBuildId")
        << " bytes=";
    for (const std::uint8_t byte : value.Bytes)
    {
        out << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(byte);
    }
    out << std::dec << '\n';
}
```

`FindEntry` 与 `ResolveActivePod` 都已在 Task 4 Step 2 定义，本任务只用。

- [ ] **Step 4: 加完整回放链，跑到红**

```cmake
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/hotswap.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/plan.txt
get
eject Vase.Hello
file stage Vase.Hello $<PATH:CMAKE_PATH,$<TARGET_FILE:HelloPluginPrime>>
file install Vase.Hello
adopt Vase.Hello
get
pod destroy
exit
")
add_test(NAME VaseConsoleHotSwap
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/hotswap.txt")
# 兄弟用例：换件是否**真的**发生，退出码看不见（CLI 不比较串）——只能钉文本。
# ⚠ 此条无视退出码，码由上面的 VaseConsoleHotSwap 钉（spec §2.19）。
add_test(NAME VaseConsoleHotSwapReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/hotswap.txt")
set_tests_properties(VaseConsoleHotSwapReason PROPERTIES
    PASS_REGULAR_EXPRESSION "prime v2")
```
Expected（实现前）：`get` 与 `file` 三条都是未知命令 → 非零 → FAILED。做完 Step 1–2 与 Step 4 后它必须绿。

**这条用例的判据力按平台不对称**（`file install` 在 Linux 上空转），Task 7 要把它记进 `CLAUDE.md` 规矩 6 那一节。

- [ ] **Step 5: 实现 `get`（一条命令就能把"行为换了"断出来，故提前到这里）**

`get` 用 `TryGet`（Global Constraints 里那条），并把"没有提供方"当成**证据**而不是崩溃：

```cpp
void Bench::CmdGet(std::ostream& out, const std::vector<std::string>&)
{
    vase::PodHandle handle;
    vase::Pod* pod = ResolveActivePod(out, handle);
    if (pod == nullptr)
    {
        MarkFailed();
        return;
    }
    // TryGet 而不是 Get：Get 走 required=true，缺失即 ProgrammerError 终止。
    // eject 之后正好要用"拿不到"来表达注册已被撤掉。
    samples::IGreeter* greeter = pod->Root().TryGet<samples::IGreeter>();
    if (greeter == nullptr)
    {
        out << "get: no provider for Vase.Hello.Greeter\n";
        return; // 这是一次有效观察，不是失败
    }
    out << "get: " << greeter->Greet() << '\n';
}
```

`Commands.cpp` 需要 `#include "Greeter.h"`，而 `Greeter.h` 住在 `Samples/HelloCommon`——`Tools/VaseConsole/CMakeLists.txt` 加：

```cmake
target_include_directories(VaseConsole PRIVATE "${PROJECT_SOURCE_DIR}/Samples/HelloCommon")
```

- [ ] **Step 6: 全跑一遍并提交**

```bash
cmake --preset win-x64-clang-debug && cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug -R VaseConsole --output-on-failure
```
Expected: 6 条全 passed，其中 `VaseConsoleHotSwap` 匹配到 `prime v2`。

```bash
git add Tools/VaseConsole Samples/HelloPluginPrime CMakeLists.txt
git commit -m "VaseConsole：换件路径 file stage/install/show 与 HelloPluginPrime；get 用 TryGet 表达无提供方"
```

---

## Task 6: 剩余命令面 —— `emit` / `plugins` / `pod` 组补齐

**Files:**
- Modify: `Tools/VaseConsole/{Commands.h,Commands.cpp,CMakeLists.txt}`
- Test: `ctest --preset win-x64-clang-debug -R "VaseConsoleBehaviour|VaseConsoleRefusedIsFailure"`

**Interfaces:**
- Consumes: Task 3–5 的全部
- Produces: `emit <n>`、`plugins`；命令表完整

- [ ] **Step 1: 实现两条**

`Commands.h` 的 `private:` 段补两行声明（`CmdGet` 已在 Task 5 声明过）：

```cpp
    void CmdEmit(std::ostream& out, const std::vector<std::string>& args);
    void CmdPlugins(std::ostream& out, const std::vector<std::string>& args);
```

```cpp
void Bench::CmdEmit(std::ostream& out, const std::vector<std::string>& args)
{
    vase::PodHandle handle;
    vase::Pod* pod = ResolveActivePod(out, handle);
    if (pod == nullptr || !CheckArity(out, args, 1, "emit <n>"))
    {
        MarkFailed();
        return;
    }
    int seq = 0;
    const std::string& text = args[0];
    const char* first = text.data();
    const std::from_chars_result parsed = std::from_chars(first, first + text.size(), seq);
    if (parsed.ec != std::errc{} || parsed.ptr != first + text.size())
    {
        out << "emit: <n> must be an integer, got " << text << '\n';
        MarkFailed();
        return;
    }
    pod->Root().Emit(samples::GreetEvent{seq});
    out << "emitted GreetEvent seq=" << seq << '\n';
}

void Bench::CmdPlugins(std::ostream& out, const std::vector<std::string>&)
{
    vase::PodHandle handle;
    vase::Pod* pod = ResolveActivePod(out, handle);
    if (pod == nullptr)
    {
        MarkFailed();
        return;
    }
    out << "plugins live=" << pod->PluginCount() << '\n';
    for (const std::string& id : pod->PluginIds())
    {
        out << "  " << id << '\n';
    }
    for (const vase::FailedPluginRecord& failure : pod->Failures())
    {
        out << "  failed: " << failure.Id << " [" << PhaseText(failure.Stage) << "] " << failure.Message << '\n';
        MarkFailed();
    }
}
```

- [ ] **Step 2: 加两条 ctest（一条正向断行为、一条负向断拒绝）**

```cmake
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/behaviour.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/plan.txt
emit 1
get
plugins
eject Vase.Hello
get
emit 2
adopt Vase.Hello
get
pod destroy
exit
")
add_test(NAME VaseConsoleBehaviour
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/behaviour.txt")
# 兄弟用例：`no provider` 是一次**有效观察**（eject 后拿不到服务），不是失败，
# 所以退出码不会为它而变 —— 只能靠文本钉。⚠ 此条无视退出码（spec §2.19）。
add_test(NAME VaseConsoleBehaviourReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/behaviour.txt")
set_tests_properties(VaseConsoleBehaviourReason PROPERTIES
    PASS_REGULAR_EXPRESSION "no provider")

# eject 一个不存在于任何 plan 的 id → 响亮拒绝 → 非零
file(GENERATE OUTPUT "${VASE_CONSOLE_REPLAY_DIR}/unknown_id.txt"
     CONTENT "pod new ${VASE_CONSOLE_REPLAY_DIR}/plan.txt
eject Vase.Nowhere
exit
")
add_test(NAME VaseConsoleRefusedIsFailure
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/unknown_id.txt")
set_tests_properties(VaseConsoleRefusedIsFailure PROPERTIES WILL_FAIL TRUE)
# ⚠ 原计划此处同设 WILL_FAIL + PASS_REGULAR_EXPRESSION —— 实测该组合**物理跑不绿**
#    （命中正则反而判 Failed，spec §2.19）。拆成兄弟用例：
add_test(NAME VaseConsoleRefusedReason
         COMMAND VaseConsole --script "${VASE_CONSOLE_REPLAY_DIR}/unknown_id.txt")
set_tests_properties(VaseConsoleRefusedReason PROPERTIES
    PASS_REGULAR_EXPRESSION "eject refused")
```

- [ ] **Step 3: 全绿后提交**

```bash
ctest --preset win-x64-clang-debug -R VaseConsole --output-on-failure
git add Tools/VaseConsole
git commit -m "VaseConsole：emit 与 plugins 补齐命令面，并加行为观察与拒绝两条用例"
```

---

## Task 7: 门禁全跑与文档基数收口

前面每个任务只跑了 Windows 的一条线。**这一步把六条线、三条 tidy、format 一次跑完，并把实测数字写进唯一该写它的地方。**

**Files:**
- Modify: `CLAUDE.md`（两张基数表、Samples 行、规矩 6 那一节）
- Modify: `docs/superpowers/specs/2026-09-21-vase-console-design.md`（§7.2 生成方式已定、§3.3 命令表与实际实现的差异、§10/§11）

**Interfaces:** 无新代码。

- [ ] **Step 1: Linux 两条线（必须经登录 shell，且 `$` 别写进双引号）**

```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --preset linux-x64-clang-debug'
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug'
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && ctest --preset linux-x64-clang-debug'
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && ctest --preset linux-x64-clang-debug -N | grep "Total Tests"'
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && ctest --preset linux-x64-clang-debug -R VaseConsole --output-on-failure'
```
Expected: 全 passed；`Total Tests` = 原 Linux debug 基数 + 本次新增条数。
**特别看 `VaseConsoleHotSwap`**：Linux 上 `file install` 的覆盖为空转（规矩 6），它仍应绿——绿的原因是 `adopt` 的档三身份比对，不是覆盖本身。若它在 Linux 上红而 Windows 上绿，先查 `get` 打出的串是不是 prime v2，别去放宽断言。

- [ ] **Step 2: 剩下三条 Windows 线，用现成的脚本做"删树重配"全量**

```bash
cmd //c "Scripts\\win-verify.cmd"
```
Expected: 四棵树各自 configure → build → ctest → `ctest -N`，末尾汇总以失败步数为退出码，退出 0。脚本不复制基数，所以基数仍要拿 Step 3 的表去逐位对。

- [ ] **Step 3: 读三条 debug 线的 tidy（**退出码单独不够**）**

```bash
cmd //c "Scripts\\win-clang-tidy.cmd" clangcl
cmd //c "Scripts\\win-clang-tidy.cmd" msvc
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && bash Scripts/linux-clang-tidy.sh'
```
Expected: 三条各自"退出 0 + 正文 `error:` 0 条 + 正文 `warning:` 0 条"。记下每条的 **文件数 / Suppressed 合计 / NOLINT 命中数**——它们要进 Step 5 的表。若新 TU 带出正文告警，**优先改写法**；确无出路的就地 `NOLINT` 并写明"这个检查在这一行为什么没有代码级写法"（现行口径不是数量门槛）。
`ExcludeHeaderFilterRegex` 是否如期生效的判据：正文里不应出现任何 `ThirdParty/` 路径的 diagnostic。

- [ ] **Step 4: format 门禁**

```bash
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' \
  | xargs -0 clang-format --dry-run --Werror
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | wc -l
```
Expected: rc=0。文件数比上一轮多出的应当正好是 VaseConsole 的 7 个 + HelloPluginPrime 的 1 个（`ThirdParty/` 不被枚举，已实测）。**先确认所有新文件都已 `git add`**——未暂存的新文件会被 `git ls-files` 静默跳过，门禁照样绿（本仓库实测过）。

- [ ] **Step 5: 把实测数字写进 `CLAUDE.md` 的两张表（真值只此一处）**

- 「构建与测试」的按线基数表：`win-x64-{clang,msvc}-debug` / `-release` / `linux-x64-clang-debug` / `-release` 各改成实测值，并核"与 Win debug 的差"那一列的成因描述是否仍成立（本次新增的用例**不含** `#ifndef NDEBUG` 门与平台门，所以 debug/release 与 Win/Linux 的既有差值不变）。
- 「核这些门禁时，退出码单独用是不够的」里的 tidy 表：三条线的文件数与 `Suppressed` 合计、NOLINT 命中数换成实测值。
- 「项目状态」的示例行 `Samples/{HelloCommon,HelloPlugin,HelloPluginPrime,Embedding}` 与工具行 `Tools/VaseConsole`，并给 VaseConsole 一句 `VaseConsole [--script <file>]`。
- **规矩 6 那一节补第二实例**：`VaseConsoleHotSwap` 的 `file install` 判据力按平台不对称（与 T12 同源），两平台同跑得到的不是同一件事的两份拷贝。
- **架构文档 §10 的目录布局补 `ThirdParty/` 一行**。注意 §10 是文档里少数标着"已确认"的部分（`CLAUDE.md` 就是这么陈述的），给它加一个顶层目录属于**扩写一份已确认的约定**，不是记一件新事实——所以这一步要需求方过目再落笔，不要顺手写完就当无事发生。

- [ ] **Step 6: 回写 spec 的三处实现期变更**

- §7.2：把"待实测（`file(GENERATE)` vs `add_custom_command`）"改成实测结论 —— 用的是 `file(GENERATE)` + `$<PATH:CMAKE_PATH,$<TARGET_FILE:…>>`，`.in` 模板不存在。
- §3.3：命令名与 arity 按实际实现校正（`pod` 是子菜单还是扁平名；`swap <id> <rounds>` 两条合并成一条；`get` 用 `TryGet`）。
- §11：第 6、7 桩标为已打掉，附上 Step 1–4 的实测数字指向 `CLAUDE.md`（**不要把数字抄进 spec**）。
- **顺带定掉 spec §10 表里那条悬着的问题**：`.claude/skills/vase-cpp-engineering/` 要不要记"我们 Own 一份 fork、它的 core 路径必须无异常、包含它必须用引号"这一句。按规矩 7 的口径判：这三条里**基数不写**（只住 CLAUDE.md）；"引号包含"属"嵌在论述里的单个字面值"，可以写但必须带指回 `CLAUDE.md` 规矩 3 / `Cmake/VaseThirdParty.cmake` 的链路；"我们 Own 一份 fork"是架构事实，`references/architecture.md` 写一份判据（为什么 fork 而不是自写/端口）即可，不要把实现细节抄第二遍。**给出你的判断与理由，不要默默略过。**

- [ ] **Step 7: 提交**

```bash
git add CLAUDE.md docs/superpowers/specs/2026-09-21-vase-console-design.md
git commit -m "VaseConsole 门禁全跑：六 preset、三条 tidy 与基数表回填"
```

- [ ] **Step 8: 最终自查（技能·提交前清单）**

逐项确认并在汇报里明说任何**跳过**的步骤与原因：

- [ ] 六 preset 全绿，`ctest -N` 逐位对上新的基数表，差值成因说得出
- [ ] 三条 tidy 线各自跑、各自读正文（0 error / 0 warning），Linux 线单独跑
- [ ] format 全绿且新文件都已 `git add`
- [ ] 新 target 都链了 `VaseBuildOptions`；`HelloPluginPrime` 经 `vase_add_plugin_fixture`
- [ ] 全仓 `grep -n "throw\|try {\|catch ("  Tools/VaseConsole` 零命中
- [ ] VaseConsole 里没有 `.at()`、没有不带 `error_code` 的 `<filesystem>` 调用、没有 `std::sto*`
- [ ] cli 头是**引号**包含的（`grep -rn "include <cli/" Tools/VaseConsole` 应为空）
- [ ] 宿主侧没有缓存 `Pod*`/服务指针；`get` 走 `TryGet`
- [ ] Vase 自己的头一律引号（规矩 3）
- [ ] 提交信息无任何 AI 署名尾注
