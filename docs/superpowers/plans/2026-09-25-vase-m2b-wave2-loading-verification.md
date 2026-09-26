# Vase M2b 第二波（加载期执法 / enum / 前端改接）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 兑现 spec `docs/superpowers/specs/2026-09-25-vase-m2b-wave2-loading-verification-design.md`（决定 D67–D89）：加载期期望比对、`AdoptPlugin` 期望化与 `KnownBinaries` 退役、`enum`+choices+`kHeaderVersion` 2→3、Console 双入口与 Samples 两员、handoff 账四条。

**Architecture:** 期望形 `ManifestExpectation` 住 Host（**拥有值形、零借用窗**——需求方 2026-09-25 对偏离 #2 的改判裁决），Catalog 生产并编排（`BuildExpectation` 值返回 / `AdoptInto`），调用方搬运；比对是 CreatePod 与 Adopt 共用的唯一函数。enum 走 `ValueKind::kEnum` + `FieldInfo` 尾追加两槽（bump v3），JSON 面 label、内部 value、唯一转换 helper。前端双入口：catalog 主链默认执法、`pod new-raw` 保留裸计划旁路。

**Tech Stack:** C++20（无异常，`Result<T>` 全通道）、CMake + presets + vcpkg（gtest 1.18 / nlohmann-json）、GoogleTest + CTest、clang-tidy / clang-format（LLVM 23.1.0）、Win(cl/clang-cl) + Linux(clang/libc++)。

**Spec:** `docs/superpowers/specs/2026-09-25-vase-m2b-wave2-loading-verification-design.md`（本计划与它冲突处以磁盘+本文「偏离登记」为准，验收时回挂勘误）。

## Global Constraints

每个任务隐含包含本节全部：

- 新 target 必链 `VaseBuildOptions`；新插件 target 必走 `vase_add_plugin_fixture`（CLAUDE.md 规矩 1/5）。
- 全项目关异常：**零** `throw/try/catch`；测试里**禁用 `EXPECT_THROW` 一族**，失败断言只用 `Result` 返回值（规矩 2）。
- Vase 自己的头**引号**包含；`ThirdParty/cli` 同样引号（规矩 3）。
- 聚合体新字段一律带 NSDMI（`-Wmissing-designated-field-initializers` + WX 是 error）。
- 提交信息：中文，**不加任何 AI 署名尾注**（无 `Co-Authored-By:` 等）。
- 格式门（每个改了 `.h/.cpp` 的任务收尾）：`git add` 新文件后 `git ls-files -z --cached --others --exclude-standard '*.h' '*.hpp' '*.cpp' '*.cc' '*.ixx' | xargs -0 clang-format --dry-run --Werror`。Allman、`PointerAlignment: Left`、初始化表每式一行逗号行首；宏参数内花括号单行例外。
- 规矩 6（本波全触发）：**动到 Adopt/描述符/`HeaderVersion` 的任务（T2、T8、T9、T12）完成后立刻双平台跑** `ctest --preset win-x64-clang-debug -R 'HotSwap|Eject|Adopt'` **与** `wsl -d Ubuntu -- bash -lc 'cd /mnt/d/Git/Vase && ctest --preset linux-x64-clang-debug -R "HotSwap|Eject|Adopt"'`（注意 WSL 命令里 `$` 不进双引号的既有教训——上面这条没有 `$`，可直接用）。
- 日常构建用 `win-x64-clang-debug`；msvc 线要经 `Scripts/msvc-env.cmd`；六线全量只在 T14 收口做（删树重配）。
- 基数与文档真值只在 T13/T14 落 CLAUDE.md，**不要**提前抄进技能目录（规矩 7）。
- clang 的 `-Wmissing-designated-field-initializers`、`/Zc:preprocessor` 等承重要点见 CLAUDE.md「工具链 flag 是承重的」；两个工具链文件的 flag 本波**一个都不动**。

## File Structure（全波次地图）

```text
新建
  Include/Vase/Host/ManifestExpectation.h     期望形 POD（T7）
  Source/Host/Detail/ManifestCompare.h/.cpp   比对唯一实现 + 消息格式器（T7）
  Source/Catalog/Detail/ChoiceCoerce.h        label↔value 唯一转换（T4）
  Include/Vase/Catalog/CatalogAdopt.h         BuildExpectation 声明（T9，若 ManifestView.h 已够重就并入它——二选一，T9 里定）
  Tests/Integration/LoadTimeComparisonTests.cpp      判据 13（T7）
  Tests/HotSwap/AdoptManifestTests.cpp               就地重读三态（T9）
  Tests/TestingSupport/AdoptExpectations.h           迁移用的手抄期望字面量（T12）
  Samples/DependentPlugin/{CMakeLists.txt,DependentPlugin.cpp}   （T11）
  Samples/FailingPlugin/{CMakeLists.txt,FailingPlugin.cpp}       （T11）
  Tests/Integration/fixtures/manifests/{dependent,failing}/plugin.json（T11）

改动（要点）
  Include/Vase/Config/Value.h                 kEnum + 三处分支（T1）
  Include/Vase/Host/ConfigBlob.h + Source/Host/ConfigBlob.cpp  EnumStored（T1）
  Include/Vase/Config/FieldInfo.h             ChoiceInfo/ChoiceView/Meta 追加 + FieldInfo 尾两槽（T2）
  Include/Vase/Config/ConfigMacros.h          _DETAIL_FIELD 两行（T2）
  Include/Vase/PluginDescriptor.h             kHeaderVersion=3 + 注释（T2）
  Include/Vase/Catalog/ManifestView.h + Source/Catalog/ManifestJson.cpp  enum schema（T3）
  Source/Catalog/Solve.cpp                    Coerce enum（T4）；②两段制重写 + ⑦填 Expected（T9/T10）
  Source/Host/PluginHost.cpp                  ValueKindName（T1）；MakeInstance 成员（T5）；CreatePod 比对点（T7）；AdoptImpl/新重载（T8）→ 退役表/旧重载（T12）
  Include/Vase/Host/PluginHost.h              AdoptRequest + 重载（T8）→ 删 KnownBinaries（T12）
  Include/Vase/Host/LoadPlan.h                Expected 指针（T7）
  Include/Vase/Host/Evidence.h                AdoptReport.ManifestVerified（T8）
  Include/Vase/Catalog/PluginCatalog.h + Source/Catalog/PluginCatalog.cpp  AdoptInto + scratch 存储（T9）
  Source/Catalog/PresetJson.cpp               根级 OnlyKeys（T10）
  Samples/{HelloCommon/Greeter.h,HelloPlugin/HelloPlugin.cpp,HelloPluginPrime/HelloPlugin.cpp,Embedding/main.cpp}（T6/T12）
  Tools/VaseConsole/{CMakeLists.txt,Console.h,Console.cpp}  双入口（T12）
  Tests/{HotSwap/AdoptTests.cpp,HotSwap/HotSwapLoopTests.cpp,Abi/ImportEnforcementTests.cpp,
         Integration/ConfigApplyTests.cpp,Integration/RecursiveTeardownTests.cpp,
         Unit/DescriptorTests.cpp,Unit/ConfigValueTests.cpp,Unit/ConfigBlobTests.cpp,
         Unit/ConfigMacroTests.cpp,Unit/ManifestJsonTests.cpp,Unit/PresetJsonTests.cpp,
         Integration/SolveTests.cpp,Integration/CatalogScanTests.cpp,Integration/AssemblyFromSolveTests.cpp,
         Integration/fixtures/manifests/*/plugin.json}  各任务分头
  Tests/CMakeLists.txt                        新测试源 ×2
  CMakeLists.txt                              Samples 两行（T11）
```

---

### Task 1: `kEnum` 值面与 `ConfigBlob::EnumStored`

**Files:**
- Modify: `Include/Vase/Config/Value.h`
- Modify: `Include/Vase/Host/ConfigBlob.h`、`Source/Host/ConfigBlob.cpp`
- Modify: `Source/Host/PluginHost.cpp`（`ValueKindName`，约 :143-163）
- Test: `Tests/Unit/ConfigValueTests.cpp`、`Tests/Unit/ConfigBlobTests.cpp`

