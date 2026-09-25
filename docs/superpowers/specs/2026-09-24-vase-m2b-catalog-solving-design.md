# Vase M2b 第一波：Catalog 与求解设计（spec）

**日期**：2026-09-24 · **状态**：已评审修订并经实施性核验——grilling 写回 D60–D66（两轮 11 问；其中 D61 拆掉初稿 §3 借用模型里的 MSVC SSO 悬垂雷） · **实施计划**：复审通过后由 writing-plans 另立 `docs/superpowers/plans/2026-09-24-vase-m2b-catalog-solving.md` · **分支**：`m2b`

**上游权威**：`wiki/vase-architecture.md` v3 的 §1（三层）/ §3.3–3.4（清单形态与权威性）/ §4.1–4.4 / §5.1 / §6.1–6.3 / §10 / §10.2 / §11.1 / §12（判据 4、7、8、13、14）/ §13.1 / §13.2；
`2026-09-23-vase-m2a-assembly-foundation-design.md` 的延期表与 D22 / D23 / D27 / D33 / D35 / D40 中预告给 M2b 的部分；
以及本分支 `456078d`（清单定名固定 `plugin.json`、每插件一个子目录——本文 schema 以它为唯一命名来源）。
历史决定 D1–D43 不改史，本文决定从 **D44** 续号。

**本文的地位**：M2b 拆两波（D44）。本文定死**第一波**（清单解析 + Catalog + Solve + Preset，纯元数据）；第二波（加载期比对 + Adopt 改接 + `enum` + bump + 前端）届时另立 spec，本文第 10 节只立边界，不预告其结论。凡本文与磁盘代码冲突处以磁盘为准（仓库惯例）；本文与架构文档冲突处以本文为准并回头给架构文档挂勘误（第 9 节清单）。

---

## 0. 范围

**第一波做**（新增一个库 target + 一个第三方依赖 + 测试；**`VasePod` / `VaseHost` 零行改动**）：

1. `nlohmann-json` 入 vcpkg manifest；`JSON_NOEXCEPTION` 形态（D55）。
2. `VaseCatalog` target：`Source/Catalog/` 四 TU、`Include/Vase/Catalog/` 四头、`VASE_CATALOG_API`（§1）。
3. `PluginCatalog::Refresh()`：扫 `*/plugin.json`、解析、Id 查重、事务性快照（§3，D50、D58）。
4. `plugin.json` schemaVersion = 1 定形与执法（§4，D48、D49）。
5. `LoadPreset()` 结构解析 + `Preset` 值形态（§5，D59）。
6. `PluginCatalog::Solve()` → `SolveOutcome{LoadPlan, Notes}`：逐格语义、确定性与执法（§6，D46、D47、D51–D53、D60、D62、D63、D66）。
7. 测试：Unit 两簇 + Integration 三簇，含「Solve 产计划喂 `CreatePod`」端到端证人（§8，D57）。

**第一波不做**（各归其位，不许顺手）：

| 不做 | 归属 |
|---|---|
| 清单↔二进制逐字段比对、`schemaVersion` 的加载期闸 | M2b 第二波 |
| `AdoptPlugin` 就地重读清单、`KnownBinaries` 代偿表（`PluginHost.cpp` 注释预告的「连注释一起删」） | M2b 第二波 |
| 清单所指 `binary` 文件的存在性检查（Catalog 不碰二进制，§4.4） | M2b 第二波（加载期天然覆盖） |
| `enum` 配置型与 choices schema、`FieldInfo`/`Value` 布局、`kHeaderVersion` bump（D22） | M2b 第二波 |
| VaseConsole 改接、`Samples/DependentPlugin` / `FailingPlugin`（D19） | M2b 第二波 |
| `VaseCli scan/validate/plan`（其清单消费面即本波的 Catalog） | M5 |
| Host 装配预检的任何改动 | 保留原样（D56），无波次移动 |
| §3.3 清单例里的 `enabledByDefault` / `binary` 进 `PluginMeta`（描述符无此二槽，「全部由代码生成」对这两字段暂不成立） | M2b 第二波随 bump 一并定夺 |

---

## 1. 决定表

