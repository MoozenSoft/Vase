# Vase macOS x64 平台腿 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 Vase 的第三条平台腿（macOS x64 / clang + libc++）接进构建与验证体系，补齐档二（`_dyld_*` 映射清单）与档三（`LC_UUID` 身份特征），并把「六线全绿」的口径扩成「八线全绿」。

**Architecture:** 平台代码沿既有的三分法铺开——**纯字节解析器跨平台编译**（新增 `ImageInspectMachO.cpp`，与 PE/ELF 并列，八条线上都有合成字节证人），**平台 API 实现按 CMake 三分支选 TU**（新增 `ImageInspectDarwin.cpp` 对位 `ImageInspectLinux.cpp` / `ImageInspectWindows.cpp`）。格式选择（PE/ELF/Mach-O）收进一个 `ImageFormat` 枚举与一处 `PlatformImageFormat()`，使 `Loader.cpp` 的平台分支从三处降为一处。

**Tech Stack:** C++20（全项目 `-fno-exceptions`，错误走 `Result<T>`/`Error`）· CMake 4.4 + Ninja + vcpkg manifest · MacPorts clang 23.1.2（`clang++-mp-23`）· GoogleTest 1.18.0

**Spec:** `docs/superpowers/specs/2026-10-01-vase-macos-x64-platform-leg-design.md`（D156–D179；本计划每一步的**理由**都在那里，本文只写**做什么、怎么验**）

## Global Constraints

以下每条都是全项目要求，每个 task 的要求都隐含包含它们（字面值取自 spec 与 `CLAUDE.md`）：

- **C++20**；全项目关闭异常，**不写 `throw` / `try` / `catch`**，错误一律 `Result<T>` / `Error` 显式返回。
- **提交信息只留中文正文**，不加 `Co-Authored-By:` / `Generated-with:` / `Signed-off-by:` 一类尾注。
- 新 target 必须链 `VaseBuildOptions`；插件 target 一律经 `vase_add_plugin_fixture`（规矩 1 / 5）。
- Vase 自己的头**一律引号包含**：`#include "Vase/…"`，不用尖括号（规矩 3）。
- 测试**不用** `EXPECT_THROW` 一族（编译期硬失败）；要测「必须失败」只能读 `Result<T>`（规矩 2）。
- 格式：Allman、`PointerAlignment: Left`、`PackConstructorInitializers: Never` + `BreakConstructorInitializers: BeforeComma`、`BreakTemplateDeclarations: Yes`。
- 命名：命名空间 `lower_case`、类型/函数/成员 `CamelCase`、参数与局部 `camelBack`、常量与枚举值 `k` + `CamelCase`。
- 注释判据：**删掉它读者会不会踩坑**。单条 ≤2 行为宜、硬上限 3 行；论证进计划/spec，代码只留结论 + 指针。
- 平台条件用 `#if defined(_WIN32) / #elif defined(__APPLE__) / #else`；**能用 `#else` 共用的就共用**并把文案改成「POSIX」，确需不同的才三分支。
- **macOS 各线经 `ssh -o BatchMode=yes moozen-macos` 从本开发机跑**；Mac 上的同名路径 `~/WindowsGit/Vase` 与 `D:\Git\Vase` 是同一份工作树，无需同步。
  - **`-o BatchMode=yes` 是承重的，不是可选的整洁**：无 TTY 时它让 ssh **失败**，不加就**挂住**——挂住没有报错、没有超时提示，只是不动，是最难查的一类失效。人在终端里手跑可以省掉它，脚本与代理化调用必须带。
  - 主机别名写在本计划里是**对的**（现场记录 + 本计划要被直接执行）；`CLAUDE.md` / `README` 里**不写**（别人机器上没有这个别名），见 spec 的 D179。
- LLVM 口径：Win/Linux 是 **23.1.0**，macOS 是 **23.1.2**，统一写作「同为 **23.1.x**」。

---

## 文件结构（本波创建/修改的全集）

| 文件 | 职责 | 动作 |
|---|---|---|
| `Source/Host/Detail/ImageBytes.h` | 三个解析器共用的读字节助手（`InBounds` / `ReadU8..U64` / `ReadNulTerminated`） | **新建** |
| `Source/Host/ImageInspectMachO.cpp` | Mach-O 纯字节解析（`LC_UUID` 身份 / `LC_LOAD_DYLIB` 依赖名）——**无条件编译** | **新建** |
| `Source/Host/ImageInspectDarwin.cpp` | Darwin 平台实现（`_dyld_*` 枚举 / 路径规范化 / 身份） | **新建** |
| `Source/Host/ImageInspectLinux.cpp` | 由 `ImageInspectPosix.cpp` **改名**（内容是 ELF 专属，名字今天在撒谎） | 改名 |
| `Source/Host/ImageInspectCommon.cpp` | 收纳 `PlatformImageFormat()` 与 `ParseImportedLibraryNames()`；PE/ELF 段改用共用助手 | 修改 |
| `Source/Host/ImageInspectPlatform.h` | Host 内部桥头：补 `PlatformImageFormat()` / `ParseImportedLibraryNames()` 声明与 macOS 注释 | 修改 |
| `Source/Host/Loader.cpp` | 两处 `#ifdef` 消失（改走 `PlatformImageFormat()`）；Unload 证据那段**不动** | 修改 |
| `Source/Host/PluginHost.cpp` | §8.7 执法按 basename 归一（D176） | 修改 |
| `Source/Host/CMakeLists.txt` | 三分支选平台 TU；Mach-O 解析器**无条件**加入 | 修改 |
| `Include/Vase/Detail/ImageInspect.h` | `IdentityKind` 加值去默认、`ImageFormat`、两个 Mach-O 纯函数 | 修改 |
| `Include/Vase/Catalog/LibraryFileName.h` | `.dylib` 分支（还 D138） | 修改 |
| `Cmake/Toolchains/macos-x64-clang-libcxx.cmake` | 平台工具链 | **新建** |
| `Cmake/Triplets/x64-osx-libcxx.cmake` | vcpkg overlay triplet | **新建** |
| `CMakePresets.json` | 两个 macOS preset + build/test preset | 修改 |
| `Scripts/macos-verify.sh` | 八线里的 macOS 两线，删树重配全量 | **新建** |
| `Scripts/macos-clang-tidy.sh` | macOS tidy 线 | **新建** |
| `Tools/VaseConsole/Console.cpp` | 身份种类的三目 → 穷举 `switch`；两处判据力文案改「POSIX」 | 修改 |
| `Tools/Integration/…` 等测试 | 见 Task 8 | 修改 |
| `Tests/CMakeLists.txt` | `NoIdentityPlugin` 三平台化 | 修改 |
| `CLAUDE.md` / `README.md` / `wiki/` / `.claude/skills/…` | 见 Task 11 | 修改 |

**任务序的承重约束**：T1–T6 全部可在**今天已有的六条线上**验证（这是 D173 的用意——Mach-O 解析器不靠 macOS 就能测）。T7 起才需要 macOS。

---

## Task 1: 提取共用字节助手 `Detail/ImageBytes.h`

纯搬移、零语义变化。三个解析器（PE / ELF / Mach-O）共用同一套边界检查。

**Files:**
- Create: `Source/Host/Detail/ImageBytes.h`
- Modify: `Source/Host/ImageInspectCommon.cpp:20-78`（删掉匿名 namespace 里的助手，改用新头）

**Interfaces:**
- Consumes: 无
- Produces: `vase::detail::InBounds`、`ReadU8`、`ReadU16`、`ReadU32`、`ReadU64`、`ReadNulTerminated(span, at)` 与 `ReadNulTerminated(span, at, end)` —— 全部 `inline`、`[[nodiscard]]`

- [ ] **Step 1: 写新头**

```cpp
#pragma once

// PE / ELF / Mach-O 三个镜像解析器共用的读字节助手（macOS 腿提取：第三个解析器进来时
// 把那 ~90 行边界检查收成一处，PE/ELF 一并改用；纯搬移，无语义变化）。
//
// 纪律（三个解析器共同遵守）：每个取字节前先 InBounds，查不过就返回 Err，绝不越界读；
// 整数一律 memcpy 进本地变量——非对齐安全（三种格式的字段本来都不保证对齐）。
// 目标矩阵（x64 / arm64）全是小端，故「按本机序读」就是 LE 读法，不需要逐字节拼。
//
// inline 而非另立 .cpp：每个函数 3–8 行，独立 TU 只会让三个解析器跨 TU 调用、挡住内联。

#include "Vase/Detail/Result.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <utility>

namespace vase::detail
{

[[nodiscard]] inline bool InBounds(std::size_t offset, std::size_t length, std::size_t total)
{
    return offset <= total && length <= total - offset;
}

[[nodiscard]] inline std::uint8_t ReadU8(std::span<const std::uint8_t> bytes, std::size_t at)
{
    std::uint8_t value = 0;
    std::memcpy(&value, bytes.subspan(at, sizeof(value)).data(), sizeof(value));
    return value;
}

[[nodiscard]] inline std::uint16_t ReadU16(std::span<const std::uint8_t> bytes, std::size_t at)
{
    std::uint16_t value = 0;
    std::memcpy(&value, bytes.subspan(at, sizeof(value)).data(), sizeof(value));
    return value;
}

[[nodiscard]] inline std::uint32_t ReadU32(std::span<const std::uint8_t> bytes, std::size_t at)
{
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.subspan(at, sizeof(value)).data(), sizeof(value));
    return value;
}

[[nodiscard]] inline std::uint64_t ReadU64(std::span<const std::uint8_t> bytes, std::size_t at)
{
    std::uint64_t value = 0;
    std::memcpy(&value, bytes.subspan(at, sizeof(value)).data(), sizeof(value));
    return value;
}

// 读一条 NUL 结尾的字符串，扫到 end 仍没有 NUL 就是 Err（不越界、不猜）。
// end 由调用方给：PE/ELF 传镜像末尾，Mach-O 传该 load command 的末尾（名字只许落在条目内）。
[[nodiscard]] inline Result<std::string> ReadNulTerminated(std::span<const std::uint8_t> bytes, std::size_t at,
                                                           std::size_t end)
{
    std::string text;
    for (std::size_t index = at; index < end && index < bytes.size(); ++index)
    {
        const std::uint8_t value = ReadU8(bytes, index);
        if (value == 0U)
        {
            return Result<std::string>::Ok(std::move(text));
        }
        text.push_back(static_cast<char>(value));
    }
    return Result<std::string>::Err(Error{"unterminated string in image"});
}

[[nodiscard]] inline Result<std::string> ReadNulTerminated(std::span<const std::uint8_t> bytes, std::size_t at)
{
    return ReadNulTerminated(bytes, at, bytes.size());
}

} // namespace vase::detail
```