**Interfaces:**
- Consumes: 无（本波第一个任务）
- Produces: `ValueKind::kEnum`；`Value::From<enum>/GetAs<enum>`；`ConfigBlob::EnumStored{ std::int32_t Value; }` 与其 `Set/Find` 通路。T2/T4/T5 都吃这些。

- [ ] **Step 1: 写失败测试**（`ConfigValueTests.cpp` 尾部追加；`ConfigBlobTests.cpp` 同）

「`underlying ≠ int32` 编译失败」与「枚举带 choices 的配对闸」都是**编不过的形状反例，不进编译面**（本仓纪律，T2 Step 2 同此），以注释一行记录其存在。运行期可断言的：

```cpp
// ConfigValueTests.cpp 追加 —— 文件级放一个测试枚举
enum class ET : std::int32_t { A = 0, B = 7 };

TEST(EnumValue, RoundTripsViaInt32Bits)
{
    const vase::Value v = vase::Value::From<ET>(ET::B);
    EXPECT_EQ(v.Kind, vase::ValueKind::kEnum);
    EXPECT_EQ(v.GetAs<ET>(), ET::B);
    EXPECT_EQ(static_cast<std::int32_t>(static_cast<std::uint32_t>(v.Bits)), 7); // 位形低 32 位
    EXPECT_TRUE(v == vase::Value::From<ET>(ET::B));
    EXPECT_FALSE(v == vase::Value::From<std::int32_t>(7)); // Kind 参与相等
}

// ConfigBlobTests.cpp 追加
TEST(ConfigBlobEnum, SetAndFindRoundTrip)
{
    vase::ConfigBlob blob;
    blob.Set("e", vase::Value::From<ET>(ET::B));
    const auto got = blob.Find("e");
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(got->Kind, vase::ValueKind::kEnum);
    EXPECT_EQ(got->GetAs<ET>(), ET::B);
    EXPECT_NE(blob.Find("i"), std::nullopt); // 下面混入 int32 后两者不可混读
}
TEST(ConfigBlobEnum, DistinctFromInt32)
{
    vase::ConfigBlob blob;
    blob.Set("i", vase::Value::From<std::int32_t>(7));
    blob.Set("e", vase::Value::From<ET>(ET::B));
    EXPECT_EQ(blob.Find("i")->Kind, vase::ValueKind::kInt32);
    EXPECT_EQ(blob.Find("e")->Kind, vase::ValueKind::kEnum); // 折进 int32 则此条红（D78 的第一红测）
}
```

- [ ] **Step 2: 跑红**：`cmake --build --preset win-x64-clang-debug --target VaseTestsUnit` 失败于 `kEnum`/`EnumStored` 不存在（target 名以 `Tests/CMakeLists.txt` 现磁盘为准，测试目录共用一个可执行 target 的现例）。
- [ ] **Step 3: 实现 `Value.h`**：`ValueKind` 尾加 `kEnum`；`KindOf` 在 `const char*` 分支**前**插 `else if constexpr (std::is_enum_v<T>)`（体内 `static_assert(std::is_same_v<std::underlying_type_t<T>, std::int32_t>, "enum config field must have : std::int32_t underlying (D75)"); return ValueKind::kEnum;`）；`From` 同样插枚举分支（`out.Bits = static_cast<std::uint64_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(value)));`）；`GetAs` 在最后 else 的 static_assert **前**插 `else if constexpr (std::is_enum_v<T>) { return static_cast<T>(GetAs<std::int32_t>()); }`。删头注释「enum 不做（D22）」句，换「`kEnum` 自 M2b 波 2（D75）：值走 int32 位形，choices 表在 FieldInfo」。两处 NOLINT 的 union 访问点**复用既有块**，不新增。
- [ ] **Step 4: 实现 `ConfigBlob`**：public 段加 `struct EnumStored { std::int32_t Value; };`，`Storage` variant 追加它；`Set` 的 switch 在 `kString` 前后插 `case ValueKind::kEnum: converted = EnumStored{view.GetAs<std::int32>() 的位形解码——用 GetAs<std::int32_t>()};`（`GetAs<int32>` 对 kEnum 的 Value 合法：Bits 低 32）；`ToView` 加 `get_if<EnumStored>` 分支构 `Value{.Kind=kEnum, .Bits=…}`（NOLINTNEXTLINE 同文件既有口吻）。头注释补一句 kEnum。
- [ ] **Step 5: 补 `ValueKindName`**（PluginHost.cpp :143）：加 `case vase::ValueKind::kEnum: return "kEnum";`——漏了它 `-Wswitch` 在本工具链是 error，构建当场响。
- [ ] **Step 6: 全构建 + 测试绿**：`cmake --build --preset win-x64-clang-debug`；`ctest --preset win-x64-clang-debug -R "Config"`。全绿后按 Global Constraints 过格式门。
- [ ] **Step 7: Commit**：`git add -A && git commit -m "M2b波2-T1：ValueKind::kEnum 值面与 ConfigBlob::EnumStored（D75/D78）"`

---

### Task 2: `FieldInfo` choices 槽、宏面配对执法、`kHeaderVersion` 2→3

**Files:**
- Modify: `Include/Vase/Config/FieldInfo.h`、`Include/Vase/Config/ConfigMacros.h`
- Modify: `Include/Vase/PluginDescriptor.h`（`kHeaderVersion`，:31）
- Test: `Tests/Unit/DescriptorTests.cpp`（:47 字面量）、`Tests/Unit/ConfigMacroTests.cpp`

**Interfaces:**
- Consumes: T1 的 `kEnum`
- Produces: `ChoiceInfo{int32, const char*}`、`ChoiceView{const ChoiceInfo*, std::uint32_t}`、`Meta::Choices`、`FieldInfo::Choices/ChoiceCount`（尾部）、`vase::ChoicesOf(std::array)`、`vase::CheckedChoices<T>(Meta)`；`kHeaderVersion=3`。

- [ ] **Step 1: 改钉死字面量的既有测试**：`DescriptorTests.cpp:47` → `EXPECT_EQ(vase::kHeaderVersion, 3U);`。跑构建（`--target VaseTestsUnit` 级别）验证它红——**先红**（字面量 vs 常量）。
- [ ] **Step 2: 写失败测试**（`ConfigMacroTests.cpp` 追加）

```cpp
enum class Mood : std::int32_t { Quiet = 0, Loud = 1 };
inline constexpr std::array<vase::ChoiceInfo, 2> MoodChoices = {{ {0, "安静"}, {1, "响亮"} }};
VASE_CONFIG(EmotionConfig,
    (std::int32_t, Repeats, 1, vase::Meta{.Label = "次数"}),
    (Mood, Feel, Mood::Quiet, vase::Meta{.Label = "情绪", .Choices = vase::ChoicesOf(MoodChoices)}));

TEST(ConfigMacroEnum, FieldInfoCarriesChoicesTable)
{
    const auto& fields = EmotionConfig::kFields;
    EXPECT_EQ(fields[1].Kind, vase::ValueKind::kEnum);
    ASSERT_EQ(fields[1].ChoiceCount, 2U);
    ASSERT_NE(fields[1].Choices, nullptr);
    EXPECT_EQ(fields[1].Choices[0].Value, 0);
    EXPECT_STREQ(fields[1].Choices[1].Label, "响亮");
    EXPECT_EQ(fields[1].Default.GetAs<Mood>(), Mood::Quiet);
    EXPECT_EQ(fields[0].Choices, nullptr);   // 非枚举字段全零槽
    EXPECT_EQ(fields[0].ChoiceCount, 0U);
}
```

两个编译期反例（enum 无 choices / 非 enum 带 choices）**不进编译面**，其存在性以文件头注释一行记录（与本仓「形状反例不编」纪律同格）。
- [ ] **Step 3: 实现 `FieldInfo.h`**：加 `struct ChoiceInfo{ std::int32_t Value; const char* Label; };` 与 `struct ChoiceView{ const ChoiceInfo* Items = nullptr; std::uint32_t Count = 0; };`；`Meta` 尾加 `ChoiceView Choices{};`（**作者面唯一新东西**，tuple 仍四项——见偏离登记 #1）；helper：

