# VasePack 移动端发布 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在仓库内完成 Android arm64 静态包 → JNI `.so` 集成，以及 iOS arm64 静态归档与最终应用链接验证。

**Architecture:** 宿主原生伴生构建执行既有描述符生成器，生成候选清单并作为应用只读资源打包。Catalog 新增内容导入入口，与目录导入共用解析及快照规则；移动应用通过静态双入口走现有 Solve / Host 整包预检。移动端框架与插件静态链接，文件来源装载明确拒绝。

**Tech Stack:** C++20、CMake/Ninja、vcpkg；Android NDK r30 / Clang 21、Java/AGP 最小 APK 壳；iOS 独立 SDK 18.2 / MacPorts Clang 23。

**Spec:** [移动端发布设计](../specs/2026-10-15-vasepack-mobile-design.md)，以 2026-10-16 grilling 最终共识修订版为准。

## Global Constraints

- 分支 `vasepack-mobile`；当前用户工作树就地执行，不另建分支或迁移已有文件。
- Android A：Vase 与插件静态链入 JNI `.so`；iOS 静态链入应用。Android B、热插拔、CI/CD、独立 SDK 分发不在本波。
- 最低 Android API 29 / iOS 15.0；Android ABI 为 arm64-v8a；iOS 为 iPhoneOS arm64，不新增 macOS arm64 或模拟器支持。
- 不安装 Xcode；iOS SDK 路径来自本地环境。编译链接打包不依赖连接手机，目标执行证据单列。
- 正常启动只读资源：不生成、改写或复制清单到磁盘，不引入包准备库或多包目录替换。
- C++ 描述符是元信息唯一真值；MANIFEST 仅贡献 enabledByDefault。宿主与目标元信息须一致，漂移拒绝，不自动修正。
- 保持单线程 Host、Context/Effect 注册通道和插件只依赖 Pod；所有自有二进制目标链接 VaseBuildOptions。
- 无异常；不使用 EXPECT_THROW 一族；JSON 解析/序列化仅在 Source/Catalog，不泄露 nlohmann 到公开头。
- 不改描述符布局、PackageEntryPoints 或 kHeaderVersion；Catalog 宿主 API/ABI 增量明确记录。
- `.claude/settings.local.json` 不属于本任务，不编辑、不暂存、不提交。
- 常规构建/检查命令、基数与提交门禁只引用根 CLAUDE「构建与测试」「静态检查与格式」「核这些门禁时，退出码单独用是不够的」。每项完成先定点验证；提交前补齐根规则要求的门禁，不用定点绿灯代替全量。

## Review Focus

1. 资源缓冲区已释放、后续失败导入：Catalog 必须自持值，旧快照及借用窗保持；归任务 1。
2. 新候选生成失败但旧文件仍在：APK 构建必须失败，不能继续打包旧资源；归任务 4、6。
3. 只有禁用成员发生目标元信息漂移：参与包仍须在任何插件运行前拒绝；归任务 2、6。
4. JNI 归档漏链/负对照成员被全量提取：从干净最终链接检查符号，并做会红的变异；归任务 5、7。
5. iOS 最终链接误用 macOS SDK/归档/运行库：检查全部输入架构及最终 LC_BUILD_VERSION / 依赖；归任务 7。

## 文件职责与依赖顺序