| # | 决定 | 理由 |
|---|---|---|
| D44 | M2b 拆两波：第一波纯元数据（清单/Catalog/Solve/Preset），第二波加载期接入 + `enum` + 前端 | 两波验证面不同：第一波 Unit/Integration 验穿、规矩 6 不触发；第二波每步带 HotSwap 双平台义务。混波把回归窗口放到最大 |
| D45 | `VaseCatalog` 链 `VaseHost`（→ `VasePod`）；否决「把 `LoadPlan`/`ConfigBlob` 下沉 `VasePod`」 | §10.2 只给了 target 名没给链接边。`Solve` 产定形 `LoadPlan`（含 `VASE_HOST_API` 的 `ConfigBlob`），类型搬家 = 重开 D23，且「JSON 只落 Catalog 层」约束的是核心库二进制不带 JSON，不是层序高低 |
| D46 | `LoadRequest` **无** `PluginDirectory`；目录属主是快照（`Refresh` 参数） | 双源会漏穿「同一次计算」的快照一致性论证。§5.1 提议形与 §4.1「两种输入」句挂勘误（第 9 节） |
| D47 | `Solve` 返回 `Result<SolveOutcome>`，`SolveOutcome{ LoadPlan Plan; std::vector<SolveNote> Notes; }` | §4.1 既定的 warn（未知 Id/key）必须可达调用方；`LoadPlan` 是 M2a 定形不塞诊断；out-param 与「Catalog 存上次 notes」均劣于返回值自带 |
| D48 | `plugin.json` schemaVersion = 1 字段形定形（§4 表） | 必填仅 `schemaVersion` 与 `id`，其余全有缺省；§3.3 提议文的兑现与收窄 |
| D49 | 清单 unknown 字段一律拒绝；「minor 容忍」（§3.3）指**格式世代**方向，不要求旧 parser 容纳新语法 | parser 随库版本独一，scan/host/validate 同一实现（§11.1「同一套规则」）。波 2 的 `enum`+`choices` 不 bump major，只需同一份 parser 认识它；「旧宿主读新清单」无使用场景 |
| D50 | 执法不对称：单个坏清单 → `Refresh` 整体 error；子目录缺 `plugin.json` → 跳过 + `CatalogWarning`；Id 重复 → error | 坏清单静默出局会让「插件没参与」伪装成「插件不在集合里」，扭曲 §4.4 静态预测承诺；缺清单的编译残留/外来目录是常态，不该响 |
| D51 | Preset 执法表：未知插件 Id、未知 config key → **warn**（`SolveNote`）；类型不符、越 `min/max`、非加宽数值、major 不符 → **error 不出计划** | 前两保 §4.1 的「没人怕删插件」；后四为真漂移，§5.1「清单损坏」同类。D35 的越界执法落在此点，用清单随带 schema 判定（Catalog 不需要二进制） |
| D52 | optional 边**参与拓扑定序**、不参与跳过判定；纯 optional 环 → error | 声明 `optionalRequires` 即声明「在场则先行」；在场却后到 = `TryGet` 薛定谔的 null。确定性优于运气。纯 optional 环在场时双方都不愿先行，无合法序可言 |
| D53 | 计划确定性：Kahn + 同层 Id 字典序；`kSkip` 条目尾部按 Id 序追加、`ResolvedConfig` 置空 blob | §12 判据 4 的逐项比对要求计划可字节比较；跳过条目的配置预览该走 `ManifestView`，不该借道计划 |
| D54 | `BinaryPath = 快照目录 / 子目录名 / LibraryFileName(binary stem)`；`LibraryFileName` 住 `Source/Catalog/Detail`（`WIN32 ? ".dll" : ".so"` 字符串拼接） | 磁盘上无现成 helper（Host 全线收完整路径）。§13.1「平台差异收口在 Loader」指**加载行为**；文件命名是纯字符串，挂一条勘误划界（第 9 节），不构成第二处收口破口 |
| D55 | `nlohmann-json` 入 `vcpkg.json` `dependencies`，`builtin-baseline` **不动**；`JSON_NOEXCEPTION` + `parse(…, nullptr, false)` + `is_discarded()`；json 类型零出现于 `Include/`（解析封在 `Source/Catalog/*.cpp`） | baseline 一动连带 gtest 版本漂移，六线全震。公开头零 json 让「JSON 只落 Catalog 层」在包含面上可 grep 验证 |
| D56 | Host 装配预检（D27/D33）与 Adopt 执法（D43）原样保留；「不加载二进制的提前判定」由 `Solve` 在元数据层承担 | M2a「提前到装载前随 M2b 清单自然成立」的兑现形态说明：`CreatePod` 仍可吃手写计划（D12 双生产者），Host 的信息源仍是二进制，预检不得撤。规则同源（等值比对/碰撞/缺失）、判定面不同（参与集 vs 活集合），非重复执法 |
| D57 | 端到端证人用例进 Integration：给既有 fixture DLL 手写清单 → `Refresh` → `Solve` → `CreatePod` → 装载序断言 → `DestroyPod`；Host 零改动 | 「Solve 产同形计划、Host 无感」是 D12 零返工的直接证据，且是 §12 判据 4 本波可交的那一半（另一半「注入运行时失败后断言分歧恰在那」随波 2/3） |
| D58 | `Refresh` 事务性：新快照构建成功前不动旧快照；失败 → 返回 `Err`，旧快照原样可用 | 编辑器在报错后还要能看着上一张表干活；「清一半」是最坏形态 |
| D59 | 校验两段制：`LoadPreset` 只做**结构与语法**（JSON 合法、形状对、major 闸）；类型核对、加宽、越界在 `Solve` 内做（那里才有清单 schema） | Preset 不可能脱离清单独立定类型（§4.1 增量语义：没提到 = 默认值）。`Preset` 值形态里的数值以 JSON 原样（int64/double）入 `ConfigBlob`，`Solve` 按字段 `Kind` 收窄 |
| D60 | `LoadRequest` 增 `std::span<const ServiceRef> HostProvided`（复用 `PluginDescriptor.h` 的公开 POD，借用窗 = `Solve` 调用期，与 `CreatePod` 对 `Id` 的契约同律）：② 按已满足、③ 按潜在提供方参与碰撞判定 | 宿主经 `Stage0` 注册服务是实有形态（`EdgeConsumer` 在磁盘上就 `Requires` 宿主服务），Solve 看不见即**静态误跳**；声明面比「Catalog 理解宿主」便宜得多，且「工具与运行时同规则」（§11.1）照吃同一份声明 |
| D61 | `Plan.Ordered[i].Id` **借用快照字符串**（构建后只读）；`SolveOutcome` 不拥有任何被借字段；借用窗 = 到下一次 `Refresh` | 初稿的「Outcome 自持 Id 拥有表」是 SSO 悬垂雷：MSVC 短串（`"Vase.Combat"` 11B < 15B 阈值）存身 string 本体，`Result<SolveOutcome>` 的 move 无声挪走 `string_view` 指向的字节、没有任何东西报警。快照借用无自借用、move 安全，窗口纪律与 §4.4 快照语义同线 |
| D62 | ② 闭包对**非平凡跳过**（提供方存在但被禁/被跳；同名版本不符）追记 `SolveNote`；`SolveNote{Kind, PluginId, Key, Cause, Message}`，`Kind` 四值枚举（§3） | §4.4「取消勾选 Physics 将导致 Combat、Vfx 被跳过」是 Solve 的公开卖点，让每个消费者重推闭包 = 要防的「两次各算一遍」；断言走 `Kind+Cause` 字段、不走消息子串（M2a 收口同一条纪律） |
| D63 | Catalog 记「已扫描」标记；首扫前 `Solve` → `Err`（空目录经 `Refresh` 后正常出空计划） | 「没喂数据就出空结果」与 `ctest` 零测试返回 0 同族，本仓静默陷阱清单上的老熟人 |
| D64 | 解析硬化一揽：`binary` 只许**纯文件名**（无分隔符、无 `..`、非空）；数组内 `(service,version)` / `config` 的 `key` 重复 → 拒；`min > max` → 拒；服务 `version ≥ 1`（`0` 拒；全仓现有声明无一为 0，关门零震动）；`schemaVersion` 只收 JSON 整数形（`1.0` 拒）；`Overrides` 重复 Id → `Solve` Err；`Refresh` 失败时快照与 `Warnings()` **都不换**（D58 一贯）；JSON 对象内重复键不特判（nlohmann 后值覆盖语义） | 清单是生成物不是人写物，倒挂/重复本身说明上游坏了；`BinaryPath` 原样喂 `CreatePod`，路径字符不闸即爬出子目录；重复键检测要上 parser callback、成本不成比例 |
| D65 | D57 证人沙箱复用 `HotSwapLoopTests` 的 temp 装置（`temp_directory_path` + `copy_file` + 析构 `remove_all`，声明序契约照抄）；**同 Id 对（`Hello*`、`VersionedA*`）不入同一沙箱**——顺带成为 `CatalogScanTests` 的 D50 查重负例材料；名单含 `EdgeConsumer`，`HostProvided` 真喂 | 现有 fixture 全在 bin/ 平铺（不可改，CLAUDE.md 钉死），新放置要求每插件一子目录；复制时机在加载之前，Windows 文件锁不参与 |
| D66 | Preset 执法（类型/越界/未知 key）作用域 = **快照内全量插件**，与本局参与与否无关 | 类型错是真漂移，不该等插件复活才响；且编辑器里勾来勾去，报错集合不该随勾选抖动 |