```cpp
template <std::size_t N>
consteval ChoiceView ChoicesOf(const std::array<ChoiceInfo, N>& table)
{
    return ChoiceView{table.data(), static_cast<std::uint32_t>(N)}; // 命名存储：inline constexpr 数组，与 MetaArray 同一条寿命纪律（D77）
}

template <typename FieldType>
consteval ChoiceView CheckedChoices(Meta m)
{
    constexpr bool isEnum = KindOf<FieldType>() == ValueKind::kEnum;
    static_assert(!isEnum || m.Choices.Count > 0, "enum config field must carry choices (D76)");
    static_assert(isEnum || m.Choices.Count == 0, "choices only legal on enum fields (D76)");
    return m.Choices;
}
```

`FieldInfo` **尾部**追加 `const ChoiceInfo* Choices = nullptr; std::uint32_t ChoiceCount = 0;`（NSDMI；注释一行「尾两槽：v3 新增，全零 = 非枚举」）。头注释里「Min/Max 无执法（D35）」保留，补「choices 的成员执法在 Solve 与装配点（D79/D80），此处仍纯数据」。
- [ ] **Step 4: 实现宏面**（`ConfigMacros.h`）：`_DETAIL_FIELD` 在 `.Apply` 行**之前**插两行：

```cpp
.Choices = vase::CheckedChoices<VASE_CONFIG_DETAIL_T1 T>(VASE_CONFIG_DETAIL_TAIL T).Items,                                          \
.ChoiceCount = vase::CheckedChoices<VASE_CONFIG_DETAIL_T1 T>(VASE_CONFIG_DETAIL_TAIL T).Count,                                      \
```

（两次调用同一 consteval，零成本；`T` 是 struct 类型实参、`VASE_CONFIG_DETAIL_T1 T` 取字段类型——与 `.Kind` 行同法。）
- [ ] **Step 5: bump**：`PluginDescriptor.h:31` `kHeaderVersion = 3U`；注释续句式 `// 2 → 3（M2b-波2）：FieldInfo 尾追加 Choices/ChoiceCount 两槽（D76）。` 顺手收 `Value.h` 若还挂着「bump 时再定」的残句（改到即收）。
- [ ] **Step 6: 旧版号拒载证人（判据 12 现形扩展，不新造手法）**：`Tests/Integration/fixtures/StaleHeaderPlugin/StaleHeaderPlugin.cpp:41` 的 `.HeaderVersion = 999` 改为 **`2U`**（上一版号——比 999 更真的漂移形；它的 :46 注释「不是旧而是任何不等」随改成「钉的是上一代：v2 二进制配 v3 宿主」），并在 `Tests/Integration/FailureSemanticsTests.cpp:41` 的子串断言旁边加两条：`EXPECT_NE(failure.Message.find("binary 2"), …)`、`find("host 3")`——消息格式 `"HeaderVersion mismatch: binary 2, host 3"` 两侧数字都在场。
- [ ] **Step 7: 全构建**（树内所有 fixture/Sample 重编自动带 v3）+ `ctest --preset win-x64-clang-debug -R "Descriptor|ConfigMacro|FailureSemantics"`。
- [ ] **Step 8: 规矩 6 双平台**（描述符布局变更）：跑 Global Constraints 那两条 `-R 'HotSwap|Eject|Adopt'`，**两平台各自全绿留输出**。
- [ ] **Step 9: 格式门 + Commit**：`git commit -m "M2b波2-T2：FieldInfo choices 槽与宏面配对执法，kHeaderVersion 2→3（D76/D77）"`

---

### Task 3: manifest 的 enum schema（`schemaVersion` 仍 1）

**Files:**
- Modify: `Include/Vase/Catalog/ManifestView.h`、`Source/Catalog/ManifestJson.cpp`
- Test: `Tests/Unit/ManifestJsonTests.cpp`

**Interfaces:**
- Consumes: T1 `kEnum`
- Produces: `ManifestChoice{ std::int32_t Value; std::string Label; }`、`ManifestConfigField::Choices`（拥有形）；parser 的 enum 硬闸全表。T4/T9 吃 `Choices`。

- [ ] **Step 1: 写失败测试**（`ManifestJsonTests.cpp`，沿文件既有 `ParseManifestFile` 直调形）

```cpp
TEST(ManifestEnum, LegalEnumFieldParses)   // config 含 mood：type enum、choices 两条、default "响亮"
TEST(ManifestEnum, RequiresChoices)        // type enum 无 choices → Err
TEST(ManifestEnum, ChoiceShape)            // 元素非对象 / 缺 value 或 label / value 非整数 / 越 int32 → Err（越界复用上波 T8 的 int64 越界拒绝通道）
TEST(ManifestEnum, ChoiceUniqueness)       // value 重、label 重 → 各 Err
TEST(ManifestEnum, DefaultMustBeKnownLabel)// default 写整数、写未知 label → Err
TEST(ManifestEnum, MinMaxForbidden)        // enum 配 min/max → Err
TEST(ManifestEnum, ChoicesOnlyOnEnum)      // type int32 配 choices → Err
```

每例断言 `IsOk()`/消息含点名 token（`"enum"`、`"choices"`、`"duplicate"`、`"range"`——与本文件既有断言口吻一致，最小子串）。
- [ ] **Step 2: 跑红**（`--target` 测试线）确认失败于枚举不存在/schema 未识。
- [ ] **Step 3: 实现**：`ManifestView.h` 加 `struct ManifestChoice{ std::int32_t Value; std::string Label; };`，`ManifestConfigField` 尾加 `std::vector<ManifestChoice> Choices;`（注释：拥有形；`Kind==kEnum` 时非空）。`ManifestJson.cpp` 的 type 映射表加 `{"enum", ValueKind::kEnum}`；config 条目解析段（既有 `min/max` 与 `default` 校验旁边）按 D80 表实现七条闸；`default` 对 enum 存 **label 的 kString `Value`**（借 `DefaultsRaw`，既有模式），成员执法在 Solve（T4）——这里只闸「label ∈ choices」。头注释「`type: "enum"` 等未来值域按 unknown 值拒（波 2 开闸，D49）」改为「`enum` 自波 2 合法（D49 预留兑现）」。
- [ ] **Step 4: 跑绿 + 既有全量不破**：`ctest --preset win-x64-clang-debug -R "Manifest"`，再全跑 ctest（catalog 用例不该红——Solve 尚不识 kEnum，但既有 fixture 无 enum 字段）。
- [ ] **Step 5: 格式门 + Commit**：`git commit -m "M2b波2-T3：plugin.json 的 enum/choices schema 与七条硬闸（D80/D49）"`

---

### Task 4: Solve 的 enum 通道与唯一转换 helper

**Files:**
- Create: `Source/Catalog/Detail/ChoiceCoerce.h`（header-only 内部件）
- Modify: `Source/Catalog/Solve.cpp`（`Coerce`，:77 区）
- Test: `Tests/Integration/SolveTests.cpp`

**Interfaces:**
- Consumes: T3 `ManifestChoice`、T1 blob `EnumStored`
- Produces: `vase::catalog_detail::LabelToValue(label, choices) -> std::optional<std::int32_t>`、`ValueToLabel(value, choices) -> std::optional<std::string_view>`；Solve 的 enum override → `EnumStored`。T9 的 `BuildExpectation` 复用同 helper。

- [ ] **Step 1: 写失败测试**（`SolveTests.cpp`；沙箱清单复用既有 fixture 手法 + 一份含 enum 字段的清单字符串）

```cpp
TEST(SolveEnum, LabelOverrideCoercedToValue)   // override {"mood":"响亮"} → ResolvedConfig 该 key 的 Value Kind==kEnum 且解码 ==1
TEST(SolveEnum, UnknownLabelIsError)           // "疯狂" → Err，消息点名合法集（含 安静/响亮 两 token）
TEST(SolveEnum, IntegerOverrideIsError)        // 写 1（JSON 整数）→ Err（enum 只认 label，D80）
TEST(SolveEnum, MissingFieldCoerce)            // 被跳插件的 enum 覆盖类型错照样响（D66）
TEST(SolveEnum, DefaultsFlowAsEnumStored)      // 三层合并：清单 default "响亮" → blob kEnum==1
```

