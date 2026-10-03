# Vase macOS x64 平台腿（构建接入 + Mach-O 档二/档三）（spec）

**日期**：2026-10-01 · **状态**：brainstorming 定稿（spike 先置核查 + 三问三裁 + 设计四节圈定）→ **grilling 三轮收敛（12 问全部裁可，Q4 在第三轮被自己的结论推翻并改口径，见裁定注）** · **实施计划**：spec 复审通过后由 writing-plans 另立 · **分支**：`macos-x64`

**上游权威**：`wiki/vase-architecture.md` v3 的 §8.2（档二/档三平台分歧与身份特征）、§8.5（工具链与 STL 矩阵）、§8.7（导入表执法共用一条解析路径）、§10（目录布局）、§12.3（M5 行的平台腿）；`docs/superpowers/specs/2026-09-14-vase-m0-m1-design.md` 的 D2（macOS arm64 到 M5）；
M4 spec 的 D104（macOS 腿顺延 M5）；M5 spec 的 D138（`LibraryFileName` 的 macOS 分支本波不写）与 §9 记名的「M5 后续波次」；M6 spec 的「macOS 腿与 `.dylib` 分支仍归 macOS 那一波；doctor 第④项在 Apple 平台形态同 Linux，届时随平台腿一并点亮」。
历史决定 D1–D155 不改史，本文决定从 **D156** 续号。

**本文的地位**：兑现 §12.3 里挂了五波的那条平台腿（M5 记名欠账五件之一），并把 D138 那笔欠账一次还清。**本波有公开面增量**：`IdentityKind` 追加一个枚举值、新增公开类型 `ImageFormat`、`ImageInspect.h` 去掉一处默认值并增两个纯函数声明、一处公开函数签名换形；`kHeaderVersion` **不动**（理由见 D163）。凡本文与磁盘代码冲突处以磁盘为准（仓库惯例）；本文与架构文档冲突处以本文为准并回头挂勘误（§7 清单）。

> **[事实取证注 · 实测（2026-10-01，MacPorts clang 23.1.2，macOS 14.8.9 x86_64）]** 本波的形状是**先跑探针问出来的**，不是推的。探针是一次性材料，跑完即删（当日已从本仓库与 Mac 两侧清掉）——**下面逐条摘的输出原文因此是这些结论的唯一记录**；落地时真构建本身是更好的探针，不再重造这一份。
>
> ① **编译器闸直接过，根 `CMakeLists.txt:22-37` 一行不用改**。`cmake -DCMAKE_CXX_COMPILER=/opt/local/bin/clang++-mp-23` 报 `VASE_ID=Clang`、`VASE_VER=23.1.2`、`VASE_SYS=Darwin x86_64`；预定义宏里 `__apple_build_version__` **不存在**（只有 `__clang_major__ 23`），所以 ID 是 `Clang` 而不是 `AppleClang`。doctor 认 `Clang` + `^23\.`，正合（D158）。
> ② **dyld4 neverUnload 在插件形态上不成立**。四档插件（无全局对象 / 带静态析构 / 无 UUID / 链了带静态析构的桩）`dlclose` 后**全部从清单消失**：`_dyld_image_count=45 → 44`，链依赖那档 `46 → 44`（两枚镜像一起走），且 `[匹配] 两种形态都没命中`。带静态析构那档的原文顺序（stdout 已置无缓冲，见 `macho_probe.cpp` 的 `setvbuf`）：
> ```
>   装载前 _dyld_image_count=44, 命中=-1
> PROBE: 全局对象构造
>   dlopen ok; _dyld_image_count=45, 命中=44
>   dlsym(VaseProbeMarker)() = dtor1
>   内存侧身份: 4a3ef7f73c903de0bf1577a6add800a6
> PROBE: 全局对象析构（static terminator 跑了）
>   dlclose 后 _dyld_image_count=44, 命中=-1
> ```
> 即：`references/portability.md:77` 登记的「static terminators 可能让 `dlclose` 返回 0 而镜像永在」**在本仓库关心的插件形态上没有出现**。范围限定见 D171。
> ③ **M4 的统一换件序列在 macOS 上拿到新字节**。`rename(live → live.old)` + `copy(fresh → live)` 之后第二回合 `dlopen` 的 `marker=v2`、`内存侧身份: 7823…`（= fresh 的文件身份）；卸干净前的对照见 ④。
> ④ **档三在 macOS 上有判据力**（本条是本波最关键的证伪）。镜像**驻留**时换掉磁盘字节：
> ```
>   驻留时·磁盘身份: 0aa09368675c32dbb0e758d05c65e040
>   驻留时·内存身份: 0aa09368675c32dbb0e758d05c65e040
>   ...换件...
>   换后·磁盘身份: 7823d5d0accf335780c9ed3f72086f1e
>   换后·内存身份: 0aa09368675c32dbb0e758d05c65e040   <- 与磁盘不等即「没真卸」被看见
>   --- 仍驻留时再 dlopen 一次（refcount++）---
>   dlopen=ok marker=v1  <- 期望仍是旧字节
>   --- 卸干净后再 dlopen ---
>   dlopen=ok marker=v2  <- 期望是新字节
> ```
> ⑤ **身份特征齐活**。每个 `-dynamiclib` 产物默认带 `LC_UUID`（`ncmds=15`，链接器必写）；`-Wl,-no_uuid` 能摘掉（`plug_nouuid.dylib` 的 dump 里没有 `LC_UUID` 行），**档三负例可造**（D159/D167）。内存侧与文件侧 LC_UUID 相等 → 两侧比对成立。`LC_LOAD_DYLIB` 可枚举（`/usr/lib/libc++.1.dylib`、`/usr/lib/libSystem.B.dylib`、以及插件自己的 `libVaseProbePod.dylib`）→ 缺依赖诊断（§8.7）可做。
> ⑥ **dyld 记的镜像名是规范化路径**。传 `/tmp/vp/out/plug_v1.dylib` 回去全等匹配落空，拿 `/private/tmp/vp/out/plug_v1.dylib` 才命中（`[匹配] 规范化路径命中 index=44`）。实现必须先把路径规范化（D164）。
> ⑦ **同一份源码重复构建，`LC_UUID` 是确定的**。两轮探针里 `plug_v1.dylib` 都是 `0aa09368675c32dbb0e758d05c65e040`。→ 「重编同样的代码」这种换件，身份比对抓不到；能抓到的只有真换了字节（M4 的阶梯正是后者）。
> ⑧ **vcpkg 在 macOS 上原生两处都不合用**：`triplets/community/x64-osx.cmake` 是 `VCPKG_LIBRARY_LINKAGE static`；`scripts/toolchains/osx.cmake`（73 行）`grep -i compiler` **零命中**——原生构建时编译器留给 CMake 平台默认值，也就是 Apple clang，gtest 会被它编掉。故自定义 overlay triplet 是必需而非选择（D160）。
> ⑨ **工具链与 vcpkg 就位**：`cmake 4.4.3` / `ninja 1.13.2` / `VCPKG_ROOT=/Users/moozen/Git/vcpkg` / `clang-format-mp-23`、`clang-tidy-mp-23`、`run-clang-tidy-mp-23` 均在 `/opt/local/bin`（D169/D172）。
> ⑩ **Mac 上的仓库就是这棵**：`~/WindowsGit/Vase` 与 `D:\Git\Vase` 是同一份工作树，分支已是 `macos-x64`——无同步步骤。macOS 的各条线**由本任务经 `ssh -o BatchMode=yes moozen-macos` 自己跑**（代理化调用必带 `BatchMode`——无 TTY 时它失败而不是挂住，理由见 D179），不由需求方代跑。主机别名写在这里是**现场记录**，与 D179 定的「随仓库分发的文档不写别名」不冲突。
> ⑪ **自定义 triplet 端到端验证通过**（grilling 期补测）。gtest 自己的 CMakeCache 是权威证据：
> ```
> CMAKE_CXX_COMPILER:STRING=/opt/local/bin/clang++-mp-23
> CMAKE_C_COMPILER:STRING  =/opt/local/bin/clang-mp-23
> CMAKE_OSX_ARCHITECTURES  =x86_64
> ```
> 产物是 `libgtest.1.18.0.dylib`（**dynamic 生效**，不是 `.a`）、带 `LC_UUID`，且链接上的探针测试 `[  PASSED  ] 1 test.`。**D160 的机制不是推测，是跑出来的。**
> ⑫ **`install_name` 不是裸名**。CMake 产物实测 `LC_ID_DYLIB = @rpath/libVasePod.dylib`（policy 默认 `MACOSX_RPATH` 开）——与 Windows 的裸名 `VasePod.dll`、Linux 的 `libVasePod.so` **都不同**。这直接决定 D176，并改准 §4 表里那条期望值。
> ⑬ **`pkg-config` 是 macOS 独有的环境缺口**（grilling 期撞出来的）。实测缺它时 vcpkg 的 gtest port 编到最后一格倒在 `ports/gtest/portfile.cmake:49 (vcpkg_fixup_pkgconfig)` → `Could not find pkg-config`；装上 MacPorts `pkgconf` 3.0.7 后 configure/build 双双 RC=0。Windows/Linux 两条线今天能跑，说明那边本来就有（D178）。
> ⑭ **Apple clang 是 16.0.0，且与我们的 libc++ 头树不同源**：`/usr/bin/clang++` 报 `Apple clang version 16.0.0`、头来自 `…/MacOSX.sdk/usr/include/c++/v1`；`clang++-mp-23` 报 `23.1.2`、头来自 `/opt/local/libexec/llvm-23/include/c++/v1`（MacPorts 自带那份，`/opt/local/include/c++/v1` **不存在**）。这是 D160 里「必须钉编译器」的事实依据。
> ⑮ **`ThirdParty/cli` 本来就有 Apple 分支**：`include/cli/detail/platform.h:37` 与 `detail/rang.h:8` 都有 `#elif defined(__APPLE__) || defined(__MACH__)`。不是新风险，登记一笔免得重复排查。