---

## 2. Target 与文件落位

```text
Include/Vase/Catalog/          （新，公开头——零 json 类型，D55）
├── PluginCatalog.h            快照、Refresh、Solve、只读视图
├── LoadRequest.h              LoadRequest、PluginOverride
├── Preset.h                   Preset 值形态 + LoadPreset()
└── ManifestView.h             ManifestEntry / ManifestConfigField / CatalogWarning / SolveNoteKind / SolveNote / SolveOutcome

Source/Catalog/                （新 target：VaseCatalog，SHARED）
├── PluginCatalog.cpp          Refresh、扫描、快照、查重
├── ManifestJson.cpp           plugin.json → Manifest（唯一读 json 的 TU 之一）
├── PresetJson.cpp             Preset 文件 → Preset
├── Solve.cpp                  八步求解
└── Detail/LibraryFileName.h   （内部）平台库文件名字符串拼接（D54）
```

链接与构建立面：

- `VaseCatalog` 链 `VaseBuildOptions` + `VaseHost`；`nlohmann_json::nlohmann_json` 为 **PRIVATE**（规矩 1：新 target 必链 VaseBuildOptions）。
- `Detail/Export.h` 增 `VASE_CATALOG_API`（`VaseCatalog_EXPORTS` 定义方对称于现有两宏）。
- `vcpkg.json`：`dependencies += "nlohmann-json"`；baseline 不动（D55）。
- 导出类带 STL 成员（快照 vector、`Preset` 的 vector）：吃规矩 1 全局 `/wd4251` 的既有代价，宿主面站点数增加，已知接受。
- 命名空间：全部住 `vase::`（库面，不镜像目录段；与 `Tools` 层的 `tools::` 约定不同）。
- 可见性：库 target 按 VaseHost 同法（导出宏控制面），不走 `vase_add_plugin_fixture`——那不是插件（规矩 5 只管插件形态）。
- 测试接线：`Tests/CMakeLists.txt` 新目录不需要（文件落既有 `Tests/Unit`、`Tests/Integration`），测试 target 增链 `VaseCatalog`。

