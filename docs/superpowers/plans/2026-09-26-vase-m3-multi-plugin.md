# Vase M3（判据 3a 全量形 / 3d 进程级状态）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 兑现 spec `docs/superpowers/specs/2026-09-26-vase-m3-multi-plugin-design.md`（决定 D90–D103）：判据 3a 全量形（三插件局 50 轮 Eject/Adopt + 相邻实例全程未受扰 + 空壳可拆）、判据 3d（`ProcessStateDesc` 进描述符、`kHeaderVersion` 3→4、Eject 按「无活实例」口径自动 Reset）、清单侧 `processStates` 双向等值收口、前端打点。

**Architecture:** 进程级状态由**描述符**承载（§9.1：不经过 Pod，否则「进程级容器持有实例级对象」有了合法入口），`Loader` 的 `BinaryRecord` 在装配点把描述符里的表记下来，`EjectPlugin` 在全局闸处按「这份 binary 在任何 Pod 都无活实例」逐条 `Reset` 并写入 `EjectReport::ProcessStatesReset`。清单侧按 D89 同构：顶层 `processStates` 名字数组 ↔ 描述符 Name 集**多重集双向等值**，把「作者忘写 `.ProcessStates`」从静默点变成主链上加载被拒。比对器的「全字段」纪律由一条**结构化绑定**承重（标识符个数必须等于非静态数据成员数）。

**Tech Stack:** C++20（无异常，`Result<T>` 全通道）、CMake + presets + vcpkg（gtest 1.18 / nlohmann-json）、GoogleTest + CTest、clang-tidy / clang-format（LLVM 23.1.0）、Win(cl/clang-cl) + Linux(clang/libc++)。

**Spec:** `docs/superpowers/specs/2026-09-26-vase-m3-multi-plugin-design.md`（本计划与它冲突处以磁盘 + 本文「偏离裁决记录」为准，验收时回挂勘误）。

## Global Constraints

每个任务隐含包含本节全部：

- 新 target 必链 `VaseBuildOptions`；新插件 target 必走 `vase_add_plugin_fixture`（CLAUDE.md 规矩 1/5）。
- 全项目关异常：**零** `throw/try/catch`；测试里**禁用 `EXPECT_THROW` 一族**，失败断言只用 `Result` 返回值（规矩 2）。
- Vase 自己的头**引号**包含（规矩 3）；`ThirdParty/cli` 同样引号。
- 聚合体新字段一律带 NSDMI（`-Wmissing-designated-field-initializers` + `/WX` 是 error）。
- **不用 `operator[]` 访问容器/数组**（`cppcoreguidelines-pro-bounds-*`）：用迭代器、`std::next`、`Begin()`。
- 提交信息：中文，**不加任何 AI 署名尾注**（无 `Co-Authored-By:` 等）。
- 格式门（每个改了 `.h/.cpp` 的任务收尾）：
  `git add` 新文件后
  `git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror`
- 规矩 6 双平台义务：**T1、T3、T4、T5、T6、T7** 完成后立刻跑
  `ctest --preset win-x64-clang-debug -R 'HotSwap|Eject|Adopt' --output-on-failure`
  与
  `wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && ctest --preset linux-x64-clang-debug -R "HotSwap|Eject|Adopt" --output-on-failure'`
  （WSL 那条里**没有** `$`，可直接用；有 `$` 的命令必须先落脚本文件——CLAUDE.md 的教训）。
- 日常构建用 `win-x64-clang-debug`；msvc 线要经 `Scripts/msvc-env.cmd`；六线全量只在 T10 收口做（删树重配）。
- 基数与文档真值只在 T9/T10 落 `CLAUDE.md`，**不要**提前抄进技能目录（规矩 7）。
- 本波**不动**任何工具链文件与 `Cmake/` 下的 flag。

## File Structure（全波次地图）

```text
新建
  Tests/HotSwap/fixtures/StatefulPlugin/StatefulPlugin.cpp        进程级状态探针（T5）
  Tests/HotSwap/fixtures/NeighborC/NeighborC.cpp                  第二只邻居（T6）

改动（要点）
  Include/Vase/PluginDescriptor.h              ProcessStateDesc + PluginMeta 尾追加槽 + kHeaderVersion=4（T1）
  Tests/Unit/DescriptorTests.cpp               槽布局 + NSDMI 零改动 + 钉字面值 3U→4U（T1）
  Include/Vase/Catalog/ManifestView.h          ManifestEntry.ProcessStates（T2）
  Source/Catalog/ManifestJson.cpp              processStates 解析 + OnlyKeys 白名单（T2）
  Tests/Unit/ManifestJsonTests.cpp             解析正反用例（T2）
  Include/Vase/Host/ManifestExpectation.h      ManifestExpectation.ProcessStates（T3）
  Source/Host/Detail/ManifestCompare.cpp       结构化绑定绊线 + CompareStringSet（T3）
  Source/Catalog/PluginCatalog.cpp             BuildExpectation 填 ProcessStates（T3）
  Tests/Integration/LoadTimeComparisonTests.cpp 单向臂（期望多列 → 拒）（T3）
  Tests/Integration/fixtures/EdgeConsumerPlugin/EdgeConsumerPlugin.cpp  声明进程级状态（T4）
  Tests/Integration/fixtures/manifests/edge_consumer/plugin.json        列名（T4）
  Tests/TestingSupport/AdoptExpectations.h     MakeEdgeConsumerExpectation 补字段（T4）
  Include/Vase/Host/Loader.h                   BinaryRecord 记表（T5）
  Source/Host/PluginHost.cpp                   MakeInstance 登记（T5）；EjectPlugin 三处 Reset（T5）
  Tests/HotSwap/EjectTests.cpp                 Reset 三态用例（T5）
  Tests/HotSwap/fixtures/BCommon.h             IStateProbe / IPulse 两个标识（T5/T6）
  Tests/HotSwap/fixtures/CMakeLists.txt        两个新 target（T5/T6）
  Tests/CMakeLists.txt                         两个新 VASE_FIXTURE_* 宏（T5/T6）
  Tests/HotSwap/HotSwapLoopTests.cpp           三插件局 + 50 轮 + 每轮派拍（T6）
  Tests/Integration/RecursiveTeardownTests.cpp 空壳 Eject 语义重写（T7）
  Tools/VaseConsole/Console.cpp                PrintEjectReport 补打点 + 注释改准（T8）
  Samples/Embedding/main.cpp                   同上（T8）
  CLAUDE.md / wiki/vase-architecture.md        文书义务（T9）
  Tests/Integration/fixtures/RecursiveTeardownTests.cpp 已在 T7
```

---

### Task 1: `ProcessStateDesc` 槽与 `kHeaderVersion` 3→4

**Files:**
- Modify: `Include/Vase/PluginDescriptor.h`
- Test: `Tests/Unit/DescriptorTests.cpp`

**Interfaces:**
- Consumes: 无（本波第一个任务）
- Produces: `vase::ProcessStateDesc{ std::string_view Name; void (*Reset)(); }`；`PluginMeta::ProcessStates`（`MetaArray<ProcessStateDesc, 16>`）；`vase::kHeaderVersion == 4U`

- [ ] **Step 1: 改描述符头**

`Include/Vase/PluginDescriptor.h`：`kHeaderVersion` 那一行的注释块加一行并改值。

```cpp
// 1 → 2（M2a-T3）：PluginMeta 布局变更——新增 OptionalRequires 与 Config 两槽（D26）。
// 2 → 3（M2b-波2）：FieldInfo 尾追加 Choices/ChoiceCount 两槽（D76）。
// 3 → 4（M3）：PluginMeta 尾追加 ProcessStates 一槽（D92）。
inline constexpr std::uint32_t kHeaderVersion = 4U;
```

在 `struct ServiceRef` **之后**加新类型：

```cpp
// §9.1 进程级状态的登记项（M3/D92）：作者侧一行一条。Name 是点名与清单比对的**身份**
// （D93 多重集等值），Reset 由 Eject 在卸货前按 D91 调用——**必须幂等**（§9.1 契约）。
// 纯 POD + 函数指针：不放 std::function（跨 DLL 且描述符是只读数据段，M5 的 scan 要读它）。
struct ProcessStateDesc
{
    std::string_view Name;
    void (*Reset)();
};
```

`struct PluginMeta` **尾追加**一槽（`Config` 之后）：

```cpp
    // NSDMI `{}` 是刻意的：指定初始化省略豁免的前提，既有站点零改动靠它成立（同 OptionalRequires）。
    // NOLINTNEXTLINE(readability-redundant-member-init) NSDMI 为刻意，理由见上条注释。
    MetaArray<ProcessStateDesc, 16> ProcessStates{}; // M3/D92：Eject 时自动 Reset 并写入报告（§9.1）
```

- [ ] **Step 2: 改既有断言，写新断言**

`Tests/Unit/DescriptorTests.cpp`：把钉字面值那行改成 4（这是**预期红**，注释说明了它为什么存在）：

```cpp
    EXPECT_EQ(vase::kHeaderVersion, 4U); // 钉字面值：自比对拦不住常量被误改，而它是 §8.3 的描述符 ABI 闸
```

在文件里再加**第二只探针**（证明「既有站点零改动」——它**不写** `.ProcessStates`）：

```cpp
class DescriptorProbeNoStatesPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        static_cast<void>(ctx);
        return vase::Result<void>::Ok();
    }
};
} // namespace
```