| 单元 | 文件 | 职责 |
|---|---|---|
| 资源导入 | Include/Vase/Catalog/{PackageManifest,PluginCatalog}.h；Source/Catalog/{PackageJson,PluginCatalog}.cpp；新 Detail/CatalogSnapshot.{h,cpp} | 内容解析、事务快照、资源归因 |
| 桌面证人 | 新 Tests/Integration/CatalogResourceTests.cpp；Tests/Unit/PackageJsonTests.cpp；Tests/StaticLink/PackageAssemblyTests.cpp | 两入口同源、所有权、真实静态整包预检 |
| 构建选择 | 新 Cmake/VaseBuildProfile.cmake；根 CMakeLists.txt / vcpkg.json；Source 三个 CMakeLists；Include/Vase/Detail/Export.h | desktop / manifest-host / mobile 配置，静态框架及依赖裁剪 |
| 包与宿主生成 | 新 Samples/Mobile/{CMakeLists.txt,Packages.cmake}；新 Cmake/VaseMobileResources.{cmake,h.in}；Tests/Package/CMakeLists.txt | 共用包定义、候选清单、资源名表 |
| 移动工具链 | 新 Cmake/Toolchains/{android-arm64-clang-libcxx,ios-arm64-clang-libcxx}.cmake；新 Cmake/Triplets/arm64-ios-libcxx.cmake；CMakePresets.json | 官方 NDK、独立 iOS sysroot、隔离构建树 |
| 静态平台 | 新 Source/Host/LoaderStatic.cpp / ImageInspectStatic.cpp；Source/Host/CMakeLists.txt | 文件装载拒绝及共享层必须满足的平台接口 |
| 共用应用证人 | 新 Samples/Mobile/{MobileScenario.h,MobileScenario.cpp} | 从资源导入到反复建销的完整调用链 |
| Android 接入 | 新 Samples/Mobile/Android/JniBridge.cpp 及 apk/ 下 Gradle、Java、Manifest、instrumentation 文件 | APK 资源读取、JNI 边界与设备结果 |
| iOS 接入 | 新 Samples/Mobile/Ios/{BundleResources.h,BundleResources.cpp,main.cpp} | 真实资源读取接线与 iPhoneOS 最终链接证人 |
| 验证与文档 | 新 Scripts/{VerifyMobileSymbols.py,android-mobile-verify.ps1,ios-mobile-verify.sh}；根 CLAUDE、README、wiki、既有技能 references | 干净链接证据、门禁、平台支持状态 |

执行顺序：1 → 2 → 3 → 4 → 5 → 6 → 7 → 8。任务 1、2 可在已有桌面环境独立交付。
现有源码的具体行号可能漂移，执行时先用 CodeGraph 查符号并读其契约，不凭计划中的路径猜接口。

## Task 1：Catalog 只读资源入口与共用快照

**Files:** 修改 Include/Vase/Catalog/{PackageManifest,PluginCatalog}.h、Source/Catalog/{PackageJson,PluginCatalog}.cpp、Source/Catalog/Detail/ManifestFields.{h,cpp}、Source/Catalog/CMakeLists.txt、Tests/Unit/PackageJsonTests.cpp、Tests/CMakeLists.txt；创建 Source/Catalog/Detail/CatalogSnapshot.{h,cpp}、Tests/Integration/CatalogResourceTests.cpp。

**Interfaces:** 在 vase 命名空间新增：

```cpp
struct PackageResource
{
    std::string_view Location;
    std::string_view Contents;
};
VASE_CATALOG_API Result<PackageManifest> ParsePackageManifest(std::string_view contents, std::string_view location);
// PluginCatalog 的公开成员：
Result<void> RefreshResources(std::span<const PackageResource> resources);
```

CatalogSource 尾追加 `std::string ResourceLocation`；文件来源只填 ManifestPath，资源来源只填 ResourceLocation。
内部共用 `catalog_detail::PendingCatalogSnapshot`，其 Entries / Sources / Warnings 分别为 vector<ManifestEntry> / vector<CatalogSource> / vector<CatalogWarning>。
共享签名为 `Result<void> AppendCatalogEntry(PendingCatalogSnapshot& pending, ManifestEntry entry, const CatalogSource& origin)`、
`Result<void> AppendCatalogPackage(PendingCatalogSnapshot& pending, PackageManifest package, CatalogSource source)`；文件与资源入口共用包成员关联、重复检查及排序。