## 3. `PluginCatalog` 公开 API（形状）

```cpp
// Include/Vase/Catalog/LoadRequest.h
struct PluginOverride
{
    std::string           Id;            // Preset 条目与临时覆盖共用此形（§4.1「同一种数据结构」）
    std::optional<bool>   Enabled;       // nullopt = 未提及
    ConfigBlob            Config;        // 只存增量；数值按 D59 以 JSON 原样入
};

struct LoadRequest
{
    std::optional<Preset>           Preset;
    std::span<const PluginOverride> Overrides;    // 本局临时覆盖；同 Id 的 key 覆盖 Preset 层；重复 Id → Err（D64）
    std::span<const ServiceRef>     HostProvided; // 宿主声明「本局 Stage0 会注册哪些 {service,version}」（D60）；
                                                  // ServiceRef 复用 PluginDescriptor.h 公开 POD，借用窗 = 本次 Solve
};

// Include/Vase/Catalog/Preset.h
class VASE_CATALOG_API Preset   // 值语义，拥有自己的条目
{
public:
    [[nodiscard]] std::uint32_t SchemaVersion() const;
    [[nodiscard]] const std::string& DisplayName() const;
    [[nodiscard]] const std::vector<PluginOverride>& Entries() const;   // Id 字典序（nlohmann 对象 = std::map）

private:
    friend Result<Preset> LoadPreset(const std::filesystem::path&);
    std::uint32_t Version = 1;
    std::string Name;
    std::vector<PluginOverride> Items;
};

VASE_CATALOG_API Result<Preset> LoadPreset(const std::filesystem::path& file);   // 仅结构/语法（D59）

// Include/Vase/Catalog/ManifestView.h —— 快照元素：拥有型（成员自持值）；
// 其中 Value 的 kString 借用同条目内的拥有串——**自借用，对外只经 Find 取指针、不再复制**
struct ManifestDependency    { std::string Service; std::uint32_t Version; };
struct ManifestConfigField
{
    std::string Key;
    ValueKind   Kind;        // 六型（§4）；enum 波 2 开闸
    Value       Default;
    Value       Min, Max;    // kNone = 未设
    std::string Display;
    std::string DefaultsRaw; // Default/Min/Max 的拥有存储
};
struct ManifestEntry
{
    std::string Id, DisplayName, Version, Subdirectory, Binary;
    bool EnabledByDefault = true;
    std::vector<ManifestDependency> Requires, OptionalRequires, Provides;
    std::vector<ManifestConfigField> Config;
};

struct CatalogWarning { std::string Subdirectory; std::string Message; };   // Refresh 期非致命诊断

enum class SolveNoteKind : std::uint8_t                                        // 断言走 Kind+Cause 字段，不走消息子串（D62）
{
    kUnknownPluginId,         // Preset/Overrides 引用快照不存在的 Id
    kUnknownConfigKey,        // 覆盖的 key 不在该插件清单 config 里
    kProviderSkipped,         // 非平凡跳过：提供方存在但被禁/被跳（Cause = 该提供方 Id）
    kVersionMismatchProvider, // 同名仅主版本不符在场（Cause = 在场者 Id）
};
struct SolveNote { SolveNoteKind Kind; std::string PluginId; std::string Key; std::string Cause; std::string Message; };

struct SolveOutcome
{
    LoadPlan               Plan;   // 条目 Id 是 string_view，借用 Catalog 快照（D61）——Outcome 自身无被借字段，move 安全
    std::vector<SolveNote> Notes;
};

// Include/Vase/Catalog/PluginCatalog.h
class VASE_CATALOG_API PluginCatalog
{
public:
    Result<void> Refresh(const std::filesystem::path& pluginDirectory);   // 事务性：失败时快照与 Warnings 都不换（D58、D64）
    Result<SolveOutcome> Solve(const LoadRequest&) const;                 // 纯函数，吃当前快照；首扫前 → Err（D63）

    [[nodiscard]] std::vector<std::string> Ids() const;                   // 值拷贝，字典序（D53 同源纪律）
    [[nodiscard]] const ManifestEntry* Find(std::string_view id) const;   // 借用快照，下次 Refresh 失效；无则 nullptr
    [[nodiscard]] const std::vector<CatalogWarning>& Warnings() const;    // 本轮 Refresh
};
```

