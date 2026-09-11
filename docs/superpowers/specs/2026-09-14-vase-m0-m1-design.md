# Vase M0 / M1 设计

> **勘误与冻结（2026-09-16）**：本文是 M0/M1 的历史决策记录，**正文不改史**。以下三处已被架构 **v3**（[`wiki/vase-architecture.md`](../../../wiki/vase-architecture.md)）取代，最新口径以 v3 为准：
> ① 2.4 验收判据中「有 Session 存活时拒绝卸载」——已改判：卸载门槛为「无实例 + 账上无反向边」（v3 5.6 / 8.1，热插拔档 ①）；
> ② 3.3(4) 与 D16 的「glibc 因静态 TLS 不真正卸载」因果措辞——已修订：缓存复用的成因是 refcount / `DT_NEEDED` / `RTLD_NODELETE` / `STB_GNU_UNIQUE`，TLS 超量属 dlopen 侧失败；「重载后静态状态归零」主判据升格为三平台通用的「档三身份特征验新」（v3 8.2 / 附录 B）；
> ③ `Reload` 双版本 fixture——升格为 `Tests/HotSwap`，且热插拔从 M4 提前进 M1 闭环（v3 12.1 / 12.3）。连带：`Session` 全文更名 `Pod`（v3 1.5）。
>
> **状态**：设计文档，已通过 brainstorming 评审，M0 已完成；M1 计划待按 v3 口径重制。
> **上游**：~~架构 v2~~ **架构 v3**（下文引用「架构文档」处，凡与上行冲突以 v3 为准）。
> **范围**：把架构文档拆成可执行的里程碑序列，并把前两个里程碑（M0 构建地基、M1 最小闭环）设计到可直接开工的粒度。

---

## 0. 本文件与架构文档的关系

架构文档描述**做什么**；本文件描述**按什么顺序做、先做到什么程度、以及哪些提案在 C++ 语法上过不去**。

凡与架构文档冲突之处，第 8 节单独列出，并标注需要回头修订架构文档的位置。本文件**不**修改架构文档。

---

## 1. 已定决策

| # | 决策 | 理由摘要 |
|---|---|---|
| D1 | 里程碑按**纵切**切：最小闭环优先 | 架构文档标记为「承重」的验证项全部依赖真实二进制，而 Catalog 是零风险层。先做 Catalog 会让最危险的部分最晚被验证 |
| D2 | **Win + Linux 双平台**从 M1 起跑绿；macOS arm64 到 M5 | Windows 代表 MSVC ABI / 文件锁语义，Linux 代表 libc++ / POSIX inode 语义。两者合起来暴露绝大多数平台差异，代价不到三平台的三倍；且 8.2 卸载证据的平台分歧在 M1 就暴露 |
| D3 | M1 边界 = **闭环 + 卸载证据**（不含双版本 reload fixture） | 纵切的意义就是让二进制生命周期风险最早暴露。卸载证据便宜，可进 M1；12.1 的双版本 fixture 重，留 M4 |
| D4 | 测试框架 **GoogleTest** | 架构文档 12 节把「Debug 断言会触发」当正式验证手段（#16、#17、7.2、5.3），验证它需要 death test；三家主流框架只有 GoogleTest 内建支持 |
| D5 | 依赖管理 **vcpkg manifest 模式**，M0 起用 | 13.2 已决定 CMake + vcpkg。M0 即接入，避免依赖管道推迟到 M2 才第一次撞上 |
| D6 | Windows 编译器：**clang-cl 与 cl.exe 都支持**，各自一套 preset。clang-cl **由 PATH 环境变量解析**（不写死安装路径） | **2026-09-15 由需求方扩为两者并存**。8.5 说两者是同一 ABI 的两个前端（都目标 MSVC ABI、都用 MSVC STL），本体两套都支持，才能让「宿主与插件可以各用一个」不只是纸面约定。**clang-cl 仍是主构建**：与 Linux/macOS 同前端，clang-tidy / clang-format 判据一致。cl.exe 的两处代价见 2.1.1(e)。**注意**：原始理由里「warning 集合一致」一句已收回，见 2.1.1(d) |
| D7 | Linux 侧落点 **WSL2 Ubuntu** | 本机，迭代最快。CI 推至 M5 |
| D8 | Effect 存储形态 = **`IEffect` 虚接口 + Scope 内 size-class 空闲链表** | 回收路径与注册点类型完全无关，账目统一性最好；跨 DLL 删除语义与 3.1 的 `VasePluginDestroy_<T>()` 同构 |
| D9 | `EffectHandle` = **索引 + 代际** | 与 5.1 的 `SessionHandle` 同构；代际一次解决 7.2 幂等与 7.3 的「从账上移除」 |
| D10 | 节点池**跨 Session 复用**，挂在 `PluginHost` 上 | 让「反复 Play / Stop」在稳态下零分配。与 1.2 不冲突：池里只有空闲节点，不持有实例级对象 |
| D11 | CMake 建**两个动态库 target**：`VaseSession`、`VaseHost` | 「插件不依赖 Host」从约定变成链接期事实 |
| D12 | M1 手写 **`LoadPlan`**，不手写 `.plugin.json` | `LoadPlan` 是普通 struct，Catalog 接上后走同一条路径，不产生返工 |
| D13 | `HeaderVersion` **提前**到 M1 | 它是描述符结构体的字段，而布局是对外契约；12 节标为承重。晚加要改布局 |
| D14 | `LeakyPlugin` 与泄漏归属 **提前**到 M1 | 让「计数归零」这条判据不至于只证明了一个恒为零的计数器 |
| D15 | 属主追踪器 M1 只上**最小版** | 进程级容器插入时断言属主为空或为进程级；完整归属标记等 M2 有多插件后再补 |
| D16 | Linux 卸载**主判据** = 重载后静态状态归零 | 它测的是行为（「下次加载如同首次」）而不是簿记，正是 0.2 的核心承诺本身 |
| D17 | **全项目不使用 C++ 异常**，以关闭异常的方式编译（Windows `/EHs-c-`、Linux `-fno-exceptions`） | 由需求方指定。它让 8.3「异常不得穿过插件边界」从约定变成**编译期强制**；错误一律经 `Result<T>` / `Error` 返回。代价与清单见下第 9 节 |

---

## 2. M0：构建地基

### 2.1 工具链落点（由本机实测确定，非假设）

| | Windows | WSL Ubuntu 24.04 | 用途 |
|---|---|---|---|
| CMake | 4.4.3 | 4.4.2（`/opt/cmake-4.4.2`，非发行版包） | **项目下限定为 4.4**，见 2.1.1(a) |
| Ninja | 1.13.2 | 1.11.1 | 生成器；`compile_commands.json` 干净，clang-tidy 直接可用 |
| 编译器 | clang-cl **23.1.0**（`D:\Developer\LLVM\bin`，独立安装，**由 PATH 解析**） | clang **23.1.0**（`/opt/llvm-23.1.0`，tarball 手动安装，链接进 `/usr/local/bin`） | **两侧同版本、同源 commit（`ea7d852a`）**，见 2.1.1(c) |
| clang-format / clang-tidy | 23.1.0 | 23.1.0 | 版本一致 → 验收 #1 / #3 / #5 双平台成立 |
| 标准库 | MSVC STL（由 VS 安装提供） | libc++ 23.1.0（`/usr/local/lib`，实测编译+运行验证） | 8.3 的约束是「同平台 + 同 STL」 |
| vcpkg | `D:\Developer\vcpkg` HEAD `114d9fe6` | `/mnt/d/Developer/vcpkg-linux` HEAD `114d9fe6` | 见 2.2 |