- [ ] **Step 2: 跑红**。
- [ ] **Step 3: 实现 `ChoiceCoerce.h`**：两个 `inline` 纯函数（线性扫表；十量级）。**实现 Solve**：`Coerce(Value, ValueKind)` 签名改 `Coerce(Value, const ManifestConfigField&)`——`kEnum` 时输入必须是 `kString` → `LabelToValue` → 构 `Value{kEnum}` 位形；非 enum 字段收到 `kEnum` 值不可能（blob 面无 enum 来源，除旁路 Set——kind 等值检查原样）。`OutOfRange` 对 enum 恒过（schema 已禁 min/max）。既有 numeric/string 通路零行为变更（改调用点传 field）。
- [ ] **Step 4: 跑绿** + `-R "Solve"` 全绿。
- [ ] **Step 5: 格式门 + Commit**：`git commit -m "M2b波2-T4：Solve 的 enum label→value 唯一转换与 D51 成员闸（D80）"`

---

### Task 5: 装配点成员执法（D79）

**Files:**
- Modify: `Source/Host/PluginHost.cpp`（`MakeInstance`，:547 区的 D32 kind 核对点）
- Test: `Tests/Integration/ConfigApplyTests.cpp`

**Interfaces:**
- Consumes: T1 `EnumStored`、T2 `FieldInfo::Choices`
- Produces: `MakeInstance` 对 `kEnum` 的 `Err`（消息含 `"not in choices"` 与字段名）；此点为 CreatePod 与 Adopt **共用**（`MakeInstance` 一台机器）。

- [ ] **Step 1: 写失败测试**（`ConfigApplyTests.cpp` 追加）

```cpp
TEST(ConfigApplyEnum, OutOfChoicesBlobIsErrNotApply)
{
    // 手写 LoadPlan（Expected 无值，走既有旁路）+ ResolvedConfig 里 Set("mood", 位形 99 的 kEnum Value)——
    // 构造 Value{kEnum, Bits=99}（不经清单的合法域）：
    // 期望 CreatePod 该条目落 FailedPluginRecord（Phase::kLoad），消息含 "not in choices" 与 "mood"。
    // 断言形抄本文件既有 kind-mismatch 用例（它钉 "kInt32" token 的方式）。
}
TEST(ConfigApplyEnum, InChoicesApplies)        // Bits=1 → OnStart 后读回服务串含 "响亮"（用 HelloPrime v2 的 mood 输出，T6 先跑则用本地 fixture）
```

注：本任务先于 T6 落地时第二例改用 `ConfigConsumerPlugin`——它没有 enum 字段；所以**第二例挪到 T6**（Task 6 Step 1），本任务只钉第一例 + 既有 kind 用例不红。
- [ ] **Step 2: 跑红**。
- [ ] **Step 3: 实现**：`MakeInstance` 里 kind 等值通过后，`view.Kind == kEnum` 时扫 `field.Choices[0..ChoiceCount)` 比对 `static_cast<std::int32_t>(static_cast<std::uint32_t>(view.Bits))`；miss → 既有 Err 通路（`"config \"" + key + "\": value not in choices for field …"`，D32 消息家族同位）。`ApplyToImpl` 零改动（Kind 二线原样）。
- [ ] **Step 4: 跑绿**；本任务动了装配共用机器 → **规矩 6 双平台** `-R 'HotSwap|Eject|Adopt'` 各留一次绿（低成本防御，T5 若绿面不扩）。
- [ ] **Step 5: 格式门 + Commit**：`git commit -m "M2b波2-T5：装配点 enum 成员执法走 D32 通道（D79）"`

---

### Task 6: Hello 家族 `Mood` 字段（肉眼证人）与 hello 清单校准

**Files:**
- Modify: `Samples/HelloCommon/Greeter.h`（或旁生 `Samples/HelloCommon/Mood.h`——取旁生，Greeter 是服务接口不该背配置型）
- Modify: `Samples/HelloPlugin/HelloPlugin.cpp`、`Samples/HelloPluginPrime/HelloPlugin.cpp`（:14/:16 的 VASE_CONFIG 与 :32/:34 的问候串）
- Modify: `Tests/Integration/fixtures/manifests/hello/plugin.json`
- Test: `Tests/Integration/ConfigApplyTests.cpp`（T5 挪来的第二例）

**Interfaces:**
- Consumes: T2 宏面
- Produces: Hello/Prime 描述符的 `Mood` 字段（choices：`{0,"安静"}` `{1,"响亮"}`；默认 Quiet/Loud）；问候串尾缀 `[quiet]`/`[响亮]`——**定死**：`"… x" + std::to_string(cfg.Repeats) + " [" + (cfg.Mood == Mood::Loud ? "响亮" : "安静") + "]"`（Prime 用同一 helper 函数式，串仍含既有 `prime v2` 子串——**`VaseConsoleHotSwapReason` 钉的 "prime v2" 不能破**）。

- [ ] **Step 1: 写失败测试**：`ConfigApplyTests` 第二例（InChoicesApplies，经 hello 沙箱 `CreatePod` 后 `get`）+ 既有 console `VaseConsoleHotSwap*` 四条**不许红**（跑 `-R VaseConsole` 核对）。
- [ ] **Step 2: 实现** `Samples/HelloCommon/Mood.h`：`enum class Mood : std::int32_t { Quiet = 0, Loud = 1 };` + `inline constexpr std::array<vase::ChoiceInfo, 2> MoodChoices = {{ {0, "安静"}, {1, "响亮"} }};`。两插件 `VASE_CONFIG` 加第二元组（`(Mood, MoodValue, Mood::Quiet/Loud, vase::Meta{.Label = "情绪", .Choices = vase::ChoicesOf(MoodChoices)})`）；问候串按上式扩。
- [ ] **Step 3: hello 清单校订**：`manifests/hello/plugin.json` 的 `config` 追加 `{"key":"MoodValue","type":"enum","choices":[{"value":0,"label":"安静"},{"value":1,"label":"响亮"}],"default":"安静","displayName":"情绪"}`，且**逐字核对** `id/displayName/version/provides` 与 `HelloPlugin.cpp` 描述符（displayName "示例插件"、version "0.1.0"）。
- [ ] **Step 4: 全构建 + `ctest --preset win-x64-clang-debug`（全量！清单变了，catalog 全簇都要过）**；msvc 线随手跑一次（`Scripts/msvc-env.cmd cmake --build --preset win-x64-msvc-debug`——M2a 教训：任务波即跑 cl 线防 `/Zc:preprocessor` 面回归）。
- [ ] **Step 5: 格式门 + Commit**：`git commit -m "M2b波2-T6：Hello 家族 Mood 字段与 hello 清单逐字校准（D87）"`

---

### Task 7: `ManifestExpectation`、唯一比对、CreatePod 比对点

**Files:**
- Create: `Include/Vase/Host/ManifestExpectation.h`、`Source/Host/Detail/ManifestCompare.h/.cpp`
- Modify: `Include/Vase/Host/LoadPlan.h`、`Source/Host/PluginHost.cpp`（`CreatePodImpl` 的 InspectBinary 后、:249 区）
- Create: `Tests/Integration/LoadTimeComparisonTests.cpp`；Modify: `Tests/CMakeLists.txt`（源列表加一行）

**Interfaces:**
- Consumes: T2 `FieldInfo::Choices`、T1 `kEnum`
- Produces: `LoadPlanEntry::Expected`（`std::optional<ManifestExpectation>`，拥有值形）；`vase::detail::CompareDescriptor(const ManifestExpectation&, const vase::PluginDescriptor&) -> Result<void>`（消息单源：`"manifest/binary mismatch for \"<id>\": <detail>"`）。T8/T9 吃这两件。

- [ ] **Step 1: 写期望头**（拥有值形，全成员 NSDMI；注释钉 D67/D74/D72 三件事 + 「零借用窗、拷贝移动自包含」）