- [ ] **Step 2: 删掉 `ImageInspectCommon.cpp` 里的旧助手，加上 include**

在 `#include "ImageInspectPlatform.h"` 之后加 `#include "Detail/ImageBytes.h"`；
删掉 `:20` 起的匿名 namespace 内 `InBounds` / `ReadU8` / `ReadU16` / `ReadU32` / `ReadU64` / `ReadNulTerminated` 六段定义（`:31-78`）。
**保留** `MissingCodeViewError` / `MissingBuildIdError`（它们不是通用助手）与 ELF 段独有的 `AlignUp4`。

> 注意 `AlignUp4` 在 ELF 段使用，仍留在匿名 namespace 内——Mach-O 不用它，别顺手搬进新头。

- [ ] **Step 3: 六线回归（增量即可，本步是纯重构）**

Run（Windows）：
```bash
cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug
```
Run（Linux，经登录 shell）：
```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug'
```
Expected: 两条线全绿、用例数与改动前逐位相同（331 / 331）。

- [ ] **Step 4: Commit**

```bash
git add Source/Host/Detail/ImageBytes.h Source/Host/ImageInspectCommon.cpp
git commit -m "macOS 腿 T1：把 PE/ELF 的读字节助手提成 Detail/ImageBytes.h（纯搬移）"
```

---

## Task 2: `IdentityKind` 加 `kMachOUuid`、去默认值、`Console` 三目改穷举 switch

**Files:**
- Modify: `Include/Vase/Detail/ImageInspect.h:20-32`
- Modify: `Tools/VaseConsole/Console.cpp`（三目所在处；新增 `IdentityKindText` 静态助手）

**Interfaces:**
- Consumes: 无
- Produces: `vase::detail::IdentityKind::kMachOUuid`（Task 4 的解析器要赋它）；`ImageIdentity` **不再可默认构造**

- [ ] **Step 1: 改 `ImageInspect.h`**

```cpp
enum class IdentityKind : std::uint8_t
{
    kElfBuildId,  // Linux：.note.gnu.build-id 的 desc 字节（sha1 形态 20B）
    kPdbCodeView, // Windows：RSDS 的 GUID(16B) + Age(4B)
    kMachOUuid,   // macOS：LC_UUID 的 16 字节
};

struct ImageIdentity
{
    // 无默认值：Kind 必须由生产者显式给。留个 kElfBuildId 当默认，等于给「新解析路径
    // 忘了赋 Kind」埋一个静默错——贴错标签后档三的 Kind+Bytes 全等比对恒不等，
    // 表现为「Adopt 总是拒绝」，看不出根因（D175）。
    IdentityKind Kind;
    std::vector<std::uint8_t> Bytes;

    bool operator==(const ImageIdentity&) const = default;
};
```

- [ ] **Step 2: 写 `Console.cpp` 的穷举助手（放在 `SkipReasonText` 旁边，`Console.cpp:197` 一带）**

```cpp
const char* IdentityKindText(vase::detail::IdentityKind kind)
{
    switch (kind)
    {
    case vase::detail::IdentityKind::kPdbCodeView:
        return "pdbCodeView";
    case vase::detail::IdentityKind::kElfBuildId:
        return "elfBuildId";
    case vase::detail::IdentityKind::kMachOUuid:
        return "machOUuid";
    }
    return "?";
}
```

- [ ] **Step 3: 换掉 `Console.cpp:988-989` 的三目**

原：
```cpp
    out << "  identity kind=" << (value.Kind == vase::detail::IdentityKind::kPdbCodeView ? "pdbCodeView" : "elfBuildId")
        << " bytes=";
```
改：
```cpp
    out << "  identity kind=" << IdentityKindText(value.Kind) << " bytes=";
```

- [ ] **Step 4: 验证「加了枚举值会硬编译失败」这条守卫真的在**

临时把 `IdentityKindText` 里 `case kMachOUuid:` 那两行注释掉，构建 console 线：

```bash
Scripts/msvc-env.cmd cmake --build --preset win-x64-msvc-debug --target VaseConsole
```
Expected: **构建失败**，报 `warning C4062`（被 `/WX` 升为 error）或 clang 的 `-Wswitch` 同类；
然后**恢复那两行**，再构建一次 Expected: 通过。

- [ ] **Step 5: 六线回归**

Run: 同 Task 1 Step 3（加 `win-x64-msvc-debug`）。
Expected: 全绿；用例数不变（本 task 不加用例）。

- [ ] **Step 6: Commit**

```bash
git add Include/Vase/Detail/ImageInspect.h Tools/VaseConsole/Console.cpp
git commit -m "macOS 腿 T2：IdentityKind 加 kMachOUuid 并去掉默认值；Console 身份种类改穷举 switch"
```

---

## Task 3: Mach-O 解析器 + 合成字节用例（八线同幅）

本 task 的全部见证都在**跨平台的合成字节**上——这正是让解析器不靠 macOS 就能测的关键。

**Files:**
- Create: `Source/Host/ImageInspectMachO.cpp`
- Modify: `Source/Host/CMakeLists.txt`（**无条件**加入，见 Step 5）
- Modify: `Include/Vase/Detail/ImageInspect.h`（两个纯函数声明 + 错误文案约定）
- Modify: `Tests/Unit/LoaderTests.cpp`（`MakeMinimalMachO()` + 三条用例）

**Interfaces:**
- Consumes: `vase::detail::IdentityKind::kMachOUuid`（Task 2）、`Detail/ImageBytes.h`（Task 1）
- Produces:
  - `Result<ImageIdentity> ParseMachOUuidFile(std::span<const std::uint8_t> fileBytes)`
  - `Result<std::vector<std::string>> ParseMachODylibNamesFile(std::span<const std::uint8_t> fileBytes)`
  - **契约**：两者都返回**文件里的原文**，不做任何路径归一（`@rpath/` 的处置在比对点，Task 5）

- [ ] **Step 1: 先写失败用例 `MakeMinimalMachO()` + 三条**

加到 `Tests/Unit/LoaderTests.cpp` 的匿名 namespace 内（紧挨 `MakeMinimalElf()` 之后）：

```cpp
// 最小 Mach-O 64：32 字节头 + 一条 LC_UUID + 一条 LC_ID_DYLIB + 一条 LC_LOAD_DYLIB。
// 与 MakeMinimalPe / MakeMinimalElf 同形状——不依赖宿主平台，八条线都能跑（D173）。
std::vector<std::uint8_t> MakeMinimalMachO(bool withUuid = true)
{
    std::vector<std::uint8_t> b(0x200, 0);
    Put32(b, 0, 0xFEEDFACFU);  // MH_MAGIC_64
    Put32(b, 4, 0x01000007U);  // CPU_TYPE_X86_64
    Put32(b, 16, withUuid ? 3U : 2U); // ncmds
    Put32(b, 20, withUuid ? 0x18U + 0x40U + 0x38U : 0x40U + 0x38U); // sizeofcmds

    std::size_t at = 32U;
    if (withUuid)
    {
        Put32(b, at, 0x1BU);      // LC_UUID
        Put32(b, at + 4U, 0x18U); // cmdsize = 8 + 16
        for (std::size_t i = 0; i < 16U; ++i)
        {
            b[at + 8U + i] = static_cast<std::uint8_t>(0x30U + i);
        }
        at += 0x18U;
    }
    // LC_ID_DYLIB：名字是自名，**不该**出现在依赖名清单里
    Put32(b, at, 0xDU);
    Put32(b, at + 4U, 0x40U);
    Put32(b, at + 8U, 24U); // lc_str.name 相对本条目起点的偏移
    PutStr(b, at + 24U, "libSelf.dylib");
    at += 0x40U;
    // LC_LOAD_DYLIB：这才是导入
    Put32(b, at, 0xCU);
    Put32(b, at + 4U, 0x38U);
    Put32(b, at + 8U, 24U);
    PutStr(b, at + 24U, "@rpath/libSibling.dylib");
    return b;
}
```

> `Put32` / `PutStr` 是 `LoaderTests.cpp` 里既有的构造助手（PE/ELF 两段在用），照用。