- [ ] **Step 1 — 写失败证人。** PackageJson 加 `TextAndFileParsingShareSchema`：同一完整 JSON 得到相同 Id、配置、服务、processStates；坏 JSON、unknown 字段、包内重复 Id 两入口同拒。资源错误包含原始逻辑位置，含嵌入 NUL 的内容按完整长度解析、不能截断成有效前缀。
- [ ] **Step 2 — 写事务/所有权证人。** 新 `CatalogResources` 套件断言：输入销毁后 Find/Solve 仍有效；重复位置、空位置、含 NUL 的位置、重复 sourceId、跨包重复 Id、dynamic 包均 Err；失败保留 Ids/Sources/Warnings/Directory 及此前 Plan；空集合成功生成空快照。位置唯一性按字节比较，不作文件路径归一或大小写折叠。
- [ ] **Step 3 — 跑红。** 按根 CLAUDE 构建测试目标；新 API 尚不存在应在明确调用处编译失败，不把环境错误记为红证据。
- [ ] **Step 4 — 实现最小入口。** 内容/文件读取共用 ParsePackageDocument 与 ParsePluginFields；不新增 JSON 规则。ManifestFields 的错误/字段解析件增加 string_view 位置重载，文件入口继续用真实路径诊断，资源入口保留原始 UTF-8 标签，不经 filesystem::path 往返、不 absolute、不打开文件。资源按 Location 字节序排序；成员 Subdirectory 用来源 Id 展示，不参与加载寻址。成功才一次提交快照并置 HasScanned；资源模式 Directory 为空，Warnings 为空；文件模式保留原语义。增加 Unicode 位置和非磁盘位置成功导入证人。
- [ ] **Step 5 — 跑绿及切换证人。** 定点命令：`ctest --preset win-x64-clang-debug -R 'CatalogResources|PackageJson|PackageCatalog' --output-on-failure`。补 `DirectoryAndResourcesReplaceEachOtherTransactionally`：两种入口互换后定位字段清空正确、失败仍不换快照；无悬空字符串。
- [ ] **Step 6 — 记录宿主 ABI 增量。** 更新头注中的来源、排序、Directory、借用窗契约；按根门禁复验后才提交，提交仅含本任务文件。

## Task 2：真实双 STATIC 包消费资源快照

**Files:** 修改 Tests/StaticLink/PackageAssemblyTests.cpp、Tests/StaticLink/CMakeLists.txt；必要时复用 Tests/TestingSupport/PackageDescriptorProbe.{h,cpp}。

**Interfaces:** 消费任务 1 的 RefreshResources；使用现有 Provider / Consumer 生成头中的 kEntryPoints，SourceId 仍为 Provider / Consumer。
不增加 Host API，不改变现有 LoadRequest、InProcessBinding、LoadPlan。

- [ ] **Step 1 — 写调用链证人。** 新 `StaticResourceAssembly` 套件从实际 PackageReleaseStage 产物读取 JSON 为内存资源，导入后释放缓冲。显式启用 Provider 并配置 Value=7，Solve/CreatePod 后 IResult.Observed()==7，DestroyPod.Clean()==true。
- [ ] **Step 2 — 写负例及循环。** 同一 Host 连续 50 轮，值按 7/3/7 循环；每轮账本边、PluginInstances、Scopes 销局后均归零。错入口、缺绑定、配置 bool 代替 int32 均拒；含禁用成员的参与包元信息漂移在实例数增至非零前拒绝。全禁用包沿用“不要求绑定、不进计划”的既有语义。
- [ ] **Step 3 — 先运行并检查失败。** 本任务是任务 1 的集成回归；若已绿，不人为制造实现缺陷。对禁用成员负例临时抽掉整包检查，确认该证人转红后恢复。
- [ ] **Step 4 — 完成测试接线。** 注册 FIXTURES_REQUIRED PackageReleaseStage，保证单跑也自足。定点运行 `ctest --preset win-x64-clang-debug -R 'StaticResourceAssembly' --output-on-failure`，再跑原 StaticPackageAssembly 回归。
- [ ] **Step 5 — 按根门禁核实并提交。** 保留该调用链为后续 Android/iOS 共用证人的行为依据，不把桌面结果记为移动验证。