> **[裁定注 · brainstorming（2026-10-01）]** 逐条落点：
> **Q1 · 本波形状（D156）**：spike 先置核查已做完（取证注①–⑩），四条未知全部有利，**采全量一波**——构建 + 平台代码 + 测试 + 文书一次做完，收口八线全绿。
> **Q2 · Mach-O 解析落点（D161）**：**拆出共用字节助手**。`ImageInspectCommon.cpp` 现 672 行装着 PE + ELF 两套；第三个解析器不复制那 ~90 行边界检查，改把 `InBounds`/`ReadU8..U64`/`ReadNulTerminated` 提入内部头，PE/ELF 一并改用。代价是动一次既有文件（纯提取，无语义变化）。
> **Q3 · 范围之外的**：`Tests/CMakeLists.txt` 的 `NoIdentityPlugin` 三平台化、`LibraryFileName` 的 `.dylib` 分支、`doctor` ④ 的 macOS 形态**全部并入本波**（原分别记在 M4 spec D109 / M5 spec D138 / M6 spec 的平台腿行）。

> **[裁定注 · grilling 第一轮（十二问里前八问 + 补遗，全部裁可）]** 逐条落点：
> **Q1（D161 补）**：`ImageInspectMachO.cpp` **无条件**进 VaseHost——它是 D173 的承重前提，放进 `APPLE` 分支会让八线合成用例链接失败。
> **Q2（D165 修正）**：`Loader.cpp` 有**三处** `#ifdef`，原稿的「不动」只对第一处成立；`:104-130` 两处必须三分支。**不改的后果远重于原判**：`PluginHost.cpp:1093-1097` 对 `ImportedLibraryNamesFromFile` 的 Err **直接 Refusal**，所以 macOS 上不是「含兄弟导入的被拒」，而是**每一次 Adopt 都拒**。
> **Q3（D160 修正）**：原稿的头号理由（community 那份是 static ⇒ 与 8.3 冲突）**不成立**——`VCPKG_LIBRARY_LINKAGE` 只影响 gtest 一个测试库，它静态链进 `VaseTests`，不跨我们的模块边界。真理由是**钉编译器**（取证注⑭：Apple clang 16.0.0 + SDK 头树 vs MacPorts 23.1.2 + 自带头树）。
> **Q4（D176 前身，第三轮被推翻）**：原定「在 Mach-O 解析层剥掉 `@rpath/`」。
> **Q5（D157 钉死）**：arm64 **永久移出**，将来若要是**新增**一套 preset+triplet，不是改这一套。
> **Q6（D171 扩）**：历史文档里的 arm64 口径**挂勘误注**，不重写。
> **Q7（D174）**：`FirstUnresolvableImport` 的 `bool isPe` 换 `ImageFormat` 枚举。
> **Q8（D175）**：`IdentityKind` 去掉默认值。
> **Q12（D178）**：`pkg-config` 写进 macOS 环境前提；**明确否掉「仓库出 shim」**——那会把真实环境依赖伪装成已满足，`.pc` 会写成空壳。

> **[裁定注 · grilling 第二、三轮（Q9/Q10/Q11/Q13，全部裁可；Q13 修正 Q4）]** 逐条落点：
> **Q9（D177）**：`FlipMachineField` 的 macOS 支翻 **`cputype`**。机理澄清（原普查描述有偏差，按磁盘原文更正）：该函数的调用者是 `VaseCliDoctorTests.cpp:163` 的 `UnloadableBinaryIsNamedByCheckTwo`，它翻的是**装载器认得的字段**（Windows COFF `Machine` / ELF `e_machine`），让平台装载器拒收——而 `ParseElfProgramHeaders` **根本不读 `e_machine`**，所以被篡改的二进制 ① 照常 `identity ok`、只有 ② 红。这层机理必须写进注释。
> **Q10（D174 落点）**：`PlatformImageFormat()` 住 `ImageInspectCommon.cpp`（全平台都编，与三个解析器同处一地），声明进 `Source/Host/ImageInspectPlatform.h`——收益是 `Loader.cpp` 的平台分支从三处降到**一处**。
> **Q11（D179）**：`CLAUDE.md` 的 macOS 命令块**不硬编码主机名**（与 D6 同取向），只在脚注里写明「可从开发机经 ssh 触发，主机名见本机 ssh config」。
> **Q13（D176）**：`@rpath/` 归一到**比对点**（basename），不放在解析层——**这条修的是通用形态，不是 macOS 专属**：ELF 的 `DT_NEEDED` 同样允许带路径（`ParseElfNeededFile` 原样取出），所以「导入名带路径 ⇒ §8.7 执法静默不咬」在 Linux 上今天就潜伏着，只是没人写过带路径的 `DT_NEEDED`。放解析层只堵 Mach-O 一处，放比对点一次堵三处。

---

## 0. 范围

**本波做**：

1. **构建系统（§1，D157–D160/D178/D179）**：平台工具链文件、自定义 overlay triplet、两个 preset、两个验证脚本，以及 macOS 的环境前提记账。
2. **平台代码三方化与 Mach-O 侧（§2，D161–D164/D166/D174/D175/D176）**：`Source/Host/CMakeLists.txt` 三分支；新增 Mach-O 解析（**无条件编译**）与 Darwin 平台实现；`ImageInspectPosix.cpp` 改名；`LibraryFileName.h` 的 `.dylib` 分支；`ImageFormat` 枚举取代 `bool isPe`；`@rpath` 在比对点归一。
3. **档二/档三在 macOS 的形态（§3，D165/D170/D177）**：`Loader.cpp` 三处 `#ifdef` 逐个定性、`_dyld_*` 映射清单作为档二判据、`LC_UUID` 作为档三身份特征、`-Wl,-no_uuid` 负例、`FlipMachineField` 的 macOS 支。
4. **测试三方化（§4，D167/D168/D173）**：平台条件编译的完整清单处置、`NoIdentityPlugin` 的 macOS 支、档三两条由双平台转三平台、Mach-O 解析器的合成字节用例（八线同幅）。
5. **验证口径（§5，D169/D172）**：八线 + tidy 四线 + format。
6. **文书义务（§7，D171/D172/D178/D179）**：`CLAUDE.md` 两张表与各处口径、`README` 环境前提、技能 `references/{portability,abi-boundary}.md`、`LibraryFileName.h` 的 D138 注释、历史文档的 arm64 勘误注；**以及「目标平台」这条需求方指定前提的改口**（D157）。