三条用例：

```cpp
TEST(ImageInspect, MachOUuidExtracted)
{
    const auto b = MakeMinimalMachO();
    const vase::Result<vase::detail::ImageIdentity> r = vase::detail::ParseMachOUuidFile(b);
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    EXPECT_EQ(r.Value().Kind, vase::detail::IdentityKind::kMachOUuid);
    ASSERT_EQ(r.Value().Bytes.size(), 16U);
    EXPECT_EQ(r.Value().Bytes.front(), 0x30U);
    EXPECT_EQ(r.Value().Bytes.back(), 0x3FU);
}

TEST(ImageInspect, MachODylibNamesListed)
{
    const auto b = MakeMinimalMachO();
    const vase::Result<std::vector<std::string>> r = vase::detail::ParseMachODylibNamesFile(b);
    ASSERT_TRUE(r.IsOk()) << r.GetError().Message();
    ASSERT_EQ(r.Value().size(), 1U);                                  // LC_ID_DYLIB 不算导入
    EXPECT_EQ(r.Value().front(), "@rpath/libSibling.dylib");          // 契约：返回文件原文，不归一（D176）
}

TEST(ImageInspect, MachOWithoutUuidFailsLouder)
{
    const auto b = MakeMinimalMachO(/*withUuid=*/false);
    const vase::Result<vase::detail::ImageIdentity> r = vase::detail::ParseMachOUuidFile(b);
    ASSERT_FALSE(r.IsOk());
    EXPECT_NE(r.GetError().Message().find("-no_uuid"), std::string::npos); // 指路（§8.2）
}
```

- [ ] **Step 2: 跑用例确认它们失败**

Run: `cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R ImageInspect`
Expected: 链接失败（`ParseMachOUuidFile` 未定义）——这正是「红」的形态。

- [ ] **Step 3: 实现 `Source/Host/ImageInspectMachO.cpp`**

```cpp
#include "Vase/Detail/ImageInspect.h"

#include "Detail/ImageBytes.h"
#include "Vase/Detail/Result.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

// 手写常量与字段偏移，**不 include <mach-o/loader.h>**：本 TU 无条件编进 VaseHost
// （D161——八线的合成字节用例都要链接它），而系统头只在 Darwin 上存在。
namespace vase::detail
{
namespace
{

constexpr std::uint32_t kMachMagic64 = 0xFEEDFACFU; // MH_MAGIC_64（小端）

// mach_header_64：magic@0 ncmds@16(4) sizeofcmds@20(4)；头长 32。
constexpr std::size_t kMachHeader64Size = 32U;
constexpr std::size_t kMachNcmdsOffset = 16U;
constexpr std::size_t kMachSizeOfCmdsOffset = 20U;

// load_command：cmd@0(4) cmdsize@4(4)。
constexpr std::size_t kLoadCommandSize = 8U;

constexpr std::uint32_t kLcUuid = 0x1BU;
constexpr std::uint32_t kLcLoadDylib = 0x0CU;
constexpr std::uint32_t kLcLoadWeakDylib = 0x18U;

constexpr std::size_t kUuidCommandSize = 24U;  // cmd(4) + cmdsize(4) + uuid(16)
constexpr std::size_t kDylibNameOffset = 8U;   // dylib_command 内 lc_str.name 的偏移
constexpr std::size_t kMachUuidBytes = 16U;

[[nodiscard]] Error MissingMachOUuidError()
{
    return Error{"no LC_UUID load command — 插件构建用了 -Wl,-no_uuid（见 "
                 "Cmake/Toolchains/macos-x64-clang-libcxx.cmake，§8.2 档三）"};
}

// 遍历 load commands。Call(cmd, 条目起点, cmdsize)。
// 返回 false = 「这不是一份能读的 Mach-O」，与「读得出但没有要找的东西」分开——
// 调用方据此选错误文案，不把「格式不对」混进「缺特征」。
template <typename Call>
[[nodiscard]] bool ForEachLoadCommand(std::span<const std::uint8_t> image, Call&& call)
{
    if (image.size() < kMachHeader64Size || ReadU32(image, 0) != kMachMagic64)
    {
        return false;
    }
    const std::uint32_t count = ReadU32(image, kMachNcmdsOffset);
    const std::uint32_t sizeOfCmds = ReadU32(image, kMachSizeOfCmdsOffset);
    if (!InBounds(kMachHeader64Size, sizeOfCmds, image.size()))
    {
        return false;
    }
    const std::size_t end = kMachHeader64Size + sizeOfCmds;
    std::size_t at = kMachHeader64Size;
    for (std::uint32_t index = 0; index < count && at + kLoadCommandSize <= end; ++index)
    {
        const std::uint32_t cmd = ReadU32(image, at);
        const std::uint32_t cmdSize = ReadU32(image, at + 4U);
        if (cmdSize < kLoadCommandSize || at + cmdSize > end)
        {
            return false; // 结构越界：当作「读不出可用的东西」，绝不越界读
        }
        call(cmd, at, cmdSize);
        at += cmdSize;
    }
    return true;
}

} // namespace

Result<ImageIdentity> ParseMachOUuidFile(std::span<const std::uint8_t> fileBytes)
{
    using Identity = Result<ImageIdentity>;

    std::vector<std::uint8_t> found;
    const bool walked = ForEachLoadCommand(fileBytes,
                                           [&fileBytes, &found](std::uint32_t cmd, std::size_t at, std::uint32_t cmdSize)
                                           {
                                               if (cmd != kLcUuid || !found.empty() || cmdSize < kUuidCommandSize)
                                               {
                                                   return;
                                               }
                                               const std::span<const std::uint8_t> uuid =
                                                   fileBytes.subspan(at + kLoadCommandSize, kMachUuidBytes);
                                               found.assign(uuid.begin(), uuid.end());
                                           });
    if (!walked)
    {
        return Identity::Err(Error{"not a Mach-O image"});
    }
    if (found.empty())
    {
        return Identity::Err(MissingMachOUuidError());
    }
    ImageIdentity identity;
    identity.Kind = IdentityKind::kMachOUuid;
    identity.Bytes = std::move(found);
    return Identity::Ok(std::move(identity));
}

Result<std::vector<std::string>> ParseMachODylibNamesFile(std::span<const std::uint8_t> fileBytes)
{
    using Names = Result<std::vector<std::string>>;

    std::vector<std::string> names;
    const bool walked = ForEachLoadCommand(
        fileBytes,
        [&fileBytes, &names](std::uint32_t cmd, std::size_t at, std::uint32_t cmdSize)
        {
            // LC_ID_DYLIB 是**自名**不是导入，不收（与 DT_NEEDED / PE 导入表同口径）。
            if (cmd != kLcLoadDylib && cmd != kLcLoadWeakDylib)
            {
                return;
            }
            const std::uint32_t nameOffset = ReadU32(fileBytes, at + kDylibNameOffset);
            if (nameOffset >= cmdSize)
            {
                return; // 坏条目：跳过而不是整体失败（与 PE 导入表的 break 同精神）
            }
            Result<std::string> name = ReadNulTerminated(fileBytes, at + nameOffset, at + cmdSize);
            if (name.IsOk())
            {
                names.push_back(std::move(name.Value()));
            }
        });
    if (!walked)
    {
        return Names::Err(Error{"not a Mach-O image"});
    }
    return Names::Ok(std::move(names));
}

} // namespace vase::detail
```

- [ ] **Step 4: 在 `ImageInspect.h` 补两个声明**

放在 ELF 那组之后：

```cpp
// —— Mach-O（macOS）。纯字节进、纯数据出，**跨平台编译**（八线的合成字节用例都调它）。
//    契约：名字一律返回**文件里的原文**，不做路径归一——`@rpath/` 的处置在比对点（D176）。
VASE_HOST_API Result<ImageIdentity> ParseMachOUuidFile(std::span<const std::uint8_t> fileBytes);
VASE_HOST_API Result<std::vector<std::string>> ParseMachODylibNamesFile(std::span<const std::uint8_t> fileBytes);
```

- [ ] **Step 5: `Source/Host/CMakeLists.txt` 把它加进无条件那组**

在 `add_library(VaseHost SHARED …)` 的既有列表里（`ImageInspectCommon.cpp` 旁边）加 `ImageInspectMachO.cpp`，并加一条注释：

```cmake
    # Mach-O 解析器**无条件**编译（D161）：它是纯字节解析，与 PE/ELF 两段同性质；
    # 八条线的合成字节用例都要链接它。放进 APPLE 分支会让那族用例在非 macOS 上链接失败。
    ImageInspectMachO.cpp
```

- [ ] **Step 6: 跑用例确认通过（六线）**

Run: 六条 preset 各 build + `ctest -R 'ImageInspect|Loader'`
Expected: 全绿；新增三条在三平台六线上都注册（无 `#ifdef`、无 `NDEBUG` 门）。

- [ ] **Step 7: Commit**

```bash
git add Source/Host/ImageInspectMachO.cpp Source/Host/CMakeLists.txt Include/Vase/Detail/ImageInspect.h Tests/Unit/LoaderTests.cpp
git commit -m "macOS 腿 T3：Mach-O 纯字节解析器（LC_UUID / LC_LOAD_DYLIB）+ 八线同幅合成用例"
```

---

## Task 4: `ImageFormat` 枚举、`PlatformImageFormat()`、`Loader.cpp` 平台分支降到一处

