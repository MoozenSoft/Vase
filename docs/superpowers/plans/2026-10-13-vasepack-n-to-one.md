# VasePack N:1 清单与装配链 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 通过 pack.json 和显式发布目标，把动态组合库与同宿主多静态包从真实描述符生成、发现、求解到整包预检、装配和干净关停接成可复用链路。

**Architecture:** Catalog 解析两种用途的文档并产拥有形分析结果，Solve 物化来源与完整包期望。Host 在任何 Pod/插件实例和 Stage0 创建前，使用随后装配的同一镜像完成整包校验。工具共用物化/校验件，CMake 提供专属静态入口、生成声明头及显式的生成/staging 目标。

**Tech Stack:** C++20、关闭异常、CMake/Ninja、现有 vcpkg gtest 与 nlohmann-json；工具链与平台矩阵以根 CLAUDE.md 为准。

**Spec:** [2026-10-11-vasepack-n-to-one-design.md](../specs/2026-10-11-vasepack-n-to-one-design.md)，Q1–Q17 已确认。

## Global Constraints

- 一包一目录，固定 `pack.json`；`plugin.json` 与它按用途并存，不作格式升级。
- `schemaVersion` 为整数 1；包根白名单为 `schemaVersion/sourceId/source/plugins`，来源为 `dynamic/inProcess`。字段约束按 spec §2。
- Catalog 不加载镜像、不执行入口；nlohmann 只在 Source/Catalog 的 PRIVATE 实现中使用。
- 包计划每次建局在 Stage0、Pod、插件/配置实例、OnLoad/OnStart 前全量预检；参与包检查全部成员，全跳过包免加载/绑定。
- 包不一致整局 Err；镜像失败留架、不 Reset；回调失败的宽容/Strict 规则不变。原始手写计划保留低层语义。
- STATIC 入口专属，SHARED 两枚固定入口不变；不改 PluginDescriptor/PluginMeta 布局、Plugin vtable 或 kHeaderVersion。
- scan 默认保留损坏 pack.json；显式重建才重置 enabledByDefault。构建生成从成员源码清单取得启用策略，不读取 staging 产物作为输入。
- staging 单包先准备后替换、失败保留原内容；不承诺跨包事务或并发读取无空窗。只管理显式指定的包目录。
- 插件经 vase_add_plugin_fixture，新 target 链 VaseBuildOptions；编译期输入在调用点给出。注释、格式、命名、中文提交及禁止 AI 尾注按根 CLAUDE.md。
- 当前分支 `vasepack-n-to-one`；使用用户选定的当前工作树，不另开 worktree。先记录现有文书改动，不重置或覆盖它们。
- 每个任务先 red 再 green，功能检查有正文证据；任务提交前完成项目要求的门禁。八线命令、基数与 tidy 阈值只读根 CLAUDE.md，不在计划另存副本；门禁未完成的任务不得标成已提交/全绿。

## Review Focus

1. F1：参与包的禁用成员漂移应在 Stage0 前拒绝，全禁用包则不触碰来源；由 T5/T6 的负例分别把关。
2. F2：两份真实 STATIC 包必须可分别绑定，不能只把两个归档写在链接行上；由 T3/T12 验双入口与跨包服务消费。
3. F3：仅修改源清单启用策略，发布目标必须更新；手改 staging 不得污染下次生成；由 T10/T11 的构建级证人把关。
4. F4：源库/输出目录含空格，且包目录旁有其他动态库时，CLI 与生成器仍必须选择指定源；由 T7/T11 覆盖。
5. F5：第二次目录 rename 失败时，原目录必须恢复；不能清理掉备份后再报错；由 T11 的真实文件内容与注入 rename 失败证人把关。

## File Structure

| 文件 | 职责/任务 |
|---|---|
| `Include/Vase/PluginDescriptor.h` | 查询/枚举指针类型与纯数据双入口，T1 |
| `Include/Vase/Host/PackagePlan.h`、`PackageInspect.h` | 包来源/期望和共用检查接口，T1/T5/T6 |
| `Source/Host/PackageInspect.cpp`、`Detail/PackagePreflight.{h,cpp}` | 安全枚举、成员比对、Host 所属镜像预检，T1/T6 |
| `Include/Vase/Catalog/PackageManifest.h`、`Analysis.h` | 包文档、快照来源与分析/绑定值形，T2/T4/T5 |
| `Source/Catalog/PackageJson.cpp`、`Detail/ManifestFields.{h,cpp}` | 包/单插件共用字段规则与 JSON 写盘口，T2 |
| `Source/Catalog/{PluginCatalog,Solve}.cpp` | 事务性快照与一套求解算法，T4/T5/T9 |
| `Cmake/VasePackDispatch.cpp.in`、`VasePackEntries.h.in` | 分发表、STATIC 专属声明头，T3 |
| `Cmake/VasePluginHelpers.cmake`、`VasePackageStage.cmake`、`VasePackageGenerator.cpp.in` | 包元数据属性和可复用发布目标，T3/T11 |
| `Tools/VaseCli/PackageBuild.{h,cpp}`、`PackageStage.cpp`、`Detail/PackageStage.h` | 生成输入策略与目录事务，T10/T11 |
| `Tools/VaseCli/{Scan,Validate,Plan,Doctor}.cpp`、`Cli.cpp` | 两种来源的命令行为，T7/T8 |
| `Tests/Unit/PackageInspectTests.cpp`、`PackageJsonTests.cpp` | 描述符安全与文档语法/往返，T1/T2 |
| `Tests/Integration/Package{Catalog,Solve,Preflight,Cli,Build,Stage,Assembly}Tests.cpp` | 各任务的行为证人，T4–T12 |
| `Tests/StaticLink/MultiPackageLinkTests.cpp`、`PackageAssemblyTests.cpp` | 真双包链接及装配，T3/T12 |
| `Tests/Package/fixtures/`、`Tests/Package/CMakeLists.txt` | 两个带配置/服务契约的真实成员、发布与重生成 fixture，T3/T11 |

每次新增源同时加入所属 Source/Host、Source/Catalog、Tools/VaseCli 或 Tests 的 CMakeLists。不用全仓 GLOB；动态包不链进 VaseTests，静态证人只链进独立 VaseStaticLinkTests。

## Interface Contract

以下名称是任务交接接口；字段布局遵循 NSDMI 和已有指定初始化纪律。新增值形不带 JSON 类型，函数分别用 VASE_HOST_API / VASE_CATALOG_API 导出。