**拥有与借用（契约，写进头注释）**：`Plan.Ordered[i].Id` 与 `Find` 返回的 `ManifestEntry*` 同源——都借用**快照**字符串（快照构建后只读，D61）；借用窗 = 到下一次 `Refresh`。`SolveOutcome` 自身不拥有任何被借字段（move/copy 皆安全），但把 `Plan` 带过下一次 `Refresh` 使用即悬垂——头注释与证人用例都按这条窗写。`ServiceRef`（`HostProvided`）反向：借调用方的串，窗 = 本次 `Solve` 调用（`CreatePod` 对 `Id` 的同一条律）。

**线程**：`PluginCatalog` 不在 `PluginHost` 的线程绑定契约（§1.4）范围内——它是宿主/工具自持的普通对象，无内部锁；契约随宿主用法（同一线程顺序调）。头注释明写，防「Catalog 也绑线程」的误读。

## 4. `plugin.json` schema（schemaVersion = 1）

放置（`456078d` 定形）：`<宿主插件目录>/<子目录>/plugin.json`，与 DLL 同目录即配对；子目录名仅参与 `binary` 缺省推导，与 Id 无约束关系。

| 字段 | 必填/缺省 | 校验 |
|---|---|---|
| `schemaVersion` | 必填，整型 | major ≠ 1 → 拒；只收 JSON 整数形（`1.0` 拒，D64） |
| `id` | 必填，字符串 | 非空；快照内唯一（D50 查重 error） |
| `displayName` | 缺省 = `id` | — |
| `version` | 缺省 = `""` | 纯展示（§3.3），不校验内容 |
| `binary` | 缺省 = **子目录名** | 非空、**纯文件名**（含路径分隔符或 `..` → 拒，D64）；stem，不含扩展名（D54） |
| `enabledByDefault` | 缺省 `true` | bool |
| `requires` / `optionalRequires` / `provides` | 缺省 `[]` | 条目恰 `{service, version}` 双必填；version 整数且 **≥ 1**（`0` 拒，D64）；同数组内 `(service,version)` 重复 → 拒（D64） |
| `config` | 缺省 `[]` | 条目 `key`/`type`/`default` 必填，`min`/`max`/`displayName` 选填；`type` ∈ `bool` `int32` `int64` `float` `double` `string`；`default` 必与 `type` 同型（数值加宽仅 `int → float/double` 允许）；`min/max` 仅数值型可出现、与 `type` 同型、`min ≤ max`（倒挂拒，D64）；`key` 重复 → 拒（D64） |