**本波不做**（各归其位，不许顺手）：

| 项 | 去向 |
|---|---|
| **macOS arm64 / Android / iOS** | 需求方 2026-10-01 裁：**macOS 只做 x64**，arm64 **永久移出**（D157）。移动端两平台与 M0–M6 口径一致，仍不在列 |
| `VasePack` 与 C4251 还债 | 仍按 M5 记名欠账原位；本波不动 `.clang-tidy` 的 `/wd4251` 口径 |
| CMake 后置步骤（D125） | 原位；`Scripts/macos-verify.sh` 是手跑脚本，不是构建后钩子 |
| CI | 仍无（M5 记名欠账原位）。本波不引入 |
| `-mmacosx-version-min` / 部署目标策略 | **不做**：本机只有一个 SDK，钉一个没有消费者的下限是猜。记账一行，等真有分发需求（VasePack）那一波 |
| iOS 的 `.a` 形态、通用二进制（universal binary） | 不做；`LibraryFileName` 只加 `.dylib` 一支 |
| doctor ④ 的 macOS 支改造 | **不改代码**：macOS 与 Linux 同属「不可探测」（覆盖是 truncate-in-place），既有的 `#else` 文案已正确（D170） |
| 其它 macOS 专属负例（如签名/公证、`@rpath` 改写） | 不做；本波负例只有 `-Wl,-no_uuid` 一条 |
| 「重编同样的代码」这类换件的身份判据 | **结构上不可能**（取证注⑦）——`LC_UUID` 对同源同参的构建是确定的。不为此加机制，记入 §6 风险 |
| 在仓库里为 `pkg-config` 出 shim | **明确不做**（D178）：那会把真实环境依赖伪装成已满足 |

---

## 1. 构建系统（D157–D160、D178、D179）

### D157 目标平台改口：macOS **x64**，arm64 永久移出

需求方 2026-10-01 裁：macOS 腿只做 x64，不做 arm64。据此：

- preset 名 `macos-x64-clang-{debug,release}`；构建树 `build-macos/<presetName>/`；
- `CLAUDE.md`「技术前提」的目标平台行、`wiki/vase-architecture.md` §8.5 的矩阵、技能 `references/portability.md:7` 的平台表**一律改准**（这是本波唯一一处需求方指定前提的变更，要在 §7 逐处点名）；
- `CMAKE_OSX_ARCHITECTURES` **显式**写 `x86_64`——「凡影响产物形态的默认值一律显式写出」；本机恰是 x86_64，所以这条不能靠「反正默认就对」。
- **将来若要 arm64**：是**新增**一套 `macos-arm64-clang-{debug,release}` preset + 一份 `arm64-osx-libcxx.cmake` triplet，**不是**改这一套（把将来的改动形状先定下来，省得下次重新讨论）。

### D158 编译器：MacPorts `clang++-mp-23`，根 doctor 零改动

- 工具链文件用 `find_program(... NAMES clang++-mp-23 clang++)`，找到后**只规范化目录、名字保留 `++`**——与 `linux-x64-clang-libcxx.cmake:34` 的理由逐字同构（驱动按 `argv[0]` 里有没有 `++` 判 C++ 模式）。`clang++-mp-23` **排在前面**：MacPorts 的 `clang++` 会随默认版本漂移，钉住 23 才与版本闸一致；真漂到别处时由 doctor 响亮拒绝，不静默降级。
- 版本闸**不动**：取证注① 实测 ID 为 `Clang`、版本 `23.1.2`，`CMakeLists.txt:22-28` 的 `Clang` + `^23\.` 正合。**本条要写进注释**：后来人看到「要求 23.x」会以为 macOS 上要放宽，实际不用——MacPorts 的 clang 不是 AppleClang。
- `find_program` 结果进缓存的陷阱与 Linux 侧同（`README` 已记），macOS 段照抄该结论并指向同一处。
- 路径前提：`/opt/local/bin` 须在 PATH 里（实测裸 ssh 会话里已有）。

### D159 身份特征：`LC_UUID`，**不追加 linker flag**

取证注⑤ 实测链接器必写 `LC_UUID`。于是本平台**没有**对位于 Windows `/DEBUG:FULL` 与 Linux `-Wl,--build-id=sha1` 的承重 flag。

处置：在工具链文件里**写一条注释说明这件事实及其后果**，而不是留空——`CLAUDE.md`「工具链 flag 是承重的」那节在 macOS 上因此变成一条「无对应 flag」的条目，同时说明**承重性由负例反向把守**：`NoIdentityPlugin` 的 macOS 支用 `-Wl,-no_uuid` 主动摘掉它，档三必须拒（D167）。没有这一条，「摘掉标志 → 运行期响亮失败」这条判据在 macOS 上无从适用，而 flag 表里会出现一个看起来像漏写的空格。

### D160 自定义 overlay triplet：`Cmake/Triplets/x64-osx-libcxx.cmake`

取证注⑧⑪⑭：内置与 community 两份都不合用，自定义**必需**；且端到端已验证。

- `VCPKG_TARGET_ARCHITECTURE x64`、`VCPKG_OSX_ARCHITECTURES x86_64`、`VCPKG_CMAKE_SYSTEM_NAME Darwin`；
- **头号理由 = 把编译器钉到 `clang++-mp-23`**：`scripts/toolchains/osx.cmake` 通篇不提编译器，不钉就会用 Apple clang——实测那是 **16.0.0**、且用 SDK 的 libc++ 头树，与我们 TU 用的 **23.1.2 + MacPorts 自带那份 libc++ 头树**不同源（取证注⑭）。`VCPKG_CMAKE_CONFIGURE_OPTIONS` 同时给 `CMAKE_C_COMPILER=clang-mp-23` 与 `CMAKE_CXX_COMPILER=clang++-mp-23`。
- **`VCPKG_LIBRARY_LINKAGE dynamic`**：理由**降级为「与另三条线一致」**，不是正确性。注释里要写明——`community/x64-osx.cmake` 那份是 `static`，而 **static 在 macOS 上只影响 gtest 一个测试库**（它静态链进 `VaseTests`，从不跨我们的模块边界；`nlohmann-json` 是 header-only），**不构成 8.3 的堆问题**。别让后一个人照着这条注释推出一个错误的因果。
- `VCPKG_CXX_FLAGS` **留空**：macOS 上 clang 默认即 libc++（取证注⑤ 的 `LC_LOAD_DYLIB /usr/lib/libc++.1.dylib` 是证据），不需要 Linux 侧的 `-stdlib=libc++`。同样写进注释，免得后来人按 Linux 那份补齐。
- 与 Linux 侧一样，**工具链与 triplet 重复一遍编译器知识是已知的不对称**（triplet 是独立脚本，拿不到工具链解析出的真实目录）——照抄 `x64-linux-libcxx.cmake:27-30` 那段说明，不另起一套说法。

### D178 macOS 环境前提：`pkg-config`

取证注⑬：缺它时 vcpkg 的 gtest port 无条件倒在 `vcpkg_fixup_pkgconfig`，于是 `find_package(GTest CONFIG REQUIRED)` 落空、**macOS 上什么都编不出来**。处置：

- `README` 的 macOS 段列为前提（MacPorts `pkgconf`；Homebrew `pkg-config`）；
- `Scripts/macos-verify.sh` 起手做一次存在性检查，缺了**响亮报错并指名**（不静默往下跑）；
- **不在仓库里出 shim**：那会把真实环境依赖伪装成已满足，`.pc` 写成空壳、将来任何真读 pkg-config 的依赖静默拿到错东西——正是规矩 3 那类「门禁看起来还是绿的」，只是这次绿的是别人的构建。