**Files:**
- Modify: `Include/Vase/Detail/ImageInspect.h`（`ImageFormat` + `FirstUnresolvableImport` 换形）
- Modify: `Source/Host/ImageInspectPlatform.h`（两个声明）
- Modify: `Source/Host/ImageInspectCommon.cpp`（两处实现）
- Modify: `Source/Host/Loader.cpp:104-130`（两处 `#ifdef` 消失）
- Modify: `Tests/Unit/LoaderTests.cpp:186/217`（`isPe` → 枚举）

**Interfaces:**
- Consumes: `ParseMachODylibNamesFile`（Task 3）
- Produces: `vase::detail::ImageFormat`、`vase::detail::PlatformImageFormat()`、`vase::detail::ParseImportedLibraryNames(span)`

- [ ] **Step 1: `ImageInspect.h` 加枚举、换签名**

```cpp
// 镜像格式。三值而非 bool：`isPe` 那个布尔是「只有两种格式」时代的形状（D174）。
enum class ImageFormat : std::uint8_t
{
    kPe,
    kElf,
    kMachO,
};
```
把 `:53` 换成：
```cpp
VASE_HOST_API std::string FirstUnresolvableImport(std::span<const std::uint8_t> fileBytes, ImageFormat format);
```

- [ ] **Step 2: `ImageInspectPlatform.h` 加两个声明**

```cpp
// 当前平台的镜像格式——格式知识只此一处（D174）。三个平台条件与
// Source/Host/CMakeLists.txt 的三分支一一对位。
[[nodiscard]] ImageFormat PlatformImageFormat();

// 按当前平台读「导入表 / 依赖名」。存在理由：让 Loader.cpp 不再需要平台分支（D174）。
[[nodiscard]] Result<std::vector<std::string>> ParseImportedLibraryNames(std::span<const std::uint8_t> fileBytes);
```

- [ ] **Step 3: `ImageInspectCommon.cpp` 实现两者**

```cpp
ImageFormat PlatformImageFormat()
{
#if defined(_WIN32)
    return ImageFormat::kPe;
#elif defined(__APPLE__)
    return ImageFormat::kMachO;
#else
    return ImageFormat::kElf;
#endif
}

Result<std::vector<std::string>> ParseImportedLibraryNames(std::span<const std::uint8_t> fileBytes)
{
    // 穷举 switch（无 default）：加格式时 clang 线的 -Wswitch 会顶出来。
    switch (PlatformImageFormat())
    {
    case ImageFormat::kPe:
        return ParsePeImports(fileBytes, /*loadedInMemory=*/false);
    case ImageFormat::kElf:
        return ParseElfNeededFile(fileBytes);
    case ImageFormat::kMachO:
        return ParseMachODylibNamesFile(fileBytes);
    }
    return Result<std::vector<std::string>>::Err(Error{"unreachable image format"});
}
```
把 `FirstUnresolvableImport` 的实现改成：

```cpp
std::string FirstUnresolvableImport(std::span<const std::uint8_t> fileBytes, ImageFormat format)
{
    Result<std::vector<std::string>> names = Result<std::vector<std::string>>::Err(Error{"unreachable image format"});
    // 穷举 switch（无 default）：加格式时 clang 线的 -Wswitch 会顶出来。
    switch (format)
    {
    case ImageFormat::kPe:
        names = ParsePeImports(fileBytes, /*loadedInMemory=*/false);
        break;
    case ImageFormat::kElf:
        names = ParseElfNeededFile(fileBytes);
        break;
    case ImageFormat::kMachO:
        names = ParseMachODylibNamesFile(fileBytes);
        break;
    }
    if (!names.IsOk() || names.Value().empty())
    {
        return {};
    }
    return names.Value().front();
}
```

- [ ] **Step 4: `Loader.cpp` 两处 `#ifdef` 换成平台无关调用**

```cpp
Result<std::vector<std::string>> Loader::ImportedLibraryNamesFromFile(const std::filesystem::path& path)
{
    const std::vector<std::uint8_t> bytes = ReadImageFileBytes(path);
    if (bytes.empty())
    {
        return Result<std::vector<std::string>>::Err(Error{"cannot read file: " + path.string()});
    }
    return ParseImportedLibraryNames(bytes);
}

std::string Loader::DescribeLoadFailure(const std::filesystem::path& path)
{
    const std::vector<std::uint8_t> bytes = ReadImageFileBytes(path);
    if (bytes.empty())
    {
        return {};
    }
    return FirstUnresolvableImport(bytes, PlatformImageFormat());
}
```
同时在 `:71-82` 那段 Unload 证据的分支上就地补一句注释：

```cpp
    // macOS 属 #else 这一支：dyld 的镜像清单能可靠回答「还在不在」（与 dl_iterate_phdr 同构），
    // 且 open(O_WRONLY) 与是否映射无关，故 ReopenWritable 同样无判据力（D165）。
```

- [ ] **Step 5: 改 `Tests/Unit/LoaderTests.cpp` 的两处 `isPe` 实参**

`:186` 的 `FirstUnresolvableImport(b, /*isPe=*/true)` → `(b, vase::detail::ImageFormat::kPe)`；
`:217` 的 `(b, /*isPe=*/false)` → `(b, vase::detail::ImageFormat::kElf)`。

- [ ] **Step 6: 六线回归**

Run: 六条 preset 各 build + full `ctest`。
Expected: 全绿、用例数不变——**本 task 是零行为变化的换形**。

- [ ] **Step 7: Commit**

```bash
git add Include/Vase/Detail/ImageInspect.h Source/Host/ImageInspectPlatform.h Source/Host/ImageInspectCommon.cpp Source/Host/Loader.cpp Tests/Unit/LoaderTests.cpp
git commit -m "macOS 腿 T4：ImageFormat 枚举 + PlatformImageFormat()，Loader.cpp 平台分支降到一处"
```

---

## Task 5: `@rpath` 归一到比对点

**Files:**
- Modify: `Source/Host/PluginHost.cpp:1101-1105`

**Interfaces:**
- Consumes: 无
- Produces: 无（行为修正）

> **为什么在比对点而不是解析层**：ELF 的 `DT_NEEDED` 同样允许带路径，这个静默失效在 Linux 上今天就潜伏着——解析层只堵 Mach-O 一处，比对点一次堵三处（D176）。

- [ ] **Step 1: 改比对**

原：
```cpp
    const std::string selfName = LowerAscii(absPath.filename().string());
    for (const std::string& imported : imports.Value())
    {
        const std::string lowered = LowerAscii(imported);
        const bool siblingHit = std::ranges::any_of(siblings, [&lowered](const std::string& sibling)
                                                    { return LowerAscii(sibling) == lowered; });
        if (lowered != selfName && siblingHit)
```
改：
```cpp
    const std::string selfName = LowerAscii(absPath.filename().string());
    for (const std::string& imported : imports.Value())
    {
        // 导入名可能带路径：macOS 的 LC_LOAD_DYLIB 记 `@rpath/libX.dylib`，而 ELF 的
        // DT_NEEDED 同样允许带斜杠。兄弟集是裸文件名，故两侧都按 basename 归一——
        // 归一放在这里而不是解析层，三个格式一次覆盖（D176）。
        const std::string lowered = LowerAscii(std::filesystem::path(imported).filename().string());
        const bool siblingHit = std::ranges::any_of(siblings, [&lowered](const std::string& sibling)
                                                    { return LowerAscii(sibling) == lowered; });
        if (lowered != selfName && siblingHit)
```

- [ ] **Step 2: 六线回归**

Run: 六条 preset 各 build + `ctest -R 'Abi|Adopt'`
Expected: 全绿。两条执法用例（`Abi.AdoptRejectsBinaryThatImportsSiblingPlugin`、`Adopt.RequestSiblingImportCaughtDespiteMixedCase`）的**输入在 Linux/Windows 上不带路径**，故是 no-op 回归；它们的正向证人（带 `@rpath/` 的输入）在 macOS 线上，Task 9 跑。

- [ ] **Step 3: Commit**

```bash
git add Source/Host/PluginHost.cpp
git commit -m "macOS 腿 T5：§8.7 执法按 basename 归一导入名（@rpath/ 与 DT_NEEDED 同形）"
```

---

## Task 6: `LibraryFileName.h` 的 `.dylib` 分支（还 D138）

**Files:**
- Modify: `Include/Vase/Catalog/LibraryFileName.h:5,14-20`
- Modify: `Tests/Unit/LibraryFileNameTests.cpp:33-37`

**Interfaces:**
- Consumes: 无
- Produces: macOS 上 `LibraryFileName("X") == "libX.dylib"`、`LibraryStem("libX.dylib") == "X"`

- [ ] **Step 1: 改头**

```cpp
#ifdef _WIN32
inline constexpr std::string_view kLibraryPrefix;
inline constexpr std::string_view kLibrarySuffix = ".dll";
#elif defined(__APPLE__)
inline constexpr std::string_view kLibraryPrefix = "lib";
inline constexpr std::string_view kLibrarySuffix = ".dylib";
#else
inline constexpr std::string_view kLibraryPrefix = "lib";
inline constexpr std::string_view kLibrarySuffix = ".so";
#endif
```
头注释里「macOS 的 .dylib 与 iOS 的 .a 分支归 VasePack/M5 后续波次（D138）」改成「iOS 的 .a 分支归 VasePack 那一波」。

- [ ] **Step 2: 测试三分支**