```cpp
struct ExpectedService     { std::string Name; std::uint32_t Version = 0; };
struct ExpectedChoice      { std::int32_t Value = 0; std::string Label; };
struct ExpectedConfigField {
    std::string Key; ValueKind Kind = ValueKind::kNone;
    std::optional<ConfigBlob::Storage> Default{}, Min{}, Max{}; // 拥有 variant（含 EnumStored）；nullopt=未设
    std::string Label;
    std::vector<ExpectedChoice> Choices{};                      // 按序（D72）
};
struct ManifestExpectation {
    std::string Id, DisplayName, Version;
    std::vector<ExpectedService> Requires{}, OptionalRequires{}, Provides{};
    std::vector<ExpectedConfigField> Config{};
};
```

（`ManifestExpectation.h` include `ConfigBlob.h` 取 `Storage`；导出宏不需要——与 `LoadPlan.h` 同格纯数据。）

- [ ] **Step 2: 写失败测试** `LoadTimeComparisonTests.cpp`（基字面量 = HelloPlugin 描述符逐字抄，含 T6 的 Mood；每例只篡改一个字段）

```cpp
namespace { // 基期望 = HelloPlugin.cpp 的 VASE_PLUGIN 逐字抄；拥有形以函数构值（篡改 = 取值后只改一处）
inline vase::ManifestExpectation HelloExpectation()
{
    vase::ManifestExpectation exp;
    exp.Id = "Vase.Hello"; exp.DisplayName = "示例插件"; exp.Version = "0.1.0";
    exp.Provides = { {.Name = "Vase.Hello.Greeter", .Version = 1} };
    exp.Config = {
        { .Key = "Repeats", .Kind = vase::ValueKind::kInt32, .Default = vase::ConfigBlob::Storage{std::int32_t{1}} },
        { .Key = "MoodValue", .Kind = vase::ValueKind::kEnum,
          .Default = vase::ConfigBlob::Storage{vase::ConfigBlob::EnumStored{0}},
          .Choices = { {0, "安静"}, {1, "响亮"} } },
    };
    return exp;
}
// 建局 helper（本文件私有）：单条 LoadPlan{.Id="Vase.Hello", .BinaryPath=VASE_FIXTURE_HELLO,
//   .Decision=kLoad, .Expected = 传入值或 std::nullopt}——构形抄 Tests/HotSwap/AdoptTests.cpp 的 Plan() 现例。
}
TEST(LoadTimeComparison, ExactMatchLoads)          // Expected 喂真期望 → pod 正常、无 Failed
TEST(LoadTimeComparison, NullExpectedSkipsCompare)  // M2a 行为：Expected 无值不比对（既有全部手写用例的保护性断言）
TEST(LoadTimeComparison, DisplayNameDriftRefused)   // DisplayName 改一个字 → DestroyPod 报告含 Failed（"示例插件X"），Message 含 "manifest/binary mismatch" 与 "displayName"
TEST(LoadTimeComparison, ServiceSetOrderInsensitive)// Provides 两元素互换（多一个同构用例带第二条）→ 等值过
TEST(LoadTimeComparison, ServiceMissingRefused)     // 多声明一条 → 拒，点名 "provides[…]"
TEST(LoadTimeComparison, ConfigKindDriftRefused)    // Repeats 期望 kInt64 → 拒含 "config \"Repeats\".kind"
TEST(LoadTimeComparison, ChoicesOrderSensitiveRefused) // MoodTbl 互换 → 拒含 "choices[0]"
TEST(LoadTimeComparison, EmptyConfigVsManifestDeclinesRefused) // D89：期望带 config、描述符 .Config 全零 → 拒
```

失败断言形：`CreatePod` 返回 Ok（部分失败语义），`DestroyPod` 的 `report.Failures` 里找 Id==Vase.Hello、`failure.Stage==Phase::kLoad`、`Message` 子串——抄 `FailureSemanticsTests.cpp:28-41` 的现形。
- [ ] **Step 3: 跑红**（`CompareDescriptor` 无定义）。
- [ ] **Step 4: 实现 `ManifestCompare`**：单函数，`std::vector<std::string> diffs` 收不齐项、首错即拼消息（消息要含字段路径全名——测试钉的三 token 各归各家）：字符串逐 `string_view==`；服务三元组各构 `std::vector<std::pair<std::string,uint32_t>>` 排序后比；config 以 key 建 `std::map<std::string_view, …>` 双向比，条目内 Kind→Default（kString 比**文本**：`std::string_view(GetAs<const char*>())==`）→Min/Max/Label→Choices（size 等值后**按序**逐项）；Skip：`Expected` 无值由调用点判。`Source/CMakeLists(Host 源列表)` 加 `Detail/ManifestCompare.cpp`（Host 的 target 定义在根 `CMakeLists.txt`——以现磁盘源列表处加）。
- [ ] **Step 5: 实现 CreatePod 挂点**：`CreatePodImpl` 取 `desc` 成功后、注册表动作前：`if (entry.Expected.has_value()) { cmp = detail::CompareDescriptor(*entry.Expected, *desc); if (!cmp.IsOk()) { 按 §5.2 落 FailedPluginRecord（复用 HeaderVersion 失败同一路径）+ 级联; continue; } }`。
- [ ] **Step 6: 跑绿** + 全量 ctest；**规矩 6 暂不触发**（Adopt 未动）但 `-R "FailureSemantics"` 跑一遍。
- [ ] **Step 7: 格式门 + Commit**：`git commit -m "M2b波2-T7：ManifestExpectation 与 CreatePod 加载期比对（D67/D68/D72/D89）"`

---

### Task 8: `AdoptRequest` 新重载（过渡期双轨，旧路径原样留守）

**Files:**
- Modify: `Include/Vase/Host/PluginHost.h`、`Include/Vase/Host/Evidence.h`、`Source/Host/PluginHost.cpp`
- Test: `Tests/HotSwap/AdoptTests.cpp`（追加，不迁移既有用例——它们吃旧重载，T12 才迁）

**Interfaces:**
- Consumes: T7 `CompareDescriptor`、`ManifestExpectation`
- Produces: `struct AdoptRequest{ std::string Id; std::filesystem::path BinaryPath; const ManifestExpectation* Expected; std::vector<std::string> SiblingBinaries; };`（**指针形：过渡期 `nullptr` = 跳过比对、`ManifestVerified=false`；T12 起 `nullptr` 即 `Err "adopt refused: manifest expectation required"`**，头注释明写这半句）；`AdoptPlugin(PodHandle, const AdoptRequest&)`；`AdoptReport::ManifestVerified`。

- [ ] **Step 1: 写失败测试**（`AdoptTests.cpp` 尾追加）

```cpp
TEST(Adopt, RequestOverloadHappyPath)   // eject hello 后 AdoptRequest{路径, &helloExp, {}} → kAdopted 且 report.ManifestVerified==true
TEST(Adopt, RequestExpectationIdMismatchIsErr) // Expected.Id 换 "Vase.Other" → Err 含 "adopt request/expectation id mismatch"
TEST(Adopt, LegacyOverloadLeavesManifestUnverified) // 旧双参 overload 成功 → ManifestVerified==false（过渡期形状，T12 删本例）
```

- [ ] **Step 2: 跑红**。
- [ ] **Step 3: 重构**：把 `AdoptPlugin(handle, id)` 函数体原样搬进私有 `Result<AdoptReport> AdoptImpl(PodHandle, std::string id, const std::filesystem::path& absPath, const ManifestExpectation* expected, const std::vector<std::string>& siblings)`——内部各步编号（⓪/①/②/③/③'/④/⑤/⑥）与判定**一字不动**；差异只在：⓪ 前加 `expected && expected->Id != id → Err "adopt request/expectation id mismatch"`；③ 后插 `if (expected) { cmp=CompareDescriptor(*expected,*desc); Err 透传（Refusal 包装）; verified=true; }`；④ 的兄弟循环从 `KnownBinaries` 改吃 `siblings`（lowercase 判定体不动）；报告 `ManifestVerified = verified`。**旧 overload** = 查 `KnownBinaries`（无表项仍 Err "unknown plugin id…" 原文）**+ `expected=nullptr` + 旧路径构的 siblings**，委托 `AdoptImpl`。注释：过渡双轨，T12 单轨。
- [ ] **Step 4: 跑绿 + 规矩 6 双平台**（Adopt 路径动过）。
- [ ] **Step 5: 格式门 + Commit**：`git commit -m "M2b波2-T8：AdoptRequest 重载与比对挂点（过渡双轨，D69/D70）"`