## Task 3：构建 profile、静态框架与移动文件装载拒绝

**Files:** 创建 Cmake/VaseBuildProfile.cmake、Samples/Mobile/CMakeLists.txt、Source/Host/{LoaderStatic,ImageInspectStatic}.cpp、Tests/Build/{StaticFrameworkSmoke.cpp,CheckBuildProfiles.cmake}；修改根 CMakeLists.txt、vcpkg.json、Source/{Pod,Host,Catalog}/CMakeLists.txt、Include/Vase/Detail/Export.h、Cmake/VasePluginHelpers.cmake、Cmake/VasePackageStage.cmake、Tests/CMakeLists.txt。

**Interfaces:** `VASE_BUILD_PROFILE` 为 desktop（默认）/manifest-host/mobile；desktop 不变，其他两种框架 STATIC。
manifest-host 只允许原生平台，mobile 只允许 Android/iOS。vcpkg 的 gtest 移入默认 feature `desktop-tests`，mobile/manifest-host 在首次 project 前禁用默认 feature，仍安装 nlohmann-json。
静态框架 PUBLIC 传播 VASE_STATIC，仅使 VASE_POD_API / HOST_API / CATALOG_API 为空；VASE_EXPORT 的描述符/入口规则保持。

- [ ] **Step 1 — 写配置与链接证人。** 不合法 profile、在原生系统请求 mobile 均应配置失败并点名原因；manifest-host 的 `VaseCheckStaticFramework` 目标构建并运行真实静态框架 smoke，导入空资源、创建/销毁空 Pod，返回 0。将 CheckBuildProfiles.cmake 注册为桌面 CTest `BuildProfiles`，子构建输出使用独立目录和资源锁，继承实际宿主工具链而非伪造平台变量。先确认新模式尚未实现导致的失败。
- [ ] **Step 2 — 实现 profile 裁剪。** Cmake/VaseBuildProfile.cmake 在 project 前处理 profile 名称及 vcpkg features；目标系统检查在 project 载入真实工具链后执行。desktop 保留原 Samples/Tools/Tests；manifest-host 不建立桌面测试，mobile 不建立桌面工具/动态 Samples。先建立 Samples/Mobile/CMakeLists.txt 的静态 smoke 接线，任务 4 再加入真实包。不使用全局 BUILD_SHARED_LIBS 暗改现有目标，不允许复用同一构建树切换 profile。
- [ ] **Step 3 — 实现静态插件形。** 非 desktop 的 PACK 插件以原名 INTERFACE 目标承载属性并传播 VaseBuildOptions，真正编译到 `.Pack`；不建开发 SHARED。两类目标共用源码/依赖参数，包函数仍从同一属性取成员/类名/策略。无 PACK 的移动包输入响亮拒绝。
- [ ] **Step 4 — 实现移动平台件。** 精确实现 Loader 的现有五个平台入口及 ImageInspectPlatform.h 中的平台桥；ReadImageFileBytes 仍由已有 ImageInspectCommon.cpp 唯一定义，不重复实现。PlatformLoad 返回 `linked-in-only build does not support file loading`；不提供假装载句柄。身份/导入查询 Result 返回不支持；格式枚举按 Android ELF / iOS Mach-O；无可观测卸载证据不得置真。只在 mobile profile 选择这些文件，宿主伴生仍用真实桌面平台件。
- [ ] **Step 5 — 验证早闸。** mobile 误调用桌面 vase_stage_pack 在配置期带名拒绝。静态 smoke 在 Windows 验证无 dllimport 未解析符号；后续移动证人必须验证文件装载 Err、进程内销局 Clean 和 Eject 既有早闸。
- [ ] **Step 6 — 回归与提交。** desktop 默认的目标集合、既有测试名不变；按根门禁跑对应全量及规矩 6 专项，不因该任务“主要是 CMake”跳过。

## Task 4：共用包定义、宿主候选清单与资源名表