**T1 基础类型，namespace vase：**

```cpp
using PluginEnumerationEntryPoint = const PluginDescriptor* const* (*)(std::uint32_t*);
struct PackageEntryPoints { PluginEntryPoint Lookup = nullptr; PluginEnumerationEntryPoint Enumerate = nullptr; };
enum class PackageSourceKind : std::uint8_t { kDynamic, kInProcess };
Result<std::vector<const PluginDescriptor*>> InspectDescriptors(PluginEnumerationEntryPoint enumerate);
Result<std::vector<const PluginDescriptor*>> InspectPackageMembers(PackageEntryPoints entries);
Result<std::vector<const PluginDescriptor*>> InspectPackageMembers(const detail::BinaryRecord& record);
Result<void> ComparePackage(std::span<const ManifestExpectation> expected,
                          std::span<const PluginDescriptor* const> actual);
```

枚举结果借镜像驻留期。InspectPackageMembers 校验版本、非空 Meta/Id、Id 唯一、查询/枚举指向同一描述符并按 Id 排序；ComparePackage 先检查输入描述符版本/有效性，再比较 Id 集和逐字段，逐字段唯一走 CompareDescriptor。工具与 Host 使用同一实现。

**T2/T4 文档与来源，namespace vase：**

```cpp
struct PackageManifest {
    std::string SourceId; PackageSourceKind Kind = PackageSourceKind::kDynamic;
    std::string Binary; std::vector<ManifestEntry> Plugins;
};
struct CatalogSource {
    std::string Id; bool IsPackage = false; PackageSourceKind Kind = PackageSourceKind::kDynamic;
    std::filesystem::path ManifestPath, BinaryPath; std::vector<std::string> MemberIds;
};
Result<PackageManifest> ParsePackageManifestFile(const std::filesystem::path& file);
Result<void> WritePackageManifestFile(const std::filesystem::path& file, const PackageManifest& manifest);
const std::vector<CatalogSource>& PluginCatalog::Sources() const;
const CatalogSource* PluginCatalog::SourceFor(std::string_view pluginId) const;
```

ManifestEntry 尾加自持 `SourceId`（内部关联，不写进成员 JSON）。单插件来源键为 `@single/<子目录>`，包 sourceId 不含 @，两命名空间不冲突。包成员 Subdirectory 可保留目录展示值，Binary 留空；实际加载只读 CatalogSource。Sources/SourceFor 借用窗到下一次成功 Refresh；失败保留原窗。

**T5 分析/计划，namespace vase：**

```cpp
struct AnalyzedPlugin {
    std::string Id, SourceId; PackageSourceKind Kind = PackageSourceKind::kDynamic;
    LoadDecision Decision = LoadDecision::kLoad; SkipReason Reason{};
    ConfigBlob ResolvedConfig; ManifestExpectation Expected;
};
struct AnalysisOutcome { std::vector<AnalyzedPlugin> Ordered; std::vector<SolveNote> Notes; };
struct InProcessBinding { std::string_view SourceId; PackageEntryPoints Entries; };
struct PackagePlanSource {
    std::string SourceId; PackageSourceKind Kind = PackageSourceKind::kDynamic;
    std::filesystem::path BinaryPath; PackageEntryPoints Entries;
    std::vector<ManifestExpectation> Members;
};
Result<AnalysisOutcome> PluginCatalog::Analyze(const LoadRequest& request) const;
Result<SolveOutcome> PluginCatalog::Solve(const LoadRequest& request,
                                         std::span<const InProcessBinding> bindings) const;
```

保留现有 Solve(request) 委派空 bindings。LoadPlan 尾加 `std::vector<PackagePlanSource> PackageSources`，LoadPlanEntry 尾加自持 `std::string PackageSourceId`，空表示原始逐条目计划。分析结果全部拥有；Solve 的 Id 仍借 Catalog 快照，不能借临时 AnalysisOutcome。只有包条目带 PackageSourceId；单插件来源不伪装成包。

**T10/T11 生成，namespace tools::cli：**

```cpp
using DescriptorEntryPoint = const vase::PluginDescriptor* (*)();
struct PackageMemberInput {
    std::string DescriptorSymbol; DescriptorEntryPoint Descriptor = nullptr;
    std::filesystem::path SourceManifest;
};
struct PackageBuildOptions {
    std::string SourceId; vase::PackageSourceKind Kind = vase::PackageSourceKind::kDynamic;
    std::filesystem::path BinaryPath; vase::PackageEntryPoints Entries;
    std::vector<PackageMemberInput> Members;
    std::filesystem::path OutputDirectory; std::vector<std::filesystem::path> RuntimeFiles;
    std::vector<std::filesystem::path> ProtectedDirectories;
};
vase::Result<vase::PackageManifest> GeneratePackageManifest(const PackageBuildOptions& options);
vase::Result<void> BuildAndStagePackage(const PackageBuildOptions& options);
```

Members 按构建包成员自动生成，非调用者手写 Id。动态成员用 DescriptorSymbol 从同一已加载包取描述符函数；静态成员用 Descriptor；两种读法都核对它属于包枚举且源清单 Id 与该成员一致。SourceManifest 空表示未声明，非空但缺失是错误。OutputDirectory/RuntimeFiles/ProtectedDirectories 只归 staging，GeneratePackageManifest 不写盘；CMake 调用为 ProtectedDirectories 注入项目根和构建根，文件系统根与输入文件祖先检查始终执行。

## Execution Gates

执行前读取工程技能与根 CLAUDE.md，核对工作树/索引和三平台运行入口。先完成基线检查，未跑的平台明确记为未验证。局部 red/green 命令以下以 win-x64-clang-debug 为例；构建必须先于 ctest/tidy，完整配置/构建配方不在此复制。

每任务 Step 5 是提交检查点：按根 CLAUDE.md 完成适用完整门禁、逐项核对正文/基数和新用例，再用该任务 Files 的明确文件列表 git add，中文提交。未过门禁时保持任务未完成，不把局部绿当全量绿。若调整批次，先记录原因和门禁覆盖，不能静默削弱规则。

任务依赖：T1 → T2/T3；T2 → T4；T4 → T5 → T6；T3/T6 → T7/T8/T9；T2/T3/T7 → T10 → T11；全部 → T12。为避免公共接口漂移，实际按下列编号串行执行。

### Task 1: 双入口与共用整包检查