**环境前提**：clang-cl 需要 MSVC 的头/库/链接器。用 Ninja 生成器时若 clang-cl 自动探测不奏效，从 VS Developer 环境配置。此条写入 README，并列为 M0 验证项。

> **两侧 LLVM 均手动安装，版本对齐到 23.1.0（同一源 commit `ea7d852a`）。**
>
> - **Windows**：独立安装的 clang-cl 23.1.0，位于 `D:\Developer\LLVM\bin`，**由 PATH 解析**（见 D6）——不再用 VS 18 自带的 22.1.3。
> - **Linux**：Ubuntu 24.04 自带的 clang 是 18.1.3，差四个大版本会破坏 `-Werror` 与格式/tidy 判据的双平台一致性；apt.llvm.org 路线**实测不可行**（国内无任何镜像站，直连 19–23 KB/s，152 MB 约需 100 分钟；该源与密钥已移除）。最终从 LLVM 官方 release 取 `LLVM-23.1.0-Linux-X64.tar.xz` 装到 `/opt/llvm-23.1.0`，实测 libc++ 编译 + 运行通过。
>
> 两侧同版本同 commit，意味着 `clang-format` / `clang-tidy` 的规则版本完全一致——这比「22 与 23 恰好输出相同」更强，验收 #1 / #3 / #5 因此**确定性地**双平台成立（差异只剩目标三元组与 STL，见 2.1.1(c)）。

#### 2.1.1 五处环境陷阱

**（a）CMake 下限定为 4.4，以及 4.x 的 `<3.5` 报错。**

两侧实测为 4.4.3（Windows）与 4.4.2（WSL，装在 `/opt/cmake-4.4.2`，非发行版包），只差一个补丁号，因此项目**直接以 4.4 为下限**：

- `cmake_minimum_required(VERSION 4.4)`
- `CMakePresets.json` 的 `"version"` 取 **12**

schema 12 由 CMake 4.4 引入（对照：6→3.25、8→3.28、10→3.31、11→4.3、12→4.4）。实测 4.4.2 接受 6–12，并以 `"version": 99` 作对照组确认该测试确实在验证而非静默放行。

**不采用 `<min>...<max>` 写法**（如 `3.28...4.4`）：两侧实际都是 4.4.x，声明 3.28 只会让读者误以为 3.28 也被支持。

**4.x 的破坏性变更里只有一条会咬人**：`cmake_minimum_required(VERSION <3.5)` 自 **4.0 起直接报错**（3.27 起警告，3.31 起弃用 <3.10）。受影响的是**第三方依赖**——它们的 `CMakeLists.txt` 由别人写，我们改不了。M0 用 vcpkg 构建 GoogleTest 时就可能撞上。应急开关：

```bash
-DCMAKE_POLICY_VERSION_MINIMUM=3.5
```

这是 CMake 4.x 时代「依赖构建失败」最常见的原因，且报错指向依赖库而非我们的代码。

**（b）环境变量的加载口径（已实测）。** 值的**唯一来源**放 `/etc/profile.d/vcpkg.sh`，`~/.bashrc` 末尾只加一行引用：

```bash
[ -r /etc/profile.d/vcpkg.sh ] && . /etc/profile.d/vcpkg.sh
```

实测覆盖矩阵（在 WSL Ubuntu 24.04 上逐条验证）：

| 调用方式 | 覆盖 |
|---|---|
| `bash -l -c`（登录） | ✓ |
| `bash -i -c`（交互非登录） | ✓ |
| `sh -l -c`（登录） | ✓ |
| `bash -c` / `sh -c`（非登录非交互） | **✗ 结构性补不上** |

**为什么不能只放一处。** Ubuntu 的 `.bashrc` 开头有一道 `case $- in *i*) ;; *) return;; esac`，非交互 shell 在读到你追加在文件末尾的 `export` 之前就返回了；而 `/etc/profile` 只被**登录** shell 读取，`bash -i` 不读。两者各覆盖一半——**任一单独使用都会漏**。且 `~/.bashrc` 里只能是引用而非第二份定义，否则同一个值就有两处，迟早漂移。

**为什么 `bash -c` 那一格补不上。** 非登录非交互 shell 不读任何 rc 文件，这是 shell 的加载规则，不是配置能修的。受影响的是 CI 步骤、构建钩子、任何用 `sh -c` 拉起 CMake 的地方——**这些调用方必须自己显式带上 `VCPKG_ROOT`**。

**这对 M0 是真风险**：`CMakePresets.json` 用 `$env{VCPKG_ROOT}` 定位 vcpkg 工具链，拿到空值即 configure 失败，且报错信息指向工具链文件而非环境变量。

**（c）LLVM 版本对齐——已闭合，附安装期的一个真坑。**

**结论**：两侧同为 **clang 23.1.0、同一源 commit `ea7d852a`**（官方 release 的两平台预编译包）。Windows 侧的解析方式是 **PATH 环境变量**（D6）：工具链文件里用裸名 `clang-cl`（CMake 从 PATH 解析），**不写死任何安装路径**——换 LLVM 只改 PATH，不动仓库。实测裸名解析：`clang` / `clang-cl` / `clang-format` / `clang-tidy` 在 Windows 全部解析到 `D:\Developer\LLVM\bin`；Linux 侧 179 个工具链进 `/usr/local/bin`，**交互与非交互 shell 均验证可解析**（`/usr/local/bin` 本就在默认 PATH，不受 2.1.1(b) 那个 `.bashrc` 缺口影响）。

版本一致即意味着：验收 #1（`-Werror`）、#3（`run-clang-tidy`）、#5（格式校验）**双平台执行成立**——同版本的 format/tidy 规则是同一份代码。此前「22 vs 23 输出恰好一致」的实测现在只是佐证，不再是依赖。

**残余的平台差异**只剩两件，且都不是版本问题：Windows 目标三元组是 `x86_64-pc-windows-msvc` + MSVC STL，Linux 是 `x86_64-unknown-linux-gnu` + libc++——这正是 8.3「同平台 + 同 STL」约束允许的两套平台形态。**独立安装的 clang-cl 仍依赖 VS 提供 MSVC 头/库/链接器**（经注册表自动发现），与 2.1「环境前提」同条。

**安装期真坑（记录以防重蹈）**：LLVM 包自带 `libunwind.so.1`。若把它的 libunwind 也放进 `/usr/local/lib`，会**盖过发行版的同名库**——`/usr/local/lib` 在 ldconfig 搜索路径中优先，而 `libc++abi` 恰好依赖 `libunwind.so.1`。影响面是所有需要 libunwind 的进程，故障表现为无关程序诡异崩溃。**处置：只搬 `libc++*`，libunwind 用发行版的**；实测 `ldd` 解析正确（`libc++.so.1 → /usr/local/lib`、`libunwind.so.1 → /lib/x86_64-linux-gnu`）。