Hmm——第二只探针要落在**同一个**匿名命名空间里，所以上面那段应与既有探针同处（不要在中间闭合再开）。把 `OnLoad` 体照抄既有探针即可。

```cpp
VASE_PLUGIN(DescriptorProbeNoStatesPlugin){
    .Id = "Vase.DescriptorProbeNoStates",
    .DisplayName = "省略进程级状态的探针",
    .Version = "0.0.1",
};
```

给**第一只**探针补进程级状态（`ResetProbeState` 放匿名命名空间，与探针类同格）：

```cpp
void ResetProbeState() {} // 幂等、无状态：本条只证槽的装载与指针非空
```

```cpp
    .Config = vase::FieldsOf<ProbeConfig>(),
    .ProcessStates = {{.Name = "Vase.DescriptorProbe.State", .Reset = &ResetProbeState}},
```

新用例（放 `GetPluginRoutesByIdentity` 之前）：

```cpp
TEST(Descriptor, ProcessStatesSlotPopulatedAndOptional)
{
    const vase::PluginDescriptor* d = VasePluginDesc_DescriptorProbePlugin();
    ASSERT_EQ(d->Meta->ProcessStates.Size(), 1U);
    EXPECT_EQ(d->Meta->ProcessStates.Begin()->Name, "Vase.DescriptorProbe.State");
    EXPECT_NE(d->Meta->ProcessStates.Begin()->Reset, nullptr);

    // NSDMI 零改动：省略 .ProcessStates 的站点照常编过，且默认是空表（不是垃圾）。
    const vase::PluginDescriptor* bare = VasePluginDesc_DescriptorProbeNoStatesPlugin();
    EXPECT_EQ(bare->Meta->ProcessStates.Size(), 0U);
    EXPECT_TRUE(bare->Meta->ProcessStates.Empty());
}
```

- [ ] **Step 3: 构建并跑该文件**

Run: `cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R Descriptor --output-on-failure`
Expected: PASS（4 条 Descriptor.*）。若 `bare->Meta->ProcessStates.Size()` 非 0，说明 NSDMI 没生效——回去查尾追加位置。

- [ ] **Step 4: 全量重编（本步必红一次，然后转绿）**

Run: `cmake --build --preset win-x64-clang-debug`
Expected: 第一次可能因其它 TU 里硬编码 3 而红；已知唯一的钉字面值点在 `DescriptorTests.cpp`（Step 2 已改）。其余编译错误逐个读，**不要**用 `-D` 或放宽断言绕。

- [ ] **Step 5: 规矩 6 双平台选择子**

Run: `ctest --preset win-x64-clang-debug -R 'HotSwap|Eject|Adopt' --output-on-failure`
Run: `wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug -R "HotSwap|Eject|Adopt" --output-on-failure'`
Expected: 两侧全绿。描述符布局变更**必须**两侧各留证据。

- [ ] **Step 6: 格式门 + 提交**

```bash
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
git add Include/Vase/PluginDescriptor.h Tests/Unit/DescriptorTests.cpp
git commit -m "M3-T1：PluginMeta 尾追加 ProcessStates 槽，kHeaderVersion 3→4"
```

---

### Task 2: 清单 `processStates` 解析面

**Files:**
- Modify: `Include/Vase/Catalog/ManifestView.h`（`ManifestEntry`）
- Modify: `Source/Catalog/ManifestJson.cpp`（新 helper + `OnlyKeys` + 解析块）
- Test: `Tests/Unit/ManifestJsonTests.cpp`

**Interfaces:**
- Consumes: 无（纯清单面，不依赖 T1）
- Produces: `ManifestEntry::ProcessStates`（`std::vector<std::string>`）；`ParseManifestFile` 认识顶层键 `processStates`

- [ ] **Step 1: 加字段**

`Include/Vase/Catalog/ManifestView.h`，`ManifestEntry` 的 `Config` 之后：

```cpp
    std::vector<ManifestConfigField> Config;
    std::vector<std::string> ProcessStates; // M3/D93：进程级状态的**名字**清单（缺键 = 空集）
```

- [ ] **Step 2: 写解析 helper（照 `ParseDeps` 的体例）**

`Source/Catalog/ManifestJson.cpp`，紧邻 `ParseDeps`：

```cpp
// M3/D93：进程级状态的名字数组。重名即刻拒——与 choices 的重值/重 label 同例（D80），
// 而不是留给比对器报「多重集不等」（那是作者拿不到的归因）。
Result<std::vector<std::string>> ParseProcessStates(const json& array, const std::filesystem::path& file)
{
    std::vector<std::string> out;
    if (!array.is_array())
    {
        return Result<std::vector<std::string>>::Err(
            ManifestError(file, "field \"processStates\" must be an array"));
    }
    for (const json& item : array)
    {
        if (!item.is_string() || item.get_ref<const std::string&>().empty())
        {
            return Result<std::vector<std::string>>::Err(
                ManifestError(file, "processStates entries must be non-empty strings"));
        }
        const std::string& name = item.get_ref<const std::string&>();
        for (const std::string& held : out)
        {
            if (held == name)
            {
                return Result<std::vector<std::string>>::Err(
                    ManifestError(file, "processStates: duplicate \"" + name + "\" (D93)"));
            }
        }
        out.push_back(name);
    }
    return Result<std::vector<std::string>>::Ok(std::move(out));
}
```

- [ ] **Step 3: 白名单与解析块**

`OnlyKeys` 的列表里，`"config",` 之后加一行：

```cpp
                      "processStates",
```

`ParseManifestFile` 的 `config` 解析块之后（`return Result<ManifestEntry>::Ok(...)` 之前）：

```cpp
    if (const auto array = root.find("processStates"); array != root.end())
    {
        auto parsed = ParseProcessStates(*array, file);
        if (!parsed.IsOk())
        {
            return Result<ManifestEntry>::Err(parsed.GetError());
        }
        entry.ProcessStates = std::move(parsed.Value());
    }
```

- [ ] **Step 4: 写用例（先红）**

`Tests/Unit/ManifestJsonTests.cpp` 末尾（`} // namespace` 之前）加三条。该文件已有临时文件写入的 helper——照它的既有写法取一个可写路径（若无，用 `std::filesystem::temp_directory_path()` + 唯一名，与 `CatalogSandbox` 同法）。

```cpp
TEST(ManifestJson, ProcessStatesParsedInOrder)
{
    const auto file = WriteTempManifest(R"({
        "schemaVersion": 1, "id": "Vase.PS", "version": "0.0.1",
        "processStates": ["Vase.PS.Registry", "Vase.PS.Cache"]
    })");
    const auto parsed = vase::ParseManifestFile(file, "ps");
    ASSERT_TRUE(parsed.IsOk());
    ASSERT_EQ(parsed.Value().ProcessStates.size(), 2U);
    EXPECT_EQ(*parsed.Value().ProcessStates.begin(), "Vase.PS.Registry");
    EXPECT_EQ(*std::next(parsed.Value().ProcessStates.begin()), "Vase.PS.Cache");
}

TEST(ManifestJson, ProcessStatesMissingIsEmpty)
{
    const auto file = WriteTempManifest(R"({ "schemaVersion": 1, "id": "Vase.PS2", "version": "0.0.1" })");
    const auto parsed = vase::ParseManifestFile(file, "ps2");
    ASSERT_TRUE(parsed.IsOk()); // 缺键 = 空集，向后兼容（D93）
    EXPECT_TRUE(parsed.Value().ProcessStates.empty());
}

TEST(ManifestJson, ProcessStatesDuplicateRefused)
{
    const auto file = WriteTempManifest(R"({
        "schemaVersion": 1, "id": "Vase.PS3", "version": "0.0.1",
        "processStates": ["Vase.PS3.A", "Vase.PS3.A"]
    })");
    const auto parsed = vase::ParseManifestFile(file, "ps3");
    ASSERT_FALSE(parsed.IsOk());
    EXPECT_NE(parsed.GetError().Message().find("duplicate"), std::string::npos);
}
```

Run: `cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R ManifestJson --output-on-failure`
Expected: 三条新用例 PASS（若 `WriteTempManifest` 尚不存在，本条即红——照 `CatalogSandbox.h` 的 temp 树写法补一个 TU 内 helper，别引 gtest 之外的东西）。

- [ ] **Step 5: 格式门 + 提交**

```bash
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
git add Include/Vase/Catalog/ManifestView.h Source/Catalog/ManifestJson.cpp Tests/Unit/ManifestJsonTests.cpp
git commit -m "M3-T2：清单顶层 processStates 名字数组的解析与重名闸"
```

---

### Task 3: 期望形、双向等值比对与「全字段」绊线

**Files:**
- Modify: `Include/Vase/Host/ManifestExpectation.h`
- Modify: `Source/Host/Detail/ManifestCompare.cpp`
- Modify: `Source/Catalog/PluginCatalog.cpp`（`BuildExpectation`）
- Test: `Tests/Integration/LoadTimeComparisonTests.cpp`

**Interfaces:**
- Consumes: `ManifestEntry::ProcessStates`（T2）、`PluginMeta::ProcessStates`（T1）
- Produces: `ManifestExpectation::ProcessStates`（`std::vector<std::string>`）；`CompareDescriptor` 覆盖该字段；比对器开头的结构化绑定绊线

- [ ] **Step 1: 期望形加字段**

`Include/Vase/Host/ManifestExpectation.h`，`ManifestExpectation` 的 `Config` 之后（**保持在 NOLINT 块内**）：