`Tests/Unit/LibraryFileNameTests.cpp:32-38` 的
```cpp
#ifdef _WIN32
    EXPECT_FALSE(vase::catalog_detail::LibraryStem(".dll").has_value());
#else
    EXPECT_FALSE(vase::catalog_detail::LibraryStem("lib.so").has_value());
#endif
```
改成三分支（`#elif defined(__APPLE__)` → `"lib.dylib"`）。
**必须显式三分支，不能靠 `#else` 巧合通过**——`lib.so` 在 macOS 上本就非法，那条断言会绿得没道理。

- [ ] **Step 3: 六线回归**

Run: 六条 preset 各 build + `ctest -R LibraryFileName`
Expected: 全绿；`LibraryFileName` 那族在 macOS 上条目数不变（体内 `#ifdef`，各平台各一条）。

- [ ] **Step 4: Commit**

```bash
git add Include/Vase/Catalog/LibraryFileName.h Tests/Unit/LibraryFileNameTests.cpp
git commit -m "macOS 腿 T6：LibraryFileName 落 .dylib 分支（还 D138）"
```

---

## Task 7: `ImageInspectDarwin.cpp` + `ImageInspectPosix.cpp` 改名 + CMake 三分支

本 task 在**既有六条线上是 no-op**（Linux 支行为逐字不变），这是它可独立验收的原因。

**Files:**
- Rename: `Source/Host/ImageInspectPosix.cpp` → `Source/Host/ImageInspectLinux.cpp`
- Create: `Source/Host/ImageInspectDarwin.cpp`
- Modify: `Source/Host/CMakeLists.txt:4-12`
- Modify: `Source/Host/ImageInspectPlatform.h`（注释）
- Modify: `Source/Host/LoaderPosix.cpp:53`（注释）

**Interfaces:**
- Consumes: `ParseMachOUuidFile`（Task 3）
- Produces: Darwin 侧的 `IsImageMapped` / `MemoryIdentityPlatform` / `FileIdentityPlatform`（与 Linux/Windows 同签名）

- [ ] **Step 1: 改名**

```bash
git mv Source/Host/ImageInspectPosix.cpp Source/Host/ImageInspectLinux.cpp
```
文件头加一句：`// Linux 专属（<elf.h>/<link.h>/dl_iterate_phdr）；macOS 的对位实现是 ImageInspectDarwin.cpp。`

- [ ] **Step 2: CMake 三分支**

`Source/Host/CMakeLists.txt:4-12` 改成：
```cmake
# 平台文件按平台选：Windows 走 PE，macOS 走 Mach-O + dyld，其余走 ELF + dl_iterate_phdr。
# 两套实现同一组接口，由 Source/Host/ImageInspectPlatform.h 声明（Host 内部，不进公开头）。
# **注意 Mach-O 的纯字节解析器不在这里**——它无条件编译（见 add_library 那一段）。
if(WIN32)
    set(VASE_HOST_PLATFORM_SOURCES
        ImageInspectWindows.cpp
        LoaderWindows.cpp)
elseif(APPLE)
    set(VASE_HOST_PLATFORM_SOURCES
        ImageInspectDarwin.cpp
        LoaderPosix.cpp)
else()
    set(VASE_HOST_PLATFORM_SOURCES
        ImageInspectLinux.cpp
        LoaderPosix.cpp)
endif()
```

- [ ] **Step 3: 写 `ImageInspectDarwin.cpp`**

```cpp
#include "Vase/Detail/ImageInspect.h"

#include "ImageInspectPlatform.h"
#include "Vase/Detail/Result.h"

#include <mach-o/dyld.h>
#include <mach-o/loader.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace vase::detail
{
namespace
{

// dyld 记的是**规范化路径**（实测 /tmp 记成 /private/tmp）——全等匹配前必须规范化（D164）。
// canonical 失败（文件刚被换掉的窗口）退 weakly_canonical，再失败退回原路径：那时该做的
// 是「按能拿到的路径比对、如实报找不到」，不是崩（§9.2：filesystem 一律 error_code 形态）。
//
// **对测试是承重的**：CatalogSandbox 把 fixture 铺进临时目录，macOS 的 temp_directory_path()
// 落在 /var/folders/...，其真身是 /private/var/folders/...——不规范化则每条走装载的用例都匹配落空。
[[nodiscard]] std::string Normalized(const std::filesystem::path& path)
{
    std::error_code ec;
    const std::filesystem::path resolved = std::filesystem::canonical(path, ec);
    if (!ec)
    {
        return resolved.string();
    }
    std::error_code weakEc;
    const std::filesystem::path weak = std::filesystem::weakly_canonical(path, weakEc);
    return weakEc ? path.string() : weak.string();
}

[[nodiscard]] int FindImageIndex(const std::filesystem::path& path)
{
    const std::string target = Normalized(path);
    const std::uint32_t count = ::_dyld_image_count();
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const char* name = ::_dyld_get_image_name(index);
        if (name != nullptr && target == name)
        {
            return static_cast<int>(index);
        }
    }
    return -1;
}

} // namespace

bool IsImageMapped(const std::filesystem::path& path) { return FindImageIndex(path) >= 0; }

Result<ImageIdentity> MemoryIdentityPlatform(const void* raw, const std::filesystem::path& path)
{
    static_cast<void>(raw); // 与 Linux 同：由路径在 dyld 清单里定位，不用句柄
    const int index = FindImageIndex(path);
    if (index < 0)
    {
        return Result<ImageIdentity>::Err(Error{"image not mapped: " + path.string()});
    }
    const mach_header* header = ::_dyld_get_image_header(static_cast<std::uint32_t>(index));
    if (header == nullptr)
    {
        return Result<ImageIdentity>::Err(Error{"image header unavailable: " + path.string()});
    }
    const auto* typed = reinterpret_cast<const mach_header_64*>(header);
    // 内存镜像的 load commands 紧跟 32 字节头，sizeofcmds 是它们的总长；解析器从前者之后起读。
    // 「对象表示 → 字节」只有 reinterpret_cast 一个写法（它就是它）。
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const std::span<const std::uint8_t> image{reinterpret_cast<const std::uint8_t*>(header),
                                              sizeof(mach_header_64) + typed->sizeofcmds};
    return ParseMachOUuidFile(image);
}

Result<ImageIdentity> FileIdentityPlatform(const std::filesystem::path& path)
{
    const std::vector<std::uint8_t> bytes = ReadImageFileBytes(path);
    if (bytes.empty())
    {
        return Result<ImageIdentity>::Err(Error{"cannot read file: " + path.string()});
    }
    return ParseMachOUuidFile(bytes);
}

} // namespace vase::detail
```

- [ ] **Step 4: 补注释**

- `Source/Host/ImageInspectPlatform.h`：三个函数的契约注释补 macOS 形态（档三内存侧写「Linux：`dl_iterate_phdr` 全等匹配；macOS：`_dyld_image_count` 枚举，路径先规范化」，`IsImageMapped` 的「仅 Linux」改「Linux 与 macOS，各自机制不同」）。
- `Source/Host/LoaderPosix.cpp:53`：「Linux 侧恒真」→「POSIX 侧恒真（Linux 与 macOS）」。

- [ ] **Step 5: 六线回归**

Run: 六条 preset 各 build + full `ctest` + `ctest -N`。
Expected: 全绿、基数与改动前逐位相同（Linux 支只是换了文件名与 TU 名，行为逐字不变）。

- [ ] **Step 6: Commit**

```bash
git add -A Source/Host/
git commit -m "macOS 腿 T7：ImageInspectPosix 改名 Linux；新增 Darwin 平台实现；CMake 三分支"
```

---

## Task 8: 构建接入（toolchain / triplet / presets / scripts / `NoIdentityPlugin` 三分支）

**Files:**
- Create: `Cmake/Toolchains/macos-x64-clang-libcxx.cmake`
- Create: `Cmake/Triplets/x64-osx-libcxx.cmake`
- Modify: `CMakePresets.json`
- Create: `Scripts/macos-verify.sh`、`Scripts/macos-clang-tidy.sh`
- Modify: `Tests/CMakeLists.txt:122-129`（`NoIdentityPlugin` 链接选项三分支）

**Interfaces:**
- Consumes: Task 7 的三分支
- Produces: preset `macos-x64-clang-debug` / `macos-x64-clang-release`，构建树 `build-macos/<preset>/`

- [ ] **Step 1: 写 triplet**

`Cmake/Triplets/x64-osx-libcxx.cmake`：
```cmake
# 自定义 triplet：macOS x64 + libc++。
#
# 为什么不能直接用内置/community 的那份：
#   ① **编译器**——`scripts/toolchains/osx.cmake`（73 行）通篇不提编译器，原生构建时它
#      留给 CMake 平台默认值，也就是 Apple clang（实测 16.0.0，用 SDK 的 libc++ 头树），
#      而我们的 TU 由 MacPorts clang 23.1.2 编、吃的是 /opt/local/libexec/llvm-23 下自带
#      的那份 libc++ 头。不钉就会让 gtest 与我们在两棵头树上生成代码。
#   ② **linkage**——community/x64-osx.cmake 是 static。那**不构成** 8.3 的堆问题
#      （VCPKG_LIBRARY_LINKAGE 只影响 gtest 一个测试库，它静态链进 VaseTests、不跨我们的
#      模块边界），这里取 dynamic 只是为了与另三条线一致。**别把它读成正确性理由。**
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES x86_64)

# 必须连**编译器**一起钉，光钉 flags 不够（与 x64-linux-libcxx.cmake 同一条理由）：
# vcpkg 原生构建时不选编译器，探测那一步会拿 Apple clang 去编。
# **与平台工具链的一处不对称，已知且暂容**：工具链文件把编译器路径做了目录去引用化，
# 这里却只能给裸名——triplet 是独立脚本，拿不到工具链解析出的真实目录，而重复一遍
# find_program + REAL_PATH 会是同一条规则的第二次声明。
set(VCPKG_CMAKE_CONFIGURE_OPTIONS
    "-DCMAKE_C_COMPILER=clang-mp-23"
    "-DCMAKE_CXX_COMPILER=clang++-mp-23")

# **留空是对的**：macOS 上 clang 默认即 libc++（实测产物 LC_LOAD_DYLIB 里是
# /usr/lib/libc++.1.dylib），不需要 Linux 那一份的 -stdlib=libc++。别照 Linux 补齐。
set(VCPKG_CXX_FLAGS "")
set(VCPKG_C_FLAGS "")
```