**Files:** 修改 Include/Vase/PluginDescriptor.h、Host/Inspect.h、Source/Host/Inspect.cpp、Source/Host/CMakeLists.txt、Tests/CMakeLists.txt；新增 Host/PackagePlan.h（先只定义来源 enum）、Host/PackageInspect.h、Source/Host/PackageInspect.cpp、Tests/Unit/PackageInspectTests.cpp、Tests/TestingSupport/PackageDescriptorProbe.{h,cpp}。

**Interfaces:** 产 Interface Contract 的 T1。测试支撑 namespace testing_support 提供 `PackageEntryPoints ProbeEntries()`、`std::vector<ManifestExpectation> ProbeExpectations()`、`void ResetPackageProbe()`、`void SetProbeFailure(std::string_view failure)`；正常成员 Id 固定为 Probe.A/Probe.B。failure 字面只用于测试：lookup-mismatch/duplicate-id/null-meta/stale-header/extra-member/empty-enumeration/null-descriptor/empty-id。另产 `PackageProbeCounts{Lookup,Enumerate,Create,CreateConfig,OnLoad,Reset}`（size_t 零初始化字段）及 `PackageProbeCounts ReadPackageProbeCounts()`；探针只持元数据和标量计数，不持 Pod 实例。

- [ ] **Step 1 — 写 PackageInspect.Red 的行为测试**：有效双成员通过；分别设置上述故障，断言返回 Err 且包含出错 Id/字段；删 Expected 一员，枚举仍双员，ComparePackage 必须拒绝。增加 null 枚举入口、空表、null 描述符与空 Id 的直接负例。

```cpp
TEST(PackageInspect, RejectsLookupEnumerationMismatch)
{
    testing_support::ResetPackageProbe();
    testing_support::SetProbeFailure("lookup-mismatch");
    EXPECT_FALSE(vase::InspectPackageMembers(testing_support::ProbeEntries()).IsOk());
}
```

- [ ] **Step 2 — 跑 red**：`ctest --preset win-x64-clang-debug -R '^PackageInspect\.' --output-on-failure`；缺接口的编译失败可记为首轮 red，随后每个故障必须实际观察到正确拒绝。
- [ ] **Step 3 — 实现**：原 BinaryRecord 枚举与显式指针共用版本闸；新 PackageInspect 在读取 Meta 前检查版本，在读 Id 前检查 Meta。跨函数指针转换只在既有平台边界就地说明，不向公共 API 传播 void*。
- [ ] **Step 4 — 跑 green 与既有 Inspect/Descriptor 用例**：上条过滤改为 `^(PackageInspect|Inspect|Descriptor)\.`；每个否定条件独立归因，CompareDescriptor 域规则不复制。
- [ ] **Step 5 — 门禁后提交**：`新增包双入口与共用成员校验`；包含上述新源的测试与 CMake 接入。

### Task 2: pack.json 解析、往返与单插件字段共用

**Files:** 新增 Include/Vase/Catalog/PackageManifest.h、Source/Catalog/PackageJson.cpp、Source/Catalog/Detail/ManifestFields.{h,cpp}、Tests/Unit/PackageJsonTests.cpp；修改 Source/Catalog/ManifestJson.cpp、Source/Catalog/CMakeLists.txt、Tests/CMakeLists.txt。

**Interfaces:** 产 T2 文档接口。内部 `catalog_detail::ManifestFieldMode{kSinglePlugin,kPackageMember}`；`Result<ManifestEntry> ParsePluginFields(const json&,const path&,string_view,ManifestFieldMode)`、`json SerializePluginFields(const ManifestEntry&,ManifestFieldMode)`、`Result<void> WriteJsonAtomically(const path&,const json&)` 只在 Source/Catalog 内声明。既有 ParseManifestFile/WriteManifestFile 签名保留。

- [ ] **Step 1 — 写 PackageJson 用例**：用既有 CatalogSandbox::WriteFile 写 spec 动态/静态包；断言 SourceId/Kind/成员默认值。参数化非法版本 1.0/2、未知根/source/成员键、空 sourceId/成员集、重复 Id、inProcess 带 binary、dynamic 缺 binary、含路径 stem、非法 enum default；字符串短值与 choices 写出再读相等。

```cpp
TEST(PackageJson, RejectsDuplicateMemberId)
{
    testing_support::CatalogSandbox sandbox{"package-json"};
    sandbox.WriteFile("P/pack.json", R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"inProcess"},"plugins":[{"id":"Probe.A"},{"id":"Probe.A"}]})");
    EXPECT_FALSE(vase::ParsePackageManifestFile(sandbox.Root / "P/pack.json").IsOk());
}
```

- [ ] **Step 2 — red**：过滤 `^PackageJson\.`，确认非法输入归因到文件/成员；原单插件 ManifestJson 仍作为对照。
- [ ] **Step 3 — 实现**：抽取现有成员字段 parser/serializer，保持单插件字段与默认值；包成员不带 schemaVersion/binary。验证排序不改变 choices 展示顺序。写文件先完整验证值形，错误不覆盖原件。
- [ ] **Step 4 — green**：过滤 `^(PackageJson|ManifestJson)\.`；新增原件哨兵测试证明失败后字节未变，包内 SourceId/Subdirectory 关联字段不被序列化。
- [ ] **Step 5 — 门禁后提交**：`新增包清单并共用插件字段规则`。

### Task 3: 专属 STATIC 入口与真实双包材料

**Files:** 修改 Cmake/VasePluginHelpers.cmake、VasePackDispatch.cpp.in、Tests/StaticLink/CMakeLists.txt、StaticLinkTests.cpp、Tests/CMakeLists.txt；新增 Cmake/VasePackEntries.h.in、Tests/StaticLink/MultiPackageLinkTests.cpp、Tests/Package/CMakeLists.txt、Tests/Package/fixtures/PackageServices.h、Provider/{CMakeLists.txt,PackageProviderPlugin.cpp,plugin.json}、Consumer/{CMakeLists.txt,PackageConsumerPlugin.cpp,plugin.json}。