**Files:** 创建 Samples/Mobile/Packages.cmake、Cmake/VaseMobileResources.{cmake,h.in}、Tests/Build/{CheckMobileManifests.cmake,VerifyMobileManifests.cpp}；修改根 CMakeLists.txt、Samples/Mobile/CMakeLists.txt、Tests/Package/CMakeLists.txt、Tools/VaseCli/CMakeLists.txt、CMakePresets.json。

**Interfaces:** Packages.cmake 共用现有 Provider/Consumer fixture 源、PackageServices 目标和两个 STATIC 包定义；仅移动走新目录，桌面原输出保持。
包目标属性 VASE_MOBILE_SOURCE_ID 分别为 Provider / Consumer；列表 VASE_MOBILE_PACKAGE_TARGETS 是资源表和阶段目标的共同输入。
宿主 profile 建立 `VaseMobileCandidateStage`，输出 `${CMAKE_BINARY_DIR}/mobile-resources/vase/<SourceId>/pack.json`；
模板生成 `VaseMobileResources.h`：`kPackageResources` 为相对 asset 名与 SourceId 的表，`kPackageBindings` 为 InProcessBinding 表；由包属性推导入口头和 namespace，不含主机绝对路径和手抄成员 Id。

- [ ] **Step 1 — 写构建级证人。** manifest-host 建立内部 VaseMobileManifestVerifier，编译 VerifyMobileManifests.cpp 并链接 Catalog：唯一参数为候选包根，Refresh 后要求 Provider 默认 false、Consumer Requires 中有 Counter v1、两来源均 inProcess，否则非零。CheckMobileManifests.cmake 在独立宿主树执行阶段目标后调用它；解析走现有 Catalog，不增加脚本 JSON 校验器。未声明/缺失策略按现有规则处理。
- [ ] **Step 2 — 共用定义与生成规则。** 从 Tests/Package 提取 PackageServices、两 fixture 与双 STATIC 包定义到 Packages.cmake；保持原输出例外与所有桌面消费者。manifest-host 建 VaseCliCore 内部件和生成器，不建立 CLI main/ctest 用法腿。
- [ ] **Step 3 — 建立显式发布编排。** manifest-host 的每包生成仍用原 GeneratePackageManifest / 单包 staging；输入含源码、生成头、共用定义及源策略。移动树只接受 `VASE_MANIFEST_HOST_BUILD_DIR` 指向的、profile/host 架构经过检查的宿主树。
- [ ] **Step 4 — 防止旧清单假绿。** 证人只修改构建树中的策略副本：先成功，再破坏副本使生成失败；即使旧输出仍在，总阶段命令也必须失败。恢复策略后重生成成功；不改源策略文件。由现有 Catalog 解析验证内容，不在脚本增加另一套 JSON schema。
- [ ] **Step 5 — 跑绿并提交。** 新增宿主 preset `mobile-manifest-host`，仅供本机执行；Windows 用现有 clang-cl 工具链、Mac 用既有 macOS x64 工具链。宿主原生代码不能定义目标平台内置宏。普通 desktop 发布/静态包族仍全绿，再按根门禁提交。

## Task 5：Android 原生构建与共用应用证人

**Files:** 创建 Cmake/Toolchains/android-arm64-clang-libcxx.cmake、Samples/Mobile/{MobileScenario.h,MobileScenario.cpp}、Samples/Mobile/Android/JniBridge.cpp、Scripts/VerifyMobileSymbols.py；修改 CMakePresets.json、Samples/Mobile/{CMakeLists.txt,Packages.cmake}、Tests/StaticLink/CMakeLists.txt、根编译器门禁。

