# Vase
Vase is a plugin framework that treats every module like a branch in flower arranging — carefully selected, gracefully placed, and cleanly removed.

## 环境前提

- **CMake ≥ 4.4**，**Ninja**（任一近期版本）。
- **clang / clang-cl 23.1.x**，且**由 PATH 解析**。Windows 侧把 LLVM 的 `bin`
  目录加入 PATH 最前（本机为 `D:\Developer\LLVM\bin`）。仓库不写死任何 LLVM
  安装路径。
  - **但「只改 PATH 就够」这个直觉不准确**：解析结果会进构建树的缓存，光改 PATH
    不够，换 LLVM 后必须**干净 configure**（新构建树，或删掉缓存），
    **同一个 build 目录里重跑 `cmake --preset` 不算数**——它复用缓存，不会重新解析 PATH。
    - Linux 侧：`find_program` 的结果缓存为 `VASE_CLANGXX_EXECUTABLE`
      （实测当前值 `/usr/local/bin/clang++`），可以用
      `cmake --preset <p> -U VASE_CLANGXX_EXECUTABLE` 单独把它踢掉。
      **这条只实测到「重跑不报错，且解析结果仍为该路径」**（在 PATH 未变的前提下），
      它证明的是「该命令不破坏现有构建树」——**区分不出「重新解析」与「复用旧值」**。
      「换版本后 `-U` 是否真能让新版本生效」**未孤立验证**，故保留此写法作为
      建议处置而非既有事实；真换版本时请另用新构建树核对一次。
    - Windows 侧：缓存里的是**名字** `clang-cl`，而**解析出来的路径**
      （形如 `D:/Developer/LLVM/bin/clang-cl.exe`）另记在
      `build-win/<preset>/CMakeFiles/<CMake 版本>/CMakeCXXCompiler.cmake` 里
      （均已实读缓存确认）。这条路没有 `-U` 的等价写法，删构建树最省事。
  - 两侧 LLVM 必须同版本：`clang-format` / `clang-tidy` 的判据一致性依赖此条。
  - **configure 阶段的版本校验只覆盖 CXX 编译器**（clang 需为 `23.x`；cl.exe 分支
    只查平台），**管不到 `clang-format` / `clang-tidy` 可执行文件本身**。
    所以「CXX 编译器版本不符」会在 configure 报错，但「tidy / format 版本不符」
    不会——它由 PATH 保证，需要自己核。
- **vcpkg**，钉定 commit `114d9fe62faf35856b45cf55cb93b57028a45d63`，
  并由环境变量 `VCPKG_ROOT` 指向其根目录（Windows：`D:\Developer\vcpkg`；
  WSL：`/mnt/d/Developer/vcpkg-linux`）。
  - Linux/WSL：`VCPKG_ROOT` 由 `/etc/profile.d/vcpkg.sh` 提供，登录 shell 可见；
    **非登录非交互的 `bash -c` 拿不到它**，这类调用方（CI 步骤、构建钩子）必须
    自己显式带上 `VCPKG_ROOT`。这是 shell 的加载规则，配置改不了。
- **Linux 侧要 libc++，不是 libstdc++**（8.5）。clang 装在 `/opt/llvm-23.1.0`，
  libc++ 头/库在 `/usr/local/lib` 与 `/opt/llvm-23.1.0/include/c++/v1`。
  仓库用自定义 triplet `Cmake/Triplets/x64-linux-libcxx.cmake` 把它传给 vcpkg
  的依赖（目前是 gtest）——内置 `x64-linux` 走 libstdc++，两侧 STL 不一致会
  让「跨 DLL 的 C++ 约束」（8.3）不成立。
  - `/usr/local/bin/clang++` 是指向 `/opt/llvm-23.1.0/bin/` 的**符号链接**，
    工具链文件会把它解析成「真实目录 + 仍叫 clang++ 的名字」。这两件事都不能省，
    理由（模块扫描找不到 libc++ 头 / 驱动退化成 C 模式不带 `-lc++`）写在
    `Cmake/Toolchains/linux-x64-clang-libcxx.cmake` 的注释里，改那一带前先读。