**Interfaces:** STATIC target 属性 `VASE_PACK_HEADER`、`VASE_PACK_ENTRY_NAMESPACE`，包属性 `VASE_PACK_MEMBERS/VASE_PACK_KIND`，成员属性 `VASE_PLUGIN_MANIFEST`。生成 `<target>.entries.h`，namespace `vase::pack::<key>` 中 `inline constexpr PackageEntryPoints kEntryPoints`；key = MAKE_C_IDENTIFIER 后 TOLOWER 的 target，记录全项目已用 key，碰撞配置期拒绝。STATIC C 符号 `VasePack_<key>_GetPlugin/Descriptors`，SHARED 保留原名字。PackageServices.h 的 namespace package_fixture 定义 ICounter::Value() const 与 IResult::Observed() const（均返回 int32_t），kName 分别为下列 Counter/Result 服务名，kVersion=1；实例与配置由插件持有，不放进全局容器。

- [ ] **Step 1 — 建双包 red 见证**：Provider Id=`Vase.PackageProvider`，Provide=`Vase.PackageProvider.Counter@1`，int32 配置 Value 默认 3；Consumer Id=`Vase.PackageConsumer`，Require 该服务并 Provide=`Vase.PackageConsumer.Result@1`，OnStart 保存读取值，实例级结果由 Context/EffectScope 管理。两个插件经 helper 的 PACK/MANIFEST 参数定义，不互链。STATIC targets 为 VasePackageProvider/VasePackageConsumer。两包声明头 Lookup 地址不同，各自仅返回对应 Id；交叉查询返回 null。

```cpp
TEST(MultiPackageLink, EntriesRouteToTheirOwnMembers)
{
    constexpr auto kProvider = vase::pack::vasepackageprovider::kEntryPoints;
    constexpr auto kConsumer = vase::pack::vasepackageconsumer::kEntryPoints;
    EXPECT_NE(kProvider.Lookup, kConsumer.Lookup);
    ASSERT_NE(kProvider.Lookup("Vase.PackageProvider"), nullptr);
    ASSERT_NE(kConsumer.Lookup("Vase.PackageConsumer"), nullptr);
    EXPECT_EQ(kProvider.Lookup("Vase.PackageConsumer"), nullptr);
}
```

- [ ] **Step 2 — red**：过滤 `^MultiPackageLink\.`；若现有固定符号导致链接/取址不成立，如实记录，不把某种归档提取行为提前说成所有平台必然事实。
- [ ] **Step 3 — 实现**：给模板参数化入口名；生成头只依赖 PluginDescriptor.h，不让 pack 链 Host。对 `foo-bar` 与 `foo_bar` 等编码冲突配置期拒绝。Source manifest 路径在定义成员时绝对化、保存属性，未声明存空。
- [ ] **Step 4 — green**：真链接两个 STATIC targets 的独立 exe；迁移原 StaticLinkTests 取址到生成头，保活负例与对照组继续同一平台机制。符号判据必须从干净全量链接读。Tests/Package 定义 fixtures/targets 先于 Tests/StaticLink；VaseTests 只拿动态路径，不能链接静态包。
- [ ] **Step 5 — 门禁后提交**：`生成静态包专属入口并验证双包链接`。

### Task 4: 事务性双清单发现与来源表

**Files:** 修改 ManifestView.h、PackageManifest.h、PluginCatalog.h、Source/Catalog/PluginCatalog.cpp；新增 Tests/Integration/PackageCatalogTests.cpp，修改 Tests/CMakeLists.txt。

**Interfaces:** 产 T4 CatalogSource 与 Sources/SourceFor，ManifestEntry.SourceId 尾追加并初始化；旧来源键 `@single/<子目录>`，包条目 Binary 为空。BuildExpectation 仍只投影插件元信息，不带来源字段。

- [ ] **Step 1 — 写 PackageCatalog 用例**：多一级目录发现单插件和动态/静态包；static 目录只有 pack.json 可成功。全禁用也拒绝跨包 Id 重复；同目录两清单、重复 sourceId 拒绝。根清单与框架库不被扫。先建有效快照再放坏包，失败后 Ids/Sources/Warnings/Directory 逐项不变。

```cpp
TEST(PackageCatalog, FailedRefreshKeepsSourceSnapshot)
{
    testing_support::CatalogSandbox sandbox{"package-catalog"};
    sandbox.WriteFile("P/pack.json", R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"inProcess"},"plugins":[{"id":"Probe.A"}]})");
    vase::PluginCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    sandbox.WriteFile("Q/pack.json", "{broken");
    EXPECT_FALSE(catalog.Refresh(sandbox.Root).IsOk());
    ASSERT_EQ(catalog.Sources().size(), 1U);
    EXPECT_EQ(catalog.Sources().begin()->Id, "P");
}
```

- [ ] **Step 2 — red**：过滤 `^PackageCatalog\.`，特别验证坏包不能先部分写入来源表。
- [ ] **Step 3 — 实现**：在现有一级目录枚举内判两类清单，排序后归一化；nextEntries/nextSources/nextWarnings 都在本地构造后一起提交。ManifestPath/BinaryPath 自持绝对路径；不新增 canonical/equivalent 去重。
- [ ] **Step 4 — green**：过滤 `^(PackageCatalog|CatalogScan|CatalogWiringSmoke)\.`；验证包目录名不同于 sourceId 仍正确找到库路径，警告消息调整不改变失败级别。
- [ ] **Step 5 — 门禁后提交**：`发现单插件与包清单并维护事务性来源快照`。

### Task 5: 一套纯数据分析算法与双入口计划绑定

**Files:** 新增 Include/Vase/Catalog/Analysis.h、Tests/Integration/PackageSolveTests.cpp；修改 PluginCatalog.h、Source/Catalog/Solve.cpp、Host/PackagePlan.h、Host/LoadPlan.h、Tests/CMakeLists.txt。

**Interfaces:** 产 T5 的全部类型/方法。现有 SolveError/Notes 去重/配置 coercion 保持唯一实现；Analyze 返回自持结果，Solve 基于它与来源表物化。Groups 存全部包成员期望，Entries.PackageSourceId 明确关联。

- [ ] **Step 1 — 写 PackageSolve 用例**：同源成员 Value=7 的 override 与禁用 Provider 导致 Consumer skip/归因；Analyze 无文件/无绑定仍成功。缺任一静态入口、重复/未知/动态来源绑定、两标识同 Lookup 拒绝。全禁用/依赖全跳过无需绑定，计划仍展示 skip。分析 move 后和 Catalog 销毁后，其拥有字段可读；Solve.Id 则只断言既定快照窗。

```cpp
TEST(PackageSolve, AnalyzeDoesNotRequireExecutableBinding)
{
    testing_support::CatalogSandbox sandbox{"package-solve"};
    sandbox.WriteFile("P/pack.json", R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"inProcess"},"plugins":[{"id":"Probe.A"}]})");
    vase::PluginCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    EXPECT_TRUE(catalog.Analyze(vase::LoadRequest{}).IsOk());
    EXPECT_FALSE(catalog.Solve(vase::LoadRequest{}).IsOk());
}
```