---

### Task 9: Catalog 编排：`BuildExpectation`、`AdoptInto`、Solve 自动填期望 + D57 校准

**Files:**
- Modify: `Include/Vase/Catalog/ManifestView.h`（`BuildExpectation` 声明——并入 PluginCatalog.h 亦可，取 ManifestView.h，它已含两面类型）
- Modify: `Source/Catalog/PluginCatalog.cpp`（新函数 + scratch）、`Source/Catalog/Solve.cpp`（⑦ 填 Expected）、`Include/Vase/Catalog/PluginCatalog.h`（`AdoptInto` 成员 + `#include "Vase/Host/PluginHost.h"`）
- Create: `Tests/HotSwap/AdoptManifestTests.cpp`；Modify: `Tests/CMakeLists.txt`
- 校准: `Tests/Integration/fixtures/manifests/{edge_consumer,shared_provider,shared_consumer2}/plugin.json` ↔ 各自 fixture 源

**Interfaces:**
- Consumes: T7/T8；T4 `LabelToValue`（`BuildExpectation` 的 enum default 转换）
- Produces: `VASE_CATALOG_API ManifestExpectation BuildExpectation(const ManifestEntry&)`（**拥有值返回**，入参只读）；`Result<AdoptReport> PluginCatalog::AdoptInto(PluginHost&, PodHandle, std::string_view id)`；`Solve` 产出的 `Plan.Ordered[*].Expected` **必有值**（深拷，无窗；Outcome 余下的唯一借用仍是 `Id` 的 D61 窗，头注释只写它）。

- [ ] **Step 1: 先校清单**（否则本任务自撞）：三对 fixture（`edge_consumer↔EdgeConsumerPlugin.cpp`、`shared_provider↔SharedProviderPlugin.cpp`、`shared_consumer2↔SharedConsumer2Plugin.cpp`）**以 .cpp 描述符为唯一源**逐字补 `displayName`/`version`（缺失即加：如 edge_consumer 现清单没有 `displayName`，补 `"账本消费者探针"`、`"version":"0.0.1"` 等）与 config 全等值——规则：**声明必现、未声明必无**。跑 `-R "Catalog|Solve|Assembly"` 保持绿（此时比对还没进 Solve 链，校订零风险）。
- [ ] **Step 2: 写失败测试** `AdoptManifestTests.cpp`（`CatalogSandbox` + hello DLL 拷进 `<sandbox>/VaseHello/HelloPlugin.dll` + 清单落 `<sandbox>/VaseHello/plugin.json`）

```cpp
TEST(AdoptManifest, AdoptIntoHappyPath)            // refresh→pod new(hello 裸计划)→eject→AdoptInto("Vase.Hello") kAdopted + ManifestVerified
TEST(AdoptManifest, FreshManifestNotFromSnapshot)  // 快照建立后改盘上清单（displayName 加后缀）→ AdoptInto 拒（Err 含 "manifest/binary mismatch"）——证「就地重读」不吃快照
TEST(AdoptManifest, MissingManifestInSnapshotDirErr)// 删沙箱内 plugin.json → Err 含 "plugin.json"（读失败响亮）
TEST(AdoptManifest, IdNotInSnapshotErr)            // AdoptInto("Vase.Nowhere") → Err 含 "not in catalog snapshot"（D81 归因）
```

- [ ] **Step 3: 跑红**。
- [ ] **Step 4: 实现**（拥有值形——需求方对偏离 #2 的改判裁决）：`BuildExpectation(const ManifestEntry&) -> ManifestExpectation`（**ManifestView.h 声明 / PluginCatalog.cpp 定义，值返回**：字符串直拷；三服务数组逐条转 `ExpectedService`；`ManifestConfigField` 的 `Value Default/Min/Max` 转进 `ConfigBlob::Storage` variant——kString 拷文本、kEnum 的 **label 经 T4 的 `LabelToValue` 换成 `EnumStored`**，「期望形只存 value 形」在此收口；`ManifestChoice → ExpectedChoice` 按序）。**Solve ⑦**：每个条目（kLoad 与 kSkip 皆填）`Entry.Expected = BuildExpectation(At(SnapshotEntries, index));`——深拷、无 scratch 成员、无借用窗；`PluginCatalog.h` 的头注释里 D61 段改为「余下唯一借用 = `Plan.Ordered[i].Id` 的快照窗」。**AdoptInto**：`Find(id)` 取子目录 → `ParseManifestFile(dir/"<sub>/plugin.json", sub)`（**spec 的 LoadManifestFile 即此现成公开函数，不另立新名——偏离 #3 裁决**）→ 函数局部 `ManifestExpectation exp = BuildExpectation(parsedEntry)` → 兄弟名 = 快照全量 `LibraryFileName(entry.Binary)`（D54 helper 现）→ `host.AdoptPlugin(handle, AdoptRequest{ std::string(id), abs路径, &exp, siblings })`（同步调用，局部量安全）。`AdoptInto` 成员声明进 `PluginCatalog.h`（include `Vase/Host/PluginHost.h`；Catalog 链 Host，方向合法——若嫌头重，拆 `CatalogAdopt.h` 自由函数，本步实测后择一并在提交信息里记择了哪个）。
- [ ] **Step 5: 跑绿 + D57 全链**：`ctest --preset win-x64-clang-debug -R "Assembly|AdoptManifest|Catalog"`——AssemblyFromSolve 的 `CreatePod` 从此带期望执法（Step 1 的校准在此见红/见绿）。**规矩 6 双平台**。
- [ ] **Step 6: 格式门 + Commit**：`git commit -m "M2b波2-T9：BuildExpectation/AdoptInto/Solve 自动填期望与 D57 校准（D81/D84）"`

---

### Task 10: handoff 三账：归因两段制、Preset 根级闸、再绑定证人

**Files:**
- Modify: `Source/Catalog/Solve.cpp`（② 段，:346-415 区重写）
- Modify: `Source/Catalog/PresetJson.cpp`（根对象键闸）
- Test: `Tests/Integration/SolveTests.cpp`、`Tests/Unit/PresetJsonTests.cpp`、`Tests/Integration/CatalogScanTests.cpp`

**Interfaces:**
- Consumes: T9 的 ⑦（同文件，先合 T9 再动本段）
- Produces: `SkipReason`/`Notes` 的终态语义（D82）；preset 根级 unknown 拒绝（D83）；D61 窗口的再绑定用例。

- [ ] **Step 1: 写失败测试**

```cpp
// SolveTests 追加：
TEST(SolveAttribution, FinalStateNotIntermediate)   // A←B←C 链禁 A：B、C 的 Reason==kMissingDependency、各一条 kProviderSkipped
                                                    // （B.Cause=A、C.Cause=B）——现状红点：迭代轮次里 C 可能先看到 B「缺席」再看到 B 被跳，归因抖动
TEST(SolveAttribution, DisabledSelfKeepsKDisabled)  // 被禁者本身 Reason 恒 kDisabled（即便上游也缺）
TEST(SolveAttribution, NotesDeterministicSorted)    // 双跑 + 乱序喂 Overrides → Notes 按 (Kind,PluginId,Key,Cause) 字典序逐字节等
// PresetJsonTests 追加：
TEST(PresetRoot, UnknownRootKeyRejected)            // 根级多一个 "author":… → Err 含 "unknown field"
// CatalogScanTests 追加（§3.3/D61）：
TEST(CatalogWindow, RebindAfterSecondRefresh)       // Refresh(A)→Solve→Refresh(B)→Solve：新 Plan 的 Ids/Expected 全按 B、Directory()==B、Find 只命中 B
```