**Interfaces:** 新 preset `android-arm64-clang-release`；环境 ANDROID_NDK_HOME 指向已安装 r30，采用 NDK 官方 toolchain，vcpkg chainload 同一文件，ANDROID_STL=c++_shared、API29。工具链先检查 source.properties 及 NDK 路径，NDK Clang21 是 Android 特定例外。
共用证人提供 `vase_mobile::ScenarioReport{std::uint32_t CompletedRounds; bool Clean;}` 及
`vase::Result<ScenarioReport> RunPackageScenario(std::span<const vase::PackageResource> resources)`。
JNI `NativeBridge.runScenario(AssetManager)` 返回 NativeResult（success、completedRounds、error），最终库名 `VaseMobileJni`。
MobileScenario.h 另定义内部拥有值 `OwnedPackageResource{std::string Location; std::string Contents;}`，供 JNI/iOS 读取资源使用，不进入 Catalog 公开 API。

- [ ] **Step 1 — 写共用调用证人。** Packages.cmake 为各 profile 建立内部静态库 VaseMobileScenarioCore，消费生成的 kPackageBindings，链接框架和双包。VaseMultiPackageLinkTests 也链接它，使用任务 2 的真实资源执行 50 轮 7/3/7；失败走 Result，不以 Release 下消失的 assert 判错。先确认未实现符号导致失败，再实现。
- [ ] **Step 2 — 实现 NDK 构建。** 新树 configure/build；确保编译命令使用 NDK 编译器、目标 arm64 和同一 libc++；静态框架和成员 PIC。NDK 缺失/版本错误须配置失败，不回退主机编译器。C++ 模块扫描如遇 NDK 缺工具，基于实际错误处理；项目无模块可在移动配置明确关闭扫描，不改变桌面。
- [ ] **Step 3 — 实现 JNI 资源读取。** AAssetManager 只读生成名表中的资源，按长度拥有字节并检查读取失败；所有资源读完后构建借用视图同步导入。保持缓冲存活到导入返回，之后可释放。JNI 单次调用在同一线程创建 Host、运行循环、销局，再返回结果，不缓存服务或 Host 到 Java 全局。
- [ ] **Step 4 — 实现 APK 输入目标。** `VaseMobileApkInputs` 先构建宿主 CandidateStage，再收集 JNI `.so`、同 NDK libc++_shared.so 及候选资源到移动构建树 `apk-inputs/{jniLibs/arm64-v8a,assets}`。只在显式目标中拷贝构建产物，正常 App 启动不拷贝；宿主生成失败必须阻断目标。
- [ ] **Step 5 — 检查干净符号。** JNI 额外链接 KeepAliveProbe.Pack 但不选入包。VerifyMobileSymbols.py 调用指定 llvm-nm/llvm-readobj，不解析 JSON：要求双包入口、Provider/Consumer 描述符为 defined；禁止 defined 的 VasePluginDesc_KeepAliveProbePlugin。临时去掉 Consumer 包链接应链接失败，临时选入负对照应使禁止断言失败，随后恢复并干净重建。
- [ ] **Step 6 — 记录并提交。** 文件头必须是 AArch64；实际链接不能导入 VasePod/Host/Catalog 动态库，允许 libc++_shared.so。本步不声称 JNI 已在设备执行；按根门禁和规矩 6 回归后提交。

## Task 6：Android APK 打包与真实目标执行

**Files:** 创建 Samples/Mobile/Android/apk/{settings.gradle.kts,build.gradle.kts,gradle.properties,gradle/wrapper/gradle-wrapper.properties,gradle/wrapper/gradle-wrapper.jar,gradlew,gradlew.bat} 及 app/ 下 build.gradle.kts、src/main/AndroidManifest.xml、src/main/java/com/moozen/vase/mobile/{NativeBridge,NativeResult,MainActivity}.java、src/androidTest/java/com/moozen/vase/mobile/MobilePackageTest.java；创建 Scripts/android-mobile-verify.ps1；更新 .gitignore 仅忽略本示例 Gradle 输出/本机配置。

**Interfaces:** APK 只打包任务 5 的预构建 JNI/运行库和只读资源，不再用 externalNativeBuild 建第二套 native 目标。
Gradle 项目采用 CppDemo 已使用的 AGP9.4.1、Gradle9.6.0、compileSdk/targetSdk37；minSdk29、Java11 源级别。
JDK 运行版本读取 AGP 实际要求并校验，不能把 Java 源级别当 Gradle JDK 要求；机器路径留本地。