### D179 主机别名的记账口径：按**文献性质**分，不按「写不写」分

macOS 线从这台开发机**只能经 ssh** 触发，但主机别名是个人环境（别人的机器上多半没有）。判据不是「ssh 命令该长什么样」，而是**这份文献是给谁读的**：

| 文献 | 写不写具体别名 | 理由 |
|---|---|---|
| 随仓库分发的操作文档（`CLAUDE.md` / `README`） | **不写** | 别人 clone 到没有这个别名的机器上，那条命令对他不可执行——与 D6「clang 不写死安装路径」同一条取向。写成「在 macOS 开发机上本地跑」形态，另加脚注：**本仓库的 macOS 线通常由 macOS 开发机执行；从 Windows 开发机触发走 ssh，主机别名见本机 ssh config（不入库）**。这条脚注是 D169 验证口径成立的前提——它说明 macOS 线**可以**从开发机远程跑 |
| 本波的现场记录与执行命令（本 spec 的取证注、plan 里的 `Run:` 行） | **写具体别名** | ① 它们记录的是「这一波在这台机器上**实际怎么跑**」，写占位符就是伪造现场；② **plan 是要被执行的**，执行者（尤其 subagent）需要能直接粘贴的命令，占位符会让每一步多一次查 ssh config 的往返；③ 本仓既有 plan 本来就写着机器专属路径（`/mnt/d/Git/Vase`、`D:\Developer\…`），同惯例 |

**`-o BatchMode=yes` 单独记一条理由，不两处各抄一种拼法**——两者的实质差别只有一条：**无 TTY 时，加了它 ssh 会失败；不加它会挂住**。而「挂住」是最难查的一类失效（没有报错、没有超时提示，只是不动），所以从脚本或代理化调用时**必须**加：

- 人在终端里跑：`ssh <别名> '…'`（钥匙在 agent / keychain 里，按需提示）；
- 从脚本 / 代理化调用：`ssh -o BatchMode=yes <别名> '…'`。

本 spec 与配套 plan 里的命令都是**后一种**，因为它们就是被代理执行的。

### 工具链文件的其余接线

照 `linux-x64-clang-libcxx.cmake` 的骨架，逐项：

- `VCPKG_ROOT` 守卫（`message(FATAL_ERROR ...)`），措辞按 macOS 的 `~/.zshenv`（该机器上环境类变量住 `~/.zshenv`）；
- `VCPKG_OVERLAY_TRIPLETS` 与 `VCPKG_TARGET_TRIPLET` **都写在 `include(vcpkg.cmake)` 之前**（位置要求同 Linux 那份注释所述）；
- `CMakePresets.json` 加 hidden `macos-x64-clang` + 两个具体 preset + 对应 build/test preset，`CMAKE_EXPORT_COMPILE_COMMANDS: ON`（tidy 要用）。
- `CMakeLists.txt` 的 `else()` 支（`-Wall -Wextra -Werror` 与 `-fno-exceptions`）**天然覆盖 macOS，不改**。

### 脚本

- `Scripts/macos-verify.sh`：删树重配 → configure → build → ctest → `ctest -N`，两线，逐步打印退出码，与 `linux-verify.sh` 同契约；起手做 `pkg-config` 存在性检查（D178）。
- `Scripts/macos-clang-tidy.sh`：与 `linux-clang-tidy.sh` 同契约（日志落脚本旁、打三判据、不复制阈值），二进制名是 `run-clang-tidy-mp-23`。

---

## 2. 平台代码三方化（D161–D164、D166、D174–D176）

### D162 名与分支：`Source/Host/CMakeLists.txt` 三分支

```
if(WIN32)        → ImageInspectWindows.cpp  LoaderWindows.cpp
elseif(APPLE)    → ImageInspectDarwin.cpp   LoaderPosix.cpp
else()           → ImageInspectLinux.cpp    LoaderPosix.cpp
```

`ImageInspectPosix.cpp` **改名 `ImageInspectLinux.cpp`**：它的内容是 `<elf.h>` / `<link.h>` / `dl_iterate_phdr`（`:14-15`、`:69`），全是 Linux 专属。macOS 一进来，`Posix` 这个名字就在撒谎。

`LoaderPosix.cpp` **保留名字与内容**：`dlopen`/`dlclose`/`dlsym` 是 POSIX 通用，`PlatformReopenWritable`（`:51-66`）在 macOS 上行为相同（写开成功，与是否映射无关）。唯一改动是注释：`:53` 的「Linux 侧恒真」改成「POSIX 侧恒真（Linux 与 macOS）」。

### D161 新增 `Source/Host/ImageInspectMachO.cpp`（**无条件编译**）+ 提出 `Detail/ImageBytes.h`

**编译面是承重的**：`ImageInspectMachO.cpp` 与 `ImageInspectCommon.cpp` 一样**无条件**加进 `VaseHost`，**不进** `APPLE` 分支——它是 D173 的前提（八线都要链接 `ParseMachOUuidFile`），与 `ParseElfBuildIdFile` 今天在 Windows 上照样编译是同一个道理。只有 `ImageInspectDarwin.cpp`（用 `_dyld_*`）进 `APPLE` 分支。**这层理由要写进 `Source/Host/CMakeLists.txt` 的注释**，否则后来人会以「macOS 的解析器不该在 Windows 上编」为由把它挪走。

- 新内部头 `Source/Host/Detail/ImageBytes.h` 收纳 `InBounds` / `ReadU8` / `ReadU16` / `ReadU32` / `ReadU64` / `ReadNulTerminated`——**纯提取，无语义变化**；`ImageInspectCommon.cpp` 的 PE 与 ELF 两段一并改用。
- 新源 `ImageInspectMachO.cpp`：纯字节进、纯数据出，与另两个格式同风格——**不 `#include <mach-o/loader.h>`**（那会让它无法跨平台编译，与上一条冲突），**手写常量与字段偏移**。需要的最小面：`mach_header_64` 的 32 字节头、`load_command` 的 8 字节头、`LC_UUID`(0x1b, 24B)、`LC_ID_DYLIB`(0xd)、`LC_LOAD_DYLIB`(0xc)、`LC_LOAD_WEAK_DYLIB`(0x18)、`MH_MAGIC_64`(0xfeedfacf)。
- `Include/Vase/Detail/ImageInspect.h` 增两个声明，与 ELF 侧对位：
  - `Result<ImageIdentity> ParseMachOUuidFile(std::span<const std::uint8_t> fileBytes);`
  - `Result<std::vector<std::string>> ParseMachODylibNamesFile(std::span<const std::uint8_t> fileBytes);`
- 错误文案增一条，与 `MissingCodeViewError` / `MissingBuildIdError`（`ImageInspectCommon.cpp:80-90`）同形：`no LC_UUID load command — 插件构建用了 -Wl,-no_uuid（见 Cmake/Toolchains/macos-x64-clang-libcxx.cmake，§8.2 档三）`。
- **就地注释记风险 2**：`LC_UUID` 对同源同参构建是确定的（取证注⑦）——它不是内容哈希，别拿去当换件判据用。

### D174 `ImageFormat` 枚举取代 `bool isPe`；平台映射只留一处

`Include/Vase/Detail/ImageInspect.h:53` 的 `FirstUnresolvableImport(span, bool isPe)` **结构上装不下第三种格式**。改成：

```cpp
enum class ImageFormat : std::uint8_t { kPe, kElf, kMachO };
VASE_HOST_API std::string FirstUnresolvableImport(std::span<const std::uint8_t> fileBytes, ImageFormat format);
```