**（d）clang-cl 是另一个驱动，它的默认值和你以为的不一样。**

前三条都是环境配置问题，这一条不是——它是**同一个 LLVM 里两个不同驱动**的语义差异。实测于本机 clang 23.1.0（同源 commit），三处都会咬人：

| 项 | `clang` / `clang++`（GNU 驱动） | `clang-cl`（CL 驱动） | 处置 |
|---|---|---|---|
| `-Wall` 的含义 | GNU 的精选警告集 | **MSVC 的 `/Wall`，等价于 `-Weverything`**（连 `-Wc++98-compat` 都报） | 两侧不能用同一个 flag 写法：Windows 用 `/W4`、Linux 用 `-Wall -Wextra` |
| 默认 CRT | `libcmt`（`/MT` 静态） | 同左 | 8.3 要求动态 → 显式 `CMAKE_MSVC_RUNTIME_LIBRARY`，并**用产物本身验证**。注意 triplet 只管 vcpkg 构建的依赖，**管不到我们自己的 target** |
| 默认异常 | **开启** | **关闭** | D17 要求关闭 → Windows `/EHs-c-`、Linux `-fno-exceptions`，两侧都显式写 |

**「warning 集合一致」这句要收回。** 2.1.1(c) 与 D6 原先把「两侧同 clang 版本 → 诊断与 warning 集合一致」当作选择 clang-cl 的理由之一。同版本真正保证的是 **clang-tidy / clang-format 的规则一致**（验收 #3 / #5 确实因此成立），而 **warning 集合因驱动方言不同而不同**。验收 #1 只要求「各自零警告」，因此不受影响——但这条理由的强度不如原先写的。

**另有一处会白白浪费时间的坑**：Git Bash（MSYS）会把 `/MD`、`/nologo` 这类 `/` 开头的参数当路径转换掉，报错指向 `D:/Program Files/Git/MD` 之类的路径，与真实原因相距甚远。手动调用 clang-cl 前先 `export MSYS2_ARG_CONV_EXCL='*'`，或改用 `-` 前缀写法。CMake 自己调编译器不受影响。

**（e）cl.exe 与 clang-cl 不是可以互换的选项。**

D6 扩为「两者并存」之后，下面三条是实测出来的、会让它们无法共用一套配置的差异：

| 项 | clang-cl | cl.exe | 处置 |
|---|---|---|---|
| MSVC 头/库的发现 | **不需要 Developer 环境**，经注册表自动发现（实测） | **需要 Developer 环境**（`PATH` / `INCLUDE` / `LIB`） | MSVC 那条线的 preset 必须在 Developer Command Prompt 里跑；工具链文件用 `VSCMD_VER` / `VCINSTALLDIR` 做哨兵，缺了就响亮报错 |
| 关闭异常的强制力 | `try` / `throw` / `catch` **全是硬错误** → 完整强制 | 默认只报 warning C4530，编译照过 → **软** | 加 `/we4530` 把 C4530 升为错误，补上 `try/catch` 这一半。**裸 `throw` 仍漏网**（见第 9 节） |
| 中文源码 | 默认按 UTF-8 读 | **默认按系统代码页（本机 CP936）读** —— 实测一段中文注释即触发 C4819，随后直接 `error C2447` 编译崩掉 | `/utf-8` 对 cl.exe 是**必需**项，不是锦上添花。本仓库以中文注释为主，漏了会立刻炸 |

`cl.exe` 的警告等级用 `/W4`（它不认 GNU 的 `-Wall`），与 2.1.1(d) 的结论一致。

**还有一条更隐蔽的：`vcvars64.bat` 会*覆盖* `VCPKG_ROOT`，不是清空它。**

实测（VS 18 Community）：

```
BEFORE=[D:\Developer\vcpkg]
call vcvars64.bat
AFTER =[C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg]
```

差别很关键——工具链里那句 `if(NOT DEFINED ENV{VCPKG_ROOT} OR ... STREQUAL "")` 的**空值检查拦不住覆盖**，它会安然通过，然后 include 一份**非钉定**的 vcpkg，`builtin-baseline` 形同虚设。

**这不是理论风险。** M0 实施时首跑 MSVC preset 就撞上了：vcpkg 用的是 `--vcpkg-root "...\VC\vcpkg"`，而 VS 那份需要联网取 registry，`git fetch https://github.com/microsoft/vcpkg` 在本机**挂死**（CPU 时间 25 分钟零增长），configure 不返回。换回钉定的那份后不再取网，762 ms 秒回。

处置：`Scripts/msvc-env.cmd` 在 call vcvars **之前**记住调用者的值、之后还原（不写死任何路径，调用者的值才是权威）。但**手工**在 Developer Prompt 里配的人仍可能静默用错且无提示——把守卫加强为「比对钉定的路径 / commit」是一个尚未采取的设计决策。

### 2.2 vcpkg

两侧各自独立安装，**钉同一个 commit**，并作为 `vcpkg.json` 的 `builtin-baseline`：

```bash
# Windows：已就绪（D:\Developer\vcpkg @ 114d9fe6）
# WSL Ubuntu：
git clone git@github.com:microsoft/vcpkg.git ~/vcpkg      # 或 /mnt/d/Developer/vcpkg-linux
git -C ~/vcpkg checkout 114d9fe62faf35856b45cf55cb93b57028a45d63
~/vcpkg/bootstrap-vcpkg.sh -disableMetrics
# 环境变量：值写 /etc/profile.d/vcpkg.sh，~/.bashrc 只引用它 —— 见 2.1.1(b)
```

**已核对**（WSL 侧）：`VCPKG_ROOT=/mnt/d/Developer/vcpkg-linux`，commit `114d9fe6` 与 Windows 侧一致，完整克隆（非浅克隆），`bootstrap` 成功。

**不使用 submodule。** submodule 只有一份工作树，Windows 与 WSL 会往同一目录写各自的工具二进制与 `buildtrees/`；且它被设计来解决的是「多人之间版本不一致」，不是「同一份源码在两种操作系统上构建」。`builtin-baseline` 已经承担了跨平台的版本钉定职责。

**Triplet 是 8.3 的执行点**：

- Windows 必须用内置 `x64-windows`（动态 CRT、动态库）。**不能用 `x64-windows-static`**——它走 `/MT`，静态 CRT 会让跨模块 `new` / `delete` 落到不同的堆。
- Linux 需要自定义 triplet `Cmake/Triplets/x64-linux-libcxx.cmake`：vcpkg 内置的 `x64-linux` 走 libstdc++，而 8.5 要求 libc++。

**工具链接线**：CMake 只允许一个 `CMAKE_TOOLCHAIN_FILE`。方向定为「平台工具链文件是它，文件内部 include vcpkg 的」，使平台配置先于 vcpkg 生效。

**构建树位置**：源码留在 Windows 侧（`D:\Git\Vase`），WSL 的构建树放到 `/mnt/d/Git/Vase/build-linux/`——`.gitignore` 已覆盖 `build/` 与 `build-*/`，两侧目录名分开（`build-win/` / `build-linux/`）互不干涉。

