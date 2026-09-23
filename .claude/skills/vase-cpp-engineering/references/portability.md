# 可移植性：平台、工具链、以及差异的收口

---

## 1. 目标平台（需求方指定）

**开发 + 发布**：Win x64 / Linux x64 / macOS arm64
**仅发布**：Android arm64 / iOS arm64

**但仓库当前实际构建的只有 Win x64 与 Linux x64 两棵。** macOS / Android / iOS 的代码路径与构建都还没落地——`Loader` 里 macOS 的 `_dyld_*` 分支、移动端的实现都不存在。**不要把 §8.5 的平台矩阵读成"已经支持"**，它是目标不是现状。

`Loader` 里今天的平台分支是二选一（`Source/Host/CMakeLists.txt`）：

```cmake
if(WIN32)  ImageInspectWindows.cpp  LoaderWindows.cpp
else()     ImageInspectPosix.cpp    LoaderPosix.cpp
```

---

## 2. 工具链与 STL 矩阵（§8.5）

| 平台 | 编译器 | STL |
|---|---|---|
| Windows | `clang-cl` **或** `cl.exe`（两者都支持） | MSVC STL |
| Linux / macOS / Android / iOS | `clang` | libc++ |

**Windows 上支持两个编译器，正是为了让"宿主与插件可以各用一个"不只是纸面约定。** 但注意它们**不等价**——见第 5 节。

工具链文件与 triplet 的清单以根 `CLAUDE.md`「构建与测试」与「工具链 flag 是承重的」两节为准（后者还写明每条承重 flag 落在哪个文件上）。**要记住的是这个判断**：这些文件只讲编译器定位、vcvars 与 STL 选型（triplet 管 vcpkg 侧的 STL 选型），**不涉及异常设置**——异常在根 `CMakeLists.txt` 的 `VaseBuildOptions` 里，清单见规矩 1。别去工具链文件里找异常 flag。

### 版本由 configure 守，但守不全

`CMakeLists.txt` 只校验 **CXX 编译器**的 clang **主版本**（钉的是哪个主版本，以根 `CLAUDE.md`「静态检查与格式」为准）；`cl.exe` 分支只查平台、**不查版本**。

> **它管不到 `clang-format` / `clang-tidy` 这两个独立可执行文件。** "CXX 编译器的 clang 落在被钉的那个主版本"不等于"tidy / format 也同版本"——后者由环境（PATH 最前）保证，不是 configure 保证的。

---

## 3. flag 是承重的，不是优化

`/DEBUG:FULL`（Windows 两条线）与 `-Wl,--build-id=sha1`（Linux）**不是可选的优化，是身份特征的构建要求**——**两条 flag 的字面值与它们落在哪三个工具链文件，以根 `CLAUDE.md`「工具链 flag 是承重的」那节为准**。摘掉的症状是**运行期的响亮失败**：构建照常全绿，直到 Adopt 全线拒绝才暴露。详见 `abi-boundary.md` 第 5 节。

改这两条 → 必重跑 `Tests/HotSwap/` 主循环，双平台各自留证据。

**另一个 configure 期的坑**：`CMAKE_*_FLAGS_INIT` **只在工具链首次 configure 时进入缓存**。工具链后来才加上新 flag 而某棵树在那之前就配过，那棵树就是空的（实测：`LoadProbe.dll` 没有 `.pdb`，`Loader.MemoryIdentityMatchesFileIdentity` 当场失败）。**修法是删掉那棵树重新 configure。**

---

## 4. 平台差异必须收口在 `Loader`

`Loader` 是**平台差异的唯一收口**（§13.1）。新加平台相关代码时：

- 往 `Source/Host/` 加一对 `*Windows.cpp` / `*Posix.cpp`，接口由 `Source/Host/ImageInspectPlatform.h` 声明（**Host 内部头，不进公开头**）；
- 不要在主逻辑里散落 `#ifdef _WIN32`。今天的 `#ifdef` 集中在 `Detail/Export.h`（可见性宏）与工具链文件里。

三个平台的**证据判据力本来就不一样**，这件事写进了结构而不是文档：`UnloadEvidence` / `EjectReport` 里成对出现"字段值"与"本平台该字段有没有判据力"（`MappingRemovalIsObservable` / `ReopenWritableIsMeaningful`）。**加平台字段时跟着这个形状走。**