- [ ] **Step 2 — red**：过滤 `^PackageSolve\.`，静态 Lookup/Enumerate 用会记录调用的探针，断言 Analyze/Solve 调用次数为 0。
- [ ] **Step 3 — 实现**：现有参与/依赖/拓扑/配置主体改为产 AnalyzedPlugin，不重写算法。旧 Solve(req) 调新重载空集合。Id 从 Find 返回的快照串重绑定，绝不借局部分析对象。包 Groups 包含 skipped 成员；单插件条目保持空 PackageSourceId。
- [ ] **Step 4 — green**：过滤 `^(PackageSolve|Solve|AssemblyFromSolve)\.`；读 Ordered Id 集/顺序和 Notes，验证包成员被 disabled 后仍在 Group.Members。NSDMI 保住现有指定初始化点。
- [ ] **Step 5 — 门禁后提交**：`分离纯数据分析与可执行来源绑定`。

### Task 6: Host 所属镜像的建局前整包预检

**Files:** 新增 Source/Host/Detail/PackagePreflight.{h,cpp}、Tests/Integration/PackagePreflightTests.cpp；修改 Include/Vase/Host/PluginHost.h、PluginHost.cpp、Source/Host/CMakeLists.txt、Tests/CMakeLists.txt、T1 测试探针。

**Interfaces:** Host 内部 `Result<std::vector<detail::BinaryRecord*>> PreflightPackages(detail::Loader&,const LoadPlan&)`，按 PackageSources 顺序返回记录，全 skip 位置为 nullptr。指针仅在本次 CreatePod 使用，不写入计划或跨建局缓存。先纯结构检查再调用 Loader；Group.Members 与归属条目 Id 集一致，关联/Expected/来源字段缺失或矛盾拒绝。Host 添只读测试观察 `size_t ForTestResidentBinaryCount() const`，不导出句柄/地址；账本用已有 ForTestLedgerEdgeCount。测试支撑添 `LoadPlan MakeProbePackagePlan()`，生成关联完整的双成员计划，Id 借测试静态元数据，不借函数内临时串。

- [ ] **Step 1 — 写 PackagePreflight 用例**：缺/额外成员、禁用成员版本漂移都 Err，Stage0 标量调用计数=0、探针 CreateConfig/Create/OnLoad 次数=0、Host 五项实例计数=0。真实动态包错期望后记录仍 Resident、无 Reset，Host 析构后 Loader 文件/映射证据符合平台。原始手写计划不带 Groups 仍执行原路径。

```cpp
TEST(PackagePreflight, RejectsMismatchBeforeStage0)
{
    testing_support::ResetPackageProbe();
    const auto plan = testing_support::MakeProbePackagePlan();
    testing_support::SetProbeFailure("lookup-mismatch");
    std::size_t stage0Calls = 0;
    vase::PodOptions options;
    options.Stage0 = [&stage0Calls](vase::Context&) { ++stage0Calls; };
    vase::PluginHost host;
    EXPECT_FALSE(host.CreatePod(plan, options).IsOk());
    EXPECT_EQ(stage0Calls, 0U);
    EXPECT_EQ(host.ForTestLedgerEdgeCount(), 0U);
    EXPECT_EQ(testing_support::ReadPackageProbeCounts().Create, 0U);
}
```

- [ ] **Step 2 — red**：过滤 `^PackagePreflight\.`；用“清单只列一员、实际包多员”的材料，不能只制造 Group/Ordered 自相矛盾替代枚举负例。
- [ ] **Step 3 — 实现**：CreatePodImpl 现有纯计划检查后、找/创建 Pod 槽与 Stage0 前调用预检。每个参与源只 Ensure/枚举一次，ComparePackage 后装配复用返回记录；全 skip 不触碰 Loader。失败直接 Err 留架，不注册进程状态表。后面的回调与 Strict 拆除机不搬进预检。
- [ ] **Step 4 — green**：过滤 `^(PackagePreflight|AssemblyFromSolve|FailureSemantics|InProcessSource)\.`；补两次建局之间改探针元信息的负例和枚举次数断言，证明每次新检查。用同一预检来源记录消费条目，不能临时工具 Loader 验完卸掉后再加载。
- [ ] **Step 5 — 门禁后提交**：`在建局前强制检查参与包完整成员`；规矩 6 三平台证据必需。

### Task 7: 包 scan、显式源库与安全重建

**Files:** 修改 Tools/VaseCli/Scan.{h,cpp}、Cli.cpp、ManifestMerge.{h,cpp}、Tools/VaseCli/CMakeLists.txt；新增 Tests/Integration/PackageCliScanTests.cpp，修改 Tests/CMakeLists.txt。

**Interfaces:** 保留 RunScan(directory,out,err)；新增 `struct PackageScanOptions{path BinaryPath;string SourceId;bool Regenerate=false;}`、`int ScanPackage(const PackageScanOptions&,ostream&,ostream&)`、`int RunPackageScan(const vector<string>&,ostream&,ostream&)`。Cli::Run 对 scan 的 --package 模式分发，参数按 spec 与 --regenerate，重名/缺值/多余 flag 为用法档。局部测试 fixture PackageCliScanFixture 持 Sandbox/Options/Out/Err，SetUp 将 VASE_FIXTURE_VASEPLUGINS 复制到 Sandbox.Root/P 并先成功生成 pack.json；Options.SourceId=P。

- [ ] **Step 1 — 写 PackageCliScan 用例**：真实 SHARED 包显式生成，同行放无关动态库且路径有空格仍选对库；目标 plugin.json 冲突拒绝。先合法输出后设成员 enabled=false，正常更新保留；添新员默认 true。坏 pack.json 默认 rc=检查失败且字节不变，--regenerate 后成员默认 true、输出说明重置；缺符号/版本坏仍不得重建成功。

```cpp
TEST_F(PackageCliScanFixture, CorruptManifestNeedsExplicitRebuild)
{
    Sandbox.WriteFile("P/pack.json", "{broken");
    EXPECT_EQ(tools::cli::ScanPackage(Options, Out, Err), tools::cli::kExitCheckFailed);
    EXPECT_NE(Err.str().find("--regenerate"), std::string::npos);
    std::ifstream original(Sandbox.Root / "P/pack.json");
    std::string line;
    std::getline(original, line);
    EXPECT_EQ(line, "{broken");
}
```