- **映射只留一处**：`PlatformImageFormat()` 实现住 `ImageInspectCommon.cpp`（全平台都编，与三个解析器同处一地），声明进 `Source/Host/ImageInspectPlatform.h`（Host 内部桥头，不进公开面、不挂导出宏）。它是 `#if defined(_WIN32) / #elif defined(__APPLE__) / #else`，与 `Source/Host/CMakeLists.txt` 的三分支一一对位。
- 收益是结构性的：**`Loader.cpp` 的平台分支从三处降到一处**（只剩 Unload 证据那段，也就是 D165 里唯一真正平台相关的部分）；`Loader.cpp:111/125` 变成平台无关的 `Parse...(bytes, PlatformImageFormat())` 形态。
- 消费者只有三处（`Loader.cpp:111/126/128` 一带、`LoaderTests.cpp:186/217`），全在同仓。
- **取枚举而非「新加一个具名函数」的理由**：枚举把「格式集合」这个知识收进 `switch` 的穷举点，让「下次再加格式忘了处置」变成**编译期硬失败**——正是 D163 里那处三目静默错要改成的形状。加具名函数则把知识留在调用方，而调用方恰好是 `#ifdef` 选出来的，等于把编译期的 CHECK 换回运行期的约定。

### D176 `@rpath/` 归一到**比对点**，三个解析器契约统一为「返回文件原文」

取证注⑫：macOS 的 `LC_LOAD_DYLIB` 记的是 `@rpath/libX.dylib`，而 `PluginHost.cpp:1101-1105` 的 §8.7 执法是拿**裸文件名**比：

```cpp
const std::string selfName = LowerAscii(absPath.filename().string());   // 裸文件名
const bool siblingHit = std::ranges::any_of(siblings, [&lowered](const std::string& sibling)
                                            { return LowerAscii(sibling) == lowered; });
```

两边对不上 ⇒ 执法**静默不咬**。

**处置：解析器不改，比对点归一。** `ParseMachODylibNamesFile` / `ParseElfNeededFile` / `ParsePeImports` 三个**一律返回文件里的原文**（契约一致、锁死在 `ImageInspect.h` 的注释里），`PluginHost` 与测试断言各自取 `path.filename()` 再比。

**为什么不在解析层剥**（这是 grilling 第三轮推翻第一轮结论的那一条）：**ELF 的 `DT_NEEDED` 同样允许带路径**——`strtab` 里存的就是任意字符串，`ParseElfNeededFile` 原样取出。所以「导入名带路径 ⇒ 执法静默不咬」**不是 macOS 专属现象，Linux 上今天就潜伏着**，只是没人写过带路径的 `DT_NEEDED`。剥在解析层只堵 Mach-O 一处、漏另两处；剥在比对点一次堵三处，顺带把 Linux 上那个潜伏形态也堵上。

附带好处：`FirstUnresolvableImport` 的缺依赖诊断会打出 `/usr/lib/libc++.1.dylib`、`@rpath/libX.dylib` 这类**完整信息**，比剥完的裸名更可用。

代价如实记：`PluginHost` 与 `Tests/Unit/LoaderTests.cpp:279` 各加一次 `filename()`。

### D163 `IdentityKind` 追加 `kMachOUuid`，**不 bump `kHeaderVersion`**

`IdentityKind`（`Include/Vase/Detail/ImageInspect.h:20-24`）现在两值，追加第三值。

论证：它**不进插件描述符契约**——`kHeaderVersion` 管的是 `PluginMeta` / `FieldInfo` 那套插件↔宿主 ABI，`grep -n "Identity\|Inspect" Include/Vase/PluginDescriptor.h` **零命中**（已核）；而 `ImageIdentity` 是 Host 的公开读取面，消费者全在同仓（`Include/Vase/Host/Loader.h`、`Tools/VaseCli/Doctor.cpp`、`Tools/VaseConsole/Console.cpp`、`Tests/Unit/LoaderTests.cpp`）。底层类型 `std::uint8_t` 不变，枚举加值不改布局。

**仓内消费者逐个处置**（`grep -rn IdentityKind` 的全部命中）：

| 站点 | 现状 | 处置 |
|---|---|---|
| `Include/Vase/Detail/ImageInspect.h:28` | 默认值 `= kElfBuildId` | **去掉默认值**（D175） |
| `Source/Host/ImageInspectCommon.cpp:244/533` | 两处赋值 | 不动；Mach-O 侧新增一处 `kMachOUuid` 赋值 |
| `Tests/Unit/LoaderTests.cpp:172/204` | 两条 `EXPECT_EQ` | 不动——它们钉的是 PE/ELF 解析器的产出 |
| **`Tools/VaseConsole/Console.cpp:989`** | **三目**：`Kind == kPdbCodeView ? "pdbCodeView" : "elfBuildId"` | **必须改三分支**。这是本波唯一一处会**静默出错**的站点：加值后 macOS 的打印会变成 `elfBuildId`，不报错、不警告，只有读输出的人看得出来——**正是规矩 1 那类「门禁看起来还是绿的」形态**。改成无 `default` 的穷举 `switch`（`Tools/VaseCli/Validate.cpp:227-241` 已有先例），让下一次加值在这里硬编译失败 |

**公开面增量如实记**：老的树外消费者若对 `IdentityKind` 设 `default` 分支不会被影响，但穷举 `switch` 会因新值而警告/失败。这是追加枚举值的固有代价，本波接受。

### D175 `IdentityKind` 去掉默认值

`ImageInspect.h:28` 的 `IdentityKind Kind = IdentityKind::kElfBuildId;` 两个生产者都显式赋值、默认值从不生效——但加上 `kMachOUuid` 后它变成一个**静默错的埋伏**：任何新增解析路径忘了赋 `Kind`，产出会被贴成 `kElfBuildId`，而档三是「Kind + Bytes 全等」比对，于是「拿 ELF 身份去比 Mach-O 身份」**恒不等**——表现为「Adopt 总是拒绝」，看不出根因。

处置：**去掉默认值**，`ImageIdentity` 不再可默认构造，强制生产者显式赋值。零运行时代价，把「忘了设 Kind」从运行期的莫名拒绝提到编译期的缺初始化。

### D164 路径匹配必须先规范化

取证注⑥ 实测：dyld 记的是**规范化路径**（`/tmp` → `/private/tmp`）。`MemoryIdentityPlatform` / `IsImageMapped` 按路径在 `_dyld_image_count` 清单里全等匹配之前，必须先把传入路径规范化。

实现用 `std::filesystem::canonical` 的 **error_code 形态**（§9.2：filesystem 一律 error_code，绝不抛），失败时退回 `weakly_canonical` 再退回原路径——**理由要写在代码里**：调用方可能在文件刚被换掉的窗口里问身份，`canonical` 会失败，而那时更该做的是「按能拿到的路径去比对并如实报找不到」，不是崩。

**这条对测试是承重的，不只是理论**：`CatalogSandbox` 把 fixture 复制进临时目录，macOS 的 `temp_directory_path()` 落在 `/var/folders/...`，而它的真身是 `/private/var/folders/...`——不规范化则**每一条走装载/档二/档三的用例都会匹配落空**。

### D166 `LibraryFileName.h` 的 `.dylib` 分支（还 D138）

`Include/Vase/Catalog/LibraryFileName.h:14-20` 从二分改三分：

```
#ifdef _WIN32            → ""  + ".dll"
#elif defined(__APPLE__) → "lib" + ".dylib"
#else                    → "lib" + ".so"
```

文件头注释里那句「macOS 的 .dylib 与 iOS 的 .a 分支归 VasePack/M5 后续波次（D138）」改成「iOS 的 .a 分支归 VasePack 那一波」——macOS 这半笔本波还清。

---

## 3. 档二 / 档三在 macOS 上的形态

### D165 `Loader.cpp` 三处 `#ifdef` 逐个定性（**只有第一处不动**）