**drvfs 特有风险**（落到 `/mnt/d` 才会遇到）：

1. Unix 权限位默认不保留。若 `bootstrap-vcpkg.sh` 报权限错，在 `/etc/wsl.conf` 加 `[automount] options = "metadata"` 后 `wsl --shutdown`；若 git 报 mode 变化，`git config core.fileMode false`。
2. 在 `/mnt/d` 上创建 symlink 需要 Windows 开发者模式或管理员权限。若 vcpkg 解压带 symlink 的归档时报错，根因在此。
3. `buildtrees/` 是几十万个小文件，建议将 vcpkg 目录加入 Defender 排除项。

### 2.3 文件与 Target

```text
CMakeLists.txt                 顶层：project / C++20 / VASE_WERROR / 子目录
CMakePresets.json              win-x64-clang-{debug,release}
                               linux-x64-clang-{debug,release}
vcpkg.json                     manifest；gtest；builtin-baseline: 114d9fe6
Cmake/
├── Toolchains/
│   ├── windows-x64-clangcl.cmake
│   └── linux-x64-clang-libcxx.cmake      文件内 include vcpkg 的 toolchain
└── Triplets/
    └── x64-linux-libcxx.cmake            Windows 用内置 x64-windows，无需自定义
Include/Vase/Plugin.h                     最小公开面 + VASE_EXPORT
Include/Vase/Detail/Export.h
Source/Session/、Source/Host/             各一个占位翻译单元
Tests/CMakeLists.txt                      接 ctest
```

**实施修正（2026-09-15）**：D6 之后实为 **6 个 preset / 3 个工具链文件**——`CMakePresets.json`
是 `win-x64-{clang,msvc}-{debug,release}` + `linux-x64-clang-{debug,release}`，
`Cmake/Toolchains/` 下多一个 `windows-x64-msvc.cmake`（cl.exe 线，见 2.1.1 与 R9）。
上面这段清单是 D6 之前的设计记录，保留不改，只在此注明差异。

**只让有内容的 target 存在。** `VaseCatalog` / `VaseCli` / `VasePack` / 其余 Samples 在 M0 不建——空壳 target 会让人误以为已经就位。`Include/Vase/` 下其余六个子目录等 M1 / M2 有内容再立。

`-Werror` **M0 就开**（代码量为零，成本最低），做成 `VASE_WERROR` 选项以便临时关闭。

### 2.4 M0 验收判据

M0 不是「空壳可编译」，而是「构建地基已在双平台被证明」：

1. 两个 preset 都能 configure + build，`-Werror` 下零警告
   —— **实施修正（2026-09-15）**：D6 之后实为**六个 preset**（`win-x64-{clang,msvc}-{debug,release}`、`linux-x64-clang-{debug,release}`），六条线均已实测（零警告）。原文的「两个」写于 D6 之前。另注：判据 2/3 里的「两边」同理已扩到六条线，但**判据 3 的 tidy 实际只跑过三条 debug 树**（release 树未跑），见 `CLAUDE.md`「构建与测试」
2. `ctest` 两边各绿
3. 两边都生成 `compile_commands.json`，`run-clang-tidy -p <build>/` 无错
   —— **实施修正（2026-09-15）**：cl.exe 那条线的字面命令**不成立**。clang-tidy 能完整跑完、分析结果与 clang-cl 线逐条一致，但不加参数时 exit 1 并报 3 条
   `argument unused during compilation: '/external:anglebrackets'`——因为该开关只加在 cl.exe 线上，而 clang-tidy 的前端是 clang、不认它，该诊断又被 MSVC 线的 `/WX` 提升为 error。
   处置：cl.exe 线固定追加 `-extra-arg=-Wno-unused-command-line-argument`（代价：整类压掉「参数未被使用」提示）。**不要为此改 `.clang-tidy` 检查集。**
4. **跨 DLL 冒烟测试**：`VaseSession` 导出符号 → `VaseHost` 导入并调用 → GoogleTest 断言成功
5. `clang-format --dry-run --Werror` 全绿
6. `vcpkg.json` 在两侧解出**同一版本的 gtest**

第 4 条是 M0 的真正价值：它把导出宏、动态库链接、运行时库查找路径这三件最容易在 M1 才炸的事，提前到「还没有任何架构代码」时撞上。

---

## 3. M1：最小闭环

### 3.1 文件清单

```text
Include/Vase/
├── Plugin.h                    ★ 插件作者唯一入口：宏 + 基类 + Context + Result + Error
├── PluginDescriptor.h          描述符契约类型（插件与宿主共用，纯数据）
├── Effect/
│   ├── IEffect.h
│   ├── EffectHandle.h
│   └── EffectScope.h
├── Service/Service.h           标识约定、HasServiceIdentity、Provide/Get 声明
├── Event/Event.h               事件标识约定、HasEventIdentity、Emit 声明
├── Session/
│   ├── Context.h
│   └── Session.h               SessionHandle、SessionReport
├── Host/
│   ├── PluginHost.h
│   ├── Loader.h                跨平台加载收口（架构文档 13.1 第二条）
│   └── LoadPlan.h              LoadPlan / LoadRequest / SessionOptions / PluginId
└── Detail/
    ├── Export.h                VASE_EXPORT / VASE_HIDDEN
    ├── Result.h                Result<T> / Error
    ├── MetaArray.h             固定容量内联数组（见 3.3）
    └── ScopePool.h             size-class 空闲链表 + 槽位池

Source/
├── Effect/EffectScope.cpp
├── Service/ServiceRegistry.cpp
├── Event/EventBus.cpp
├── Session/Context.cpp、Session.cpp
└── Host/
    ├── PluginHost.cpp          二进制表、CreateSession / DestroySession、诊断
    ├── LoaderWindows.cpp       LoadLibraryExW / FreeLibrary / 文件锁探测
    └── LoaderPosix.cpp         dlopen / dlclose / dl_iterate_phdr

Samples/
├── HelloPlugin/                最小插件：提供并消费一个服务，注册可数的 Effect
└── Embedding/                  play / stop

Tests/
├── Unit/                      Result、逆序回收顺序、MetaArray、描述符装配
├── Integration/               阶段顺序、阶段 0、HeaderVersion 拒绝、Loader
│   └── fixtures/StaleHeaderPlugin/    手写一个版本号错误的描述符
└── Lifecycle/                 反复建销、计数归零、泄漏定位
    └── fixtures/LeakyPlugin/
```

### 3.2 组件与接口

**Effect 层**

| 单元 | 接口 |
|---|---|
| `IEffect` | 保护非虚析构、禁拷贝、`virtual void Recycle() = 0` |
| `EffectHandle` | `{EffectScope*, Slot, Generation}`；`Release()`、`IsValid()`；可平凡拷贝（幂等由代际保证） |
| `EffectScope` | `Create<T>(Args...) -> EffectHandle`（`static_assert` T 派生自 `IEffect`）、`Dispose()`、`EffectCount()` |

`EffectScope` 内部三样：槽位表（索引稳定、空闲链）、注册顺序的逆序双向链、按大小分档的 `IEffect` 存储区。槽位表与存储区属于 `ScopePool`，挂在 `PluginHost` 上跨 Session 复用。