- **unknown 字段（任何层级）→ 拒**（D49）；诊断消息格式：`plugin manifest "<完整文件路径>": unknown field "<json pointer>"`。
- `type: "enum"` 等未来值域按 unknown 值拒（波 2 开闸，D49）。
- 根目录散文件忽略；无 `plugin.json` 的子目录 → `CatalogWarning` 跳过（D50）。
- JSON 对象内**重复键不特判**（nlohmann 后值覆盖语义；检测要上 parser callback，成本不成比例——D64）。

规范样本（测试 fixture 的写法基准）：

```jsonc
// VaseCombat/plugin.json
{
    "schemaVersion": 1,
    "id": "Vase.Combat",
    "requires": [ { "service": "Vase.World", "version": 1 } ],
    "provides": [ { "service": "Vase.DamageSystem", "version": 1 } ],
    "config": [
        { "key": "criticalMultiplier", "type": "float", "default": 2.0,
          "min": 1.0, "max": 10.0, "displayName": "暴击倍率" }
    ]
}
```

## 5. `Preset` schema 与合并

```jsonc
// Presets/Client.preset.json —— §4.1 提议形的定形：schemaVersion 与 overrides 必填，displayName 选填
{
    "schemaVersion": 1,
    "displayName": "客户端",
    "overrides": {
        "<plugin id>": { "enabled": false },
        "<plugin id>": { "config": { "<key>": <标量> } }
    }
}
```

- `overrides` 的值对象只允许 `enabled`（bool）与 `config` 两键；`config` 值域 = JSON 标量，**嵌套对象/数组 → 结构错**（`LoadPreset` 即拒，§4.2 禁嵌套的兑现）。
- `LoadPreset` 判：JSON 语法、`schemaVersion` major、形状。数值此处以 JSON 原样（int64/double）入 `ConfigBlob`，不定型（D59）。
- 类型核对、加宽收窄、`min/max` 越界、未知 Id/key：全部在 `Solve` 中按清单 schema 判，产 error 或 `SolveNote`（D51 表）。
- 三层应用序 = `enabledByDefault → enabled(preset) → enabled(override)` 与 `清单 default → preset.config → override.config`，合并即 `ConfigBlob::MergeShallow` 逐 key 整体替换（M2a 现成件，§4.3）。
- 执法作用域 = 快照内**全量插件**，与本局参与与否无关（D66）：被跳插件的覆盖类型错照样响。