- [ ] **Step 2 — red**：过滤 `^PackageCliScan\.`；argv 测试直接通过 tools::cli::Run，核心内容通过 ScanPackage，不能只测试 parser。
- [ ] **Step 3 — 实现**：固定输出 BinaryPath.parent_path()/pack.json；通过同一 Loader 取两入口和全成员，深拷后卸载；校验源 Id/策略合并后原子写。目录模式按已有 pack 来源更新，缺 pack 的组合库明确指向显式模式，不转写 plugin.json。拒绝静态归档，单插件 scan 行为原样。
- [ ] **Step 4 — green**：过滤 `^(PackageCliScan|VaseCliScan)\.`；只有写成功后报告 wrote/regenerated；模拟不可写目标必须保留有效文件，错误到退出码三档映射可见。
- [ ] **Step 5 — 门禁后提交**：`增加包扫描与显式清单重建`。

### Task 8: validate/plan/doctor 共用来源与检查规则

**Files:** 修改 Tools/VaseCli/Validate.{h,cpp}、Plan.cpp、Doctor.{h,cpp}、Tools/VaseConsole/Console.cpp 的 catalog solve 展示；新增 Tests/Integration/PackageCliReadTests.cpp，修改 Tests/CMakeLists.txt。

**Interfaces:** ValidateOptions 尾加 `std::vector<vase::InProcessBinding> InProcessSources`；保留 ValidateDirectory 入口。保留 DoctorDirectory(dir,out,err)，新增 `DoctorDirectory(dir,span<const InProcessBinding>,out,err)` 重载。普通 argv 不接地址或假 --static 指针。PlanDirectory/CmdCatalogSolve 使用 Analyze，只有实际 CmdPodNew 使用 Solve。

- [ ] **Step 1 — 写 PackageCliRead 用例**：dynamic validate 整包错集/禁用成员漂移未过；static 普通 validate/doctor rc=检查失败，输出无法验证与不适用各自位置。给生成头双入口的核心调用完整验证通过。plan 在 dynamic 文件不存在或 static 无绑定时仍分析成功并准确显示来源，探针执行数=0；保留硬 skip 的失败级别和 Notes.Cause。

```cpp
TEST(PackageCliRead, StaticPlanNeedsNoRuntimeBinding)
{
    testing_support::CatalogSandbox sandbox{"static-plan"};
    sandbox.WriteFile("P/pack.json", R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"inProcess"},"plugins":[{"id":"Probe.A"}]})");
    tools::cli::PlanOptions options;
    options.PluginDirectory = sandbox.Root;
    std::ostringstream out, err;
    EXPECT_EQ(tools::cli::PlanDirectory(options, out, err), tools::cli::kExitOk);
    EXPECT_NE(out.str().find("inProcess"), std::string::npos);
}
```

- [ ] **Step 2 — red**：过滤 `^PackageCliRead\.`，验证不是输出一句 static 就默认成功。
- [ ] **Step 3 — 实现**：validate 对包使用 ComparePackage，对独立插件维持单描述符契约；完整校验不按 Preset 过滤。依赖检查用 Analyze，不能让缺静态绑定掩盖依赖结论。doctor 按 Sources 一次检查文件与来源目录，保留根写探针；锁/身份在任何动态装载前采集。来源无地址时不给 HeaderVersion 假 ok。
- [ ] **Step 4 — green**：过滤 `^(PackageCliRead|VaseCliValidate|VaseCliPlan|VaseCliDoctor)\.` 及既有 Console catalog 回放；多员来源的每组检查只出现一次，同时成员诊断仍点名 Id。
- [ ] **Step 5 — 门禁后提交**：`让读取命令支持两种包来源并区分验证能力`。

### Task 9: 包成员 AdoptInto 与物理兄弟候选

**Files:** 修改 Source/Catalog/PluginCatalog.cpp 的 AdoptInto，必要时抽到 Source/Catalog/CatalogAdopt.cpp 并加入 CMakeLists；新增 Tests/HotSwap/PackageAdoptTests.cpp，修改 Tests/CMakeLists.txt。

**Interfaces:** 公共 AdoptInto(catalog,host,handle,id) 签名不变。按 SourceFor 的 ManifestPath 重读包、Find 请求 Id、BuildExpectation；对 legacy 来源继续 ParseManifestFile。SiblingBinaries 从其他动态来源文件名获得，去重且排除目标绝对路径，static 来源不进入候选。

- [ ] **Step 1 — 写 PackageAdopt 用例**：建局后在沙箱修改 pack 目标成员版本或删除成员，AdoptInto 必须读到磁盘变化并拒绝；同包另一 Id 不生成“导入自己”的误判；static 来源明确 Err。依赖仍未齐时沿用 Host 的结构化报告，不重新发明错误码。

```cpp
TEST(PackageAdopt, RefusesInProcessSource)
{
    testing_support::CatalogSandbox sandbox{"package-adopt"};
    sandbox.WriteFile("P/pack.json", R"({"schemaVersion":1,"sourceId":"P","source":{"kind":"inProcess"},"plugins":[{"id":"Probe.A","enabledByDefault":false}]})");
    vase::PluginCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(sandbox.Root).IsOk());
    const auto solved = catalog.Solve(vase::LoadRequest{});
    ASSERT_TRUE(solved.IsOk());
    vase::PluginHost host;
    const auto created = host.CreatePod(solved.Value().Plan);
    ASSERT_TRUE(created.IsOk());
    const auto adopted = vase::AdoptInto(catalog, host, created.Value(), "Probe.A");
    EXPECT_FALSE(adopted.IsOk());
    EXPECT_TRUE(host.DestroyPod(created.Value()).Clean());
}
```

- [ ] **Step 2 — red**：过滤 `^PackageAdopt\.`；材料必须是快照建立后的磁盘修改，不能只构造首次坏清单。
- [ ] **Step 3 — 实现**：来源解析与查询留在 Catalog；Host 收同一拥有形单插件期望。包元信息重读不等于承诺一键热替换，不放宽全局持有者闸、叶约束或身份检查。
- [ ] **Step 4 — green**：过滤 `^(PackageAdopt|Adopt|Eject|HotSwap)\.`；按根规矩 6 三平台复核完整族，不用该局部过滤替代总门禁。
- [ ] **Step 5 — 门禁后提交**：`按包清单重读领回成员并去重兄弟来源`。