- [ ] **Step 2: 写平台工具链**

`Cmake/Toolchains/macos-x64-clang-libcxx.cmake`（骨架照 `linux-x64-clang-libcxx.cmake`）：
```cmake
# 平台工具链：macOS x64 + clang + libc++。
#
# 接线方向与另两侧一致：平台工具链文件是 CMAKE_TOOLCHAIN_FILE，
# 文件内部 include vcpkg 的，使平台配置先于 vcpkg 生效。

if(NOT DEFINED ENV{VCPKG_ROOT} OR "$ENV{VCPKG_ROOT}" STREQUAL "")
    message(FATAL_ERROR
        "环境变量 VCPKG_ROOT 未设置。\n"
        "macOS 侧它由 ~/.zshenv 提供（本机环境类变量住那里），交互式与登录 shell 都可见。")
endif()

# D6：由 PATH 解析，不写死安装路径。find_program 的结果进缓存——换 LLVM 后须干净 configure，
# 光改 PATH 不够（README「环境前提」同一条结论）。
# clang++-mp-23 **排在 clang++ 之前**：MacPorts 的 clang++ 会随默认版本漂移，
# 钉住 23 才与根 CMakeLists 的版本闸一致；真漂到别处时由 doctor 响亮拒绝，不静默降级。
find_program(VASE_CLANGXX_EXECUTABLE NAMES clang++-mp-23 clang++
    DOC "macOS 平台 C++ 编译器（从 PATH 解析）")
if(NOT VASE_CLANGXX_EXECUTABLE)
    message(FATAL_ERROR
        "PATH 里找不到 clang++-mp-23 / clang++。\n"
        "本仓库要求 clang 23.x：MacPorts 的 clang-23 提供 clang++-mp-23，"
        "Apple 自带的 clang 不满足（实测为 16.0.0，且 CMAKE_CXX_COMPILER_ID 的版本闸会拒）。")
endif()

# 名字必须留 "++"（驱动按 argv[0] 判 C++ 模式），只把**目录**规范化——
# 理由与 linux-x64-clang-libcxx.cmake 的【一】【二】逐字同构，不在此重复。
file(REAL_PATH "${VASE_CLANGXX_EXECUTABLE}" VASE_CLANGXX_REALPATH)
get_filename_component(VASE_CLANGXX_REALDIR "${VASE_CLANGXX_REALPATH}" DIRECTORY)
if(EXISTS "${VASE_CLANGXX_REALDIR}/clang++")
    set(VASE_CLANGXX_EXECUTABLE "${VASE_CLANGXX_REALDIR}/clang++")
elseif(EXISTS "${VASE_CLANGXX_REALDIR}/clang++-mp-23")
    set(VASE_CLANGXX_EXECUTABLE "${VASE_CLANGXX_REALDIR}/clang++-mp-23")
endif()

set(CMAKE_CXX_COMPILER "${VASE_CLANGXX_EXECUTABLE}" CACHE STRING "macOS 平台 C++ 编译器" FORCE)

# 显式写架构：「凡影响产物形态的默认值一律显式写出」。本机恰是 x86_64，不能靠「反正默认就对」。
set(CMAKE_OSX_ARCHITECTURES x86_64 CACHE STRING "macOS 目标架构" FORCE)

# **本平台没有对位的承重 linker flag**——这不是漏写。实测链接器必写 LC_UUID，
# 身份特征天然在场，无需 /DEBUG:FULL（Windows）或 -Wl,--build-id=sha1（Linux）那样的注入。
# 承重性由**负例反向把守**：Tests/CMakeLists.txt 的 NoIdentityPlugin 在 macOS 侧用
# -Wl,-no_uuid 主动摘掉它，档三必须拒。所以「摘掉标志 ⇒ 运行期响亮失败」这条判据在这里
# 由负例承担，而不是由 flag 承担。
#
# 8.5：libc++ 是 macOS 的默认，不需要 -stdlib=libc++（与 Linux 侧不同）。
set(CMAKE_MODULE_LINKER_FLAGS_INIT "")

# 位置要求：这两句都必须在下面那句 include **之前**（vcpkg 的工具链在 include 那一刻
# 就把它们读走并写进 CACHE）。理由同 linux-x64-clang-libcxx.cmake。
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_LIST_DIR}/../Triplets")
set(VCPKG_TARGET_TRIPLET x64-osx-libcxx)

include("$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
```

- [ ] **Step 3: 加 preset**

在 `CMakePresets.json` 的 `configurePresets` 里，照 `linux-x64-clang` 那段并列加：
```json
    {
      "name": "macos-x64-clang",
      "hidden": true,
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build-macos/${presetName}",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/Cmake/Toolchains/macos-x64-clang-libcxx.cmake",
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
      }
    },
    {
      "name": "macos-x64-clang-debug",
      "inherits": "macos-x64-clang",
      "displayName": "macOS x64 / clang / libc++ / Debug",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" }
    },
    {
      "name": "macos-x64-clang-release",
      "inherits": "macos-x64-clang",
      "displayName": "macOS x64 / clang / libc++ / Release",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" }
    }
```
`buildPresets` 与 `testPresets` 各加对应两条（`testPresets` 的 `output.outputOnFailure` 为 `true`，与既有五条一致）。

- [ ] **Step 4: 写 `Scripts/macos-verify.sh`**

```bash
#!/bin/bash
# 干净树两线全量（macOS x64）。在 macOS 开发机上本地跑。
set -u

# pkg-config 是 macOS 独有的环境前提：缺它 vcpkg 的 gtest port 会无条件倒在
# vcpkg_fixup_pkgconfig，症状是「什么都编不出来」而不是一条清楚的缺依赖。
if ! command -v pkg-config >/dev/null 2>&1; then
    echo "!! pkg-config 不在 PATH 上。vcpkg 的 gtest port 需要它。" >&2
    echo "!! MacPorts: sudo port install pkgconf   /   Homebrew: brew install pkg-config" >&2
    exit 1
fi

cd ~/WindowsGit/Vase || exit 1

run() {
    echo ""
    echo "########## RUN: $* ##########"
    "$@"
    echo "########## RC=$? : $* ##########"
}

for p in macos-x64-clang-debug macos-x64-clang-release; do
    rm -rf build-macos/$p
done

run cmake --preset macos-x64-clang-debug
run cmake --build --preset macos-x64-clang-debug
run ctest --preset macos-x64-clang-debug
run ctest --preset macos-x64-clang-debug -N

run cmake --preset macos-x64-clang-release
run cmake --build --preset macos-x64-clang-release
run ctest --preset macos-x64-clang-release
run ctest --preset macos-x64-clang-release -N

echo ""
echo "########## ALL LINES DONE ##########"
```

- [ ] **Step 5: 写 `Scripts/macos-clang-tidy.sh`**

照 `Scripts/linux-clang-tidy.sh` 逐字改写，只改三处：`run-clang-tidy` → `run-clang-tidy-mp-23`、`-p build-linux/linux-x64-clang-debug` → `-p build-macos/macos-x64-clang-debug`、日志名。**不复制任何阈值**（三判据与摘要由脚本打印，基数以 `CLAUDE.md` 为准）。

- [ ] **Step 6: `NoIdentityPlugin` 三分支**

`Tests/CMakeLists.txt:125-129`：
```cmake
if(WIN32)
    target_link_options(NoIdentityPlugin PRIVATE "/DEBUG:NONE")
elseif(APPLE)
    # macOS 侧摘的是 LC_UUID；工具链本就没加对位的 flag，故这里没有「要压过的东西」，
    # 与另两平台的「后者胜」形态不同——那是预期的，不是没验过（D167）。
    target_link_options(NoIdentityPlugin PRIVATE "-Wl,-no_uuid")
else()
    target_link_options(NoIdentityPlugin PRIVATE "-Wl,--build-id=none")
endif()
```

- [ ] **Step 7: macOS 首跑（预期有已知红名单）**

Run: `ssh -o BatchMode=yes moozen-macos 'bash ~/WindowsGit/Vase/Scripts/macos-verify.sh' > /tmp/macos-verify.log 2>&1`

Expected:
- configure / build **必须全绿**；
- `ctest -N` 的 `Total Tests` 与 `linux-x64-clang-debug` **同值**；
- `ctest` 里有**已知红名单**（Task 9 修）：
  - `VaseCliDoctor.UnloadableBinaryIsNamedByCheckTwo`（`FlipMachineField` 假定 ELF 头）
  - `Loader.ProbeImportsVasePod`（`#else` 期望 `libVasePod.so`）