```cpp
    std::vector<ExpectedConfigField> Config = {}; // key 对齐，双向都数得着（D89 天然覆盖）
    std::vector<std::string> ProcessStates = {};  // M3/D93：多重集双向等值，序不敏感
```

- [ ] **Step 2: 比对器加集合 helper 与字段**

`Source/Host/Detail/ManifestCompare.cpp`，`CompareServiceSet` 之后加：

```cpp
// M3/D93：字符串多重集等值（与 CompareServiceSet 同律：排序后归并走查，含条数）。
void CompareStringSet(std::string_view label, std::vector<std::string> manifestSet, std::vector<std::string> binarySet,
                      DiffList& diffs)
{
    std::ranges::sort(manifestSet);
    std::ranges::sort(binarySet);
    auto manifestIt = manifestSet.begin();
    auto binaryIt = binarySet.begin();
    while (manifestIt != manifestSet.end() || binaryIt != binarySet.end())
    {
        if (binaryIt == binarySet.end() || (manifestIt != manifestSet.end() && *manifestIt < *binaryIt))
        {
            AddDiff(diffs, std::string{label} + "[" + *manifestIt + "] missing in binary", "declared", "absent");
            ++manifestIt;
        }
        else if (manifestIt == manifestSet.end() || *binaryIt < *manifestIt)
        {
            AddDiff(diffs, std::string{label} + "[" + *binaryIt + "] missing in manifest", "absent", "declared");
            ++binaryIt;
        }
        else
        {
            ++manifestIt;
            ++binaryIt;
        }
    }
}
```

`CompareDescriptor` 改为（**结构化绑定是绊线，别删**）：

```cpp
Result<void> CompareDescriptor(const ManifestExpectation& expected, const PluginDescriptor& desc)
{
    const PluginMeta& meta = *desc.Meta;
    DiffList diffs;
    // M3/D101 替代：结构化绑定的标识符个数**必须**等于 ManifestExpectation 的非静态数据成员数，
    // 少一个即硬编译错误——给期望形加字段而忘了比对，这一行当场红。逐个接进下方比对（绑定即被用过），
    // 比对那一半因此同样有闸。构造那一半不靠它：BuildExpectation 漏填新字段会让期望恒为空集，
    // 凡清单列了名字的插件当场被加载期拒——响亮。
    const auto& [id, displayName, version, requiresRefs, optionalRequiresRefs, providesRefs, configFields,
                 processStates] = expected;
    if (id != meta.Id)
    {
        AddDiff(diffs, "id", id, meta.Id);
    }
    if (displayName != meta.DisplayName)
    {
        AddDiff(diffs, "displayName", displayName, meta.DisplayName);
    }
    if (version != meta.Version)
    {
        AddDiff(diffs, "version", version, meta.Version);
    }
    CompareServiceSet("requires", ExpectSet(requiresRefs), BinarySet(meta.Requires), diffs);
    CompareServiceSet("optionalRequires", ExpectSet(optionalRequiresRefs), BinarySet(meta.OptionalRequires), diffs);
    CompareServiceSet("provides", ExpectSet(providesRefs), BinarySet(meta.Provides), diffs);
    CompareConfig(configFields, meta, diffs);
    std::vector<std::string> declaredStates;
    declaredStates.reserve(meta.ProcessStates.Size());
    for (std::size_t index = 0; index < meta.ProcessStates.Size(); ++index)
    {
        const ProcessStateDesc& state = *std::next(meta.ProcessStates.Begin(), static_cast<std::ptrdiff_t>(index));
        declaredStates.emplace_back(state.Name);
    }
    CompareStringSet("processStates", processStates, std::move(declaredStates), diffs);
    if (diffs.empty())
    {
        return Result<void>::Ok();
    }
    return Result<void>::Err(Error{"manifest/binary mismatch for \"" + id + "\": " + diffs.front()});
}
```

**同时改 `CompareConfig` 的签名**（绊线逼出来的机械改动，不改语义）：

```cpp
void CompareConfig(const std::vector<ExpectedConfigField>& fields, const PluginMeta& meta, DiffList& diffs)
{
    for (const ExpectedConfigField& field : fields)
    {
        // …函数体其余部分逐字不动…
    }
}
```

即：第一个形参由 `const ManifestExpectation& expected` 改为 `const std::vector<ExpectedConfigField>& fields`，函数体里 `expected.Config` 改为 `fields`。`CompareDescriptor` 的调用点相应传 `configFields`（结构化绑定出来的那个名字）。

- [ ] **Step 3: Catalog 侧填字段**

`Source/Catalog/PluginCatalog.cpp` 的 `BuildExpectation`，`Config` 循环之后、`return` 之前：

```cpp
    expected.ProcessStates = entry.ProcessStates; // M3/D93：清单名字数组逐字进期望（D84 深拷）
```

- [ ] **Step 4: 写用例（先红）**

`Tests/Integration/LoadTimeComparisonTests.cpp` 末尾加两条**单向臂**（正向臂在 T4 补，它需要带进程级状态的 fixture）：

```cpp
TEST(LoadTimeComparison, ProcessStatesExtraInManifestRefused)
{
    // 期望列了名字、描述符没有（Hello 未声明进程级状态）→ 拒，点名 processStates[<name>]。
    vase::PluginHost host;
    ManifestExpectation expected = MakeHelloExpectation();
    expected.ProcessStates.push_back("Vase.Ghost.Registry");
    ExpectLoadRefused(host, SingleEntryPlan("Vase.Hello", VASE_FIXTURE_HELLO, std::move(expected)), "Vase.Hello",
                      {"manifest/binary mismatch", "processStates[Vase.Ghost.Registry] missing in binary"});
}

TEST(LoadTimeComparison, ProcessStatesEmptyOnBothSidesLoads)
{
    // 两侧皆空 = 相等（缺键与空表等价，D93）；这条同时是「新字段不打扰既有插件」的证人。
    vase::PluginHost host;
    ExpectLoaded(host, SingleEntryPlan("Vase.Hello", VASE_FIXTURE_HELLO, MakeHelloExpectation()));
}
```

Run: `cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R LoadTimeComparison --output-on-failure`
Expected: 两条 PASS。`ProcessStatesExtraInManifestRefused` 若报的是 `missing in manifest` 而不是 `missing in binary`，说明两侧参数接反了。

- [ ] **Step 5: 绊线自检（无运行时证人，用一次性探针）**

在 `ManifestExpectation` 里临时加一个 `std::string Probe = {};` 成员，构建：

Run: `cmake --build --preset win-x64-clang-debug`
Expected: **编译失败**，报在 `CompareDescriptor` 的结构化绑定那一行（标识符个数不匹配）。确认后**删掉**该临时成员、重新构建转绿。
把这条探针结论写进提交信息（无运行时证人可写，是明说的那一步）。

- [ ] **Step 6: 规矩 6 双平台 + 格式门 + 提交**

```bash
ctest --preset win-x64-clang-debug -R 'HotSwap|Eject|Adopt' --output-on-failure
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug -R "HotSwap|Eject|Adopt" --output-on-failure'
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
git add Include/Vase/Host/ManifestExpectation.h Source/Host/Detail/ManifestCompare.cpp Source/Catalog/PluginCatalog.cpp Tests/Integration/LoadTimeComparisonTests.cpp
git commit -m "M3-T3：期望形与比对接入 processStates，布线结构化绑定全字段绊线"
```

---

### Task 4: 肉眼证人 fixture 与清单校准（正向臂）

**Files:**
- Modify: `Tests/Integration/fixtures/EdgeConsumerPlugin/EdgeConsumerPlugin.cpp`
- Modify: `Tests/Integration/fixtures/manifests/edge_consumer/plugin.json`
- Modify: `Tests/TestingSupport/AdoptExpectations.h`（`MakeEdgeConsumerExpectation`）
- Test: `Tests/Integration/LoadTimeComparisonTests.cpp`、`Tests/Integration/AssemblyFromSolveTests.cpp`

**Interfaces:**
- Consumes: T1 的槽、T2 的解析、T3 的比对
- Produces: 一只**声明进程级状态且其清单列名**的 fixture——正向证人；`EdgeConsumerPlugin` 的 Id 仍是 `Vase.EdgeConsumer`，新增 `.ProcessStates`

**为什么是它**：全仓（含 `Tools/VaseConsole` 的 `file(GENERATE)` 生成清单）只有 `EdgeConsumerPlugin` 与 `SharedConsumer2Plugin` 是「恰好被 1 份清单引用」。`SharedProviderPlugin` / `HelloPlugin` / `CollisionProviderPlugin` 都有生成清单二次引用，改它们会把 Console 回放一起拖下水。

- [ ] **Step 1: 声明进程级状态**

`Tests/Integration/fixtures/EdgeConsumerPlugin/EdgeConsumerPlugin.cpp`：匿名命名空间里加一个幂等 Reset，并在 `VASE_PLUGIN` 块补槽。

```cpp
// M3/D93 的正面材料：本条只为让「清单列名 ↔ 描述符声明」两侧都有东西可比。
// 循环次数是进程级状态的一个真实形态（它跨 Pod 存活），Reset 归零（§9.1 幂等契约）。
int gEdgeLoads = 0;

void ResetEdgeLoads() { gEdgeLoads = 0; }
```

```cpp
    .Provides = {},
    .ProcessStates = {{.Name = "Vase.Test.EdgeConsumer.Loads", .Reset = &ResetEdgeLoads}},
```