| 处 | 现状 | 处置 |
|---|---|---|
| `:71-82` Unload 证据 | `#else` = 档二可观测 + `ReopenWritable` 无判据力 | **不动**。macOS 属 `#else`：取证注② 实测 `_dyld_image_count` 与路径枚举能可靠回答「镜像还在不在」，与 Linux 的 `dl_iterate_phdr` 同构；`PlatformReopenWritable` 在 macOS 上也恒真。**就地带一句注释**，否则后来人看到 `#else` 会以为漏了 macOS |
| `:104-116` `ImportedLibraryNamesFromFile` | `#else` 硬编码 `ParseElfNeededFile` | **必须三分支**（走 `ParseMachODylibNamesFile`，经 D174 的 `PlatformImageFormat()`） |
| `:118-130` `DescribeLoadFailure` | `#else` 硬编码 `FirstUnresolvableImport(bytes, false)` | **必须三分支**（同上） |

**后两处不改的后果远重于原判**：`PluginHost.cpp:1093-1097` 对 `ImportedLibraryNamesFromFile` 的 Err **直接 `Refusal`**——`ParseElfProgramHeaders` 见到 Mach-O 字节立刻 `Err{"not an ELF image"}`，于是 macOS 上**每一次 Adopt 都硬拒**，§8.7 执法变成无条件全拒。这不只是「含兄弟导入的被拒」。

### D177 `FlipMachineField` 的 macOS 支翻 `cputype`

`Tests/Integration/VaseCliDoctorTests.cpp:64-96` 的 `FlipMachineField` 服务于 `:163` 的 `UnloadableBinaryIsNamedByCheckTwo`（`:157-172`）。**它翻的是装载器认得的字段，不是身份特征**：

- Windows 支：改 COFF `Machine` → `IMAGE_FILE_MACHINE_I386` ⇒ `LoadLibraryExW` 报 `ERROR_BAD_EXE_FORMAT`；
- `#else` 支：改 `e_machine` → `EM_386` ⇒ ld.so 拒收。注意 `ParseElfProgramHeaders`（`ImageInspectCommon.cpp:311-318`）**只查 magic/class/data，根本不读 `e_machine`**——所以被篡改的二进制 ① 照常打 `identity ok`，只有 ② 变红。这正是那条用例断言「① 与 ② 独立」的全部内容。
- **macOS 支：改 `mach_header_64.cputype`（偏移 4，4 字节）→ `CPU_TYPE_I386`(7)** ⇒ dyld 以「不支持的架构」拒收（macOS 14 早已无 32 位）。sanity 检查从 `0x7F 'E' 'L' 'F'` 换成 `MH_MAGIC_64`(0xfeedfacf)@0。

**这层机理必须写进注释**：否则下一个人很可能「顺手改成篡改 `LC_UUID`」，那会让 ① 与 ② 同时红、把这条用例的独立性断言（`Vase.BadLoad: identity ok`）打掉。

### 顺带补一条 M4 序列在 macOS 上的理由（写进 `Tests/HotSwap/HotSwapLoopTests.cpp` 或所在处的注释）

`rename + copy` 而不是 in-place 覆盖，Linux 侧的理由是「映射着也写得开，所以覆盖是空转」；**macOS 侧多一条更硬的理由**：in-place 截断一个**仍被映射**的镜像会让旧映射的页失效，进程再触碰就是 `SIGBUS`。M4 的「改名离开 + 落新字节」天然避开（旧 inode 不被截断），**不许把它优化回 in-place**。

### 档三负例与档二证人

- 负例 `NoIdentityPlugin`：macOS 侧 `target_link_options(... "-Wl,-no_uuid")`（D167）。
- 档三两条（`Adopt.MissingIdentityFeatureRejectedWithPointer`、`Adopt.RenameReplacementCaughtByTierThree`）自 M4/D109 起已两平台同跑，本波转三平台。
- `Tests/HotSwap/AdoptTests.cpp:370` 的错误指路断言（Windows 指 `/DEBUG:FULL`、其余指 `--build-id`）要三分支 + macOS 指 `-no_uuid`（D168）。

### D170 `doctor` ④ 维持「不可探测」支，只核文案

`Tools/VaseCli/Doctor.cpp:241-271`：Windows 支走 `ProbeExclusiveOpen`，`#else` 打「not observable on this platform — a resident mapping does not prevent overwrites here」。macOS 与 Linux 同属 `#else`，**文案对 macOS 也成立**（覆盖是 truncate-in-place），故本波**不改代码**。M6 spec 预告的「随平台腿一并点亮」到此兑现为「点亮后确认无需改动」——这一句要写进 §7 的文书义务，免得下一波再问一遍。

---

## 4. 测试三方化（D167、D168、D173）

### D168 三方化口径与**完整站点清单**

口径：**能用 `#else` 共用的就共用并把文案改成「POSIX」；确需不同的才三分支**。

**B 类（`#else` 会错，必须动）**——这批是 grilling 期普查出来的完整集，**就是本波改动的验收面**，收口时按它逐条回核，不靠记忆：

| # | 站点 | 处置 |
|---|---|---|
| B1 | `Source/Host/Loader.cpp:111-115` | 三分支（D165） |
| B2 | `Source/Host/Loader.cpp:125-129` | 三分支（D165） |
| B3 | `Source/Host/CMakeLists.txt:4-12` | 三分支（D162） |
| B4 | `Include/Vase/Catalog/LibraryFileName.h:17-20` | `.dylib`（D166） |
| B5 | **`Tests/Integration/VaseCliDoctorTests.cpp:78-96`** | `FlipMachineField` 加 macOS 支（D177）——原稿漏掉的一处，doctor ①/② 的独立性证人 |
| B6 | `Tests/CMakeLists.txt:122-129` | `NoIdentityPlugin` 三分支（D167） |
| B7 | `Tests/Unit/LoaderTests.cpp:281-308` | 期望导入名三分支；**取 `path.filename()` 比对**（D176）——实测形状是 `@rpath/VasePod.dylib`，不是裸名 |
| B8 | `Tests/Unit/LibraryFileNameTests.cpp:33-37` | 三分支（`.dylib`）。注意 `#else` 支**会巧合通过**（`lib.so` 在 macOS 上本就不合法），所以必须显式三分支，不能靠它绿 |
| B9 | `Tests/Abi/ImportEnforcementTests.cpp:29` | 注释里的拼法补 macOS（`@rpath/BadLinkSiblingB.dylib`） |
| B10 | `Source/Host/ImageInspect.h:20-24/28/53` | `IdentityKind` 加值 + 去默认值 + `ImageFormat` 换形（D163/D174/D175） |

**C 类（只是措辞，事实对 macOS 也成立）**：`Include/Vase/Host/Evidence.h:54`、`Include/Vase/Host/Loader.h:4-6,46-48`、`Source/Host/ImageInspectPlatform.h:4,24-25,31-33`、`Source/Host/Loader.cpp:79`、`Source/Host/LoaderPosix.cpp:53,70`、`Source/Host/PluginHost.cpp:231,1032`、`Tools/VaseConsole/Console.cpp:862,867,878`、`Tools/VaseConsole/Console.cpp:989`（**此项非措辞，见 D163**）、`Tests/Unit/LoaderTests.cpp:321-329`、`Tests/HotSwap/EjectTests.cpp:53`、`Tests/HotSwap/HotSwapLoopTests.cpp:145-146`、`Tests/HotSwap/AdoptTests.cpp:361`、`Tests/Abi/fixtures/BadLinkSibling{A,B}.cpp:7`、`Tests/HotSwap/fixtures/CMakeLists.txt:27`、`Tests/Abi/fixtures/CMakeLists.txt:7`、`Tests/HotSwap/fixtures/NoIdentityPlugin/NoIdentityPlugin.cpp:5`、`Tests/CMakeLists.txt:114-118`。

**A 类（判定为不用改，登记以免重复排查）**：`Include/Vase/Detail/Export.h:13-17`、`Source/Host/Loader.cpp:76-82`、`Source/Host/{ImageInspectWindows,LoaderWindows}.cpp` 的 `WIN32_LEAN_AND_MEAN`、`Tools/VaseCli/Doctor.cpp:28-37/88-112/241-271`、`Tests/Unit/LoaderTests.cpp:321-329`、`Tests/HotSwap/EjectTests.cpp:50-54`、`Tests/Integration/VaseCliDoctorTests.cpp` 的其余 `#ifdef`、根 `CMakeLists.txt:29-32/62-64/90-94/110-113/134-136/148-159/258-260`。