- **MSVC 那条线要走 `Scripts\msvc-env.cmd`**：cl.exe 依赖 `vcvars64.bat` 设置的
  `PATH` / `INCLUDE` / `LIB`，该脚本用 vswhere 定位 VS 并 call vcvars64，然后执行
  传入的命令，例如 `Scripts\msvc-env.cmd cmake --preset win-x64-msvc-debug`。
  - 注意 `vcvars64.bat` 会**把 `VCPKG_ROOT` 改写成 VS 自带的那份 vcpkg**
    （实测 VS 18 改为 `...\Community\VC\vcpkg`）。这不是「清空」，而是「覆盖」，
    空值检查拦不住它。`msvc-env.cmd` 会在 call vcvars 之前记住调用者的
    `VCPKG_ROOT`，并在之后按两种情况处理：调用者有值时还原它，**无值时把
    vcvars 塞进来的值清掉**。后者是有意的——清掉会让工具链那句「未设置」的
    守卫当场报错，而不是让 MSVC 线静默用上非钉定的 vcpkg。故在没设
    `VCPKG_ROOT` 的机器 / CI 上跑 MSVC preset，会在 configure 阶段明确失败，
    这正是期望行为，处置就是设好 `VCPKG_ROOT=D:\Developer\vcpkg`。
    若在 Developer 环境里手工配，请自己保证 `VCPKG_ROOT` 指向
    `D:\Developer\vcpkg`。
  - `vcvars64.bat` 自身会向 stderr 打一行 `'vswhere.exe' is not recognized ...`，
    属于 VS 脚本的噪音，与本仓库无关，可忽略（实测不传任何参数直接 call 也会出现）。
- **Git Bash 下手动调用 clang-cl 时注意**：MSYS 会把 `/MD`、`/nologo` 这类
  `/` 开头的参数当成路径转换掉（实际报错会指向 `D:/Program Files/Git/MD`
  之类的路径）。手动调用前先 `export MSYS2_ARG_CONV_EXCL='*'`，或改用等价的
  `-` 前缀写法。CMake 自己调编译器不受影响，只有命令行手敲时才会撞上。

## 构建与验收

### Windows

```bash
cmake --preset win-x64-clang-debug          # 或 win-x64-clang-release
cmake --build --preset win-x64-clang-debug
ctest --preset win-x64-clang-debug
```

cl.exe 那条线同构，但**必须经 `Scripts\msvc-env.cmd` 包一层**（原因见「环境前提」）：

```bash
Scripts/msvc-env.cmd cmake --preset win-x64-msvc-debug
Scripts/msvc-env.cmd cmake --build --preset win-x64-msvc-debug
ctest --preset win-x64-msvc-debug           # ctest 不调编译器，不必包
```

`Scripts\win-verify.cmd` 是上面两条线的脚本形态（四棵 preset 树各自 `rmdir /s /q` +
configure + build + ctest + `ctest -N`，每步打印退出码，末尾汇总失败步骤并以**失败步数**
作退出码）——`Scripts/linux-verify.sh` 的 Windows 对位件。它自己 `cd` 到仓库根，从任意目录
都能跑；Linux 侧「先落脚本、再 `bash -lc 'bash <脚本>'`」那条规矩（Git Bash 会先展开
`$VAR`）与它无关，`.cmd` 不经 Git Bash。

```bat
Scripts\win-verify.cmd
```

六个 preset 全绿（最近一次全量验收：M1-T14，2026-09-17——六棵树**全部删树重配**后逐线
configure → build → ctest → `ctest -N`，构建零警告）：`win-x64-{clang,msvc}-{debug,release}`、
`linux-x64-clang-{debug,release}`。Windows 侧构建树落 `build-win/<presetName>/`，
可执行与 DLL 同处 `bin/`——这是 Windows 运行时能找到 DLL 的前提，
不要改 `CMAKE_RUNTIME_OUTPUT_DIRECTORY`。

> `ctest` 在一个测试都没发现时**同样返回 0**。所以「测试全绿」不能只看退出码，
> 要另跑一次 `ctest --preset <p> -N`，把 `Total Tests` 与下表逐位对上。
> `gtest_discover_tests` 用的是 `DISCOVERY_MODE PRE_TEST`，用例枚举发生在 ctest
> 运行时，测试被漏注册时 `ctest` 会一声不吭地报成功。

**各线 `ctest -N` 基数**（与 `CLAUDE.md` 同源）：