## 6. `Solve` 语义（八步）

```text
前置       首扫前调用 → Err（D63）；Overrides 同 Id 两条 → Err（D64）
① 参与判定   enabled = 三层合并后的最终值；false 者 → kSkip[kDisabled] 入计划尾部
② 硬需求闭包 参与集内，requires{name,ver} 的提供方须「在 HostProvided 中」或「在快照中且自身将 kLoad」：
             无提供方 / 提供方被禁或被跳 → kSkip[kMissingDependency]
             同名仅主版本在场 → kSkip[kVersionMismatch]
             （ServiceRef.Version 即主版本单数，等值比对，§6.1；HostProvided 恒满足且不入环、不定序——
              Stage0 先于装载循环，宿主服务无插件节点，D60）
             迭代至不动点；optionalRequires 不产生跳过（§6.3）
             非平凡跳过（提供方存在但被禁/被跳；同名版本不符）→ 各追 Note，Kind+Cause（D62）
③ 碰撞       参与（kLoad）集内两 provide 同 {name,ver} → error，不出计划（§6.3「不静默择一」）；
             kLoad 的 provide 撞 HostProvided 同 {name,ver} → 亦 error（宿主同样先占位，D60——手写计划走
             Host 预检那条路不受影响，D56）
④ 循环       硬边成环，或纯 optional 边成环（D52）→ error，消息点名环成员
⑤ 拓扑       Kahn，边集 = 硬 ∪ optional；同层 Id 字典序（D53）
⑥ 配置       kLoad：ResolvedConfig = 三层合并后的全量 blob（含清单 default——D23「默认已并入」的清单侧兑现；
             二进制侧「缺字段回退 kFields」原样成立，两侧不冲突）
             kSkip：空 blob（D53）
⑦ 路径       BinaryPath 按 D54 拼；文件存在性不查（第 0 节）
⑧ 输出       SolveOutcome{Plan, Notes}；kLoad 段拓扑序在前，kSkip 段 Id 序在后；
             空快照 / 全禁 → 空计划非 error
```

Notes 的产生点 = `SolveNoteKind` 四值（D62），全部 warn、不拦求解：`kUnknownPluginId`（Preset/Overrides 引用快照不存在的 Id）、`kUnknownConfigKey`（覆盖 key 不在该插件清单 `config` 里）、`kProviderSkipped` / `kVersionMismatchProvider`（② 的非平凡跳过，`Cause` 点名在场者）。作用域按 D66 全量跑。

## 7. 错误与诊断通道

- 全部显式返回值（`Result<T>`；规矩 2：**零** `EXPECT_THROW` 一族可用，测试按返回值断言）。
- `Refresh` 的 `Err`：消息含**完整文件路径**与 JSON pointer（坏清单）、或两个 Id 冲突者的路径对（查重）。`Error::Context.PluginId` 可定位时填。
- `Solve` 的 `Err`：碰撞/环/类型/越界/major——消息点名插件与字段；不出计划（§5.1）。
- `SolveNote` / `CatalogWarning`：非致命，走结构字段不走消息子串——**不为它们新增任何消息子串匹配**（M2a 收口判据迁移的同一条纪律，规矩 6 括号段）。
- 消息文本语言与现有 Host 一致（英文正文），测试断言按字段与子串**最小区分集**。

## 8. 测试分层与验收面