`OnLoad` 里 `++gEdgeLoads;`（放 `OnLoad` 开头，`static_cast<void>` 掉值）。**注意**：本 fixture 的 `VASE_PLUGIN` 块要**先读现有内容再改**，别覆盖既有 `.Requires`。

- [ ] **Step 2: 清单列名**

`Tests/Integration/fixtures/manifests/edge_consumer/plugin.json`：在 `requires` 数组之后、对象结束之前加

```json
    "processStates": [ "Vase.Test.EdgeConsumer.Loads" ]
```

（注意上一行末尾补逗号。）

- [ ] **Step 3: 校准期望工厂**

`Tests/TestingSupport/AdoptExpectations.h` 的 `MakeEdgeConsumerExpectation`（:59 起）加一行——**头注释已写明「<Xxx>.cpp 逐字抄」，插件描述符改了必来这里对一眼**：

```cpp
    expected.ProcessStates = {"Vase.Test.EdgeConsumer.Loads"};
```

- [ ] **Step 4: 正向臂用例**

`Tests/Integration/LoadTimeComparisonTests.cpp` 末尾：

```cpp
TEST(LoadTimeComparison, ProcessStatesMatchLoads)
{
    // 正向臂：期望与描述符的 Name 集相等 → 放行。材料是 EdgeConsumerPlugin（T4 起带进程级状态）。
    vase::PluginHost host;
    ExpectLoaded(host, SingleEntryPlan("Vase.EdgeConsumer", VASE_FIXTURE_EDGECONSUMER,
                                       testing_support::MakeEdgeConsumerExpectation()));
}

TEST(LoadTimeComparison, ProcessStatesMissingInExpectationRefused)
{
    // 反向半句：描述符有、期望没有 → 走 binary-only 臂，钉 "missing in manifest"。
    vase::PluginHost host;
    ManifestExpectation expected = testing_support::MakeEdgeConsumerExpectation();
    expected.ProcessStates.clear();
    ExpectLoadRefused(host, SingleEntryPlan("Vase.EdgeConsumer", VASE_FIXTURE_EDGECONSUMER, std::move(expected)),
                      "Vase.EdgeConsumer",
                      {"manifest/binary mismatch", "processStates[Vase.Test.EdgeConsumer.Loads] missing in manifest"});
}
```

Run: `cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R 'LoadTimeComparison|AssemblyFromSolve|LedgerSemantics|MultiPluginAssembly|Adopt' --output-on-failure`
Expected: 全绿。`AssemblyFromSolveTests` 走**真清单**（`edge_consumer/plugin.json` 被 stage 进沙箱）→ 它同时是「解析 → 期望 → 比对」整链的端到端证人。`LedgerSemanticsTests` 走 raw 计划（无期望）→ 不受影响。

- [ ] **Step 5: 规矩 6 双平台 + 格式门 + 提交**

```bash
ctest --preset win-x64-clang-debug -R 'HotSwap|Eject|Adopt' --output-on-failure
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug -R "HotSwap|Eject|Adopt" --output-on-failure'
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
git add Tests/Integration/fixtures/EdgeConsumerPlugin Tests/Integration/fixtures/manifests/edge_consumer Tests/TestingSupport/AdoptExpectations.h Tests/Integration/LoadTimeComparisonTests.cpp
git commit -m "M3-T4：EdgeConsumer 声明进程级状态并校准清单，补比对正向臂"
```

---

### Task 5: Host 侧登记与 Eject 三处 Reset

**Files:**
- Modify: `Include/Vase/Host/Loader.h`（`BinaryRecord`）
- Modify: `Source/Host/PluginHost.cpp`（`MakeInstance`、`EjectPlugin`）
- Create: `Tests/HotSwap/fixtures/StatefulPlugin/StatefulPlugin.cpp`
- Modify: `Tests/HotSwap/fixtures/BCommon.h`（`IStateProbe`）
- Modify: `Tests/HotSwap/fixtures/CMakeLists.txt`、`Tests/CMakeLists.txt`
- Test: `Tests/HotSwap/EjectTests.cpp`

**Interfaces:**
- Consumes: T1 的 `ProcessStateDesc` 与 `PluginMeta::ProcessStates`
- Produces: `detail::BinaryRecord::ProcessStates` / `ProcessStateCount`；`EjectReport::ProcessStatesReset` 由 Eject 填；fixture target `StatefulPlugin`（Id `Vase.Stateful`，服务 `Vase.Test.StateProbe`，宏 `VASE_FIXTURE_STATEFUL`）

- [ ] **Step 1: `BinaryRecord` 记表**

`Include/Vase/Host/Loader.h`：在 `namespace vase` 里前置声明（`namespace vase::detail` 之前）

```cpp
struct ProcessStateDesc; // M3/D92：BinaryRecord 只存指针，不需要完整类型
```

`struct BinaryRecord` 加两个字段：

```cpp
struct BinaryRecord
{
    std::filesystem::path Path; // 绝对化后的路径（表内去重键）
    void* Raw = nullptr;        // 平台句柄（HMODULE / dlopen 返回值）
    // M3/D92：描述符里的进程级状态表（指向镜像只读数据段，**不拥有**）。记在**记录**上而不是
    // 实例上：进程级状态跟着二进制走（§9.1），而记录的生命周期正是镜像驻留期；空壳与 Failed
    // 两条 Eject 分支没有实例、只有记录，Reset 要靠它。
    const ProcessStateDesc* ProcessStates = nullptr;
    std::size_t ProcessStateCount = 0;
};
```

（`<cstddef>` 若未含则补。）

- [ ] **Step 2: 装配点登记**

`Source/Host/PluginHost.cpp` 的 `MakeInstance`，紧跟 `live->Binary = &record;` 之后：

```cpp
    // M3/D92：进程级状态表随装配点登记一次——CreatePod 与 Adopt 两条路径共用 MakeInstance，
    // 故单点覆盖；config 失败早退也在它之后，失败插件的进程级状态照样登记（D99 要 Eject 时重置它）。
    record.ProcessStates = desc->Meta->ProcessStates.Begin();
    record.ProcessStateCount = desc->Meta->ProcessStates.Size();
```

- [ ] **Step 3: Eject 的 Reset**

`Source/Host/PluginHost.cpp` 的 `EjectPlugin`：在 `report.CrossPodInstancesZeroed = crossPodInstances == 0;`（:875）之后、`if (crossPodInstances > 0 || ...)`（:877）**之前**插入。这一个插入点同时覆盖三条分支——真卸货那条 `crossPodInstances == 0` 恒真，kept-resident 那条才是 D91 条件的实际约束面：

```cpp
    // M3/D91：Reset 的条件与「能不能卸货」同源不同闸——只要这份 binary 在任何局都**没有活实例**
    // 就归零（哪怕镜像因别局只剩 Failed 记录而留在架上：无人用）。有活实例时**不**重置：那是别局
    // 正在用的共享进程级状态，重置它就会造出 §9.1 那条硬契约要防的事故。
    // 幂等契约（§9.1）的用途正是不区分「用没用过」都能安全重置，故三条分支（活实例/空壳/Failed）
    // 一律跑——OnStart 失败那类跑过 OnLoad，进程级状态可能是半初始化态。
    if (binary != nullptr && crossPodInstances == 0)
    {
        for (std::size_t index = 0; index < binary->ProcessStateCount; ++index)
        {
            const ProcessStateDesc& state = *std::next(binary->ProcessStates, static_cast<std::ptrdiff_t>(index));
            state.Reset();
            report.ProcessStatesReset.emplace_back(state.Name); // 报告是返回值，必须拥有（借用教训同格）
        }
    }
```

并把 `EjectReport::ProcessStatesReset` 的既有注释（`Include/Vase/Host/Evidence.h` 的「**M1 恒空**……M3 填实现」）改成如实描述：

```cpp
    // §9.1 v3 / M3-D91：Eject 自动重置登记的进程级状态，逐条点名。**与 BinaryActuallyUnloaded
    // 可以并存为「非空 + false」**（kept-resident 分支 + 别局只留 Failed 记录）——读侧勿当互斥。
    std::vector<std::string> ProcessStatesReset;
```

- [ ] **Step 4: fixture 与标识**

`Tests/HotSwap/fixtures/BCommon.h` 末尾（`TickEvent` 之后、`} // namespace` 之前）加：

```cpp
// M3/D91 的观测量：进程级状态是**跨 Pod 存活**的静态计数，`Loads()` 报它。
// 证人形 = 「Eject 后再 Adopt，读数回到 1」——没归零的话它会继续累加到 2。
class IStateProbe
{
public:
    static constexpr std::string_view kName = "Vase.Test.StateProbe";
    static constexpr std::uint32_t kVersion = 1;

    IStateProbe() = default;
    IStateProbe(const IStateProbe&) = delete;
    IStateProbe& operator=(const IStateProbe&) = delete;
    IStateProbe(IStateProbe&&) = delete;
    IStateProbe& operator=(IStateProbe&&) = delete;
    virtual ~IStateProbe() = default;

    [[nodiscard]] virtual int Loads() const = 0;
};
```

`Tests/HotSwap/fixtures/StatefulPlugin/StatefulPlugin.cpp`（新文件，全文）：