| preset | `Total Tests` | 与 Win debug 的差 |
|---|---|---|
| `win-x64-{clang,msvc}-debug` | 75 | —（基线） |
| `win-x64-{clang,msvc}-release` | 74 | −1：T3 的 death test 受 `#ifndef NDEBUG` 门 |
| `linux-x64-clang-debug` | 77 | +2：T11 的两条 Linux-only（`Adopt.MissingIdentityFeatureRejectedWithPointer`、`Adopt.RenameReplacementCaughtByTierThree`） |
| `linux-x64-clang-release` | 76 | 同上两点相抵：+2 −1 |

**基数差是设计，不是漏注册**：debug/release 差的 1 条是 death test（`#ifndef NDEBUG` 门），
Linux/Windows 差的 2 条是 Linux-only 用例（`-Wl,--build-id=none` 的 fixture 只在 Linux 存在）。

### Linux / WSL

源码在 Windows 侧（`D:\Git\Vase`），WSL 里的路径是 **`/mnt/d/Git/Vase`**；
构建树落 **`build-linux/<presetName>/`**（Windows 侧是 `build-win/`）。

**`cmake` 必须经由登录 shell 调用**，否则 `VCPKG_ROOT` 是空的、configure 当场失败：

```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --preset linux-x64-clang-debug'
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug'
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && ctest --preset linux-x64-clang-debug'
```

`linux-x64-clang-release` 同构。`bash -c`（不带 `-l`）拿不到 `VCPKG_ROOT`——
那是 shell 的加载规则，不是配置能改的，这类调用方得自己显式带上它。

`Scripts/linux-verify.sh` 是这套流程的脚本形态（两棵 preset 各自 `rm -rf` + configure +
build + ctest + `ctest -N`，每步打印退出码）——本项目在 WSL 侧一律用「先落脚本、再
`bash -lc 'bash <脚本>'`」的写法，避开 Git Bash 先展开 `$VAR` 的老坑：

```bash
wsl -d Ubuntu -- bash -lc 'bash /mnt/d/Git/Vase/Scripts/linux-verify.sh'
```

Linux 侧**不需要** Windows 那样的 DLL 路径处理：CMake 会把链接到的共享库目录
写进构建树的 RPATH（我们的库在 `lib/`，vcpkg 的 gtest 在 `vcpkg_installed/.../lib`），
`ctest` 直接就能跑。

### 静态检查与格式

```bash
run-clang-tidy -p build-win/win-x64-clang-debug          # Windows（clang-cl 线）
run-clang-tidy -p build-linux/linux-x64-clang-debug      # Linux（同样经登录 shell）
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' \
  | xargs -0 clang-format --dry-run --Werror
```

- **三条 tidy 线都有脚本形态，且契约一致**：`Scripts/linux-clang-tidy.sh`（Linux，经登录
  shell）与 `Scripts\win-clang-tidy.cmd`（Windows 两条 debug 线；带参数 `clangcl` / `msvc`
  可单跑一条，不带则两条都跑）。日志落在脚本旁边（`*.log`，被 gitignore），stdout 打全三条
  判据（`run-clang-tidy` 退出码 + 正文 `error:` 条数 + 正文 `warning:` 条数）与摘要计数，
  **退出码非 0 就是门禁未过**，不必再手工 grep 日志；基数仍以 `CLAUDE.md` 的表为准，脚本
  不复制阈值：

  ```bash
  wsl -d Ubuntu -- bash -lc 'bash /mnt/d/Git/Vase/Scripts/linux-clang-tidy.sh'
  Scripts\win-clang-tidy.cmd
  ```

- `clang-format` 那条与 build 目录无关，全仓一条命令，两侧共用。**扩展名要列全**
  （`.h` / `.hpp` / `.cpp` / `.cc` / `.ixx`）——只列 `*.h` / `*.cpp` 时，将来出现的
  `.hpp` / `.cc` / `.ixx` 会被 glob **静默跳过**，门禁照样绿。同理 `git ls-files` 要带
  `--cached --others --exclude-standard`，否则**尚未 `git add` 的新文件**也会被静默跳过。