- [ ] **Step 1 — 写 instrumentation 断言。** 从 targetContext.assets 调 JNI，断言 success=true、completedRounds=50、error 为空。测试 APK 自己不含完整产品资源时，用其 AssetManager 调用应结构化失败并点名缺失资源。
- [ ] **Step 2 — 最小打包壳。** MainActivity 仅提供运行示例/显示结果，不添加业务 UI；Java 方法签名与任务 5 一致。Gradle preBuild 执行 VaseMobileApkInputs，失败即停止，源资源和 jniLibs 指向同一次移动构建输出，不拷入源码目录。
- [ ] **Step 3 — 编排与离线验证。** android-mobile-verify.ps1 校验环境、配置/构建宿主与 Android 树、调用 Gradle assembleDebug 和 assembleDebugAndroidTest。检查 APK zip 内有两个 pack.json、JNI 库及唯一的 libc++_shared.so；无设备也必须能完成此阶段。Gradle wrapper 取官方对应版本，不复制用户工程中的缓存/凭据。
- [ ] **Step 4 — 验证生成失败阻断打包。** 用任务 4 的构建副本接缝制造失败，APK assemble 必须非零，不能只因旧 APK 或旧资源存在便报告成功。原目录与策略恢复后再跑整条链。
- [ ] **Step 5 — 目标执行。** 先查 adb 的设备 ABI/API，只有支持 arm64-v8a 且 API>=29 的执行环境才可运行本产物；不能用普通 x86_64 模拟器替代。执行 connectedDebugAndroidTest，读 instrumentation/XML 结果确认各断言；不只 grep Value=7。
- [ ] **Step 6 — 目标负例与报告。** instrumentation 的坏资源变体从本次候选资源生成测试副本，不改正常发布物；覆盖坏 JSON、实际元信息漂移及禁用成员漂移。缺设备时保留未勾选的目标验证步骤并报告阻塞原因，继续任务 7，不把桌面代跑记为目标证据。
- [ ] **Step 7 — 按根门禁提交。** 不提交 local.properties、.gradle、APK 或签名信息；保留脚本的原始退出码和实际验证范围。

## Task 7：iOS 静态归档与最终链接证人

**Files:** 创建 Cmake/Toolchains/ios-arm64-clang-libcxx.cmake、Cmake/Triplets/arm64-ios-libcxx.cmake、Samples/Mobile/Ios/{BundleResources.h,BundleResources.cpp,main.cpp}、Scripts/ios-mobile-verify.sh；修改 CMakePresets.json、Samples/Mobile/CMakeLists.txt。

**Interfaces:** 新 preset `ios-arm64-clang-release`；环境 VASE_IOS_SDK_ROOT 指向独立 iPhoneOS18.2.sdk。
工具链使用 CMAKE_SYSTEM_NAME=iOS、arm64、deployment15.0、MacPorts Clang23，显式 sysroot 与 libc++；vcpkg overlay triplet 使用同一 SDK/编译器。
`VaseMobileIosLinkProbe` 链入任务 5 的共用证人，BundleResources.cpp 提供应用内置资源读取；不链接生成工具。
资源读取函数为 `vase::Result<std::vector<OwnedPackageResource>> vase_mobile::ReadBundlePackageResources()`，声明在新 Samples/Mobile/Ios/BundleResources.h；调用方在同步 RunPackageScenario 返回前保持拥有缓冲存活。