```cpp
// 进程级状态探针（M3/D91）：OnLoad 递增一个**跨 Pod 存活**的静态计数，Reset 归零。
// 它是 3d 的行为证人——「Eject 后再 Adopt 读数回到 1」只有在 Reset 真被调用时才成立。
#include "Vase/Plugin.h"

#include "../BCommon.h"

namespace
{

// 进程级状态：生命周期跟着二进制走（§9.1），故是文件作用域静态量。
int gLoads = 0;

void ResetLoads() { gLoads = 0; } // 幂等（§9.1 契约）

class StateProbeImpl final : public samples_fixture::IStateProbe
{
public:
    [[nodiscard]] int Loads() const override { return gLoads; }
};

class StatefulPluginImpl final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ++gLoads; // 每次装载一次
        ctx.Provide<samples_fixture::IStateProbe>(Probe);
        return vase::Result<void>::Ok();
    }

private:
    StateProbeImpl Probe;
};

} // namespace

VASE_PLUGIN(StatefulPluginImpl){
    .Id = "Vase.Stateful",
    .DisplayName = "进程级状态探针",
    .Version = "1.0.0",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.StateProbe", .Version = 1}},
    .ProcessStates = {{.Name = "Vase.Test.StateProbe.Loads", .Reset = &ResetLoads}},
};
```

`Tests/HotSwap/fixtures/CMakeLists.txt` 末尾加：

```cmake
# 进程级状态探针（M3/D91）：3d 的行为证人。
vase_add_plugin_fixture(StatefulPlugin
    SOURCES StatefulPlugin/StatefulPlugin.cpp
    LINK_LIBRARIES VasePod)
```

`Tests/CMakeLists.txt` 的宏段（`:94` 的 `VASE_FIXTURE_TIMER` 之后）加一行：

```cmake
    VASE_FIXTURE_STATEFUL="$<PATH:CMAKE_PATH,$<TARGET_FILE:StatefulPlugin>>"
```

- [ ] **Step 5: 三条用例（先红）**

`Tests/HotSwap/EjectTests.cpp` 顶部 include 段补 `#include "fixtures/BCommon.h"`，末尾加：

```cpp
// 3d 的行为证人（M3/D91/D99）：报告的 ProcessStatesReset 是**声明**，Loads() 是**事实**——
// 只有 Reset 真被调用，「Eject 后再 Adopt 读数回到 1」才成立。
TEST(Eject, ProcessStatesResetMakesReloadLikeFirstTime)
{
    // 期望用 AdoptExpectations 的手写形（raw 计划走 Adopt 需自带期望，T12 单轨）。
    const vase::ManifestExpectation stateful = testing_support::MakeStatefulExpectation();
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(Plan({{"Vase.Stateful", VASE_FIXTURE_STATEFUL}})).Value();
    vase::Pod* pod = host.Resolve(h);
    ASSERT_NE(pod, nullptr);
    EXPECT_EQ(pod->Root().Get<samples_fixture::IStateProbe>().Loads(), 1); // 首次装载

    const vase::Result<vase::EjectReport> ejected = host.EjectPlugin(h, "Vase.Stateful");
    ASSERT_TRUE(ejected.IsOk());
    EXPECT_EQ(ejected.Value().Status, vase::EjectStatus::kEjected);
    ASSERT_EQ(ejected.Value().ProcessStatesReset.size(), 1U);
    EXPECT_EQ(*ejected.Value().ProcessStatesReset.begin(), "Vase.Test.StateProbe.Loads");

    vase::AdoptRequest request;
    request.Id = stateful.Id;
    request.BinaryPath = VASE_FIXTURE_STATEFUL;
    request.Expected = &stateful;
    const vase::Result<vase::AdoptReport> adopted = host.AdoptPlugin(h, request);
    ASSERT_TRUE(adopted.IsOk()) << adopted.GetError().Message();
    // 承重的一条：仍是 1 而不是 2 —— Reset 真跑过，装载「如同首次」（§9.1）。
    EXPECT_EQ(host.Resolve(h)->Root().Get<samples_fixture::IStateProbe>().Loads(), 1);
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}

TEST(Eject, ProcessStatesKeptWhenOtherPodHoldsLiveInstance)
{
    // D91 的反半句：别局有**活实例** → 不重置（那是别局正在用的共享状态）。
    const vase::ManifestExpectation stateful = testing_support::MakeStatefulExpectation();
    vase::PluginHost host;
    const vase::PodHandle first = host.CreatePod(Plan({{"Vase.Stateful", VASE_FIXTURE_STATEFUL}})).Value();
    const vase::PodHandle second = host.CreatePod(Plan({{"Vase.Stateful", VASE_FIXTURE_STATEFUL}})).Value();
    EXPECT_EQ(host.Resolve(second)->Root().Get<samples_fixture::IStateProbe>().Loads(), 2); // 第二局再装一次

    const vase::Result<vase::EjectReport> ejected = host.EjectPlugin(first, "Vase.Stateful");
    ASSERT_TRUE(ejected.IsOk());
    EXPECT_TRUE(ejected.Value().ProcessStatesReset.empty());   // 没重置
    EXPECT_FALSE(ejected.Value().BinaryActuallyUnloaded);      // 镜像留在架上（别局持有）
    // 别局读数不动：2 → 2（被重置的话这里会是 0）。
    EXPECT_EQ(host.Resolve(second)->Root().Get<samples_fixture::IStateProbe>().Loads(), 2);

    // 最后一局 Eject → 活实例归零 → 重置。
    const vase::Result<vase::EjectReport> last = host.EjectPlugin(second, "Vase.Stateful");
    ASSERT_TRUE(last.IsOk());
    ASSERT_EQ(last.Value().ProcessStatesReset.size(), 1U);
    EXPECT_TRUE(host.DestroyPod(first).Clean());
    EXPECT_TRUE(host.DestroyPod(second).Clean());
}
```

`Tests/TestingSupport/AdoptExpectations.h` 加工厂（照 `MakeVersionedAExpectation` 的体例）：

```cpp
// StatefulPlugin.cpp 逐字抄。
inline vase::ManifestExpectation MakeStatefulExpectation()
{
    vase::ManifestExpectation expected;
    expected.Id = "Vase.Stateful";
    expected.DisplayName = "进程级状态探针";
    expected.Version = "1.0.0";
    expected.Provides = {vase::ExpectedService{.Name = "Vase.Test.StateProbe", .Version = 1}};
    expected.ProcessStates = {"Vase.Test.StateProbe.Loads"};
    return expected;
}
```

Run: `cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R 'Eject.ProcessStates' --output-on-failure`
Expected: 两条 PASS。若第二条在 `Loads() == 2` 处红，说明 kept-resident 分支被误判（查 `crossPodInstances` 是否把**活实例**数对了）。

- [ ] **Step 6: 覆盖边界（如实记进提交信息）**

空壳与 Failed 两条分支的 Reset **只**由报告字段证（两条分支的实例已亡，插件的代码在卸货后不可读，没有值证人可写）。调用本身由 `ProcessStatesResetMakesReloadLikeFirstTime` 的**同一条循环**覆盖——三条分支共用这一个 `for`。不要为此另造机器。

- [ ] **Step 7: 规矩 6 双平台 + 格式门 + 提交**

```bash
ctest --preset win-x64-clang-debug -R 'HotSwap|Eject|Adopt' --output-on-failure
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug -R "HotSwap|Eject|Adopt" --output-on-failure'
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
git add Include/Vase/Host/Loader.h Include/Vase/Host/Evidence.h Source/Host/PluginHost.cpp \
        Tests/HotSwap/fixtures/StatefulPlugin Tests/HotSwap/fixtures/BCommon.h \
        Tests/HotSwap/fixtures/CMakeLists.txt Tests/CMakeLists.txt \
        Tests/HotSwap/EjectTests.cpp Tests/TestingSupport/AdoptExpectations.h
git commit -m "M3-T5：进程级状态随装配点登记，Eject 按无活实例口径重置并写入报告

空壳与 Failed 两条分支的 Reset 只有报告字段可证（实例已亡、代码随镜像卸下）——
调用由活实例那条用例的同一条循环覆盖，不另造机器。"
```

---

### Task 6: `NeighborC` 与 50 轮全量形

**Files:**
- Modify: `Tests/HotSwap/fixtures/BCommon.h`（`IPulse`）
- Create: `Tests/HotSwap/fixtures/NeighborC/NeighborC.cpp`
- Modify: `Tests/HotSwap/fixtures/CMakeLists.txt`、`Tests/CMakeLists.txt`
- Modify: `Tests/HotSwap/HotSwapLoopTests.cpp`
- Test: 同上（本任务改的就是测试）

**Interfaces:**
- Consumes: T5 的 fixture 接线体例
- Produces: fixture target `NeighborC`（Id `Vase.NeighborC`，服务 `Vase.Test.Pulse`，宏 `VASE_FIXTURE_NEIGHBORC`）；用例 `HotSwap.FiftyRoundsBehaveLikeFirstTime`（取代 `FiveRoundsBehaveLikeFirstTime`）

**为什么另开一只服务标识**：`ServiceRegistry::Add` 对重复键在两个构建里都 `ProgrammerError`（「一服务一实现」），一个 Pod 里两个 `IHeart` 提供方当场终止——`NeighborC` **不能**复用 `IHeart`。事件可以共用 `TickEvent`（订阅无唯一性规则）。

- [ ] **Step 1: 标识与 fixture**

`Tests/HotSwap/fixtures/BCommon.h` 末尾加（与 `IStateProbe` 同格）：