| 簇 | 层 | 覆盖 |
|---|---|---|
| `ManifestJsonTests` | Unit | §4 逐执法线：必填缺失、类型不符、加宽允许/拒绝、unknown 顶层/条目字段、`enum` 拒、缺省填充（displayName/binary/enabledByDefault/各数组）、min/max 仅数值、坏 JSON（`is_discarded` 路径）；D64 硬闸逐条：`version:0`、`schemaVersion:1.0`、`binary` 含分隔符/`..`、重复 `(service,version)`、重复 `key`、`min>max` |
| `PresetJsonTests` | Unit | §5 结构线：语法、major、嵌套拒、原样数值入 blob（D59） |
| `CatalogScanTests` | Integration | temp 目录真 FS：`*/plugin.json` 枚举、无清单子目录 warn+跳过、根散文件忽略、坏清单 error（D50）、Id 查重 error（用刻意同 Id 对做材料，D65）、`Refresh` 失败旧快照与旧 Warnings 都不换（D58+D64a）、空目录空快照、`Ids()` 字典序、`Find`/`Plan` 借用到下次 `Refresh` 失效（D61） |
| `SolveTests` | Integration | §6 逐格：三层 enabled、级联跳过闭包、版本不匹配、碰撞 error、硬环/纯 optional 环点名、optional 定序（在场先行）、双跑计划逐字节等（D53）、三层配置合并、skip 空 blob、BinaryPath 平台断言（`#ifdef` 两侧各钉）、全禁空计划、Notes 四 Kind（**断言走 Kind+Cause 字段**，D62/D21 同纪律）；`HostProvided` 满足 requires / 撞 provide 两格（D60）、首扫前 Err（D63）、Overrides 重复 Id Err（D64）、被跳插件的类型错照样响（D66） |
| `AssemblyFromSolveTests` | Integration（D57 证人） | temp 沙箱（HotSwapLoop 范式，D65）：复制 fixture DLL + 落清单成「每插件一子目录」→ `Refresh` → `Solve`（含 `HostProvided = {HostOnly v1}`，名单含 `EdgeConsumer` 真喂）→ `CreatePod` → 断言实际装载序 == 计划拓扑序 → `DestroyPod` 计数归零；同 Id 对不入同沙箱 |

- 清单静态测试数据落 `Tests/Integration/fixtures/manifests/` 子树（每插件一目录，作沙箱 staging 的字面材料）。**不改**任何既有插件 fixture 源码与 CMake 落位。
- **不触发**规矩 6：Loader / 账本 / Eject / Adopt / 描述符 / `HeaderVersion` 零改动——验收记录里明写这句。
- 环境义务：vcpkg 新依赖 + 新 target → **六线删树重配全量**（`win-verify.cmd` + `linux-verify.sh`；`z-applocal` 假红先单线重跑再判）；tidy 三线各跑（TU 基数 77/77/78 起跳，实际值验收记）；新文件先 `git add` 再过 format 门。

## 9. 文书义务（验收时同步，规矩 7：只住 CLAUDE.md 的真值不复制进技能）

- **CLAUDE.md**：技术前提/项目状态行（M2b 第一波完成、第二波未起）；「磁盘上是这些」清单增 `Include/Vase/Catalog/`、`Source/Catalog/`、`VaseCatalog`；第三方依赖两条入口段补 nlohmann（vcpkg 侧）一句；六线基数表更新；§12 判据表若本波钉的 13/14 行有推进同步措辞。
- **wiki 勘误回挂（4 条，实施落地时）**：§4.1「Solve 同时接受路径与已解析 Preset」→ 改述为路径消化在 `Refresh`/`LoadPreset`，且提议形 `LoadRequest` 增 `HostProvided` 声明面（D46、D60）；§4.4/§5.1 `Solve` 提议签名 → `Result<SolveOutcome>`（D47）；§3.3「minor 容忍」读法 → 格式世代方向（D49）；§13.1「平台差异收口 Loader」→ 加「文件命名除外」一句（D54）。
- **技能**：不搬运任何字面基数/文件名清单（规矩 7 现行口径）。

## 10. 与第二波的边界（只立界，不预告结论）

加载期逐字段比对（含 §3.3 两版本号的加载期闸）、`AdoptPlugin` 就地重读与 `KnownBinaries` 退役（`PluginHost.cpp:872` 注释连身删）、`enum`+choices+`kHeaderVersion` bump（D22）、`PluginMeta` 是否收 `enabledByDefault`/`binary`（第 0 节末行）、D19 前端面（Console 改接、两个新 Sample）、§12 判据 4 的「注入运行时失败断言分歧恰在那」半句。本波交付后它们的形状**只受本文约束的接口是 `LoadPlan` 与 `SolveOutcome`**，其余自由。