- [ ] **Step 1 — 先验证真实 configure/link。** 在 Mac 上建立全新 iOS 树，原始错误完整保存。不能把 system name 改为 Darwin 冒充 iOS，也不能全局使用 STATIC try-compile 掩盖链接失败；SDK/编译器/平台不符时响亮拒绝。
- [ ] **Step 2 — 实现资源接入。** BundleResources.cpp 用 CoreFoundation 的 main bundle 资源定位与只读文件读取，拥有内容并构造相同 PackageResource 输入；main 实际引用资源读取与 RunPackageScenario，保证链接覆盖整条路径，而不是空 main 只链接无引用归档。
- [ ] **Step 3 — 干净构建并检验。** 脚本构建宿主候选清单和 iOS 框架/成员/包 `.a`，再最终链接。用 file / llvm-nm / llvm-objdump 检查所有输入为 arm64 iOS，最终 LC_BUILD_VERSION 平台 iOS、SDK18.2、minos15.0；只使用 SDK 系统运行库，不出现 MacPorts macOS libc++ 路径。
- [ ] **Step 4 — 保活和对抗。** 复用 VerifyMobileSymbols.py，从干净链接证明两个选中包存在、未选中 KeepAliveProbe 不在；做与任务 5 同形的链接/负对照变异并恢复。为 iOS-only 元信息变化构建变异产物，只报告构建结果；未在 iPhoneOS 执行不能声称漂移负例已目标验证。
- [ ] **Step 5 — 报告并提交。** 本波不安装 Xcode、不签名、不要求安装 App；只声称真实项目归档和最终链接。iOS 设备运行、bundle 资源实际读取、Clean 与元信息一致性均需目标执行后才可置为已验证。

## Task 8：全量门禁、证据落账与最终评审

**Files:** 更新 CLAUDE.md、README.md、wiki/vase-architecture.md，以及 .claude/skills/vase-cpp-engineering/references/{portability,abi-boundary,verification}.md 中被本波改变的状态句；如目录树改变，按根规则处理已确认布局的文书义务。

**Interfaces:** 不新增功能；以最终实现和原始日志修订“已实现/已构建/已目标执行”的支持矩阵。

- [ ] **Step 1 — 八线全量。** 按根 CLAUDE 指定脚本/环境逐线干净 configure/build/ctest/-N；导出用例名集合，解释新增及唯一 Debug/Release 差，不能从旧基数推算新基数。
- [ ] **Step 2 — 静态门禁。** 四条 debug 线 tidy 各读退出码、正文 error/warning 与 TU 数；format 覆盖未暂存文件。规矩 6 专项在三个桌面平台各留证据。新 NOLINT 逐处说明无代码级出路，不按数量豁免。
- [ ] **Step 3 — 移动证据复核。** 干净重跑宿主候选、Android 原生/APK、iOS 归档/链接；设备可用时跑 Android instrumentation。未执行项记原因，不能用旧 SDK 探针或桌面资源测试顶替。
- [ ] **Step 4 — 落账与审查。** 根 CLAUDE 记录实际命令、基数和范围；README 说明只读资源、同元信息限制及框架/包最终链接位置；wiki 记录 Catalog 宿主 API/ABI 增量，描述符 HeaderVersion 不变。撤掉已过期状态句但保留桌面承重规则。
- [ ] **Step 5 — 检查最终分支。** 按项目评审流程独立检查需求符合性、生命周期、静态链接闭包、资源失败路径和证据边界；修复后只重跑受影响验证。按根提交门禁提交，不包含本机 settings.local.json；合并/推送由后续明确指令决定。

## 实施边界与外部依据

计划步骤完成不等于目标运行已验证。Android 无 arm64 执行环境时，任务 6 目标步骤保持未完成；iOS 本波以静态归档和最终链接为承诺边界。
例行提交不能先于根规则要求的检查；若阶段性保留未提交工作，状态报告必须区分实现、验证和提交。

- [NDK CMake](https://developer.android.com/ndk/guides/cmake)：采用 NDK 官方工具链和明确 ABI/API。
- [vcpkg Android](https://learn.microsoft.com/en-us/vcpkg/users/platforms/android)：NDK 环境接入。
- [vcpkg triplets](https://learn.microsoft.com/en-us/vcpkg/users/triplets)：chainload 与 overlay 配置。
- [CMake sysroot](https://cmake.org/cmake/help/v4.0/variable/CMAKE_OSX_SYSROOT.html)：在 project 前明确独立 SDK。