- 除此之外**不得有别的红**。有别的红 = 本 task 没做完，按日志定位。

- [ ] **Step 8: Commit**

```bash
git add Cmake/ CMakePresets.json Scripts/macos-verify.sh Scripts/macos-clang-tidy.sh Tests/CMakeLists.txt
git commit -m "macOS 腿 T8：构建接入（toolchain/triplet/presets/scripts）+ NoIdentityPlugin 三分支"
```

---

## Task 9: 测试三方化与文案（目标：八线全绿）

**Files:**
- Modify: `Tests/Integration/VaseCliDoctorTests.cpp:64-96`（`FlipMachineField`）
- Modify: `Tests/Unit/LoaderTests.cpp:281-308`、`:321-329`
- Modify: `Tests/Abi/ImportEnforcementTests.cpp:29`（注释）
- Modify: 其余 C 类文案站点（spec §4 清单）

**Interfaces:**
- Consumes: Task 6/7/8
- Produces: 八线全绿

- [ ] **Step 1: `FlipMachineField` 加 macOS 支（B5）**

在 `#else` 之前插入：
```cpp
#elif defined(__APPLE__)
    if (at(0U) != 0xCFU || at(1U) != 0xFAU || at(2U) != 0xEDU || at(3U) != 0xFEU)
    {
        ADD_FAILURE() << "not a parsable Mach-O header in " << binary;
        return;
    }
    // 翻的是**装载器认得的字段**（cputype@4），不是身份特征——本函数服务于
    // UnloadableBinaryIsNamedByCheckTwo：要让 ② 红、① 照常绿（identity ok）。
    // 翻成 CPU_TYPE_I386(7)：macOS 14 已无 32 位，dyld 以「不支持的架构」拒收。
    *std::next(bytes.begin(), 4) = static_cast<char>(0x07);
    *std::next(bytes.begin(), 5) = static_cast<char>(0x00);
    *std::next(bytes.begin(), 6) = static_cast<char>(0x00);
    *std::next(bytes.begin(), 7) = static_cast<char>(0x00);
```
（`#ifdef _WIN32` / `#elif defined(__APPLE__)` / `#else` 三段。）

- [ ] **Step 2: `Loader.ProbeImportsVasePod` 三分支 + basename 归一（B7）**

`:281-308` 的 `#else` 前后加 `#elif defined(__APPLE__)` 支，期望名取**实测形状**：
```cpp
#elif defined(__APPLE__)
    // CMake 的 MACOSX_RPATH 默认为开，install_name 是 @rpath/libVasePod.dylib——
    // 与 Windows 的裸名、Linux 的 libVasePod.so 都不同。比对按 basename（D176）。
    const std::string expected = "libVasePod.dylib";
    const std::string found = [&]
    {
        for (const auto& n : names.Value())
        {
            if (std::filesystem::path(n).filename().string() == expected)
            {
                return expected;
            }
        }
        return std::string{};
    }();
```
同时把 Linux 支也改成同形状（`std::filesystem::path(n).filename()`），三平台一条判据。

- [ ] **Step 3: 文案站点扫尾**

按 spec §4 的 **C 类清单**逐处把「Linux」改成「POSIX（Linux/macOS）」：
`Include/Vase/Host/Evidence.h:54`、`Include/Vase/Host/Loader.h:4-6,46-48`、`Source/Host/Loader.cpp:79`、`Source/Host/LoaderPosix.cpp:53,70`、`Source/Host/PluginHost.cpp:231,1032`、`Tools/VaseConsole/Console.cpp:862,867,878`、`Tests/Unit/LoaderTests.cpp:327-328`、`Tests/HotSwap/EjectTests.cpp:53`、`Tests/HotSwap/HotSwapLoopTests.cpp:145-146`、`Tests/HotSwap/AdoptTests.cpp:361`、`Tests/Abi/ImportEnforcementTests.cpp:29`（补 macOS 的 `@rpath/` 拼法）、`Tests/Abi/fixtures/BadLinkSibling{A,B}.cpp:7`、`Tests/HotSwap/fixtures/CMakeLists.txt:27`、`Tests/Abi/fixtures/CMakeLists.txt:7`、`Tests/CMakeLists.txt:114-118`。

> `Tests/Unit/LibraryFileNameTests.cpp` **不在本步**——它的三分支是 Task 6 的 B 类处置，已在那里做完。

同时在 `Tests/HotSwap/HotSwapLoopTests.cpp` 的 `InstallPrime` 一带补一句 macOS 理由：

```cpp
    // rename + copy 而不是 in-place 覆盖：macOS 上截断一个**仍被映射**的镜像会让旧映射的页失效，
    // 进程再触碰就是 SIGBUS。M4 的序列天然避开——**不许把它优化回 in-place**。
```

- [ ] **Step 4: 八线全绿**

Run（Windows + Linux + macOS 各跑）：
```bash
Scripts/win-verify.cmd
wsl -d Ubuntu -- bash -lc 'bash /mnt/d/Git/Vase/Scripts/linux-verify.sh'
ssh -o BatchMode=yes moozen-macos 'bash ~/WindowsGit/Vase/Scripts/macos-verify.sh'
```
Expected: 八条线全绿；四条 debug 线同值、release 各比 debug 少 1（T3 的 `NDEBUG` death test）；平台差 0。

- [ ] **Step 5: 差集复测**

Run：对八条线各取 `ctest -N` 的用例名清单，两两比对。
Expected：`linux − win`、`macos − win`、`win − macos` **三个方向都为空**；`debug − release` 恰为 `EffectScopeDeath.CreateAfterDisposeTerminates` 一条。

- [ ] **Step 6: Commit**

```bash
git add Tests/ Tools/ Include/ Source/
git commit -m "macOS 腿 T9：测试与文案三方化——八线全绿"
```

---

## Task 10: 收口验证（八线 + tidy 四线 + format）

**Files:** 无（只跑门禁；发现问题的修法回到对应 task）

- [ ] **Step 1: 八线删树重配全量**

Run：`Scripts/win-verify.cmd`（四线）+ `linux-verify.sh`（两线）+ `macos-verify.sh`（两线），各跑 `configure → build → ctest → ctest -N`。
Expected: 八线全绿、构建零警告。**退出码非 0 时先看是不是缺产物**——`z-applocal` 文件锁假红在本仓库撞过多次（`CLAUDE.md` 有记录），协议是单线删树重跑一次再判。

- [ ] **Step 2: tidy 四线**

Run:
```bash
Scripts/win-clang-tidy.cmd            # Windows 两条 debug 线
wsl -d Ubuntu -- bash -lc 'bash /mnt/d/Git/Vase/Scripts/linux-clang-tidy.sh'
ssh -o BatchMode=yes moozen-macos 'bash ~/WindowsGit/Vase/Scripts/macos-clang-tidy.sh'
```
Expected: 四条线**三判据全过**（退出 0 + 正文 `error:` 0 + 正文 `warning:` 0），再连摘要行一起读。
macOS 线是**首次**跑，出现正文 warning 就代码级修掉（首跑非双 0 是可接受的，收口必须双 0）。

- [ ] **Step 3: format**

Run:
```bash
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' \
  | xargs -0 clang-format --dry-run --Werror
```
Expected: RC=0、零 violation。macOS 侧同一份命令用 `clang-format-mp-23` 复核一次（同 23.1.x，结果应逐字相同）。

- [ ] **Step 4: 基数与旧口径落账**

把八线的 `Total Tests`、tidy 四线的 TU 数/抑制合计，与 spec §5 的形态对一遍，**为 Task 11 备好数字**。

- [ ] **Step 5: Commit（若有修正）**

```bash
git add -A
git commit -m "macOS 腿 T10：收口验证（八线全绿 + tidy 四线 + format）"
```

---

## Task 11: 文书

**Files:** 见下逐条（spec §7 是权威清单）

- [ ] **Step 1: `CLAUDE.md`**

- **技术前提**的目标平台行：`macOS arm64` → `macOS x64`，并注明 arm64 由需求方 2026-10-01 裁定移出。
- **构建与测试**：preset 六条 → 八条；`ctest -N` 基数表加两行；「六线各净 +N」类叙述改「八线各净 +N」；命令块加 macOS 段（**不写主机名**，脚注：可从开发机经 ssh 触发，主机名见本机 ssh config）；`Scripts/` 清单加两个脚本；M0–M6 的历史基数叙述只加新口径段、不重写。
- **工具链 flag 是承重的**：加一条 macOS——**无对应 flag**，链接器必写 `LC_UUID`，承重性由 `-Wl,-no_uuid` 负例反向把守。
- **项目状态**：本波段落 + macOS 腿从「M5 记名欠账五件」里划掉（余四件原位）；D138 销账。
- **目录布局**：`Cmake/Toolchains/` 三件 → 四件；`Cmake/Triplets/` 一件 → 两件；`Source/Host/` 增 `ImageInspectMachO.cpp`（**无条件编译**）/ `ImageInspectDarwin.cpp` / `Detail/ImageBytes.h`。
- LLVM 口径改「同为 23.1.x」。

- [ ] **Step 2: `README.md` 环境前提**

加 macOS 段：MacPorts `clang++-mp-23`（不用 Apple clang）、**`pkgconf`**、`VCPKG_ROOT` 住 `~/.zshenv`、`clang-format-mp-23` / `run-clang-tidy-mp-23`、`find_program` 缓存陷阱同 Linux 结论。