### Task 10: 可复用包生成与源码启用策略

**Files:** 新增 Tools/VaseCli/PackageBuild.{h,cpp}、Tests/Integration/PackageBuildTests.cpp、Tests/TestingSupport/PackageBuildProbe.h；修改 Tools/VaseCli/CMakeLists.txt、Tests/CMakeLists.txt，T3 fixture 源清单。

**Interfaces:** 产 Contract 的 PackageMemberInput/PackageBuildOptions/GeneratePackageManifest；T11 再实现 BuildAndStagePackage。只共用 Scan.cpp 的元信息物化，把该函数移到 PackageBuild.cpp 时 Scan.h 仍转声明，不再保留两份转换体。测试支撑产 `PackageBuildOptions MakeProbePackageBuildOptions()`（namespace testing_support）：SourceId=Probe、Kind=inProcess、T1 双入口，Probe.A/Probe.B 的 Descriptor 用静态查询包装函数，各 SourceManifest 未声明；不承载真实发布闭环，真包另用 T3 材料。

- [ ] **Step 1 — 写 PackageBuild 用例**：分别读取真实 SHARED 与 STATIC 包，DescriptorSymbol/Descriptor 与成员逐项匹配；声明源清单 enabled=false 正确进入输出，未声明默认 true。非空缺文件/坏 JSON/错 Id/输入成员缺失或重复均 Err；源清单中版本等展示值不同但语法合法时输出仍取描述符事实。

```cpp
TEST(PackageBuild, RejectsWrongSourceManifestId)
{
    testing_support::ResetPackageProbe();
    testing_support::CatalogSandbox sandbox{"package-build"};
    sandbox.WriteFile("wrong/plugin.json", R"({"schemaVersion":1,"id":"Wrong.Id"})");
    auto options = testing_support::MakeProbePackageBuildOptions();
    options.Members.begin()->SourceManifest = sandbox.Root / "wrong/plugin.json";
    EXPECT_FALSE(tools::cli::GeneratePackageManifest(options).IsOk());
}
```

- [ ] **Step 2 — red**：过滤 `^PackageBuild\.`，关掉策略读取时 false 用例必须转红；不能只证明 JSON 默认 true。
- [ ] **Step 3 — 实现**：动态只用一个工具 Loader，静态直接显式入口；核对输入成员映射与枚举完整集。只有启用策略从 ParseManifestFile 读取，源清单其他字段不覆盖描述符；所有临时借用在返回前深拷，工具动态记录各失败支也释放。
- [ ] **Step 4 — green**：过滤 `^(PackageBuild|PackageCliScan)\.`；生成后原 source metadata 不再存指针，值形可安全 move 和写出。
- [ ] **Step 5 — 门禁后提交**：`从真实包与成员源码策略生成发布清单`。

### Task 11: 目录事务与显式 CMake 发布目标

**Files:** 新增 Tools/VaseCli/PackageStage.cpp、Detail/PackageStage.h、Cmake/VasePackageStage.cmake、VasePackageGenerator.cpp.in、Tests/Integration/PackageStageTests.cpp、Tests/Package/VerifyStageDependencies.cmake、Tests/Package/VerifyStageManifest.cpp；修改根 CMakeLists.txt、Tools/VaseCli/CMakeLists.txt、Tests/Package/CMakeLists.txt、Tests/CMakeLists.txt。

**Interfaces:** 产 BuildAndStagePackage。CMake `vase_stage_pack(stageTarget PACK_TARGET <target> SOURCE_ID <id> OUTPUT_DIRECTORY <dir> RUNTIME_FILES <paths...>)`。函数须在 VaseCliCore 定义后调用；generator exe `<stageTarget>.Generator` 使用 EXCLUDE_FROM_ALL，custom target 不带 ALL，COMMAND/路径用 VERBATIM。模板只含已生成的类型/符号映射，sourceId、文件路径从 argv 传入，不把路径插成未转义 C++ 字面量。

- [ ] **Step 1 — 写 PackageStage 用例**：输出原目录放 sentinel 与 stale 库，缺 RuntimeFile 的准备失败保持原字节；成功后 stale 消失、新 pack.json/库存在。内部 namespace tools::cli::package_detail 提供 `RenameDirectoryFn=std::function<Result<void>(const path&,const path&)>` 的测试接缝，仅第二次安装 rename 故意失败，真实目录备份必须恢复，返回 Err。接缝不进入用户 CLI/API。

```cpp
TEST(PackageStage, PreparationFailureKeepsOriginalDirectory)
{
    testing_support::ResetPackageProbe();
    testing_support::CatalogSandbox sandbox{"package-stage"};
    sandbox.WriteFile("output/sentinel", "original");
    auto options = testing_support::MakeProbePackageBuildOptions();
    options.OutputDirectory = sandbox.Root / "output";
    options.RuntimeFiles.push_back(sandbox.Root / "missing-dependency.dll");
    EXPECT_FALSE(tools::cli::BuildAndStagePackage(options).IsOk());
    std::ifstream original(options.OutputDirectory / "sentinel");
    std::string line;
    std::getline(original, line);
    EXPECT_EQ(line, "original");
}
```

- [ ] **Step 2 — red**：过滤 `^PackageStage\.`；把 restore 临时移除时备份恢复证人必须转红，测试不是仅对 callback 次数断言。
- [ ] **Step 3 — 实现**：检查输出非文件系统根/项目根/构建根且不是输入文件祖先；拒绝指向 reparse/symlink 的输出与重名 runtime 文件覆盖 pack.json/主库。同父目录唯一 temp/backup，先生成/复制/验证，再 old→backup、temp→output，失败恢复；无法恢复时保留备份并 Err 点名恢复位置，绝不删除唯一原件。成功才清备份和报告。内部 IO 处理走 error_code 形态。
- [ ] **Step 4 — 接 CMake 与构建级 green**：生成器 dynamic 不链接包，只依赖构建产物；static 链对应包，使用 T3 头和成员 descriptor 函数。所有源清单/包目标/runtime 输入为显式 DEPENDS，普通 build 不执行 staging。Tests/Package 将 fixture 的策略输入复制到构建树、由 fixture MANIFEST 属性声明，测试只修改该副本并在结束恢复，不碰 Samples 源文件。VerifyStageDependencies.cmake 运行目标→改策略→只重跑目标→比对输出→手改 staging→再跑目标→确认源策略恢复；缺 runtime 输入用测试自有 fixture-extra-runtime.dat，不删除真实框架库。路径至少含一处空格，fixture 更改不得并发。

  构建脚本不另解析 JSON：VerifyStageManifest.cpp 编成 VasePackageStageVerifier，链 VaseBuildOptions/VaseCatalog，收参数 `<pack.json> <plugin-id> <true|false>`，通过 ParsePackageManifestFile 查询该成员并用进程退出码报告匹配。脚本调用它验证策略，同时对自己复制的策略输入做已知字节替换；每个失败分支恢复输入副本，再报告错误。生成器与 verifier 都不进入插件 target 的链接图。