```cpp
// 第二只邻居的服务标识（M3/D90）：与 IHeart **必须不同名**——一个 Pod 里两个同键提供方
// 会被 ServiceRegistry 当场终止（「一服务一实现」）。心跳的驱动事件仍是 TickEvent（订阅无唯一性）。
class IPulse
{
public:
    static constexpr std::string_view kName = "Vase.Test.Pulse";
    static constexpr std::uint32_t kVersion = 1;

    IPulse() = default;
    IPulse(const IPulse&) = delete;
    IPulse& operator=(const IPulse&) = delete;
    IPulse(IPulse&&) = delete;
    IPulse& operator=(IPulse&&) = delete;
    virtual ~IPulse() = default;

    [[nodiscard]] virtual int Pulses() const = 0;
};
```

`Tests/HotSwap/fixtures/NeighborC/NeighborC.cpp`（新文件，全文——照 `NeighborB.cpp` 的形）：

```cpp
// 第二只无依赖邻居探针（M3/D90 §12.1 的「其余实例」复数形）：提供脉冲服务并订阅 TickEvent。
// A 的进出与它无关——50 轮循环里 B 与 C 的计数都必须纹丝不动。
#include "Vase/Plugin.h"

#include "../BCommon.h"

namespace
{

class PulseImpl final : public samples_fixture::IPulse
{
public:
    [[nodiscard]] int Pulses() const override { return PulseCount; }
    void Pulse() { ++PulseCount; }

private:
    int PulseCount = 0;
};

class NeighborCPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.Provide<samples_fixture::IPulse>(Pulse);
        ctx.On<samples_fixture::TickEvent>(&NeighborCPlugin::OnTick, this);
        return vase::Result<void>::Ok();
    }

private:
    void OnTick(const samples_fixture::TickEvent& event)
    {
        static_cast<void>(event); // 只数次数
        Pulse.Pulse();
    }

    PulseImpl Pulse;
};

} // namespace

VASE_PLUGIN(NeighborCPlugin){
    .Id = "Vase.NeighborC",
    .DisplayName = "第二只无依赖邻居探针",
    .Version = "1.0.0",
    .Requires = {},
    .Provides = {{.Name = "Vase.Test.Pulse", .Version = 1}},
};
```

`Tests/HotSwap/fixtures/CMakeLists.txt` 的 `NeighborB` 段之后加：

```cmake
vase_add_plugin_fixture(NeighborC
    SOURCES NeighborC/NeighborC.cpp
    LINK_LIBRARIES VasePod)
```

`Tests/CMakeLists.txt` 宏段加：

```cmake
    VASE_FIXTURE_NEIGHBORC="$<PATH:CMAKE_PATH,$<TARGET_FILE:NeighborC>>"
```

- [ ] **Step 2: 三插件局**

`Tests/HotSwap/HotSwapLoopTests.cpp` 的 `LoopPlan` 加第三只并改注释：

```cpp
// A 与 B、C 同一局；A 的路径是工作副本（会被覆盖），B/C 是构建产物本体（只读地加载）。
vase::LoadPlan LoopPlan(const std::filesystem::path& aPath)
{
    vase::LoadPlan plan;
    plan.Ordered.push_back({.Id = "Vase.VersionedA", .BinaryPath = aPath});
    plan.Ordered.push_back({.Id = "Vase.NeighborB", .BinaryPath = VASE_FIXTURE_NEIGHBORB});
    plan.Ordered.push_back({.Id = "Vase.NeighborC", .BinaryPath = VASE_FIXTURE_NEIGHBORC});
    return plan;
}
```

`LoopAdoptRequest` 的兄弟集要带上 C（导入表执法与清单比对的兄弟集由此完整）：

```cpp
    request.SiblingBinaries = {std::filesystem::path{VASE_FIXTURE_NEIGHBORB}.filename().string(),
                               std::filesystem::path{VASE_FIXTURE_NEIGHBORC}.filename().string()};
```

- [ ] **Step 3: 50 轮 + 每轮派拍（先红）**

替换 `TEST(HotSwap, FiveRoundsBehaveLikeFirstTime)` 整个用例：

```cpp
TEST(HotSwap, FiftyRoundsBehaveLikeFirstTime)
{
    // §12.1 判据 3a 的全量形（M3/D90/D98）：三插件局、对叶 A 做 50 轮 Eject+Adopt，
    // 断言**其余实例从第一步起全程未受扰**。每轮派 k 拍是承重的——不派拍的话
    // 「Beats 恰为累计拍数」两端都是 0、断言恒真（TimerNoReentryTests 注释点名的假绿世界）。
    constexpr int kTicksPerRound = 2;
    const SwapWorkspace ws;
    vase::PluginHost host;
    const vase::ManifestExpectation versionedA = testing_support::MakeVersionedAExpectation();
    const vase::PodHandle h = host.CreatePod(LoopPlan(ws.APath)).Value();
    bool prime = false;
    int ticks = 0;
    for (int round = 0; round < 50; ++round)
    {
        vase::Pod* const pod = host.Resolve(h);
        ASSERT_EQ(CounterValue(pod), prime ? 2 : 1) << "round " << round;
        for (int step = 0; step < kTicksPerRound; ++step)
        {
            pod->Root().Emit(samples_fixture::TickEvent{ticks});
            ++ticks;
        }
        // 两只邻居的**精确计数**：循环内每轮都断（失败能定位到轮次）。
        ASSERT_EQ(pod->Root().Get<samples_fixture::IHeart>().Beats(), ticks) << "round " << round;
        ASSERT_EQ(pod->Root().Get<samples_fixture::IPulse>().Pulses(), ticks) << "round " << round;

        ASSERT_TRUE(host.EjectPlugin(h, "Vase.VersionedA").IsOk());
        std::error_code ec;
        const std::filesystem::path src = prime ? VASE_FIXTURE_VERSIONEDA : VASE_FIXTURE_VERSIONEDAPRIME; // 装回另一版
        std::filesystem::copy_file(src, ws.APath, std::filesystem::copy_options::overwrite_existing, ec);
        ASSERT_FALSE(ec) << ec.message(); // Eject 之后写不动 = 锁没解，卸载路径坏了
        // 两版描述符逐字节相同（T5 评审）——一份期望跑完 50 轮换件。
        ASSERT_TRUE(host.AdoptPlugin(h, LoopAdoptRequest(versionedA, ws.APath)).IsOk());
        prime = !prime;
        ASSERT_EQ(CounterValue(host.Resolve(h)), prime ? 2 : 1) << "round " << round; // 换装后立刻对得上「如同首次」
    }
    EXPECT_EQ(ticks, 100); // 50 轮 × 2 拍——本行同时拦住「循环轮数被悄悄改小」
    EXPECT_TRUE(host.DestroyPod(h).Clean());
}
```

**不要**用实例地址做「未受扰」的判据：`FullLoopFlipsBehaviorAndNeverTouchesNeighbor` 的既有注释有实测结论（新对象会落回被释放的堆块，同一二进制时红时绿）。

Run: `cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R 'HotSwap.FiftyRounds' --output-on-failure`
Expected: PASS，且**记下单用例实测耗时**（`ctest` 的 `--output-on-failure` 输出里带 `Total Test time`；也可 `ctest -R HotSwap.FiftyRounds -V` 看秒数）。

- [ ] **Step 4: 全 LoopPlan 用例面复跑**

`LoopPlan` 加了一只，`FullLoopFlipsBehaviorAndNeverTouchesNeighbor` 与 `FiftyRounds...` 都要跑过（其它用例若也吃 `LoopPlan`，一并看红灯）。

Run: `ctest --preset win-x64-clang-debug -R 'HotSwap' --output-on-failure`
Expected: 全绿。

- [ ] **Step 5: 规矩 6 双平台 + 格式门 + 提交**

```bash
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug -R "HotSwap|Eject|Adopt" --output-on-failure'
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
git add Tests/HotSwap/fixtures/NeighborC Tests/HotSwap/fixtures/BCommon.h \
        Tests/HotSwap/fixtures/CMakeLists.txt Tests/CMakeLists.txt Tests/HotSwap/HotSwapLoopTests.cpp
# D103 要求把实测秒数落进记录——取它并直接进 message（别留占位符）：
SECONDS=$(ctest --preset win-x64-clang-debug -R 'HotSwap.FiftyRounds' -V | grep -oE 'Passed +[0-9.]+ sec' | head -1)
git commit -F - <<MSG
M3-T6：3a 全量形——三插件局 50 轮，每轮派拍断言两只邻居全程未受扰

实测单用例耗时 ${SECONDS}（Windows / clang-debug）——D103 要求把数字落进记录，
超预算再回来改判并记入 plan 偏离登记。
MSG
```

---

### Task 7: 空壳可拆（D94）

**Files:**
- Modify: `Source/Host/PluginHost.cpp`（`EjectPlugin` 的 ①' 分支）
- Modify: `Include/Vase/Host/PluginHost.h`（契约注释）
- Test: `Tests/Integration/RecursiveTeardownTests.cpp`

**Interfaces:**
- Consumes: T5 的 Reset 插入点（同函数，先做完 T5 再动这里）
- Produces: 空壳（`Instance == nullptr` 且不在 `FailedBinaries`）可被 Eject；`not in pod` 此后只剩「未知 Id」一支

- [ ] **Step 1: 改 ①' 分支**

`Source/Host/PluginHost.cpp` 的 `EjectPlugin`，把 `else` 段（:815-838）改为：