- [ ] **Step 3: 技能**

- `references/portability.md`：`:7` 平台表（arm64 → x64）；`:10` 「macOS 的代码路径与构建都还没落地」改准；`:77` neverUnload 条目改成**带形态限定**的实测结论；`:188` 的 `_dyld_image_count` 行落实为代码事实。
- `references/abi-boundary.md`：`:129` 的 macOS `LC_UUID` 行补「无需 flag」；`:131` 「写在三个工具链文件里」→「Windows 两条与 Linux 有 flag，macOS 无」。

- [ ] **Step 4: `wiki/vase-architecture.md`**

§8.5 平台矩阵 macOS 架构改 x64；§12.3 M5 行的平台腿标「已落（x64）」并注明 arm64 永久移出。

- [ ] **Step 5: 历史文档挂勘误注（不重写）**

`docs/superpowers/specs/2026-09-14-vase-m0-m1-design.md` 的 D2 处加：「2026-10-01 由需求方改为 x64，见 macOS 腿 spec D157」。M1 plan 与 M4 spec 的 arm64 口径同法。

- [ ] **Step 6: 头文件注释**

`Include/Vase/Catalog/LibraryFileName.h`（D138 改口径）、`Include/Vase/Detail/ImageInspect.h`（`kMachOUuid` 注释、`ImageFormat` 引入、**三解析器「返回文件原文、不做路径归一」契约**）、`Source/Host/ImageInspectPlatform.h`（`PlatformImageFormat()` 的就地理由）。

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "macOS 腿 T11：文书落账（CLAUDE.md/README/技能/wiki/勘误注）"
```

---

## 偏离登记（执行期 2026-10-02）

按仓库惯例登记执行期与计划文本的偏离——**不回填计划原文**，本节即落账位；依据均见 progress.md 各 Ruling 行与对应 task 报告。

1. **T2 Step 4：cl.exe 线的穷举 switch 守卫为空。** 计划预期「C4062 或 clang 同类」；实测 C4062 默认关闭且 `/W4` 不激活它，cl.exe 线（两条）拿不到这枚绊线——守卫实证仅 clang-cl 两支与 Linux 两支有效（当时 4/6 线，macOS 腿入场后 **6/8 线**）。计划那句是**或**支，clang-cl 线实测 `-Werror,-Wswitch` 硬失败即达成。**代价**：msvc 线上未来的漏 case 不会红；未动 `VaseBuildOptions` 加 `/we4062`——工具链面变更超出计划范围、是规矩 1 的地盘，留需求方裁。
2. **T3 评审修复：计划参考码的 Mach-O `nameOffset` 读取有越界路径。** `cmdSize∈[8,11]` 的坏尾条目会在 cmdSize 下限检查之前先读 4 字节。修复轮补 `cmdSize < kDylibNameOffset+4 ⇒ skip` 一行守卫（同「坏条目跳过」的既有语义）——spec 与 `ImageBytes.h` 自宣的「绝不越界读」纪律**胜逐字转录**。被钉文案与断言零变动。
3. **T4（R-P2）：FirstUnresolvableImport 的 kMachO 证人计划文本漏了。** spec D173 明写 `MachODylibNamesListed` 含「`FirstUnresolvableImport` 入口」，计划的 T3/T4 步骤未列。裁定在既有用例内追加两行断言落地——用例数不变。**代价**：漏掉则 D176 的「返回原文」契约在八线上少一枚跨线证人。
4. **T6（R-P11）：计划普查漏 `LibraryFileNameTests` 的跨平台互斥探针。** `RejectsForeignAndMalformedNames` 的 `.dll` vs `.so`「恰一命中」探针在 `.dylib` 支落地后于 macOS 上双 false ⇒ 必红；spec §4 的 B/C/A 清单（B8 只点名 `:33-37`）均无此站点。裁定三分支化：每平台「自家名 EXPECT_TRUE + 一枚异名 EXPECT_FALSE」——比旧探针更强，用例数与计数零变动（体内 `#ifdef`）。
5. **T8：macOS 工具链实证偏离两笔（计划文本外）。** ① MacPorts 的 `clang++-mp-23` 是 wrapper 而非符号链接 ⇒ 工具链「解析真实目录」一步空转，而 `/opt/local/bin` 没有裸名 `clang-scan-deps` ⇒ CMake 把字符串 `NOTFOUND` 塞进 CXX 扫描 try-compile，exit 127、伪装成「Could NOT find Threads」。修复：`find_program(clang-scan-deps-mp-23 clang-scan-deps)` + CACHE 钉链 + 缺位 `FATAL_ERROR`。② 连带发现目录重选块的潜伏雷：`port select` 漂版本时 `clang++` 与 `clang++-mp-23` 同存一目录，原支序（`clang++` 先）会静默降级钉版——两支序反转钉回 `-mp-23` 优先（D158 胜计划文本）。**事实校正**：首跑时裸 `clang++` 本机并不存在，缺陷为**潜伏形**而非已触发；修它的依据不变。**代价**：删两棵 mac 树重配全验一轮。
6. **T8（T7-1）：`Normalized()` 的 6 行注释超上限。** 逐字落进计划的注释违反计划自己的 Global Constraints 注释上限——M4 已知缺陷类（「计划里的注释要先过上限」）再现。首验编译通过之后压缩为 ≤3 行 + D164 指针；执行位点定在 T8（不在本机对「任何本地编译器都不编的文件」做不可编译验证的转写改动，保 816379d 的逐字节锚点）。
7. **T9（R-P1/R-P8/R-P10）：控制面裁定并入三站。** ① `AdoptTests` 的错误指路断言需三分支（spec §3/D168：macOS 指 `-no_uuid`）——计划 T9 步骤未列，R-P1 并入；② `NoIdentityPlugin.cpp:5` 文案系 spec C 类清单漏列（R-P8）；③ SIGBUS 理由注释的落点——计划钉在 `InstallPrime`，但该函数实为 in-place `copy_file`，注释会与代码矛盾；R-P10 改落 rename+copy 序列实址（`AdoptTests` 的 ProbeSwapGuard/RenameReplacement 族）。
8. **T9 B9：计划提示串漏 `lib` 前缀。** 期望 `@rpath/BadLinkSiblingB.dylib`，otool 实测磁盘实形 **`@rpath/libBadLinkSiblingB.dylib`**。按磁盘为准入账（仓库惯例）。
9. **T10：脚本契约破例。** `Scripts/macos-clang-tidy.sh` 追加 `-clang-tidy-binary`——计划「日志/三判据/摘要只三处照抄」不成立：MacPorts 无裸名 `clang-tidy`，`run-clang-tidy` 按裸名找二进制，首跑 0 秒终止、RC=1 而正文双 0 即此缺口的指纹。三判据与日志契约不动，新增「缺 binary → rc=2 带名拒绝」的更响失败面。该波首跑合计 21 条正文 warning 全部代码级出路，两枚新抑制各带就地理由（canonical 位点 4→6，见 `CLAUDE.md`）。
10. **计数笔误与纯环境项。** 计划「与既有五条一致」系计数有误——磁盘 build/test preset 各六条（R-P4），按形状各加两条成八。`nlohmann-json` 下载撞 curl HTTP2 err16：以 Windows 侧 vcpkg `downloads` scp 种子 tarball 解决——**环境一手，零仓库动作**（README 环境面候选，未随本波入库）。

---

## Self-Review 记录

**Spec 覆盖**（逐节回核）：

| spec 节 | 落在 |
|---|---|
| §1 D157/D158 | T8 Step 2/3 |
| §1 D159 | T8 Step 2（无 flag + 负例把守） |
| §1 D160 | T8 Step 1 |
| §1 D178 | T8 Step 4 |
| §1 D179 | T11 Step 1 |
| §2 D161 | T1 + T3 Step 5（无条件编译） |
| §2 D162 | T7 Step 1/2 |
| §2 D163/D175 | T2 |
| §2 D164 | T7 Step 3（`Normalized`） |
| §2 D166 | T6 |
| §2 D174 | T4 |
| §2 D176 | T5 |
| §3 D165 | T4 Step 4（两处）+ T4 Step 4 末（注释） |
| §3 D177 | T9 Step 1 |
| §3 D170 | 无代码改动（spec 明写）；T11 的文书里核一次文案 |
| §4 D167 | T8 Step 6 |
| §4 D168 | T9 Step 1/2/3 |
| §4 D173 | T3 Step 1/6 |
| §5 D169/D172 | T9 Step 4/5 + T10 |
| §6 风险 1–7 | T11 Step 3（portability.md）、T3 Step 3（就地注释）、T9 Step 3（SIGBUS 注释）、T10 Step 2（风险 7 的早期摸清） |
| §7 文书表 | T11 逐条 |

**类型一致性**：`ImageFormat` / `PlatformImageFormat()` / `ParseImportedLibraryNames()` / `ParseMachOUuidFile` / `ParseMachODylibNamesFile` / `Normalized` / `IdentityKindText` 在全文只有一种拼写；`FirstUnresolvableImport` 的第二个参数从 `bool isPe` 改成 `ImageFormat` 后，T4 Step 5 同步改了两处调用点。

**已知的非本计划范围**：spec §0 的不做表逐条不在本计划里出现（arm64 / VasePack / CI / deployment target / iOS `.a` / doctor ④ 改造 / shim）。