**仍需在 macOS 上实测确认（不是推）**：`Tests/Integration/VaseCliDoctorTests.cpp` 的 T4 两条平台证人（③ 目录写探针、④ 锁探针在 macOS 上的行为）。

### D167 `NoIdentityPlugin` 三平台化

`Tests/CMakeLists.txt:122-129` 从两分支改三分支，macOS 侧 `-Wl,-no_uuid`。该处既有的「链接顺序是实测钉住的」注释（`:116-119`）要补 macOS 一条：**macOS 侧工具链本就没加 flag，所以不存在「要压过的东西」**——把这一条写清楚，免得被当成「没验过」。

### D173 Mach-O 解析器走**合成字节**用例，八线同幅

`Tests/Unit/LoaderTests.cpp` 的 `MakeMinimalPe()` / `MakeMinimalElf()` + `ImageInspect.*` 一族是**直接调纯解析器**、喂手工构造字节的用例——与宿主平台无关（`PeFileCodeViewExtracted`、`ElfFileBuildIdExtracted` 在 macOS 上照样跑、照样绿）。

Mach-O 侧照同一形状补，**同样与平台无关**：

- `MakeMinimalMachO()`：32 字节 `mach_header_64` + `LC_UUID` + `LC_ID_DYLIB` + `LC_LOAD_DYLIB` + 一段无关尾部；
- `ImageInspect.MachOUuidExtracted`——正向，`Kind == kMachOUuid`、16 字节；
- `ImageInspect.MachODylibNamesListed`——`LC_LOAD_DYLIB` 的名字列表 + `FirstUnresolvableImport` 入口；
- `ImageInspect.MachOWithoutUuidFailsLouder`——摘掉 `LC_UUID` 后错误文案指路（与 `PeWithoutCodeViewFailsLouder` 同形）。

**为什么值得单列一条决定**：它让 Mach-O 解析器在**全部八条线上**都有证人，而不是只有 macOS 线上有——这正是能在 Windows 本地快速迭代、并且让「解析器坏了」在六条既有线上立刻变红的关键。同时它给 D164 的路径规范化**留出负面**：路径匹配那一层归 Darwin 平台实现，不在这族用例的射程里（那部分只有 macOS 线能测，如实记）。

---

## 5. 验证口径（D169、D172）

**八线**（Windows 四条 + Linux 两条 + macOS 两条）：

| 平台 | preset |
|---|---|
| Windows / clang-cl | `win-x64-clang-debug`、`win-x64-clang-release` |
| Windows / cl.exe | `win-x64-msvc-debug`、`win-x64-msvc-release` |
| Linux / clang + libc++ | `linux-x64-clang-debug`、`linux-x64-clang-release` |
| macOS x64 / clang + libc++ | `macos-x64-clang-debug`、`macos-x64-clang-release` |

**macOS 两条线由本任务经 ssh 跑**（取证注⑩：同一棵工作树），各留 `configure → build → ctest → ctest -N` 的日志。**六线全绿**的口径随之改成**八线全绿**。收口时逐位对上的形态应当是：

- **四条 debug 线同值**、**两条 Win release 与两条 Linux/macOS release 同值**；
- **release = debug − 1**，那 1 条仍是 T3 的 death test（`EffectScopeDeath.CreateAfterDisposeTerminates`，受 `#ifndef NDEBUG` 门）；
- **平台差 = 0**（M4/D109 起 `linux − win` 已归零，本波要求 macOS 也不引入差集）；
- 本波新增用例**不得**受 `#ifndef NDEBUG` 门或平台门——除 D173 那族合成字节用例（八线同幅）与 D168 表里那几处**体内 `#ifdef`**（各平台各注册各的一条，计数对称）。

差集复测手段与既有各波同：逐对比较 `ctest -N` 的用例名集合。

**tidy 从三线改四线**（Windows 两条 + Linux + macOS），format 一条。

**LLVM 版本口径（D172）**：Mac 是 **23.1.2**，Win/Linux 是 **23.1.0**。仓库多处写着「全平台同一份 LLVM 23.1.0」——本次改成「**同为 23.1.x**」，并把「同版本是判据双平台成立的前提」那句的适用范围写清（判据一致要的是同一套检查器与规则版本，patch 级差异不构成反例；这一句是**口径收窄**，要记进 §7 清单）。

---

## 6. 风险与如实登记

1. **neverUnload 的结论有形态限定**（D171）。取证注② 只覆盖「纯 C++ / libc++ 插件、无 ObjC/Swift 元数据、无 `RTLD_NODELETE`」这一形态——这正是 §8.2 关心的一类，但不是 dyld4 neverUnload 清单的全部。**`references/portability.md:77` 的条目改为「实测（2026-10-01）：上述形态不触发；清单其余项未测」**，不许写成「macOS 不会 neverUnload」。
2. **`LC_UUID` 对同源同参构建是确定的**（取证注⑦）。→ 身份比对**不能**区分「重编同一份代码」带来的换件；能区分的是字节真的变了。这不是缺陷，是身份特征的定义域，但要写进 `ImageInspectMachO.cpp` 的就地注释。
3. **in-place 截断仍被映射的镜像有 `SIGBUS` 风险**（§3）。M4 序列已避开；风险留给「有人把它优化回去」这一条路径，靠 §3 的注释挡。
4. **`-Wl,-no_uuid` 的产物是否会被系统拒载**未单独验证——探针实测 `dlopen` 成功（`plug_nouuid.dylib` 装卸正常），故负例可用；但「无 UUID 的 dylib 在别的 macOS 场景（公证 / Gatekeeper）会不会被拦」不在本波范围内，不做声明。
5. **macOS 的平台专属 TU 只在 macOS 线上被检查**（与 `*Posix.cpp` 在 Linux 线上的地位相同）——tidy 的四条线因此仍不对称，四个平台的告警集合不可互推。
6. **本波顺手修掉一个 Linux 上的潜伏形态**（D176）：§8.7 执法按导入名全长比对，而 ELF 的 `DT_NEEDED` 也允许带路径——今天没有带路径的 `DT_NEEDED`，所以从未暴露。**这条要留痕**：它是本波唯一一处「改了 Linux/Windows 既有行为」的改动（虽然只在从未出现的输入上改变行为），收口时它在既有六条线上的回归证据就是那两条执法用例照常绿。
7. **macOS 独有环境缺口已被撞出一次**（D178 的 `pkg-config`）。它提示这台机器上 vcpkg 的 macOS 路径**此前从未被走过**——落地早期先跑一次空转 configure/build 摸清还有没有第二个同类缺口，比等到收口才发现便宜。

---

## 7. 文书义务