```cpp
    else
    {
        // ①' §5.6 四条补角的「Failed 可被 Eject」+ M3/D94 的「空壳亦可」：两者同是
        // **无实例的账目残留**——Failed 是加载/启动失败的记录，空壳是级联拆除留下的残条目
        // （Instance == nullptr 且不在 FailedBinaries）。今日两者待遇不同，而空壳会挡 Adopt 的 Id，
        // 使「本局重试失败插件」只对 Failed 开放；D94 把它们拉平。
        // 两类的残条目都要摘：Eject 的语义是「本局不再记得这个插件」，留一条空壳会让后续 Adopt
        // 的 already-in-pod 判在死条目上。摘除不影响 TeardownInstancesAndRoot——它本来就 continue
        // 掉 Instance == nullptr 的条目。
        const auto failed = pod.FailedBinaries.find(id);
        const bool shell = std::ranges::any_of(pod.Instances,
                                               [pluginId](const std::unique_ptr<Pod::LiveInstance>& entry)
                                               { return entry->Instance == nullptr && entry->OwnerLabel == pluginId; });
        if (failed != pod.FailedBinaries.end())
        {
            binary = failed->second;
            pod.FailedBinaries.erase(failed);
            report.HotSwapNote = "failed-record ejected";
        }
        else if (shell)
        {
            // 空壳的镜像由**残条目**的 Binary 指出（与 Failed 表分开的两条持有路径）。
            for (const std::unique_ptr<Pod::LiveInstance>& entry : pod.Instances)
            {
                if (entry->Instance == nullptr && entry->OwnerLabel == pluginId)
                {
                    binary = entry->Binary;
                    break;
                }
            }
            report.HotSwapNote = "torn shell ejected";
        }
        else
        {
            return Result<EjectReport>::Err(Refusal(id, Phase::kEject, "plugin not in pod: " + id));
        }
        std::erase_if(pod.Instances, [pluginId](const std::unique_ptr<Pod::LiveInstance>& entry)
                      { return entry->OwnerLabel == pluginId; });
        std::erase_if(pod.FailureRecords,
                      [pluginId](const FailedPluginRecord& record) { return record.Id == pluginId; });
        report.Status = EjectStatus::kEjected;  // 无实例必无边——RemovedEdges 天然为空
        report.LedgerHadNoIncomingEdges = true; // 无实例必无入边
        report.ScopeEmptied = true;             // 该插件此刻没有任何存活 Scope
    }
```

（`<algorithm>` 与 `<ranges>` 若未含则补——本文件既有 `std::erase_if`，两者应已在。）

- [ ] **Step 2: 契约注释同步**

`Include/Vase/Host/PluginHost.h` 里那条「Err 只剩误用：stale handle / not-in-pod」的契约注释，把 `not-in-pod` 的覆盖面改准：

```cpp
    // §5.6 判定流左列：执法拒绝（有消费者）= **Ok + Status=kRejectedConsumers + Consumers 逐条点名**
    // （D21，3b 兑现）；成功拆除 = Ok + Status=kEjected + RemovedEdges。Err 只剩误用：
    // stale handle / not-in-pod——只有后者的子串是契约（回放证人钉它，前者无匹配方）。
    // M3/D94 起 not-in-pod **只剩「本局无此 Id」一支**：Failed 记录与级联空壳都可被 Eject。
```

- [ ] **Step 3: 重写用例（先红）**

`Tests/Integration/RecursiveTeardownTests.cpp` 的 `TornShellRejectsEjectAndAdoptWithoutCrashing`（:92-112）整体替换：

```cpp
TEST(RecursiveTeardown, TornShellCanBeEjectedAndThenReAdopted)
{
    // D94（M3 改判）：级联空壳（无实例、无入边、镜像在架、不在 FailedBinaries）**可以**被 Eject——
    // 它与 Failed 记录同是「无实例的账目残留」，待遇从此一致。Eject 后本局不再记得它，
    // 故同 Id 可以再入局（此前被 already-in-pod 挡住）。
    vase::PluginHost host;
    const vase::PodHandle h = host.CreatePod(BehindPlan()).Value();

    const vase::Result<vase::EjectReport> ejected = host.EjectPlugin(h, "Vase.BehindStrictUnused");
    ASSERT_TRUE(ejected.IsOk()) << ejected.GetError().Message();
    EXPECT_EQ(ejected.Value().Status, vase::EjectStatus::kEjected);
    EXPECT_NE(ejected.Value().HotSwapNote.find("torn shell ejected"), std::string::npos);
    // 空壳无实例 ⇒ 必无入边、必无存活 Scope（三档之档一）。
    EXPECT_TRUE(ejected.Value().LedgerHadNoIncomingEdges);
    EXPECT_TRUE(ejected.Value().ScopeEmptied);
    EXPECT_TRUE(ejected.Value().RemovedEdges.empty());

    const vase::ManifestExpectation behind = testing_support::MakeBehindStrictUnusedExpectation();
    vase::AdoptRequest request;
    request.Id = behind.Id;
    request.BinaryPath = VASE_FIXTURE_BEHINDSTRICTUNUSED;
    request.Expected = &behind;
    const vase::Result<vase::AdoptReport> adopted = host.AdoptPlugin(h, request);
    // 空壳已摘，⓪ 的 already-in-pod 不再命中——这次卡在 ③：它声明的 Vase.Test.Behind
    // 由 StartFailProvider 提供，而那一局已经失败（本局没有活提供方）。
    ASSERT_TRUE(adopted.IsOk()) << adopted.GetError().Message();
    EXPECT_EQ(adopted.Value().Status, vase::AdoptStatus::kRejectedDependencies);
    ASSERT_EQ(adopted.Value().Missing.size(), 1U);
    EXPECT_EQ(adopted.Value().Missing.begin()->Service, "Vase.Test.Behind");

    EXPECT_TRUE(host.DestroyPod(h).Clean());
}
```

Run: `cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R RecursiveTeardown --output-on-failure`
Expected: 三条 RecursiveTeardown.* 全绿。若 Adopt 那条报 `Err(already in pod)`，说明空壳残条目没被摘掉。

- [ ] **Step 4: 未知 Id 那支仍在（回归）**

Run: `ctest --preset win-x64-clang-debug -R 'VaseConsoleRefused|Eject.Unknown' --output-on-failure`
Expected: 全绿——`VaseConsoleRefusedReason` 钉的 `plugin not in pod`（脚本 `eject Vase.Nowhere`）走的是「本局无此 Id」，D94 后原样有效。

- [ ] **Step 5: 规矩 6 双平台 + 格式门 + 提交**

```bash
ctest --preset win-x64-clang-debug -R 'HotSwap|Eject|Adopt' --output-on-failure
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && cmake --build --preset linux-x64-clang-debug && ctest --preset linux-x64-clang-debug -R "HotSwap|Eject|Adopt" --output-on-failure'
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
git add Source/Host/PluginHost.cpp Include/Vase/Host/PluginHost.h Tests/Integration/RecursiveTeardownTests.cpp
git commit -m "M3-T7：级联空壳可被 Eject，与 Failed 记录待遇拉平

连带 not in pod 只剩「本局无此 Id」一支；契约注释与证人同步改判。"
```

---

### Task 8: 前端打点与注释改准

**Files:**
- Modify: `Tools/VaseConsole/Console.cpp`
- Modify: `Samples/Embedding/main.cpp`
- Test: `Tools/VaseConsole/CMakeLists.txt` 的既有回放（不改断言，只复跑）

**Interfaces:**
- Consumes: T5 的 `EjectReport::ProcessStatesReset`
- Produces: 两份 `PrintEjectReport` 新打点（有内容才打）；两处「把报告的每个判据都打出来」的注释改成如实描述

- [ ] **Step 1: 两份打印器同步补打点**

`Tools/VaseConsole/Console.cpp` 的 `PrintEjectReport`（:153）与 `Samples/Embedding/main.cpp` 的 `PrintEjectReport`，在 `HotSwapNote` 那段**之前**插入（两处同形，只是输出通道不同）：

Console 版：

```cpp
    // M3/D100：进程级状态的每次重置都点名——只在这条非空时打，免得给不声明进程级状态的插件添噪声。
    if (!report.ProcessStatesReset.empty())
    {
        out << "  processStatesReset:";
        for (const std::string& state : report.ProcessStatesReset)
        {
            out << ' ' << state;
        }
        out << '\n';
    }
```

Samples 版（拼串后 `Out`）：

```cpp
    if (!report.ProcessStatesReset.empty())
    {
        std::string reset{"  processStatesReset:"};
        for (const std::string& state : report.ProcessStatesReset)
        {
            reset += ' ';
            reset += state;
        }
        reset += '\n';
        Out(reset);
    }
```

- [ ] **Step 2: 两处注释改准（顺手收债）**

两处注释都自称「把报告的每个判据都打出来」，而 `EjectReport` 有 14 个字段、本函数只打其中 6 个。改成如实描述：

```cpp
// §9.2「每次进出必须有账可查」的演示形态：打本平台有判据力的**档二**字段（两个 Is… 是
// 「哪个字段有判据力」的**声明**，§8.2 把这件事写进了结构——不打出来，Windows 上那个
// mappingRemoved=false 就成了没解释的噪声，本该被读作失败），外加进程级状态重置与注记。
// 档一的三个字段（LedgerHadNoIncomingEdges / ScopeEmptied / CrossPodInstancesZeroed）与
// Status / RemovedEdges **不在本函数**——Status 由调用方按枚举分支处理，其余三个的打印缺口另立一笔。
```

- [ ] **Step 3: 回放不破（回归）**

`eject` 那条新行只在 `ProcessStatesReset` 非空时输出，而回放链上的 fixture（`HelloPlugin` 等）都不声明进程级状态 → 永不输出。