**Context / Service / Event**

```cpp
template <typename T> EffectHandle Provide(T& instance);                    // 借用
template <typename T> EffectHandle Provide(std::unique_ptr<T> instance);    // 移交
template <typename T> T& Get();                                             // 缺失：Debug 断言 / Release 终止
template <typename T> T* TryGet();

template <typename E, typename C> EffectHandle On(void (C::*)(const E&), C*);  // 3.1 的常用形
template <typename E, typename F> EffectHandle On(F&& handler);
template <typename E> void Emit(const E& event);

PluginId Owner() const;
```

- `HasServiceIdentity` / `HasEventIdentity` 两个 concept，配 6.1 给定文案的 `static_assert`
- `Get<T>()` 的错误信息含**插件 Id + 服务名 + 版本**
- `ServiceOrigin` 由注册位置决定：根 Context → `kHost`，插件 Context → `kPlugin`
- **`PluginId` = `std::string_view`，指向描述符内的字符串字面量。** 因此它的有效范围是**插件二进制的存活期**，比 Session 长。`Context::Owner()` 返回它用于诊断是安全的；但把它交给宿主长期持有则不然——`Error` 因此改用 `std::string`（3.3(3)）

**Plugin 与描述符**

```cpp
class Plugin
{
public:
    virtual ~Plugin() = default;
    virtual Result<void> OnLoad(Context& ctx)  = 0;
    virtual Result<void> OnStart(Context&) { return {}; }
};
```

描述符拆两层（见 3.3）：

```cpp
struct PluginMeta                      // 作者写的部分：纯数据，可平凡拷贝
{
    std::string_view          Id;
    std::string_view          DisplayName;
    std::string_view          Version;
    MetaArray<ServiceRef, 16> Requires;
    MetaArray<ServiceRef, 16> Provides;
    // M2 补 Config，M3 补 ProcessState
};

struct PluginDescriptor                // 二进制契约
{
    uint32_t          HeaderVersion;
    const PluginMeta* Meta;
    Plugin*         (*Create)();
    void            (*Destroy)(Plugin*);
};
```

**PluginHost / Loader / 诊断**

| 单元 | 接口 |
|---|---|
| `PluginHost` | 构造即绑定当前线程；`LoadPlugin(path)`、`UnloadPlugin(handle)`、`CreateSession(const LoadPlan&, SessionOptions)`、`DestroySession(SessionHandle)`、`Resolve(SessionHandle) -> Session*` |
| 诊断 | 五项计数（Effect / 服务 / 订阅 / 插件实例 / Scope）始终维护；Debug 断言、Release 写进 `SessionReport` |
| `Loader` | `Load(path)`、`Unload(h)`、`Symbol(h, name)`、`CollectEvidence(h)` |

`SessionOptions` 在 M1 只有 `Strict` 一个字段（只有一个插件，实际不触发），但类型先立。

### 3.3 三处必须先解决的机制问题

以下三条**不是实现细节**，是架构文档的提案在 C++ 语法/语义上过不去的地方。三条均已在仓库外的探针文件上实际编译验证，**且在 clang 22.1.3 与 23.1.0 两个版本下结论一致**（`-std=c++20 -Wall -Wextra -Werror`；正向零警告、两个反例均报错、`initializer_list` 成员均零警告通过——悬垂依旧未能复现）。

#### (1) `VASE_PLUGIN` 的花括号无法嵌进聚合初始化

架构文档 3.1 的写法是宏后面跟一个 `{ .Id = ..., };` 块。但宏展开后，用户的花括号必须**成为某个对象的初始化器**——它没法塞进 `PluginDescriptor{ a, b, c, ⬛ }` 的中间，因为 `;` 会出现在花括号未闭合处。

解法：把元信息拆成独立的 `PluginMeta`，并让宏**最后一步落在一个变量声明上**，用户的花括号正好初始化它。访问函数通过**前向声明**先引用、后使用：

```cpp
#define VASE_PLUGIN(Type)                                                                                    \
    extern const ::vase::PluginMeta kVaseMeta_##Type;                     /* 前向声明 */                      \
    ::vase::Plugin* VasePluginCreate_##Type() { return new Type(); }                                          \
    void VasePluginDestroy_##Type(::vase::Plugin* raw) { delete raw; }                                        \
    extern "C" VASE_EXPORT const ::vase::PluginDescriptor* VasePluginDesc_##Type()                            \
    {                                                                                                        \
        static const ::vase::PluginDescriptor kDesc{                                                         \
            ::vase::kHeaderVersion, &kVaseMeta_##Type, &VasePluginCreate_##Type, &VasePluginDestroy_##Type}; \
        return &kDesc;                                                                                       \
    }                                                                                                        \
    extern "C" VASE_EXPORT const ::vase::PluginDescriptor* VasePlugin_GetPlugin(const char* id)               \
    {                                                                                                        \
        return std::string_view{id} == kVaseMeta_##Type.Id ? VasePluginDesc_##Type() : nullptr;              \
    }                                                                                                        \
    const ::vase::PluginMeta kVaseMeta_##Type = ::vase::PluginMeta
```

作者仍然只写一处，`{...}` 正好落在宏的最后一行上。3.1 那条「只有第一项需要手写」得以保持。

**验证结果**：`-Wall -Wextra -Werror` 零警告通过。`VasePlugin_GetPlugin("Vase.Combat") == VasePluginDesc_CombatPlugin()`、`VasePlugin_GetPlugin("Vase.Nope") == nullptr`、`desc->Create()` / `desc->Destroy()` 均实际断言通过。

#### (2) `std::span` 成员在花括号初始化下**编不过**

`.Requires = { { "Vase.World", 1 } }` 如果用 `std::span<const ServiceRef>` 做成员——**编译错误**：

```
error: no matching constructor for initialization of 'std::span<const ServiceRef>'
```

`std::span` 没有 `initializer_list` 构造函数（标准刻意不给）。所以架构文档 3.1 的这行写法与 `std::span` 不兼容。

三种候选的实测对照：

| 成员类型 | 文档的 `.Requires = { {...} }` | 容量上限 | 实测 |
|---|---|---|---|
| `std::span<const ServiceRef>` | **编不过** | — | 编译错误 |
| `std::initializer_list<ServiceRef>` | 通过 | 无 | 编译干净、`-Wdangling` 零警告、静态与动态初始化运行均正确 |
| `MetaArray<ServiceRef, 16>` | 通过 | 16，编译期可查 | 编译干净，溢出报错可读 |

> **诚实备注**：探针**未能复现** `std::initializer_list` 成员的悬垂。clang 把常量列表的底层数组 materialize 到了静态存储，掩盖了问题。因此不能声称它悬垂——只能说它的正确性依赖实现细节。

**选 `MetaArray`**：理由是它**自持存储**，正确性不依赖实现细节。一份要跨 Win / Linux / macOS 三套 STL 的描述符契约，把正确性押在「clang 恰好把底层数组放进了静态存储」上不划算。