- [ ] **Step 2: 跑红**。
- [ ] **Step 3: 重写 ②**：Phase A —— while 循环只翻 `participating`（删循环内对 `reasons` 与 `outcome.Notes` 的全部写入；`hostProvides` 判定保留）；Phase B —— 按 `SnapshotEntries` Id 序扫 `participating==0` 的条目：①`①步判过 disabled` → `kDisabled`（用一个 `std::vector<char> selfDisabled` 在 ① 段落存底）；②否则逐 `Requires`：HostProvided 满足跳过；`exactProviders` 有**终态参与**者 → 满足；否则首个 exact 存在者（不参与）→ `kMissingDependency` + `Note{kProviderSkipped, Cause=该提供方按 Id 序最小者}`；仅同名异版本在场 → `kVersionMismatch` + `Note{kVersionMismatchProvider, Cause=在场者}`；全无 → `kMissingDependency` 无 Note。③ Notes 收尾 `std::sort` 去重。既有各断言随终态语义复核（`provided by`/`unresolved declarations` 两族**不新增**消息子串——字段断言纪律）。
- [ ] **Step 4: 根级闸**：`PresetJson.cpp` 根对象处补 `OnlyKeys(root, {"schemaVersion", "displayName", "overrides"})`（条目级现成，根级补同族函数调用）。
- [ ] **Step 5: 跑绿**：`-R "Solve|Preset|CatalogScan"` 全绿 + 全量 ctest。
- [ ] **Step 6: 格式门 + Commit**：`git commit -m "M2b波2-T10：Solve 归因两段制、Preset 根级闸与再绑定证人（D82/D83）"`

---

### Task 11: Samples 两员 `DependentPlugin` / `FailingPlugin`

**Files:**
- Create: `Samples/DependentPlugin/CMakeLists.txt`、`Samples/DependentPlugin/DependentPlugin.cpp`
- Create: `Samples/FailingPlugin/CMakeLists.txt`、`Samples/FailingPlugin/FailingPlugin.cpp`
- Create: `Tests/Integration/fixtures/manifests/{dependent,failing}/plugin.json`
- Modify: 根 `CMakeLists.txt`（`add_subdirectory(Samples/…)`，:279-283 区）

**Interfaces:**
- Consumes: Hello 的 `Vase.Hello.Greeter` 服务（`Samples/HelloCommon`）
- Produces: 两个演示插件二进制 + 与描述符逐字一致的清单（判据 8 Sample 面、T12 回放材料）。

- [ ] **Step 1: 写实现**（模板抄 `Samples/HelloPlugin/` 的 CMakeLists——`vase_add_plugin_fixture(... LINK_LIBRARIES VasePod)` 形态；描述符：

```cpp
// DependentPlugin.cpp —— 服务接口用 Samples/HelloCommon 的 IGreeter（引号包含，同 HelloPlugin 法）
VASE_PLUGIN(DependentPlugin){
    .Id = "Vase.Dependent", .DisplayName = "依赖演示", .Version = "0.1.0",
    .Requires = {{.Name = "Vase.Hello.Greeter", .Version = 1}},
    .Provides = {{.Name = "Vase.Farewell", .Version = 1}},
};
// OnLoad: ctx.Provide(Vase.Farewell@1, 一个返回 "farewell via Vase.Hello" 的 trivial 服务体)
// OnStart: auto& greeter = ctx.Get<samples::IGreeter>();  // 凭声明解析（Requires 在场）
//          greeter.SetGreeting(greeter.Greeting() + " + farewell");  // 访问器名以 Greeter.h 现磁盘为准
// FailingPlugin.cpp
VASE_PLUGIN(FailingPlugin){
    .Id = "Vase.Failing", .DisplayName = "失败演示", .Version = "0.1.0", .Provides = {},
};
// OnLoad 返回 Result<void>::Ok()；OnStart 返回
//   Result<void>::Err(vase::Error{"demo: OnStart always fails"})   // 判据 4 后半/判据 8 的注入位
```

）
- [ ] **Step 2: 两份清单逐字抄**（`displayName`/`version`/`requires`/`provides` 全等；`config: []` 省略即默认空；注意 D89——两插件无 config，清单也**不得**带 config）。
- [ ] **Step 3: 构建 + 冒烟**：写一个临时 gtest 断言（放 `AssemblyFromSolveTests` 扩格）：hello+dependent+failing 三件沙箱，Solve → CreatePod → 断言 failing 落 Failed、dependent 被级联跳过（`Skips` 含 `Vase.Dependent`）、hello 活；`-R AssemblyFromSolve` 绿即删临时断言亦可保留为 §7「判据 8 Sample 面」证人——**保留**，它就是判据 4 后半在 Sample 材料上的那半格（另一半 T12 console 面）。
- [ ] **Step 4: 格式门 + Commit**：`git commit -m "M2b波2-T11：Samples 补 DependentPlugin 与 FailingPlugin（D86）"`

---

### Task 12: Console 双入口 + 迁移与退役（单轨化收口）

**Files:**
- Modify: `Tools/VaseConsole/CMakeLists.txt`（链 `VaseCatalog` + 回放资产重写）、`Tools/VaseConsole/Console.h`、`Tools/VaseConsole/Console.cpp`
- Modify: `Include/Vase/Host/PluginHost.h` + `Source/Host/PluginHost.cpp`（**删** `KnownBinaries` 成员/注册/Eject :835 注释段/Adopt :872 代偿段/旧 overload；`AdoptRequest::Expected` 收为必需——`nullptr` → Err `"adopt refused: manifest expectation required"`）
- Create: `Tests/TestingSupport/AdoptExpectations.h`
- Migrate: `Tests/HotSwap/AdoptTests.cpp`（9 处 + 删 `LegacyOverloadLeavesManifestUnverified` + `UnknownIdAndAlreadyInPodRejected` 按 D88 重写）、`Tests/HotSwap/HotSwapLoopTests.cpp`（:108/:147）、`Tests/Integration/ConfigApplyTests.cpp:118`、`Tests/Integration/RecursiveTeardownTests.cpp:100`、`Tests/Abi/ImportEnforcementTests.cpp:55`、`Samples/Embedding/main.cpp:245`

**Interfaces:**
- Consumes: 前 11 个任务的全部
- Produces: 波 2 终态的 Host API 与前端；`unknown plugin id` 子串自此退役（D88）。