Run: `cmake --build --preset win-x64-clang-debug && ctest --preset win-x64-clang-debug -R 'VaseConsole|Embedding' --output-on-failure`
Expected: 全绿（Console 回放 33 条 + Embedding 回放）。任一红都说明新行被无条件打了出来。

- [ ] **Step 4: 格式门 + 提交**

```bash
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
git add Tools/VaseConsole/Console.cpp Samples/Embedding/main.cpp
git commit -m "M3-T8：两份 PrintEjectReport 补进程级状态打点，注释改准

注释原自称「把报告的每个判据都打出来」，实际 14 个字段只打 6 个——顺手改成如实描述；
三档一字段与 RemovedEdges 的打印缺口另立一笔。"
```

---

### Task 9: 文书义务

**Files:**
- Modify: `wiki/vase-architecture.md`
- Modify: `CLAUDE.md`
- Modify: 五处代码注释与技能一句

**Interfaces:**
- Consumes: T1–T8 的全部事实
- Produces: 文档与磁盘一致

- [ ] **Step 1: wiki 四处**

1. §12.1 判据 **3a / 3d** 的「**未落**（M3）：…」改成「**已落**：<证人用例名>」。
2. §12.3 的 M3 行改回「3a + 3d」并加注：完整属主追踪器经核查否决（D97），理由见 spec §4。
3. §9.3 加勘误：属主追踪器已核查、结论不做；三条契约仍不可自动化验证，且插件侧「绕道注册」无可达形态。
4. §13.3 加两条：(a) `.ProcessStates` 已按 D89 同构收口，不再是静默点；(b) 比对器漏读新字段无机制可拦（结构化绑定守的是「加了字段没进绑定」，守不住「进了绑定但没接进比对」——两者的差别写在比对器注释里）。§9.1 的 `.Describe` 标「M3 裁定不做（D92）」。

- [ ] **Step 2: 代码注释五处**

`Include/Vase/Effect/EffectScope.h`、`Include/Vase/Pod/Pod.h`、`Include/Vase/Host/Evidence.h`、`Tests/TestingSupport/PodTestPeer.h`、`Tests/Lifecycle/DiagnosticAttributionTests.cpp` 里「属 M3 完整属主追踪器」的说法，一并改为：

```cpp
// §9.3 契约束，无机制可拦（M3/D97 已核查：插件侧「绕道注册」无可达形态——拿不到 Pod 的 ScopePool）。
```

技能 `.claude/skills/vase-cpp-engineering/references/architecture.md:127` 的同口径句同步。

- [ ] **Step 3: `CLAUDE.md`**

1. 「项目状态」节首行改 M3 完成，并把本波两笔写进去。
2. `kHeaderVersion` 的记述（§3.3 行与「插件接口 / ABI 约定」行）改 3→4。
3. **规矩 6 的子串契约清单**：`not in pod` 那一半改为「只剩本局无此 Id 一支；Failed 记录与级联空壳可被 Eject（D94）」。
4. 「尚未确定的事项」表的测试框架行：判据 3a/3d 标已落。

（六线基数表与 tidy 表的数字**不在本任务**——它们要等 T10 的收口实测，那一步是它们唯一的真值来源。）

- [ ] **Step 4: memory**

更新 `vase-v3-hotswap-status` 与 `MEMORY.md` 索引：M3 两笔已落、D97 的否决结论、下一步。

- [ ] **Step 5: 提交**

```bash
git add wiki/vase-architecture.md CLAUDE.md Include/Vase/Effect/EffectScope.h Include/Vase/Pod/Pod.h \
        Include/Vase/Host/Evidence.h Tests/TestingSupport/PodTestPeer.h \
        Tests/Lifecycle/DiagnosticAttributionTests.cpp .claude/skills/vase-cpp-engineering/references/architecture.md
git commit -m "M3-T9：文书义务——判据 3a/3d 标已落、属主追踪器关账、规矩 6 子串清单改判"
```

---

### Task 10: 收口全量验证与基数落账

**Files:**
- Modify: `CLAUDE.md`（基数与 tidy 数字）

- [ ] **Step 1: 六线全量（删树重配）**

```bash
Scripts/win-verify.cmd
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && bash Scripts/linux-verify.sh'
```

Expected: 六线全绿、构建零警告。**Windows 侧首次删树重配可能撞 `z-applocal` 文件锁假红**——退出码非 0 时先重跑那一条线再判（CLAUDE.md 有记）。

- [ ] **Step 2: 读六线基数并落表**

```bash
ctest --preset win-x64-clang-debug -N
ctest --preset win-x64-clang-release -N
ctest --preset win-x64-msvc-debug -N
ctest --preset win-x64-msvc-release -N
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && ctest --preset linux-x64-clang-debug -N'
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && ctest --preset linux-x64-clang-release -N'
```

把六个数写进 `CLAUDE.md` 的基数表，并核三条差值（debug−release 恰为 T3 那条 death test、linux−win 恰为 T11 两条、win−linux 反向为空）。**本波新增用例无一受 `#ifndef NDEBUG` 门或平台门**，故三个差值形状不变。

- [ ] **Step 3: tidy 三线**

```bash
Scripts/win-clang-tidy.cmd clangcl
Scripts/win-clang-tidy.cmd msvc
wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && bash Scripts/linux-clang-tidy.sh'
```

Expected: 三判据（退出码 0 + 正文 `error:` 0 + 正文 `warning:` 0）。把 TU 数与抑制合计写进 `CLAUDE.md` 的 tidy 表。**预期 TU 数 +2**（`StatefulPlugin` 与 `NeighborC` 各一条编译数据库条目），Windows 93→95、Linux 94→96。新增代码若带 NOLINT，逐处确保就地写了理由。

- [ ] **Step 4: 格式门**

```bash
git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror
```

Expected: 无输出、exit 0。

- [ ] **Step 5: 提交并折并**

```bash
git add CLAUDE.md
git commit -m "M3-T10：六线基数与 tidy 计数落账（收口复测）"
git log --oneline main..HEAD   # 核对本波全部提交
git rebase -i main             # 折成一笔（保留 message 一字不动），force-push 等需求方发话
```

---

## 偏离裁决记录（2026-09-26，需求方逐条裁可；已写回 spec 状态行注与相应 D 格）

1. **D101 的机制**——✅ 裁可**改判替换**：spec 原写「`ManifestExpectation` 去掉各成员的 NSDMI，让 `-Wmissing-designated-field-initializers` 在每个构造点强制写全」。取证实测：**`ManifestExpectation` 全仓零个指定初始化点**（每一处都是 `ManifestExpectation expected;` + 逐成员赋值），故该警告守不到顶层类型；去掉 NSDMI 只会打到嵌套的 `ExpectedConfigField`（逼 14 处站点补 `.Min = {}, .Max = {}, .Choices = {}` 三行噪声），还把 `Version = 0` 的初值一并去掉留下未初始化标量。**改为**在 `CompareDescriptor` 开头用结构化绑定（标识符个数必须等于非静态数据成员数）。**意图不变，守住的面更宽**：构造那一半本来就响亮（`BuildExpectation` 漏填新字段 ⇒ 期望恒空集 ⇒ 凡清单列了名字的插件当场被加载期拒），比对那一半现在有编译期闸。
2. **`not in pod` 的证人**——✅ 裁可改判：spec 初稿点名 `Eject.UnknownOrStaleRejected`，实测它**一个字都不匹配消息文本**。真实证人是 `RecursiveTeardownTests.cpp:100`（空壳，D94 必改）与 `Tools/VaseConsole/CMakeLists.txt:432`（未知 Id，不受影响）。
3. **3a 的「未受扰」判据**——✅ 裁可改判：不引入实例地址判据（`HotSwapLoopTests.cpp:133-134` 已有实测否证），改由**每轮派拍 + 循环内累计计数**承担。
4. **进程级状态的 fixture 分工**——✅ 裁可：清单闸的正面证人用 `EdgeConsumerPlugin`（全仓含生成清单范围内只有它与 `SharedConsumer2Plugin` 恰好被 1 份清单引用）；Reset 的行为证人另立 `StatefulPlugin`（它必须**提供**一个服务才能让 `Loads()` 可观测，而给 `EdgeConsumerPlugin` 加 `Provides` 会连带改清单与 6 处用例引用）。
5. **空壳与 Failed 两条分支的 Reset 覆盖**——✅ 裁可**如实划界**：两条分支只有报告字段可证，调用由活实例那条用例覆盖**同一条循环**；不为此另造机器（与「无证人可写的那一步要明说」同例）。

6. **终审回捕（2026-09-27，终审修复波追加，不改史）**——上条前提**事实有误**：「调用由活实例那条用例覆盖」不成立。单局证人全卸后 re-Adopt 落在全新镜像上、static 自行归零，删掉 `state.Reset()` 全仓仍绿（本波以本地变异实证：删调用后仅新用例红，原 `ProcessStatesResetMakesReloadLikeFirstTime` 保持绿）。**已补**spec §5 本就要求的并存证人 `Eject.ProcessStatesResetCoexistsWithKeptResidentImage`（Failed 臂：kept-resident 下 Loads==1，可证伪调用点）与 `Eject.ProcessStatesResetCoexistsWithKeptResidentShell`（空壳臂：SharedFailProvider 级联造壳）；并修 ③ 闸**计入空壳第三类持有者**（C1 Critical——spec §2.3「走同一套全局闸」自此兑现；Reset 闸维持只数活实例，与 §3.4 并存口径对上）。上条「不为此另造机器」的部分由本条裁决作废。