```cpp
template <typename T, std::size_t Capacity>
class MetaArray
{
public:
    constexpr MetaArray() = default;

    constexpr MetaArray(std::initializer_list<T> items)
    {
        if (items.size() > Capacity)
        {
            MetaArrayCapacityExceeded();   // 只声明、不定义；常量求值中调用它 → 编译错误
        }
        for (const T& item : items)
        {
            Items_[Count_++] = item;
        }
    }

    constexpr std::size_t Size() const { return Count_; }
    constexpr const T*    Begin() const { return Items_; }
    constexpr const T*    End() const { return Items_ + Count_; }

private:
    T           Items_[Capacity]{};
    std::size_t Count_ = 0;
};
```

溢出诊断实测（3 条塞进容量 2）：

```
error: constexpr variable 'kOverflow' must be initialized by a constant expression
note: non-constexpr function 'MetaArrayCapacityExceeded' cannot be used in a constant expression
      MetaArrayCapacityExceeded();
note: in call to 'MetaArray({...})'
note: declared here
     void MetaArrayCapacityExceeded();
```

note 指向那个**名字自解释的**声明处。`is_trivially_copyable` 与 `is_standard_layout` 均通过。

> **机制变更（2026-09-15，随 D17）。** 原文此处用的是
> `throw std::length_error{"MetaArray 超出容量"}`：靠常量求值中的 `throw` 触发编译错误，
> note 直接引到作者自己写的那句话上。**D17 关闭异常之后这条写法作废**——`throw` 本身
> 就是硬错误，诊断会退化成与容量无关的一句。
>
> 改用「**只声明、不定义**」的函数：常量求值中调用非 constexpr 函数必然失败，
> 诊断质量与原文相当（都指向一处声明），且不依赖异常。函数名要自解释——
> 它就是诊断里唯一能自定义的部分。运行期若真被调用则是链接错误，正是期望的「响亮失败」。
>
> 已实测对照（异常关闭下）：正向编译零警告、反例给出上述诊断。
> 另测过一个**更差的**候选：变参构造 + `static_assert`——`requires` 子句会把候选整个删掉，
> 只剩一句 "no matching constructor"，反而丢掉了原因。

**代价**：`Requires` / `Provides` 各有 16 条上限。超限是明确的编译错误，不是静默截断。

#### (3) `Error` 的所有权

架构文档 0.3 第 6 条把 `Error::Message` 列为「边界上只传当下有效的东西」的四个实例之一。但 `OnLoad` 失败、`CreateSession` 失败都要把 `Error` 交给宿主，而返回值必须比调用活得久。

更隐蔽的一处：`ErrorContext.PluginId` 若借用视图，会指向**插件镜像内的字符串字面量**——插件一卸载即失效。而 `SessionReport` 由 `DestroySession` 返回，宿主完全可能先 `UnloadPlugin` 再读报告。

**决定**：`Error` **全部拥有**（`std::string`），并在架构文档 0.3 第 6 条加一条明确的例外说明——**错误对象是返回值，不是边界回调参数**。错误路径不在热路径上，这点分配无关紧要。

```cpp
struct ErrorContext
{
    std::string PluginId;
    std::string ServiceName;
    uint32_t    ServiceVersion = 0;
    Phase       Stage          = Phase::kLoad;
};

class Error
{
public:
    Error() = default;
    Error(std::string message, ErrorContext context)
        : Message_(std::move(message)), Context_(std::move(context)) {}

    bool                IsSet() const { return !Message_.empty(); }
    const std::string&  Message() const { return Message_; }
    const ErrorContext& Context() const { return Context_; }

private:
    std::string  Message_;
    ErrorContext Context_;
};
```

#### (4) Linux 的卸载证据

架构文档 8.2 把「卸载后尝试覆盖二进制文件，覆盖失败即卸载失败」定为唯一不可伪造的卸载证据。**这条在 Windows 上成立，在 Linux 上不成立**——`dlclose` 之后用 `rename()` 覆盖 `.so` 照样成功，旧 inode 只是被解除链接。

三条候选：

| 做法 | 疑点 |
|---|---|
| `dl_iterate_phdr` 遍历，确认条目消失 | glibc 在某些条件下不真正卸载（静态 TLS、`RTLD_NODELETE`），条目会留着 |
| 重新 `dlopen` 同一路径，断言插件的静态计数器回到初值 | 依赖插件里有可观测的静态状态，需要 fixture 配合 |
| 读 `/proc/self/maps`，确认映射消失 | 与第一条同源，但依赖 Linux 特有的 `/proc` |

**决定**：以**第二条为主判据**——它测的是**行为**而不是**簿记**，「卸载干净了」的语义本来就是「下次加载如同首次」，正是 0.2 的核心承诺本身。第一条作交叉验证。

若实测发现第一条更可靠，则架构文档 8.2 应改写成按平台分述，Windows 那条降为特例。架构文档 12.1 已预留此路：「实现时若发现某平台不可行，要回来改这一节」。

### 3.4 测试清单

覆盖架构文档 12 节中的 8 条：

| 12 节编号 | 内容 | 承重 | 落在 |
|---|---|---|---|
| #1 | 反复建销计数归零 | **是** | `Tests/Lifecycle` |
| #2 | 卸载后二进制可释放 | **是** | `Tests/Integration`（双平台分支） |
| #10 | 泄漏定位到插件 | 否 | `Tests/Lifecycle` + `LeakyPlugin` |
| #12 | `HeaderVersion` 不匹配拒绝加载 | **是** | `Tests/Integration` + `StaleHeaderPlugin` |
| #15 | 失效 `SessionHandle` 被检出 | 否 | `Tests/Lifecycle` |
| #16 | 非绑定线程调用被断言抓住 | 否 | `Tests/Integration`（death test） |
| #18 | `DestroySession` 永不失败 | **是** | `Tests/Lifecycle`（在已泄漏的 Session 上销毁） |

> `StaleHeaderPlugin` 是一个**手写描述符**的 fixture（不使用 `VASE_PLUGIN`），模拟「用旧头文件编译的插件」。这是架构文档 12 节 #12 所要求的「伪造一个旧版本号」。

M1 自有测试：

- 逆序回收顺序、`Release()` 幂等、`Release()` 后 Scope 不再碰它（架构文档 7.2 / 7.3）
- 阶段 0 → 1 → 2 的顺序断言；宿主服务先于插件可见
- `ServiceOrigin` 由注册位置决定
- `Get<T>()` 缺失服务时的报错内容（Release 下立即终止 → death test）
- `VASE_PLUGIN` 宏展开的编译期正确性（正向编译 + 容量溢出应编译失败）
- `MetaArray` 自持存储（构造出的 meta 跨语句使用仍有效）

### 3.5 M1 验收判据

1. `Embedding play/stop` 循环 N 次，9.2 五项计数全部归零
2. `UnloadPlugin` 后取得卸载证据：Windows 文件可写；Linux 重载后静态状态归零（`dl_iterate_phdr` 交叉验证）
3. 上述两条 **Win + Linux 双平台都绿**
4. 非绑定线程调用 Vase API 被 Debug 断言抓住
5. 泄漏能被 `SessionReport` 定位到具体插件 Id
6. `Get<T>()` 缺失服务时报出「插件 Id + 服务名 + 版本」
7. `-Werror` 零警告；`run-clang-tidy` 无错；`clang-format --dry-run --Werror` 全绿