- [ ] **Step 1: `AdoptExpectations.h`**：为迁移点各出一个 `inline vase::ManifestExpectation MakeXxxExpectation()`（hello/versionedA/edgeConsumer/collisionProvider/sharedProvider/sharedConsumer2/configConsumer/noBuildId/behindStrictUnused/badLinkSiblingA——meta 全部**逐字抄自各 fixture 源文件**；`LoadProbe` 从 `Tests/Integration/fixtures/LoadProbe/` 现场抄）。
- [ ] **Step 2: 迁移 gtest/Sample 六文件**（每处：`AdoptPlugin(h, id)` → 构 `AdoptRequest{ id, <路径来源>, &scratch.View, <兄弟名数组> }`；`UnknownIdAndAlreadyInPodRejected` 改两断言：request/expectation Id 不匹配含 `"request/expectation id mismatch"`；路径不存在 → Err 含 `"not found"`（EnsureResident 原文，跑一次取实际 token 钉最小段）。`HotSwapLoopTests` 的 InstallPrime 位：v1/v2 期望同一份（A/Prime 描述符等值，磁盘已证）——**不需要换件时换期望**）。每文件迁完跑该文件 `-R` 窄集绿。
- [ ] **Step 3: 退役**：删表、删旧 overload、`Expected` 必需化 + `AdoptTests` 里 `AdoptRequest{…, nullptr, …}` 一条误用例。全构建 + 全量 ctest。
- [ ] **Step 4: Console 改接**（`Console.h`：`#include "Vase/Catalog/PluginCatalog.h"`、`LivePod{ … std::unique_ptr<vase::PluginCatalog> Catalog; }`（catalog 来源局）、`Staged` 双槽 `std::optional<std::pair<std::string,std::vector<uint8_t>>> StagedBinary, StagedManifest;`；`Console.cpp`：`Commands()` 加 `catalog refresh/solve`、`pod new-raw`（老 `CmdPodNew` 解析移入）、`pod new <dir> [preset]`（→局内 catalog refresh→Solve→CreatePod；`catalog solve` 逐行打 `Ordered` 的 decision/Reason + Notes 的 Kind/Cause）、`adopt` 改 `Active()->Catalog->AdoptInto(...)`（raw 局打 `"adopt requires a catalog-backed pod"` + MarkFailed）、`file stage-manifest/install-manifest`（与二进制线同构：stage 读字节进 `StagedManifest`、install 写 `Snapshot/<sub>/plugin.json` 位；raw 局同样拒）、`swap` 内部 adopt 同走 catalog。`PrintAdoptReport` 行尾加 ` manifestVerified=`（**注意**：`VaseConsoleHotSwapAdoptReason` 钉 `reusedResidentImage=` 前缀行不受影响）。
- [ ] **Step 5: 回放资产重写**（`Tools/VaseConsole/CMakeLists.txt`）：`plan.txt`→留（`pod new-raw` 用）；新增 `VaseConsoleCatalogStage` fixture 测试（`copy_if_different` 三件：HelloPlugin.dll→`replay/catalog/VaseHello/HelloPlugin.dll`、`manifests/hello/plugin.json`→同目录 plugin.json、swap 影子沿用既有 `VaseConsoleSwap` fixture 机制扩一步）；`pod_cycle/behaviour/blocked` 各脚本改 `pod new ${REPLAY}/catalog`；**hotswap.txt** 重写为双 install（stage prime 字节 → install → stage `<TARGET_FILE_DIR 侧生成的 prime manifest>`——`file(GENERATE)` 生成 `prime_manifest.txt` 内容 = HelloPrime 逐字（displayName「示例插件（prime 版）」/Mood 默认"响亮"）→ `file install-manifest` → `adopt` → `get`）；`unknown_command` 等其余脚本 `pod new`→`pod new-raw` 一字之易。
- [ ] **Step 6: 新证人族**（同 CMake 文件，沿 ctest 4.4.3「WILL_FAIL 与 PASS_REGULAR_EXPRESSION 不可合并」的现成教训）：`VaseConsoleManifestMismatchIsFailure`（脚本：stage prime **只** install 二进制 → adopt → 拒；rc≠0）+ `VaseConsoleManifestMismatchReason`（钉 `manifest/binary mismatch`）+ `VaseConsoleSwapManifestSynced`（双 install → 通过，默认 rc0）+ `VaseConsoleSwapManifestSyncedReason`（钉 `响亮` + `manifestVerified=true`——后者顺带证 `PrintAdoptReport` 函数体没被清空，与兄弟用例 3 同族）+ `VaseConsoleRawAdoptRefused*` 一对 + `VaseConsoleSolveShowcaseReason`（钉 `skip kDisabled` 类输出行）。
- [ ] **Step 7: 全量 ctest + `ctest -R VaseConsole` 计数记录（基数增量进 T14 的表）+ 规矩 6 双平台**。
- [ ] **Step 8: 格式门 + Commit**：`git commit -m "M2b波2-T12：Console 双入口、KnownBinaries 退役与 adopt 单轨化（D69/D71/D85/D87/D88）"`

---

### Task 13: 文书义务（CLAUDE.md / wiki ×3 / 技能一行）

**Files:**
- Modify: `CLAUDE.md`（状态组各节 + 规矩 6 子串段 + 基数表——**基数行先占「T14 实测后填」**，其余全落）
- Modify: `wiki/vase-architecture.md`（§8 勘误清单七处，逐条就地挂 `[M2b波2 勘误（2026-09-25，spec …）]` 形）
- Modify: `wiki/vase-console-use.md`（命令表/双入口/清单两命令/退出码族/回放资产——它描述实有工具，T12 后的命令面就是它的真值）
- Modify: `.claude/skills/vase-cpp-engineering/references/abi-boundary.md`（一行：v3 现例 = FieldInfo choices 槽）

- [ ] **Step 1: CLAUDE.md**：项目状态行改「M2b 第二波完成（…清单五词…），M3 未起」；「磁盘上是这些」加 `ManifestExpectation.h`/`ChoiceCoerce.h`/Samples 两目录/`CatalogAdopt` 若无则不加；**规矩 6 括号段**：子串清单更新（`unknown plugin id` 移除、`request/expectation id mismatch` 与 `not in catalog snapshot` 入列；「provided by/unresolved declarations 不新增」段原样）。跑规矩 7 末尾自检 grep 一遍技能目录。
- [ ] **Step 2: wiki 七处勘误**（§3.3 version 限缩、§3.4 两字段免生成、§5.6 ①落点句、§12 判据 4 后半与 13/8 状态、§10 Samples 两目录、§13.3 静默点限缩、§3.3 加载期闸=读取处——各一句 + spec 指针，**不重排任何既有段**）。
- [ ] **Step 3: console-use 整改写**；`grep -n "pod new " wiki/vase-console-use.md` 等残留自查。
- [ ] **Step 4: Commit**：`git commit -m "M2b波2-T13：文书同步——状态行/规矩6子串/wiki 七勘误/console 文档整改"`

---

### Task 14: 收口全量验证与基数落账

- [ ] **Step 1:** `git ls-files -z … | xargs -0 clang-format --dry-run --Werror`（全树，新文件已在各任务 `git add` 过——再核 `git status` 无 untracked 源码）。
- [ ] **Step 2:** tidy 三线（`Scripts/linux-clang-tidy.sh` 与 `Scripts/win-clang-tidy.cmd clangcl` / `… msvc`；树须构建过——先 build 再 tidy；**三判据读法**见 CLAUDE.md：退出码 + 正文 error: 0 + 正文 warning: 0 + 摘要行对照，抑制数漂移要能归因）。
- [ ] **Step 3:** 六线删树重配全量：`Scripts/win-verify.cmd` 四树 + `Scripts/linux-verify.sh` 两树（`z-applocal` 假红 → 单线重跑再判）。
- [ ] **Step 4:** `ctest --preset <p> -N` 六线基数逐位核（现值 194/193/194/193/196/195 + 本波增量；**差值成因**按 spec §7 的「无一受门控」复核——若实际有平台门差异，回到 spec §7 修成因再来）。
- [ ] **Step 5:** 基数表落 CLAUDE.md（唯一真值处）+ 规矩 6 双平台证据位（引用 T12 Step 7 的输出或重跑一次记录）。
- [ ] **Step 6:** `Embedding play/loop 3/swapdemo` 手跑（Windows），肉眼对 `Mood` label 输出；Linux 侧 `wsl` 跑 `play`。
- [ ] **Step 7:** Commit：`git commit -m "M2b波2-T14：六线全量收口与基数落账（tidy 三线正文双 0）"`

---

## 偏离裁决记录（2026-09-25，需求方逐条裁可；已写回 spec 状态行注与相应 D 格）

1. **D77 的宏形**——✅ 裁可计划形：choices 表由**作者以 `inline constexpr std::array<ChoiceInfo, N>` 声明**（随枚举类型导出），`Meta{.Choices = ChoicesOf(表)}` 传引用；「enum 必带 / 非 enum 禁带」的配对执法移 `CheckedChoices<FieldT>` consteval。宏生成 per-field 数组不可行的成因（取件器在 Meta brace 逗号下无法判 4/5 元）记在 D77 格内。
2. **D67/D68/D84 的期望存储**——✅ 裁可**改判升级**：plan 初稿的 `mutable std::deque<ExpectationScratch>` 在写 T9 时自曝缺陷（Solve 可反复调用 → scratch 无上界增长或旧 Outcome 悬垂，二选一都是雷），改判为 **`ManifestExpectation` 拥有值形**（string/vector/optional<Storage> 成员，`LoadPlanEntry::Expected = std::optional<…>`）：零借用窗、plan 拷贝移动自包含，`AdoptRequest.Expected` 用指针纯为过渡双轨留位（终态 nullptr=Err）。D61 的 `Id` 借用窗**不受触及**。
3. **`LoadManifestFile` 命名**——✅ 裁可合并：单点读复用波 1 已公开的 `ParseManifestFile(path, subdirectoryName)`，不另立第二动词（法律唯一本就成立）。