- [ ] **Step 5 — 门禁后提交**：`提供可复用包生成目标与事务性发布目录`。配置期拒绝两个 stage target 输出重叠目录；平台依赖解析在 T12 验真加载。

### Task 12: 两条真实发布闭环、文档与全支收口

**Files:** 新增 Tests/Integration/PackageAssemblyTests.cpp、Tests/StaticLink/PackageAssemblyTests.cpp；修改 Tests/CMakeLists.txt、Tests/StaticLink/CMakeLists.txt、Tests/Package/CMakeLists.txt、CLAUDE.md、README.md、wiki/vase-architecture.md、wiki/vase-console-use.md；只在有指针/规则变化时更新工程技能对应 reference。

**Interfaces:** 消费前十一任务的公开 API 和显式 stage targets。CMake 为 fixture setup 建立 PackageReleaseStage，gtest 运行要求该 fixture；额外 build 目标在 ctest 中不得并发写同一包输出，构建级策略改写测试加资源锁。PRE_TEST 发现阶段不能访问尚未 stage 的文件。测试局部 fixture StaticPackageAssemblyFixture 的 SetUp 只刷新生产目标的静态 staging；字段为 Catalog 与 Bindings（Provider/Consumer 两 sourceId 对应生成头），不持第二个 Host；每个测试以局部 Host 管理生命周期。

- [ ] **Step 1 — 写 PackageAssembly/StaticPackageAssembly 闭环**：从生产生成/staging 目标实际输出 Refresh，不手写成员 Id；动态从 stage 包路径加载；静态同 exe 两包各有真实 Lookup/Enumerate，Consumer 读取 Provider Value=7，拆局 Clean，重复建销。修改一个参与包禁用成员的文档字段，下一次 CreatePod 在 Stage0 前 Err；全禁用包缺库/绑定仍正常跳过。独立 validate 对该缺料包仍报告未完成/失败。

```cpp
TEST_F(StaticPackageAssemblyFixture, TwoPackagesConsumeConfiguredServiceAndStopCleanly)
{
    vase::PluginOverride provider;
    provider.Id = "Vase.PackageProvider";
    provider.Config.Set("Value", vase::Value::From(std::int32_t{7}));
    const std::array overrides{provider};
    vase::LoadRequest request;
    request.Overrides = overrides;
    const auto solved = Catalog.Solve(request, Bindings);
    ASSERT_TRUE(solved.IsOk());
    vase::PluginHost host;
    const auto created = host.CreatePod(solved.Value().Plan);
    ASSERT_TRUE(created.IsOk());
    EXPECT_EQ(host.Resolve(created.Value())->Root().Get<package_fixture::IResult>().Observed(), 7);
    EXPECT_TRUE(host.DestroyPod(created.Value()).Clean());
}
```

- [ ] **Step 2 — red/green**：过滤 `^(PackageAssembly|StaticPackageAssembly)\.`，同真动态/静态闭环各有配置或服务路径负例；新测试不通过打印成功字符串代替运行行为。打断来源绑定、完整成员校验或配置消费各取一枚变异红证据，临时改动逐字还原。
- [ ] **Step 3 — 全量收口**：运行根 CLAUDE.md 的八线删树重配、ctest 与 -N，导出名字集合和本波 TEST/ctest 增量；规矩 6 族三平台完整证据；四线 tidy 退出码+正文双计数、format 全平台。静态保活两组和入口身份从干净全量链接验，不能拿增量幽灵导出当保活成功。
- [ ] **Step 4 — 文书落账与自查**：只按实测回填 CLAUDE 的状态/基数/检查表，旧波读数保留历史；wiki 标清包发现、双静态入口、预检留架、CLI 能力与移动边界，README 给实际命令及显式构建目标用法。对照下表逐格贴证据，未跑项写原因，不从一条平台推另一条。
- [ ] **Step 5 — 门禁后提交与全支审查**：中文 `补齐包发布装配闭环与收口记录`；按所选执行流程完成整支审查。收口前不自动 merge/squash 主线、不删除唯一未归档证据。

## Spec Coverage

| spec 验收 | 任务 |
|---|---|
| A1 单插件无回归、包往返 | T2/T4/T5 |
| A2 重复/语法/事务性来源表 | T2/T4 |
| A3 配置、启用、依赖与归因 | T5/T12 |
| A4 动态生产生成/stage/装配 | T7/T10/T11/T12 |
| A5 真双 STATIC 包 | T3/T10/T12 |
| A6 绑定错误与全跳过免执行 | T5/T6/T12 |
| A7 全成员、安全与双入口一致性 | T1/T6/T8 |
| A8 Stage0 前预检与禁用成员 | T6/T12 |
| A9 scan 保真/损坏重建 | T7 |
| A10 零装载 plan | T5/T8 |
| A11 doctor 来源去重/锁顺序 | T8 |
| A12 Adopt 重读/静态拒绝 | T9 |
| A13 目录规则、无误扫、原保活/布局 | T3/T4/T12 |
| A14 每局新检查/计划误用/原始入口 | T5/T6 |
| A15 留架/无实例/宽容与 Strict | T6/T12 |
| A16 源码策略及依赖变更 | T10/T11 |
| A17 staging 事务与可复用目标 | T11/T12 |

## Plan Review / Handoff

本计划只规定将来执行的任务与验证，尚未运行构建、测试或改产品代码。接口消费/产出按 Interface Contract，新增测试文件名和 CMake 接入在任务中列明；A1–A17 与 F1–F5 均有归属。设计分支已有文书改动继续保留。

执行方式尚未选择。推荐 subagent-driven：Host/目录事务的失败语义与 CMake 跨任务接口值得逐任务独立复核；若选 native，则按同一计划串行实现，并在末尾安排全支独立审查。需要用户评审本计划并选择执行方式后开始实现。