### 3.6 M1 不做

Catalog、`.plugin.json`、Preset、`ConfigBlob`、配置宏反射、Waterfall、`CreateScope` 子作用域、拓扑排序、多插件、依赖求解、级联拆除、`Strict` 模式的实际触发、双版本 reload、`VaseCli`、`VasePack`、macOS。

---

## 4. 里程碑序列

```text
M0  骨架   CMake + 双平台工具链 + vcpkg + GoogleTest + 跨 DLL 冒烟
M1  闭环   Effect / Context / Host 单插件 + 卸载证据 + 诊断归零
M2  清单   Catalog + .plugin.json + 拓扑序 + 跳过分类 + 配置（宏反射 / ConfigBlob）
M3  诊断   进程级状态登记 + SessionReport 完善 + 完整属主追踪器
M4  重载   Tests/Reload 双版本 fixture（卸载 / 替换 / 重载的端到端验证）
M5  发布   VasePack + VaseCli + macOS arm64 + CI
```

**风险暴露顺序**：二进制生命周期最先（M1）→ 工具链最后（M5）。这正是 D1（纵切）的意图。

**注意**：M1 已提前吸收了原属 M4 的 `HeaderVersion`（D13）、原属 M3 的泄漏归属（D14）、以及原属 M4 的文件锁卸载证据（D3）。M4 因此收窄为**只做双版本 reload 的端到端验证**——那是唯一无法用单元测试替代、必须有真实二进制替换的测试（架构文档 12.1）。

**本 spec 覆盖两个里程碑。** 实施计划建议分开：M0 是一份独立的计划（它不产生任何架构结论，只证明构建地基成立），M1 在其中期开始。两者之间有硬依赖（M1 的跨 DLL 断言建在 M0 的冒烟测试之上），所以也可以是一份含两个阶段的计划——由 writing-plans 决定。

---

## 5. 零分配的落地

D8–D10 的组合给出如下形态：

- `EffectSlot` 记录与 `IEffect` 存储都来自 `ScopePool` 的空闲链表，**不是每次注册一次 `malloc`**
- 空闲链表不能是 bump 分配器：架构文档 7.3 的 `Release()` 是**任意顺序**释放，7.5 的子作用域更甚（反复建、反复放），LIFO 回退处理不了
- `Recycle()` **不是虚析构**：对象内存归 Scope 所有，销毁路径是「调 `Recycle()` 做逻辑撤销 → 内存还给空闲链表」。`delete` 只出现在移交式注册里，且发生在 `Recycle()` 内部——虚调用在插件镜像内执行，与 3.1 的 `VasePluginDestroy_<T>()` 同一性质
- 池挂在 `PluginHost` 上跨 Session 复用，于是「反复 Play / Stop」在稳态下零分配

**与铁律 1.2 的关系**：池是进程级容器，但它**只持有空闲节点**，不持有任何实例级对象，因此不违反「实例级对象不得被进程级对象持有」。**这一点需要在实现时写进代码注释**，否则日后有人会把它误判为违规。

---

## 6. 与架构文档未定项的关系

架构文档 13.1 的两条未定项，本设计解决情况：

| 13.1 项 | 状态 |
|---|---|
| `Effect` 的存储形态与分配成本 | **已定**：`IEffect` 虚接口 + size-class 空闲链表（D8–D10）。原「纯性能问题，实现时测量决定」的定性保留——若日后测量显示分配器是瓶颈，替换它不触及 `EffectHandle` 这个对外契约 |
| 平台差异的收口方式 | **部分解决**：收口点确定为 `Loader` 接口（`Load` / `Unload` / `Symbol` / `CollectEvidence`），M1 实现 Windows 与 Linux 两份。Unicode 路径、大小写敏感、路径分隔符等细节留待 M1 实现时逐项处理 |

---

## 7. 未决与风险

| 项 | 说明 |
|---|---|
| Linux 卸载证据的可靠性 | 3.3(4) 的三条候选需要在 M1 实测。**结论可能反过来要求修改架构文档 8.2** |
| `MetaArray` 的容量上限 | 16 是一个估计值，实现时按真实插件的 `Requires` / `Provides` 条数调整 |
| clang-cl 的 MSVC 环境探测 | 若自动探测不奏效，需要 VS Developer 环境。M0 验证项之一 |
| ~~Linux clang 版本~~ | **已闭合（2026-09-15）**：两侧对齐到 23.1.0 同源 commit；Windows 由 PATH 解析、Linux 手动装到 `/opt/llvm-23.1.0`。见 2.1.1(c) |
| CMake 4.x 构建第三方依赖 | 依赖库若声明 `cmake_minimum_required(VERSION <3.5)` 会直接报错。应急开关 `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`（2.1.1(a)）。M0 接 vcpkg + GoogleTest 时首次可能撞上 |
| PATH 解析的可复现性 | D6 把 clang-cl 的定位交给 PATH——本机成立不等于他人机器成立。**README 必须写明「将 23.1.x 的 LLVM bin 目录加入 PATH」**；M0 的 doctor 式检查（configure 前 verify `clang-cl --version` 主版本）能把这类环境差异变成响的错误而不是静默的降级 |
| `vcpkg` 与 CMake 4.4.x 的接线 | 完全未实测。vcpkg 的 toolchain 文件与 CMake 4.x 的交互、以及两侧是否解出**同一版本**的依赖，都待验证（M0 验收 #6） |
| `Result<T>` 的实现 | C++20 无 `std::expected`。M1 手写最小实现；不引入 `tl::expected`，以免把 ABI 面交给第三方 |

---

## 8. 需要回头修订架构文档的地方

本设计在验证过程中发现以下位置与实现现实不符，**建议修订 `wiki/vase-architecture.md`**。本节只记录，不在本文件内代改。

| # | 位置 | 问题 | 建议 |
|---|---|---|---|
| R1 | 8.2 | 「覆盖失败即卸载失败」只在 Windows 成立 | 改写为按平台分述；Windows 那条降为特例。待 M1 实测 Linux 主判据后定稿 |
| R2 | 3.1 | `VASE_PLUGIN` 的 `{...}` 块无法直接嵌进聚合初始化 | 补上 `PluginMeta` 拆分与前向声明的展开方式 |
| R3 | 3.1 | `.Requires = { {...} }` 与 `std::span` 不兼容（编译错误） | 明确使用固定容量内联数组，并记录容量上限 |
| R4 | 0.3 第 6 条 | `Error::Message` 被列为借用，但它必须比调用活得久；且 `PluginId` 会指向插件镜像内的字面量 | 补一条例外：错误对象是返回值，不是边界回调参数 |
| R5 | 13.2 | 缺「测试框架」「Windows 编译器」两项已决定事项 | 补入 GoogleTest 与 clang-cl |
| R6 | 附录 B | 「`std::function` 每次注册有一次分配」是一个**上界而非事实**——`std::function` 有 SBO，实际是否分配取决于闭包捕获的大小与 STL 实现。该表述会误导读者把「可能的分配」当成「必然的分配」 | 修正表述；本设计已改用 `IEffect`（D8），该条不再承重 |
| R7 | 0.3 / 13.2 / 13.3 | 缺「不使用 C++ 异常」这条设计原则及其连带后果 | 补入 0.3 原则 7、13.2 三行、13.3 一条契约束。**已于 2026-09-15 由需求方指示直接改入架构文档，不再是「建议」** |
| R8 | 8.3 | 「异常不得穿过插件边界」原本只是约定，宿主侧无法拦截 | 架构文档 0.3 原则 7 落地后成为**编译期强制**；但插件侧仍无强制力（见 13.3 新增条目） |
| R9 | 8.5 | 表格写 `cl.exe` **或** `clang-cl.exe`，读者容易理解成「任选其一」；本设计原先把实现收窄到了只支持 clang-cl | 已于 2026-09-15 补一句「Vase 本体两套都构建」及两处操作差异（`cl.exe` 需要 Developer 环境、`clang-cl` 是主构建） |