- **「Windows 两条线绿」不能推「Linux 绿」。** 同一版 clang-tidy（两侧均 23.1.0）跑两棵树，
  T11 实测（修复前）Linux 线报 **9 条**正文 warning，Windows 线 **0 条**。其中两类要分清：
  **两条落在两侧都编译的共享文件上**（`Source/Pod/Pod.cpp:88`、`Source/Host/PluginHost.cpp:196`
  的反向 `for`，`modernize-loop-convert`），Windows 线就是不报——成因是 **STL 不同**
  （MSVC STL 的 `rbegin()` 走另一条路径），**不是版本差**；其余 7 条落在
  `LoaderPosix.cpp` / `ImageInspectPosix.cpp`，那两个 TU **在 Windows 上根本不编译**。
  所以**三条 debug 线各自跑、各自读正文**。
- **`run-clang-tidy` 退出 0 不等于「没有 warning」**：`.clang-tidy` 的
  `WarningsAsErrors` 为空（既有且经 spec 认可），tidy 永远不会因 warning 失败。
  实测两侧都另有大量「已生成、已抑制」的 warning，**只出现在摘要行里**，既不影响
  退出码，也不被 `grep warning:` 命中：

  ```
  998 warnings generated.
  Suppressed 998 warnings (998 in non-user code).
  ```

  （摘要行**每个 TU 各打一次**。2026-09-17 实测基数：clang-cl 与 cl.exe 两条线各 48 个 TU、
  Suppressed 合计 **309464**；Linux 线 49 个 TU、合计 **133833**。两者都**全部落在非用户代码**里，
  我们自己的代码零正文 warning。摘要行里还会出现 `N NOLINT`——那是抑制的**命中次数**
  （同一处抑制会被每个包含它的 TU 各计一次），不是仓库里的抑制处数。）

  所以「没有新 warning」的判据是**三条一起**：**退出 0 + 正文 `error:` 0 条 +
  正文 `warning:` 0 条**，再连摘要行一起读。只看退出码会漏掉全部被抑制的量；
  只 grep `warning:` 也会——被抑制的那些不以 `warning:` 形式出现。
- `run-clang-tidy` 在三条 debug 线上都能跑，但 **cl.exe 线要追加一个开关**：

```bash
run-clang-tidy -p build-win/win-x64-msvc-debug -extra-arg=-Wno-unused-command-line-argument
```

  `VaseBuildOptions` **只在 cl.exe 那条线**加 `/external:anglebrackets`（clang-cl 线上
  **没有它的等价物**），而 clang-tidy 的前端是 clang，不认这个 MSVC 专有开关，会报
  `argument unused during compilation: '/external:anglebrackets'`；该诊断又被 MSVC 线的
  `/WX`（等价 `-Werror`）提升为 `error:`，于是整条命令非零退出。补上前面那个开关后
  两条线的结果**逐条一致**（实测：修这些文件之前各报 20 条、集合相同；修完各 0 条），
  故验收口径「无 `error:`」两条线都成立。**这是命令行开关，不是 `.clang-tidy` 检查集的
  问题——不要为此改检查集。**
  代价：该开关同时会压掉其它「参数未被使用」的提示，将来若有人给 MSVC 线写错一个
  编译选项，tidy 不会再提醒。

- **两条线在「第三方头」这件事上并不对称**，别把 `-imsvc` 当成我们的开关：
  clang-cl 的 Tests TU 上那个 `-imsvc <vcpkg>/include` 来自 **CMake 对 `GTest::gtest_main`
  导入目标包含目录的 SYSTEM 处理**，与本项目无关（实测：两个库 TU 上根本没有 `-imsvc`，
  因为它们没链 gtest；cl.exe 线对应的是 CMake 生成的**另一种形式**
  `-external:I <vcpkg>/include -external:W0`）。只有 cl.exe 线上才有**我们自己的**
  `/external:anglebrackets /external:W0`——这正是为什么 `CMakeLists.txt` 里那条
  「Vase 自己的头必须用引号包含」的前向陷阱、以及 `CLAUDE.md` 规矩 3，**只对 cl.exe 线成立**。

### 关于「不用异常」这条编译设置的两个边界

全项目以**关闭异常**的方式编译（Windows `/EHs-c-`，Linux/macOS `-fno-exceptions`；
统一由根 `CMakeLists.txt` 的 `VaseBuildOptions` 施加）。下面两条是读这套 flag 时
最容易想反的地方，完整实测记录在根 `CMakeLists.txt` 的注释里，这里只给结论。