---

## 5. 两个平台之间的具体差异（踩过的）

| 差异 | 具体事实 |
|---|---|
| **符号可见性** | Windows 默认不导出（必须 `dllexport`）；Linux/macOS 默认**全开**，必须靠 `CXX_VISIBILITY_PRESET hidden` 收紧。两个 DLL 与所有插件 target 都设了它 |
| **`dlopen` 选项** | Linux/macOS 一律 `RTLD_NOW \| RTLD_LOCAL`，不给宿主选项。Windows 无此开关 |
| **`alignof(std::max_align_t)`** | MSVC STL 是 **8**，libc++ 是 **16**。所以 `ScopePool::kAlignment` **写死 16**，不用 `max_align_t`——否则 `alignas(16)` 的 Effect 在 Linux 编得过、Windows 编不过 |
| **`std::array::begin()` 的返回类型** | MSVC STL 返回迭代器类**而非指针**。所以 `MetaArray::End()` 必须从 `Begin()` 用 `std::next` 起算，不能写 `Items.data() + Count` |
| **CRT 选型** | `clang-cl` 与 `clang` 的驱动默认都是**静态 CRT**（`/MT`）——静态 CRT 让每个动态库各持一份堆，跨模块 new/delete 落到不同的堆上，正是 §8.3 禁止 `x64-windows-static` 的理由。仓库在根 `CMakeLists.txt` 里**显式设** `CMAKE_MSVC_RUNTIME_LIBRARY` 为动态调试/发布版 CRT（`MultiThreaded$<$<CONFIG:Debug>:Debug>DLL`），注释原话「不该靠一个可被他人改动的默认值兜着」。**那行不是冗余，别删**；实测 Debug 下传给 clang-cl 的是 `-MDd`。**这是 §8.3「同一 CRT 配置」那条前提的落点** |
| **`/EHs-c-` 的执行力不等价** | `clang-cl` 关闭异常时 `try` / `throw` / `catch` **全是硬错误**（完整强制）；`cl.exe` 加 `/we4530` 也**拦不住裸 `throw`**（实测 exit=0）。这一形态靠 clang-cl 构建兜住——**只跑 cl.exe 线时它是漏网的** |
| **DLL 搜索** | 构建产物必须让可执行与 DLL 同处 `bin/`——这是 Windows 能找到 DLL 的前提。**不要改 `CMAKE_RUNTIME_OUTPUT_DIRECTORY`** |

---

## 6. 尚未落地的平台风险（记着，别当已解决）

- **macOS 有比 glibc 宽得多的 neverUnload 清单**（dyld4）：ObjC/Swift 元数据、**带析构函数的静态对象（static terminators）**、主程序静态依赖、`RTLD_NODELETE`——命中时 `dlclose` **返回 0 但镜像永在**。纯 C++/libc++ 栈里的主嫌疑是 terminators：**一个普通的全局 C++ 对象就可能触发。** 不设"禁用静态析构"的契约（对正常 C++ 伤得太深），登记为风险，由 M5 用真实测量回答"macOS 上哪些插件可热卸"。
- **移动端**：Apple 平台**禁下载代码 `dlopen`**——Android / iOS 上的任何热插拔都不做，档 ① 也不给例外。发布形态下插件必须静态链接。
- **libc++ 自身是带异常编译的**，其内部抛错路径若被触达，会穿过我们关闭异常的栈帧。正向路径已实测正常，错误路径行为待实测。

---

## 7. 写跨平台代码时的检查

1. 这段代码在**另一棵树**上会被编译吗？——平台专属 TU（`*Posix.cpp` / `*Windows.cpp`）**天然只在一条线上被 clang-tidy 检查**，它们的告警只有 Linux 线看得见。
2. 它依赖 STL 的哪种行为？MSVC STL 与 libc++ 在容器内部布局、迭代器类型、`max_align_t` 上都有差异。
3. 它碰了符号可见性吗？新增的公开符号要 `VASE_EXPORT`，其余保持 hidden。
4. 它写进文件系统了吗？路径处理用 `<filesystem>` 的**带 `error_code` 重载**，且注意 `Loader` 内部把路径**绝对化**后再入表。
5. 它假设了字节序 / 指针对齐 / 结构体布局吗？描述符里的 `static_assert(offsetof(...))` 是这类假设的标准写法——**新增时照做**。