| 文件 | 改什么 |
|---|---|
| `CLAUDE.md`「技术前提」 | 目标平台行：macOS **arm64 → x64**（D157，需求方改口，唯一一处前提变更） |
| `CLAUDE.md`「构建与测试」 | preset **六条 → 八条**；`ctest -N` 基数表**加两行**（macOS debug / release），并把「六线各净 +N」那类叙述改成「八线各净 +N」；命令块加 macOS 段（**按 D179：不写主机名**，脚注说明可经 ssh 触发）；`Scripts/` 清单加两个脚本；M0–M6 的历史基数叙述**不重写**，只加本波的新口径段 |
| `CLAUDE.md`「工具链 flag 是承重的」 | 加 macOS 一条：**无对应 flag**，链接器必写 `LC_UUID`，承重性由 `-Wl,-no_uuid` 负例反向把守（D159） |
| `CLAUDE.md`「项目状态」 | 本波段落 + macOS 腿从「M5 记名欠账」里划掉（余四件原位）；`LibraryFileName.h` 的 D138 欠账销账 |
| `CLAUDE.md` 目录布局表 | `Cmake/Toolchains/` 三件 → 四件；`Cmake/Triplets/` 一件 → 两件；`Source/Host/` 增 `ImageInspectMachO.cpp`（**无条件编译**）/ `ImageInspectDarwin.cpp` / `Detail/ImageBytes.h` |
| `README.md` 环境前提 | macOS 段：MacPorts `clang++-mp-23`、**`pkgconf`（D178）**、`VCPKG_ROOT` 住 `~/.zshenv`、`clang-format-mp-23` / `run-clang-tidy-mp-23`；`find_program` 缓存陷阱同 Linux 结论 |
| `.claude/skills/vase-cpp-engineering/references/portability.md` | `:7` 平台表（arm64 → x64）；`:10` 「macOS 的代码路径与构建都还没落地」改准；`:77` neverUnload 条目改按实测（D171）；`:188` 的 `_dyld_image_count` 行落实为代码事实 |
| `.claude/skills/vase-cpp-engineering/references/abi-boundary.md` | `:129` 的 macOS `LC_UUID` 行补「无需 flag」；`:131` 「写在三个工具链文件里」→「Windows 两条与 Linux 有 flag，macOS 无（D159）」 |
| `wiki/vase-architecture.md` | §8.5 平台矩阵 macOS 的架构改 x64；§12.3 M5 行的平台腿标「已落（x64）」并注明 arm64 已按需求方 2026-10-01 裁定**永久移出** |
| 历史文档挂勘误注（**不重写**，D171） | `docs/superpowers/specs/2026-09-14-vase-m0-m1-design.md` 的 D2（「macOS arm64 到 M5」）处加一句「2026-10-01 由需求方改为 x64，见 macOS 腿 spec D157」；M1 plan、M4 spec D104 的 arm64 口径同法 |
| `Include/Vase/Catalog/LibraryFileName.h` | 头注释的 D138 欠账改口径（D166） |
| `Include/Vase/Detail/ImageInspect.h` | `IdentityKind` 的 `kMachOUuid` 注释与去默认值（D163/D175）；`ImageFormat` 的引入与**三解析器「返回文件原文、不做路径归一」契约**（D174/D176）；两个 Mach-O 纯函数声明 |
| `Source/Host/ImageInspectPlatform.h` | 三个桥函数的平台注释补 macOS 形态；`PlatformImageFormat()` 的声明与就地理由（D164/D174） |
| `.probe-macos/`（spike 探针） | **已于 2026-10-01 随 spec 定稿删除**（本地与 Mac 两侧）。一次性材料，不进库；其结论已逐条摘进本文取证注，不留孤儿引用 |

---

## 8. 决定登记

| 号 | 决定 | 依据 |
|---|---|---|
| D156 | 本波 = **macOS x64 全量平台腿**（构建 + 平台代码 + 测试 + 文书），一波做完，收口八线全绿 | spike 四条未知全部有利（取证注①–⑤） |
| D157 | 目标平台 macOS **x64**；arm64 **永久移出**，将来若要是**新增**一套 preset+triplet 而非改这套 | 需求方 2026-10-01；`CMAKE_OSX_ARCHITECTURES` 显式 |
| D158 | 编译器 = MacPorts `clang++-mp-23`（排在 `clang++` 前）；根 `CMakeLists.txt` 的 doctor **零改动** | 取证注①：ID `Clang` / 23.1.2 |
| D159 | 身份特征 = `LC_UUID`，**不追加 linker flag**；承重性由 `-Wl,-no_uuid` 负例反向把守 | 取证注⑤：链接器必写 |
| D160 | 自定义 overlay triplet；**头号理由是钉编译器**（Apple clang 16.0.0 + SDK 头树 vs MacPorts 23.1.2 + 自带头树），`dynamic` 降级为「与另三条线一致」 | 取证注⑧⑪⑭；grilling Q3 |
| D161 | Mach-O 解析落点 = 新 `ImageInspectMachO.cpp`（**无条件进 VaseHost**，是 D173 的承重前提）+ 共用字节助手提入 `Detail/ImageBytes.h` | 与 PE/ELF 那族同形；grilling Q1 |
| D162 | `ImageInspectPosix.cpp` → `ImageInspectLinux.cpp`；`Source/Host/CMakeLists.txt` 三分支；`LoaderPosix.cpp` 留名留内容、只改注释 | 名字在 macOS 进来后失实 |
| D163 | `IdentityKind` 追加 `kMachOUuid`，**不 bump `kHeaderVersion`**；`Console.cpp:989` 的三目改穷举 `switch`；公开面增量如实记 | 它不进描述符契约；底层类型不变 |
| D164 | 路径 → dyld 镜像匹配**必须先规范化**，canonical 失败退 weakly_canonical 再退原路径 | 取证注⑥；对 `CatalogSandbox` 的临时目录是承重的 |
| D165 | `Loader.cpp` **三处** `#ifdef` 逐个定性：**只有 `:71-82` 不动**，`:104-130` 必须三分支 | `PluginHost.cpp:1093-1097` 对 Err 直接 Refusal ⇒ 不改则每次 Adopt 全拒；grilling Q2 |
| D166 | `LibraryFileName.h` 落 `.dylib` 分支，还 D138 | 本波平台腿 |
| D167 | `NoIdentityPlugin` 三平台化（macOS `-Wl,-no_uuid`）；档三两条转三平台 | 取证注⑤ |
| D168 | 三方化口径 + **完整站点清单**（B 类 10 条 / C 类 17 条 / A 类登记），清单即验收面 | grilling 期普查；减少平台分支的维护面 |
| D169 | 验证口径 = **八线全绿 + tidy 四线 + format 一条**；macOS 线经 ssh 由本任务跑 | 取证注⑨⑩ |
| D170 | `doctor` ④ 在 macOS 上维持「不可探测」支，**不改代码**、只核文案 | macOS 覆盖为 truncate-in-place，同 Linux |
| D171 | neverUnload 结论**带形态限定**登记进技能；历史文档的 arm64 挂勘误注、不重写 | 取证注②的覆盖范围 |
| D172 | LLVM 版本口径「同一份 23.1.0」→「**同为 23.1.x**」（Mac 实为 23.1.2） | 取证注⑨ |
| D173 | Mach-O 解析器走**合成字节**用例（`MakeMinimalMachO()` + 三条 `ImageInspect.*`），八线同幅；路径匹配那一层不在这族射程内、只有 macOS 线可测 | 与 PE/ELF 那族同形；让解析器在八条线上都有证人 |
| D174 | `FirstUnresolvableImport` 的 `bool isPe` → `ImageFormat{kPe,kElf,kMachO}`；`PlatformImageFormat()` 住 `ImageInspectCommon.cpp`、声明进 `ImageInspectPlatform.h`——**`Loader.cpp` 平台分支降到一处** | grilling Q7/Q10；枚举使格式集合成为编译期穷举点 |
| D175 | `IdentityKind` 去掉默认值 `= kElfBuildId` | 它是「忘了赋值就静默贴错标签」的埋伏；grilling Q8 |
| D176 | `@rpath/` 归一到**比对点**（basename），三个解析器契约统一为「返回文件原文」 | 取证注⑫；**修的是通用形态**——ELF 的 `DT_NEEDED` 同样可带路径，Linux 上同形潜伏；grilling Q13（推翻 Q4） |
| D177 | `FlipMachineField` 的 macOS 支翻 **`cputype`**（装载器认得的字段，不是身份字段），机理写进注释 | 那条用例要的是「装载器拒收」；grilling Q9 |
| D178 | `pkg-config` 进 macOS 环境前提；验证脚本起手检查并响亮报错；**不在仓库出 shim** | 取证注⑬：缺它整条腿编不出东西；grilling Q12 |
| D179 | 主机别名按**文献性质**分：`CLAUDE.md`/`README` **不写**（D6 同取向），spec 取证注与 plan 的执行命令**写具体别名**（现场记录 + 可直接执行的脚本）；`-o BatchMode=yes` 只记理由、不两处各抄一种拼法——无 TTY 时它**失败**而不是**挂住** | 与 D6 同取向；grilling Q11 + 2026-10-01 复审补正 |