- **`/external:W0` 管不着 C4530。** 该开关确实能把第三方头（连模板实例化出来的也
  算）的警告整体豁免出 `/WX`——实测对 C4189 有效。但 MSVC STL 自带 `try/catch` 的
  那个 `__msvc_ostream.hpp` 报出的 **C4530 不受它管辖**：实测「加上
  `/external:anglebrackets /external:W0`、且不加 `/WX` 也不加 `/we4530`」时，C4530
  照旧打印。**所以不能靠 `/external:W0` 把 STL 的异常代码从 `/we4530` 的射程里摘出去。**
  不变式（改动这一带必须重跑）：**我们自己的代码不受影响**——往自己的 `.cpp` 里插一个
  `try/catch`，cl.exe 线仍报 `error C4530`（exit 2）。
- **`_HAS_EXCEPTIONS` 不由 `/EH` 推导。** MSVC STL 在没有显式定义时一律默认它为 1，
  所以 `/EHs-c-` 这一个开关并不足以让 STL 走无异常路径。仓库在 Windows 侧显式
  `-D_HAS_EXCEPTIONS=0`（实测两条线都置 0）。
  **语义代价**：置 0 之后 STL 的**前置条件失败从「抛异常」变成「进程终止」**——
  `vector::at` 越界、`std::stoi` 解析失败、`<filesystem>` 的 error overload 等，
  全部变成 abort，没有可接住的东西。这套无异常策略下这是预期行为，但它意味着
  **错误处理必须走 `Result<T>` / `Error` 显式返回**，不能指望 STL 的前置条件检查
  给出可恢复的失败。另：`_HAS_EXCEPTIONS=0` 是 MSVC STL 的**非官方支持**配置，
  STL 升级时需要重新验证。

## 目录结构

M1 结束时的形态（目录布局由架构 v3 第 10 节确认）：

```text
Vase/
├── CMakeLists.txt  CMakePresets.json  vcpkg.json
├── Cmake/
│   ├── Toolchains/        windows-x64-clangcl.cmake、windows-x64-msvc.cmake、
│   │                      linux-x64-clang-libcxx.cmake      （承重 flag 在注释里）
│   ├── Triplets/          x64-linux-libcxx.cmake
│   └── VasePluginHelpers.cmake   vase_add_plugin_fixture：插件 target 的唯一出口
├── Include/Vase/          公开头
│   ├── Plugin.h  PluginDescriptor.h
│   └── Detail/  Effect/  Service/  Event/  Pod/  Host/
├── Source/
│   ├── Pod/               → target VasePod（效果/服务/事件/Pod/账本）
│   └── Host/              → target VaseHost（链 VasePod；Loader、PluginHost、证据）
├── Samples/
│   ├── HelloCommon/       提供方与消费方共用的接口 + 事件头
│   ├── HelloPlugin/       最小插件：Provide 一个服务 + On 一个事件
│   └── Embedding/         验证宿主 VaseEmbedding
├── Tests/
│   ├── Smoke/             跨 DLL 冒烟（M0 保留）
│   ├── Unit/  Lifecycle/  Integration/  HotSwap/  Abi/
│   ├── TestingSupport/    PodTestPeer：M1 的形态注入缝
│   └── */fixtures/        各层要真实二进制的插件 fixture（一律经 vase_add_plugin_fixture）
├── Scripts/msvc-env.cmd   cl.exe 那条线的环境入口
├── docs/superpowers/{plans,specs}/
└── wiki/vase-architecture.md
```

构建树 `build-win/` 与 `build-linux/` 不在版本控制内。

## 运行示例：`VaseEmbedding`

`Samples/Embedding` 是验证宿主（v3 §10.1：没有 UI、没有业务，只有演示命令），
证明「干净地起、干净地灭、可重复无数次」。产物与各 DLL 同处 `<构建树>/bin/`：

```bash
build-win/win-x64-clang-debug/bin/VaseEmbedding play       # 跑一局，打印 PodReport
build-win/win-x64-clang-debug/bin/VaseEmbedding loop 20    # 连跑 20 局；任一局不 Clean 就非零退出
build-win/win-x64-clang-debug/bin/VaseEmbedding swapdemo   # play → eject → adopt → stop，逐步打印报告
```

`loop 20` 也是 `ctest` 里 `EmbeddingLoop20` 这条用例的命令（判据「反复建销计数归零」的
自动化形态）。Linux 侧路径同构，在 `build-linux/<preset>/bin/`。