---

## 9. D17 的落地：关闭 C++ 异常之后要改什么

架构文档 0.3 原则 7 是**需求方于 2026-09-15 指定**的决定：全项目关闭 C++ 异常。本节记录它的实测依据与连带代价——只写「不用异常」是浅的，真正的信息量在它改动了什么。

### 9.1 编译开关：两侧默认不一致

已实测（本机 clang 23.1.0）：

| 驱动 | 不传开关时的默认 | 处置 |
|---|---|---|
| `clang-cl` | **异常关闭**（`try` / `throw` 报 `cannot use 'throw' with exceptions disabled`） | 显式 `/EHs-c-`（已实测合法且确实关闭），让意图可见、不被将来某个 `/EH` 开关改回去 |
| `clang++` | **异常开启** | 显式 `-fno-exceptions` |
| `cl.exe` | 异常开启，且**不传 `/EH` 时 `try` 只是 warning C4530，编译照过**（实测 exit=0） | 显式 `/EHs-c-` **加 `/we4530`** |

只写一侧会让同一份源码在两个平台行为不同。

**`/we4530` 是 D6 扩为双编译器之后的必需品。** 没有它，cl.exe 下「关闭异常」完全没有强制力——`try` 只报一条 warning 就过了。加上它之后，`try` / `catch` 变成 `error C4530`（已实测，exit=2），与 clang-cl 的硬错误对齐。

### 9.2 标准库：一批抛错 API 必须改走不抛形式

| 用法 | 处置 |
|---|---|
| `std::filesystem` | 一律用 `error_code` 重载。**这条对 Vase 是承重的**——`Loader` 要处理路径，而 `std::filesystem` 的主 API 默认抛 |
| `std::optional::value()`、`std::map::at`、`std::variant` | 改用 `*opt` / `find` / `get_if` |
| `std::stoi` / `std::stol` 系 | 改用 `std::from_chars` |
| 分配失败 | 落到终止（MSVC STL 的 `_HAS_EXCEPTIONS=0`、libc++ 的 `_LIBCPP_HAS_NO_EXCEPTIONS`）。**有意接受**：OOM 不是 Vase 能恢复的情形 |

### 9.3 第三方库

- **`nlohmann/json`**（13.2 已选它做 Catalog 的 JSON 层）：必须开 `JSON_NOEXCEPTION`，解析用 `parse(..., nullptr, false)` 并以 `is_discarded()` 判定失败。它的默认形态是抛 `parse_error`。影响面被 4.1 的「json 只出现在 `PluginCatalog` 层」限制住了。
- **GoogleTest**：`EXPECT_THROW` / `ASSERT_THROW` 系列在本项目的翻译单元里**不可用**——它们的展开体含 `try` / `catch`，在本项目 TU 中直接编不过。M1 的死亡测试（验收 #4 相关、12 节 #16）依赖的是 gtest 自己的 fork/exec 机制，不受影响。

### 9.4 三处残余风险

1. **libc++ 自身是带异常编译的。** 我们的 TU 关掉异常后，标准库内部的抛错路径若被触达，会穿过没有异常支持的栈帧。正向路径已在 M0 实测正常（`std::string` / `std::vector` / `std::optional` 编译并运行通过），**错误路径的行为未测**（留 M1）。
2. **插件侧无强制力。** 8.5 允许插件由 `cl.exe` 构建，插件作者若开着异常并真的 `throw`，宿主无法在加载期检测。这是 9.3 意义上的契约束，已记入架构文档 13.3。
3. **cl.exe 下裸 `throw` 是漏网的。** 实测：一个 TU 内只有 `throw`、没有 `try` / `catch` 时，cl.exe **静默接受**（exit=0），`/we4530` 也拦不住——C4530 只针对「使用了异常处理器」。

   这意味着 D17 的强制力在两个编译器上**不对称**：

   | 形态 | clang-cl | cl.exe + `/we4530` |
   |---|---|---|
   | `try` / `catch` | 硬错误 | 硬错误 |
   | 裸 `throw` | **硬错误** | **静默通过** |

   兜底方式是「**clang-cl 那条线必须始终构建**」——它连裸 `throw` 都拒。只跑 cl.exe 一条线时，这个形态会漏。M0 验收表第 7 行把这条差异固化成了检查项。

---

## 附录：探针文件

3.3 节的三条结论来自以下临时探针（仓库外，`D:\Git\.vase-probe\`）：

- `m1-syntax-probe.cpp` —— 正向：宏展开 + `MetaArray` + `Error` + 描述符装配，`-Wall -Wextra -Werror` 零警告
- `overflow-probe.cpp` —— 反例：容量溢出应为编译错误
- `dangling-span-probe.cpp` —— 反例：`std::span` 成员为编译错误
- `initlist-probe.cpp` / `initlist-final.cpp` —— `std::initializer_list` 成员的实测（未能复现悬垂）

**2026-09-15 追加的探针**（`%TEMP%\vase-noeh\`，一次性）：

- `metaarray.cpp` / `metaarray_bad.cpp` —— D17 之后 `MetaArray` 溢出诊断的**正向与反例**（候选 A：只声明不定义的函数）
- `variant.cpp` / `variant_bad.cpp` —— 被否决的候选 B（变参构造 + `static_assert`）
- `noeh.cpp` —— Linux + libc++ + `-fno-exceptions` 下 `std::string` / `std::vector` / `std::optional` 的编译与运行
- `warnprobe.cpp` —— `-Wall` 与 `/W4` 在 clang-cl 下的警告条数对照

> **一处需要留意的历史条件**：上面第一批探针的编译命令记作 `-Wall -Wextra -Werror`，却报告「零警告」。按 2.1.1(d) 的实测，clang-cl 的 `-Wall` 等价于 `-Weverything`，配上 `-Werror` 几乎不可能零警告——**那批探针应当是用 GNU 驱动的 `clang++` 跑的**，不是在 clang-cl 下。
>
> 这不影响它们关于**语言语义**的结论（宏展开、`std::span` 不可初始化、`initializer_list` 的存储），那些是前端行为、两驱动一致；但**与警告级别、默认 CRT、默认异常相关的结论不能从它们外推**。M1 落地时以 clang-cl 重跑一遍为准。
