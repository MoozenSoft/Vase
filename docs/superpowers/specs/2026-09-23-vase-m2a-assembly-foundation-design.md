# Vase M2a 装配地基设计（spec）

**日期**：2026-09-23 · **状态**：已评审修订并经实施性核验——grilling 写回 D33–D43；D27/D33 两处按「M2a 无清单、声明住二进制里」的现实改为两态形（原「加载前查声明」不成立，M2b 清单到位后复原提前形态）· **实施计划**：`docs/superpowers/plans/2026-09-23-vase-m2a-assembly-foundation.md` · **分支**：`m2a-assembly-foundation`

**上游权威**：`wiki/vase-architecture.md` v3 的 §3.2 / §4.2 / §4.3 / §5.1 / §5.2 / §5.5 / §5.6 / §6.2 / §6.3 / §12.2（判据 3b、4、5）/ §13.2，
以及 `docs/superpowers/specs/2026-09-14-vase-m0-m1-design.md` §4 的里程碑序列。历史决定 D1–D17 不改史，本文决定从 **D18** 续号。

**本文的地位**：M2 拆两波（D18），本文定死第一波（M2a）。M2b（Catalog 层）届时另立 spec；
本文第 12 节只预留接口，不预告其结论。凡本文与磁盘代码冲突处以磁盘为准（仓库惯例）；
本文与架构文档冲突处以本文为准并回头给架构文档挂勘误（第 11 节清单）。

---

## 0. 范围

**M2a 做**（全部落在库本体 + 测试，不新增第三方依赖、不新增库 target）：

1. 配置类型系统：POD 面（`Value` / `FieldInfo` / `ConfigInfo` / `VASE_CONFIG` / `FieldsOf`）+ 拥有面（`ConfigBlob` 与浅合并）。
2. 描述符变更：`PluginMeta` 增 `OptionalRequires` 与 `Config` 槽；`kHeaderVersion` 1 → 2。
3. `LoadPlan` 定形：§5.1 提议形 + `BinaryPath`；`CreatePod` 消费定形（D12 的"装配路径零返工"兑现）。
4. 多插件装配改造：计划结构性校验（D33/D39）、装配预检与归因（D41）、加载期级联、`OnStart` 失败的递归拆除（判据 5）、`Strict` 完整形（D36）、静态/运行时跳过的分开呈现、Adopt 双向声明执法（D43）。
5. 3b 薄面：Eject / Adopt 的执法拒绝与解析记录结构化（D20、D21）。
6. 「凭声明」执法扩展覆盖 `OptionalRequires`；Adopt 配置 = Pod 回放表（D34）。
7. `Samples/Embedding` 适配新形状；测试按第 9 节分层落。

**M2a 不做**（各归其位，不许顺手）：

| 不做 | 归属 |
|---|---|
| `.plugin.json` / Preset / `nlohmann/json` / `VaseCatalog` target / `Solve` / 清单↔二进制逐字段比对 / Adopt 就地重读清单的接口缝 | M2b |
| `enum` 配置类型（choices 的 schema 无处安放，§4.2 禁数组） | M2b 与 schema 同定（D22） |
| VaseConsole 扩展、`Samples/DependentPlugin`、`Samples/FailingPlugin` | M2b 前端改接时一并（D19） |
| 3a 多插件 Eject/Adopt 全循环回归、叶判定图反查的新华 | M3（§12.3 明文） |
| 进程级状态登记（`.ProcessState`）、完整属主追踪器、3d | M3 |
| `LoadRequest`、Catalog 快照、`Refresh()` | M2b |
| Waterfall、`CreateScope` 子作用域 | 不随 M2——§3.6 的 M1 不做清单对 M2a 仍然有效 |

## 1. 为什么拆两波（D18）

M2 的双源定义合起来约六块咬合的工作：配置反射、`ConfigBlob` 合并、清单解析、`Solve`、装配改造（级联+比对+Adopt 缝）、3b 结构化。
按 D1（纵切：最危险的先验证）排内部依赖：

- **最危险的是装配改造**——§12.2 判据 5 那条"过不了就得把 epoch 请回来"的递归拆除，与 JSON 机制毫无关系，
  不该被解析层的迭代速度拖住；
- **`ConfigBlob` 与浅合并是 `Solve` 的依赖品**（§4.2：合并在 blob 层、不依赖 JSON），纯数据可先做先测穷举；
- **JSON 层是低风险收尾**——纯函数 + 文件 I/O，晚做不增加风险。

两波之间以「`Solve` 产出 M2a 定形的 `LoadPlan`」为干净接口：M2b 不改装配机器，M2a 不等 JSON 依赖。
需求方 2026-09-23 选定此切法（备选"一份总计划"与"Catalog 先行"均弃）。

## 2. 决定汇总表

| # | 决定 | 理由（一句） |
|---|---|---|
| D18 | M2 拆两波：M2a 装配地基（不碰 JSON）→ M2b Catalog 层 | 最危险的递归拆除先行验证；两波接口 = 定形 LoadPlan |
| D19 | M2a 验证面以测试为主；`Embedding` 仅适配形状；console / 新 Samples 留 M2b | plan 格式改一次而不是两次；测试层级才是门禁 |
| D20 | 3b 外向面取薄：只结构化判定报告，不开常驻查询 API | YAGNI——console/编辑器 M2b+ 真有消费者再扩 |
| D21 | 执法性拒绝走 `Ok + Status` 结构化报告；环境/身份类拒绝照旧 `Err` + 子串契约 | §5.6"拒绝且点名"要可枚举；身份类失败人读一句话就够，迁移面收窄 |
| D22 | `enum` 配置类型留 M2b；`FieldInfo` 不做 speculative 预留 | 未定案的东西不焊进对外契约布局；M2b 若要动布局再 bump（两次递增都发生在 M2 内，外部无债） |
| D23 | `LoadPlan` 定形 = §5.1 提议形 + `BinaryPath`；`ResolvedConfig` 缺字段回退 `kFields` 默认 | 提议形没给 Host 二进制在哪；"默认已并入"的运行期形态 = 按字段回退 |
| D24 | 配置分 POD 面（`Include/Vase/Config/`，header-only）与拥有面（Host 的 `ConfigBlob`） | 同一个 `Value` 要同时活在插件镜像的 constexpr 表与宿主运行时合并里，两侧需求相反（平凡拷贝 vs 拥有字符串） |
| D25 | 配置对象的造/毁走 `ConfigInfo` 的镜像内配对工厂；`Context::Config<T>()` 校验 Size/Align 后 `static_cast` | 与 `Plugin` 工厂同一理由：跨 DLL 不过 CRT 边界；类型误用当场终止、两构建一致 |
| D26 | `PluginMeta` 增 `OptionalRequires` + `Config`；`kHeaderVersion` 1 → 2 | 布局变了就 bump，§8.3 前提的唯一执行点不许打折 |
| D27 | 装配预检放阶段 1 循环内、`InspectBinary` 之后 `MakeInstance` 之前（M2a 无清单，声明住二进制里；「不加载二进制」随 M2b 清单提前）；加载期级联 = 预检在拓扑序循环上的重复应用 | 上游死了服务就没注册过，下游预检自然接住——不需要显式图遍历；检查器 M2b `Solve` 复用 |
| D28 | 递归拆除波及集 = strict 声明边 ∪ 账本边的传递闭包；按数组序倒序拆；空壳留 `Instances`、镜像驻留；Adopt 路径不接线 | strict 契约者哪怕未解析也要拆；optional 只有 resolved 才交出过指针；数组序 = 拓扑序故倒序天然逆拓扑；§5.6 规则②"入局者皆为叶" |
| D29 | `SkippedRecord`/`SkipClass` 住 `Pod.h`，静态与运行时跳过进同一张报告表、类型上不许混 | 归属跟 `FailedPluginRecord` 同一逻辑（装配期生成、Pod 自持）；§5.5"分开呈现"从纪律变成类型 |
| D30 | `KnownBinaries` 为全部计划条目（含 `kSkip`）注册路径 | Adopt 不关心它当初为什么没进局；路径信息无成本 |
| D31 | §10 布局修订：新增 `Include/Vase/Config/`，header-only 无 `Source/` 对称实体 | `Config/` 不在已确认的树里且 `Detail` 明文"不对外"；本 spec 批准即修订授权 |
| D32 | blob 键/类型不匹配的检出点在 Apply 时逐字段做，首个不匹配 → 整插件 `kLoad` Failed | 宿主给的数据错是可恢复的装配失败，不是编程错误——不该 abort |
| D33 | 计划结构性校验分两段：**Id 唯一（含 `kSkip`）在任何副作用之前**（纯计划数据）；**`kLoad` 间 Provides 碰撞在装载循环内逐条目查**——M2a 无清单、声明住二进制里，提前到装载前随 M2b 清单自然成立。命中 = 整局 `Err` | 纯声明碰撞该在注册代码跑起来之前硬拒；兑现 `Context.h`「提前到硬拒」的一半（另一半 M2b Solve），checker 与求解同源 |
| D34 | `Pod` 记计划的 `Id→ResolvedConfig` 回放表，Adopt 查表（无表项 → 默认） | eject+adopt 配置如如不动；§5.6 补角「同一套合并」在 M2a 唯一可兑现形态 |
| D35 | `Min/Max` 库内无执法，纯展示元信息 | M2a 的 blob 由可信代码构造；越界执法归 M2b Preset 校验 |
| D36 | `Strict` 条件仍只看 `Failed`，预检跳过不计失败 | §5.5 没有给 Strict 第二语义的依据；边界行为钉用例 |
| D37 | 「声明 Provides 但未注册全」的一致性 M2a 不检 | 后果已被预检覆盖；独立判据不夹带，挂 M3 诊断层候选 |
| D38 | `Skips` 不进 `Clean()` 判定 | Clean = 资源归零不是参与度；console 退出码语义不动 |
| D39 | 校验范围分置：Id 唯一含 `kSkip`（防 `KnownBinaries` 覆盖地雷），Provides 碰撞只查 `kLoad` | 碰撞只在进局成员间成立；Id 重复哪怕一方只是跳过也是不确定 |
| D40 | 不加装配期环检测；环退化为互缺互跳 | 环点名 = 拓扑排序的本职（M2b `Solve`），装配层不长求解器的另一半 |
| D41 | 预检缺失归因：声明提供者已死/已跳 → `CausedBy` 点名，否则空 | 「谁的后果」与「序缺陷」对报告读者最敏感；反查表随结构校验顺手建，成本 ≈ 0 |
| D42 | 级联拆除空壳的中途 Eject = 拒绝（`not in pod`）；反查遇空壳必须干净拒绝；`DestroyPod` 统一收 | M2a 新增的第三形态先钉不炸；「拆未活成员」的三档证据属 3a/M3 shape |
| D43 | Adopt 声明执法双向：`Requires` 全绑 + `Provides` 不碰活集合；碰撞 → `Ok(kRejectedCollision) + Collisions` | 否则一次错 Adopt 走 duplicate-Provide 终止杀全进程，「失败 → X 干净退出」不成立；checker 与 D33 同源 |

## 3. 数据形状

### 3.1 `LoadPlan` 定形（`Include/Vase/Host/LoadPlan.h` 重写）

```cpp
enum class LoadDecision : std::uint8_t { kLoad, kSkip };

// 全部是静态跳过类（§4.4）。后两个值由 M2b 的 Solve 产出；
// M2a 的手写计划至多用 kDisabled（"不进局但留在计划里示众"）。运行时跳过不进计划（§5.1）。
enum class SkipReason : std::uint8_t { kDisabled, kMissingDependency, kVersionMismatch };

struct LoadPlanEntry {
    std::string_view      Id;              // 借用计划拥有者持有的串（M1 契约不变）
    std::filesystem::path BinaryPath;      // M2a 手写；M2b 由 Solve 从清单 stem + 目录填
    LoadDecision          Decision = LoadDecision::kLoad;
    SkipReason            Reason{};        // 仅 Decision == kSkip 时有意义
    ConfigBlob            ResolvedConfig;  // 空 = 全走描述符默认（见下）
};

struct LoadPlan {
    std::vector<LoadPlanEntry> Ordered;
};
```

**顺序契约**：`Decision == kLoad` 条目的数组序 = 加载序 = 关停逆序（§5.4），**应当**是拓扑序；
M2a 由手写者负责、由装配预检兜底（违序的后果从爆炸变成干净跳过，见 4.2）；M2b 起这是 `Solve` 的构造性保证。
`kSkip` 条目可出现在任意位置，装配只遍历 `kLoad` 语义。

**`ResolvedConfig` 语义**（D23）：装配期 Apply 遍历 `kFields`，blob 里有同名 key 用 blob 值，
没有则用 `FieldInfo::Default`——§5.1 注释"默认值已经并进去了"的**运行期形态**，手写计划只给增量也合法。

`PodOptions` 不改（`Strict` 语义见 4.4；`Stage0` 不动）。

### 3.2 配置 POD 面（新目录 `Include/Vase/Config/`，header-only）

经 `Plugin.h` 伞形头 `begin_exports` 转出，插件作者与描述符两侧可见。全部类型必须平凡可拷贝
（每组各带 `static_assert(std::is_trivially_copyable_v<...>)`，三套 STL 同要求）。

```cpp
enum class ValueKind : std::uint8_t { kNone, kBool, kInt32, kInt64, kFloat, kDouble, kString };

struct Value {                       // tagged union；kString 是借用指针（字面量或宿主侧 blob 的存储）
    ValueKind Kind = ValueKind::kNone;
    union {
        bool          B;
        std::int32_t  I32;
        std::int64_t  I64;
        float         F32;
        double        F64;
        const char*   Str;
    };
};
```

- `TypeTag<T>`：编译期把成员类型映射到 `ValueKind` 与 `Value` 的装/取（读侧只走带 Kind 检查的取器，union 裸读不对外）。
- `Meta`：作者书写面——`{ const char* Label = ""; Value Min{}; Value Max{}; }`；`Min/Max` 未设 = `Kind == kNone`。
- `FieldInfo`：**对外契约，布局即 ABI**——头部注释写明"布局变更 → `kHeaderVersion` 递增"。

```cpp
struct FieldInfo {
    const char* Name    = "";
    ValueKind   Kind    = ValueKind::kNone;
    Value       Default{};
    Value       Min{};                      // kNone = 未设
    Value       Max{};
    const char* Label   = "";
    void      (*Apply)(void* ConfigStruct, const Value& V) = nullptr;  // ApplyTo<&T::Field> 展开物
};
```

- `ConfigInfo` = `PluginMeta.Config` 槽形状：

```cpp
struct ConfigInfo {
    const FieldInfo* Fields      = nullptr;
    std::uint32_t    Count       = 0;
    std::uint32_t    StructSize  = 0;
    std::uint32_t    StructAlign = 0;
    void* (*CreateConfig)()  = nullptr;      // 插件镜像内 make_unique().release()
    void  (*DestroyConfig)(void*) = nullptr; // 插件镜像内 unique_ptr 接住析构（无裸 delete 表达式）
};
```

（默认构造的全零 `ConfigInfo` = "该插件无配置"，是合法形态。）

- `FieldsOf<T>()`（constexpr）：从 `T::kFields` + `sizeof/alignof T` + 生成的配对工厂拼出 `ConfigInfo`。
- `VASE_CONFIG(Type, (type, field, default, vase::Meta{...}), ...)`：字段列表写一次、展开两次——
  成员（`float CriticalMultiplier = 2.0f;`，默认值 token 同源于元信息，**一致性是构造出来的**，§3.2 的立论）
  与 `static constexpr std::array<vase::FieldInfo, N> kFields`。元组括号作参数保护壳、`VASE_FOR_EACH` 计数展开
  均为库内部技巧；宏参数内花括号不受 Allman 管辖的既有例外（CLAUDE.md「格式化」节）同样适用于本宏。
- **`enum` 类型不做**（D22）：`ValueKind` 里没有 `kEnum`；M2b 定 schema 时再加，届时 `FieldInfo` 若变布局再 bump。

### 3.3 配置拥有面（`Include/Vase/Host/ConfigBlob.h` + `Source/Host/ConfigBlob.cpp`）

宿主侧运行时数据，插件作者不可见（作者只见类型化结构体）。不依赖 JSON、不依赖插件代码——
§4.2「合并逻辑可被单元测试穷举」的兑现点。

```cpp
class ConfigBlob {
public:
    void Set(std::string_view key, Value view);   // 深拷入：kString 拷进拥有存储
    [[nodiscard]] const Value* Find(std::string_view key) const;  // 借用语义：下一次改动即失效
    void MergeShallow(const ConfigBlob& over);    // §4.2 逐字段浅合并 = 逐 key upsert
    [[nodiscard]] std::size_t Size() const;
    // 遍历访问器（装配 Apply 用）：Entry{ Key(std::string_view), ValueView }
private:
    struct Entry {
        std::string Key;
        // 拥有值。访问一律 std::get_if（std::get 抛 bad_variant_access，本仓库没有可接住的东西——规矩 4）
        std::variant<bool, std::int32_t, std::int64_t, float, double, std::string> Storage;
    };
    std::vector<Entry> Fields;   // 成员无前后缀（tidy 命名），Entry 内成员同理
};
```

`Value` 的 `kString` 借用视图指向 `Entry::Storage` 内 `std::string` 的数据——**借用窗口 = blob 对该字段的最后
一次改动之前**；Apply 在 `LiveInstance` 装配点现场完成（blob 彼时活着），不外带指针。这条契约写进头文件注释。
`ConfigBlob::FromDefaults(const ConfigInfo&)`（或等价静态工厂）：从 `kFields` 的 `Default` 铺一层，供比对与 M2b 复用。

**未知键**：M2a 的 Apply 只遍历 `kFields`，blob 里没有对应字段的键被**无声忽略**（合并层无类型、不认字段表，
这是它可穷测的同一性质）。「preset 手滑一个键名 → warn 还是拒绝」的归属随清单 schema 在 M2b 定案（§12 第 7 条）。

### 3.4 `PluginMeta` 变更与 `kHeaderVersion`

```cpp
struct PluginMeta {
    std::string_view Id;
    std::string_view DisplayName;
    std::string_view Version;
    MetaArray<ServiceRef, 16> Requires;
    MetaArray<ServiceRef, 16> OptionalRequires;  // 新增。与 Requires 同上限——溢出编译错误，不静默截断
    MetaArray<ServiceRef, 16> Provides;
    ConfigInfo Config{};                          // 新增。`.Config = vase::FieldsOf<CombatConfig>()` 显式引用
};
```

- `kHeaderVersion` 1 → 2，理由一句话：**`PluginMeta` 布局变更**（D26）。仓库内插件/fixture 同树重编，无外部兼容债。
- §13.3 的已知静默点原样成立：忘写 `.Config = FieldsOf<T>` 编得过、装得过，只表现为配置消失。接受，不补救。
- 「凭声明」执法扩为 `Requires ∪ OptionalRequires`（`Context.cpp` 的 `DeclaredInRequires`）；未声明解析照旧 ProgrammerError。
- 声明为 optional 的服务用 `Get()` 取而缺失：走既有「必需服务缺失」ProgrammerError——optional 的作者契约就是 `TryGet`（头文件注释写明）。

### 3.5 §10 布局修订记账（D31，本 spec 批准即授权）

1. `Include/Vase/Config/` 新增（§10 已确认树里没有它；`Detail` 明文"不对外"而 `vase::Meta` 是作者书写面，放不下）。
2. header-only 偏离"`Source/` 与 `Include` 对称"：`Config/` 无 `Source/` 对应实体（纯模板/constexpr，无 out-of-line 需求），在 §10 勘误里记一句。
3. M2b 预告实体 `Include/Vase/Catalog/` 本波**不立**（"等真有内容再立"仍有效）。

## 4. 装配与生命周期

### 4.1 配置应用（`MakeInstance` 扩展）

`InspectBinary` 之后、`OnLoad` 之前：

```text
ConfigInfo 为空？──是──► 不建配置对象，ctx.Config<T>() 一旦被调 → ProgrammerError（无配置可读）
      │否
      ▼
CreateConfig()（镜像内） → 遍历 kFields：blob.Find(Name) 有值且 Kind 匹配？用之 : 用 Default
      │                            Kind 不匹配 → 立即 Failed（整插件，kLoad）（D32）
      ▼
Apply 写满 → 指针入 LiveInstance（unique_ptr<void, void(*)(void*)> 持有，销毁走 DestroyConfig）
      ▼
Context 携带（const void* ConfigStore + 该插件 ConfigInfo*），暴露 template <typename T> const T& Config() const：
    ConfigStore 非空 && sizeof(T) == StructSize && alignof(T) == StructAlign？  static_cast ： ProgrammerError
```

**Adopt 配置 = Pod 回放表（D34）**：`Pod` 记计划灌入的 `Id → ResolvedConfig` 表（表只由计划写、Adopt 不改）；
Adopt 查表回放——eject 再 adopt 配置如如不动；表里没有（从未入局的新 Id）走全默认。
§5.6 补角「同一套三层合并」在 M2a 就兑现长这样（Preset/临时两层随 M2b 到位）。

**`Min/Max` 不检查（D35）**：`Apply` 只做 Kind 匹配 + 写入；`Min/Max` 是纯展示元信息（宿主面板的依据），
越界执法归 M2b 的 Preset 校验。

### 4.2 结构性校验 + 装配预检

**结构性校验（D33/D39，M2a 两态形）**：
① **计划全体 Id 唯一（含 `kSkip` 条目）**：同 Id 两次 = `KnownBinaries` 路径覆盖 + `Eject` 反查歧义，§3.4 查重的同一理由。
纯计划数据，**在任何副作用之前**（占槽、Stage0、注册、加载都不发生），命中 = 整局 `Err`，真·零副作用。
② **`Provides` 碰撞（装载循环内逐条目，`InspectBinary` 后）**：M2a 没有清单，**声明住在二进制里**——无法提前于加载。
X 的每条 Provides 查两处在册：声明登记账（循环内增量建，见下）∪ 服务注册表（宿主 Stage0 服务也是碰撞对象）；
命中（他者提供者）→ **整局 `Err`**，半成品按 Strict 同形拆除（镜像驻留残留同 M1 先例，宿主退出统一收）。
任一碰撞对必在**第二个被装载者**处检出，与顺序无关。M2b 有清单后 ①② 与预检一起提前——本段是 M2a 形状，不是永久妥协。
「Err 的局什么都没发生过」与 Strict 的「半局被拆」从此是两种清晰不同的失败形态。

**声明登记账（②与 D41 共用，循环内增量）**：条目 `InspectBinary` 成功后、建实例前，把其 Provides 逐条记
`{Service, Version, ProviderId}`；已处理者（Loaded / Failed / Skipped）都在账上，未轮到者不在——
这正是归因需要的三态。**已知失明面**：提供者连 `EnsureResident`/`InspectBinary` 都没过 → 其 provides 不在账上，
下游归因 `CausedBy=""`、碰撞也看不见它；随 M2b 清单闭合。

**装配预检（阶段 1 循环内、`InspectBinary` 之后、建实例之前）**：对每个已 inspect 的 `kLoad` 条目 X，逐条查其 `Requires`（**不含 optional**）：
`(Name, Version)` 在当前服务注册表可查到？提供者 = 宿主阶段 0 已注册 ∪ 数组序在前且已 `OnLoad` 成功的插件。任一条目查不到 →

- X **不建实例、不跑 OnLoad**（镜像已驻留——声明就是从它读的；这趟装载成本 M2b 随清单提前而消失）；
- 落一条运行时跳过记录（5.1）：`Cause = "missing service <name>@<version>"`；`CausedBy` 按 **D41** 归因——
  声明登记账里提供该服务的 P（已处理者）→ `CausedBy = P.Id`（加载级联从此有名，§4.4 的
  「Physics 倒了所以 Combat 被跳」编辑器形态 M2a 即齐）；P 尚未轮到（序缺陷）或不在计划 → `CausedBy = ""`；
- 继续下一个条目。

三条推论：

- **加载期级联不需要显式图遍历**：上游 `OnLoad` 失败 → 半程注册被 Scope 回收、服务从未存在 → 下游预检自然接住 → 级联 = 预检在拓扑序循环上的重复应用（D27）。上游 `EnsureResident` / `InspectBinary` 失败同形。
- **坏序手写计划从爆炸变成干净跳过**：M1 里 A 需要 B 而 B 在后 → `Get` 撞 ProgrammerError 终止；M2a 预检先拦。M1 靠这个形状写的"失败→级联"预期测试若存在爆炸形态，按新语义改写（计划盘点）。
- **环退化为互缺，不检测（D40）**：A strict 需 B、B strict 需 A → 双双跳过、不挂不炸，只是报告里不会出现「cycle」字样。环点名归 M2b `Solve`（拓扑排序的本职，§12 第 9 条），形状钉一条用例。

检查器实现为 Host 层纯函数（输入：meta 的 Requires/Provides 列表 + 注册表读面），结构性校验、Adopt 双向执法（D43）与 M2b 的 `Solve` 三处同源。

### 4.3 `OnStart` 失败 → 递归拆除（§5.2 判据 5；本波最硬的一块）

阶段 2 中 B 的 `OnStart` 返回 Err：B 自身走既有 Failed 处理（`DiscardInstance` + 记录 + 空壳 + `FailedBinaries`），
然后执行**递归拆除**：

**波及集（D28）**——从 B 出发求传递闭包，X 的必须波及下游 =

```text
{ C 仍活 | C.Requires ∩ X.Provides ≠ ∅ }  ∪  { C 仍活 | 账本存在边 C→X }
```

- **strict 声明边必波及**：哪怕 C 从未解析过（代码契约是"必然拿得到"，之后一 `Get` 即悬空/爆炸；§5.2 的"下游"按声明取是刻意的保守）；
- **optional 声明不波及，resolved 边才波及**（账本 C→X 存在 = 指针已交出 → 必须拆；只声明未用 → 留下继续跑，之后 `TryGet` 返回 `nullptr` 合法）——账本第二次证明自己有用；
- 同服务另有活提供者不救场：拆的是对 B 的依赖闭包，不是服务可用性（一服务一提供者，§6.2/§6.3，本不存在"改嫁"）。

**拆的次序与形状**：闭包算完后按 `Instances` 数组序**倒序**回收命中者（数组序 = 拓扑序，倒序即天然逆拓扑序，不需要新排序结构）；
每个命中者 `DiscardInstance`（账本进出两向摘除、Scope 逆序回收——复用 M1 机器）+ 空壳条目留在 `Instances`
（与 Failed 空壳同形，`DestroyPod` 拆除机器认它）+ 落运行时跳过记录（`Cause = "cascade from <B.Id>"`、`CausedBy = B.Id`，
链上多层都记最初失败者）；**镜像留驻留不解除、不进 `FailedBinaries`**（它没失败，`DestroyPod` 统一卸货）。

**这台机器对 Adopt 备而不用**（§5.6 规则②：入局者皆为叶、无入边，Adopt 失败零级联——判定流实现里不许接线）。

### 4.4 `Strict`

⑥ 分支条件不变（`FailureRecords` 非空 → 整拆半成品 → `Err`）。作用对象变了：拆的是一个**已经跑完预检与递归拆除**的局，
`TeardownInstancesAndRoot` 对空壳与拆除残留同样成立（归零断言不许因新路径破坏）。宽容模式（默认）交付的局里，
失败、预检跳过、级联拆除各归各类进报告。

### 4.5 Adopt / Eject 本波增量

- Adopt：声明执法**双向（D43）**——① `Requires` 可全绑注册表（optional 不拦入局，§6.3）；② `Provides` 不得与活集合
  已注册服务碰撞，碰撞 → `Ok(kRejectedCollision) + Collisions`（服务 + 现提供者点名）。不做 ② 的话，一次错 Adopt 会走到
  duplicate-Provide 的**终止**，§5.6「任一步失败 → X 干净退出，无人受累」在此路径不成立；checker 与 D33 同源。
  配置 = D34 回放表；`KnownBinaries` 保留为唯一路径来源（M2b 交棒）；`InspectBinary` 走 HeaderVersion 2。
- Eject：三档机器与账本反查**不动**，只换报告外衣（第 5 节）。级联拆除的空壳（§4.3 的新形态：无实例、无入边、
  镜像在架、不在 `FailedBinaries`）中途被 Eject → **一律拒绝**，消息 `not in pod`（D42）；反查路径遇空壳必须干净拒绝
  而非解引用 null——这条行为用测试钉住，空壳与残留镜像由 `DestroyPod` 统一收。
- `KnownBinaries` 注册点从"逐条目装载前"改为"遍历全部计划条目"（D30，含 `kSkip` 与后续失败的）。

## 5. 报告与执法的结构化（3b 薄面，D20 / D21）

### 5.1 跳过记录（`Include/Vase/Pod/Pod.h`）

```cpp
enum class SkipClass : std::uint8_t { kStatic, kRuntime };   // §4.4 两类，类型上不许混

struct SkippedRecord {
    std::string Id;
    SkipClass   Class;
    std::string Cause;     // 静态：计划条目 SkipReason 的名字化；运行时：缺哪个服务 / "cascade from <id>"
    std::string CausedBy;  // 运行时：级联与加载跳过的最初责任者点名（D41）；序缺陷、静态跳过为空串
};
```

- `Pod` 自持 `std::vector<SkippedRecord> SkipRecords;` + 访问器 `Skips()`（与 `Failures()` 对称）；
- `PodReport` 加 `std::vector<SkippedRecord> Skips;`——**CreatePod 时计划里 `kSkip` 条目即落静态记录**，
  计划的跳过与装配的跳过在同一张表里各归各类（§5.5"必须区分"从纪律变成类型）；
- `Pod::HasPlugin / PluginCount / PluginIds` 语义 = 活集合，跳过者不在活集合（与 Failed 空壳同待遇，现状即对，写进测试）；
- **`Skips` 不进 `Clean()`（D38）**：Clean = 五项计数归零 + 无 Residuals，测的是资源收口；跳过者无实例、无边、无 scope，
  五项差分天然不受影响——「参与度」不是 Clean 的语义，VaseConsole 的退出码（绑 Clean）因此一字不动。

### 5.2 Eject / Adopt 的结构化字段（`Include/Vase/Host/Evidence.h`）

```cpp
struct LedgerEdgeRef {           // 账本边的一次性快照（报告是返回值，必须拥有）
    std::string ConsumerId;
    std::string ProviderId;
    std::string Service;
    std::uint32_t Version = 0;
};

struct RequirementRef {          // 拥有型需求引用。描述符侧的 ServiceRef 是借用（string_view），
                                 // 报告要活过镜像驻留期，不能拿它当字段——故另立此型
    std::string   Service;
    std::uint32_t Version = 0;
};

struct CollisionRef {            // D43：碰撞报告专用——与 RequirementRef 的差别只在要多一个现提供者
    std::string   Service;
    std::uint32_t Version = 0;
    std::string   ProvidedBy;    // 活集合中当前提供者的 Id
};

// EjectReport 增：
enum class EjectStatus : std::uint8_t { kRejectedConsumers, kEjected };
    EjectStatus Status = EjectStatus::kRejectedConsumers;  // 默认悲观：成功态必须被显式置上
    std::vector<LedgerEdgeRef> Consumers;      // kRejectedConsumers：入边逐条点名（3b「拒绝报告点名消费者」的兑现）
    std::vector<LedgerEdgeRef> RemovedEdges;   // kEjected：拆除时账上双向边清单（「解析记录」Eject 侧）

// AdoptReport 增：
enum class AdoptStatus : std::uint8_t { kRejectedDependencies, kRejectedCollision, kAdopted };
    AdoptStatus Status = AdoptStatus::kRejectedDependencies;  // 同：默认悲观
    std::vector<RequirementRef> Missing;        // kRejectedDependencies：缺哪条（§5.6③「报告缺哪条」的兑现）
    std::vector<CollisionRef> Collisions;       // kRejectedCollision：碰撞的服务 + 现提供者（D43）
    std::vector<LedgerEdgeRef> Outgoing;        // kAdopted：本次入局新落账边清单；OutgoingEdges 计数保留（= .size()，向后自洽）
```

拒绝时既有证据/布尔字段保持默认值（零值 = 没发生过，不造假）。

### 5.3 `Err` 边界与断言迁移面（D21）

**边界一句话，写进 `PluginHost.h` 头注释**：执法性拒绝（图判定，消费者要逐条读）走 `Ok + Status`；
环境/身份/用法类拒绝（人读一句话就够）照旧 `Err` + 消息子串契约。

| 判定 | 通道 | 断言形态 |
|---|---|---|
| Eject：有消费者入边 | `Ok(kRejectedConsumers)` + `Consumers` | **迁移**：子串 `provided by [` → 字段枚举 |
| Adopt：声明绑不齐 | `Ok(kRejectedDependencies)` + `Missing` | **迁移** |
| Adopt：Provides 碰撞活集合 | `Ok(kRejectedCollision)` + `Collisions` | 新增执法点（D43），无迁移面 |
| Eject：级联拆除空壳（M2a 新形态） | `Err`（消息 = not in pod） | 新增钉死（D42：干净拒绝，不许炸） |
| Adopt：未知 Id / 身份不符 / 特征缺失 / 兄弟导入 / already in pod | `Err`（子串不动） | 原样 |
| Eject/Adopt：bad handle、非绑定线程 | `Err` / 终止 | 原样 |

HotSwap/Abi 里只迁前两族断言，M1 计划"子串是契约"的表述对这两族作废并在 CLAUDE.md 规矩 6 附近同步改注（第 11 节）。

## 6. 错误处理与执法边界

- **新增 ProgrammerError 点**（两构建一致终止，非裸 `assert`）：`ctx.Config<T>()` 于无配置插件 / Size-Align 不符；
  `Value` 带 Kind 检查的取器读错类型（宏生成代码内部防线）。既有执法点（未声明解析、`Get` 必需缺失）不动。
- **新增 Error 通道点**：blob 键值 Kind 与字段不符 → 该插件 `kLoad` Failed（D32，消息含插件 Id + 字段名 + 两侧 Kind）。
- `std::variant` 访问纪律：只 `std::get_if`（`std::get` 抛 `bad_variant_access`，`_HAS_EXCEPTIONS=0` 下没有可接住的东西——规矩 4）。
- 测试不许用 `EXPECT_THROW` 一族（规矩 2）；新增"必须终止"的用例走 M1 既有 death-test 形态（`#ifndef NDEBUG` 门，按线基数影响随之）。

## 7. ABI 与 `kHeaderVersion`

- bump 1 → 2 的变更面：`PluginMeta` 两个新字段；宏产出物（工厂/描述符/导出符号名）不变。
- `FieldInfo` / `ConfigInfo` 自诞生即对外契约（§3.2"把 FieldInfo 当接口对待"）：布局注释挂"改动即 bump"警示。
- Abi/Lifecycle 需覆盖：新常量断言随代码走；`HeaderVersion=1` 的旧形状描述符被 `InspectBinary` 拒——
  **计划先盘点 M1 既有覆盖形态**（若已有手搓/fixture 形态的拒旧版本用例则改常量，缺则补 Abi 一条）。
- 本波**不碰**两条承重工具链 flag；若因故碰到，规矩 6 + CLAUDE.md「工具链 flag 是承重的」全套适用。

## 8. 可移植性与门禁注意

- POD 面全部过 `is_trivially_copyable` 编译期断言，三套 STL（MSVC / libc++(Win clang-cl) / libc++(Linux)）同要求；
  `union` 对齐与 `Value` 尺寸在两条 Windows 线与 Linux 线各钉一条 `static_assert`（值以实测为准，钉行为不钉数字）。
- `ConfigBlob` 住 Host：`std::string`/`std::vector`/`std::variant` 都是宿主侧类型，不过 DLL 数据面（只 `Value` 视图过 `Apply` 调用点）。
- 新头文件走引号包含（规矩 3）；新 `.cpp` 入 target 即链 `VaseBuildOptions`（规矩 1）；`Source/Host/CMakeLists.txt` 加 `ConfigBlob.cpp`。
- 新文件先 `git add` 再跑 format 门禁（规矩/实测都在 CLAUDE.md）。

## 9. 测试计划（分层；确切基数由计划在收口任务六线重测、写回 CLAUDE.md）

| 层 | 新增文件（工作名） | 内容 |
|---|---|---|
| Unit | `ConfigValueTests` / `ConfigMacroTests` / `ConfigBlobTests` | `Value`/`TypeTag`/`ConfigInfo` 形状与 POD 断言；`VASE_CONFIG` 展开逐字段断言（成员默认值 = 元信息默认值，同源构造）；浅合并穷举（新键 / 覆旧键 / 空 override / 类型覆盖 / 借用窗口） |
| Integration | `MultiPluginAssemblyTests` / `DeclarationEnforcementTests` / `SkipReportTests` | 拓扑行为（执行序探针）、预检跳过、加载级联、**`OnStart` 递归拆除（strict 波及 / optional-resolved 波及 / optional-未用不波及 / transitive 链 / 数组序倒拆）**、静态+运行时混合呈现、坏序计划、Strict 在级联之下（D36：预检跳过不触发，边界钉死）；**结构性校验两族（Id 重复含 kSkip → 整局 Err · 真零副作用；Provides 碰撞 → 整局 Err · 实例注册双清零，镜像驻留同 Strict 先例）**、**环退化互缺（D40）**、**预检归因三态（D41：提供者已死 / 序缺陷 / 不在计划）**、**Adopt 碰撞拒绝（D43）**、**Adopt 配置回放（D34：eject+adopt 如如不动；新 Id 走默认）**、**Min/Max 越界照常应用（D35 钉「无执法」）**；`TryGet`-optional 落账、未声明照旧、`Get`-optional 缺失 |
| Lifecycle | `MultiPluginCycleTests` | 多插件反复 create/destroy 五项计数与账本归零（含跳过空壳与拆除残留） |
| HotSwap | 断言迁移 + 新用例 | `Consumers`/`Missing`/`Collisions`/`RemovedEdges`/`Outgoing` 字段断言；子串断言原样族回归；**级联空壳 Eject 拒绝且不解引用 null（D42）** |
| Abi | `HeaderVersion` 用例（盘点后定） | v2 常量、旧形状拒绝 |
| fixture | 新插件 fixture 若干（`vase_add_plugin_fixture`，规矩 5） | 多插件场景：Provider/Consumer/OptionalConsumer/TransitiveConsumer/FailOnStart/ConfigConsumer |

## 10. 验证与验收

1. 六 preset configure → build → ctest 全绿；`ctest --preset <p> -N` 按线新基数**逐位**对上新表（死 test 门与 Linux-only fixture 对基数的影响在计划里显式重算）。
2. 改动命中账本 / Eject / Adopt / 描述符布局 / HeaderVersion → `ctest -R 'HotSwap|Eject|Adopt'` **全选择子**（不是 `-R HotSwap`），Windows 与 Linux 各留证据（规矩 6；判据力按平台不对称的读法照旧）。
3. 三条 tidy debug 线各自跑、各自读正文：退出 0 + 正文 `error:` 0 + 正文 `warning:` 0；TU 基数（`ConfigBlob.cpp` 使 Host 侧 +1）与 NOLINT 计数在验收表更新。
4. `clang-format --dry-run --Werror` 全绿（新文件已暂存后跑）；宏参数内花括号的既有例外若波及 `VASE_CONFIG`，验证形态与 `CrossDllSmoke` 那条同记。
5. Windows 删树重配若撞环境性文件锁假红：先重跑单线再判（CLAUDE.md 实测记录）。
6. `Samples/Embedding` 的 `play / loop / swapdemo` 人工复跑一遍（新形状适配后的冒烟），输出贴验收记录。
7. 判据 5 的**证伪价值**在验收里单列一句：递归拆除用例过/不过、若不过的读法（§12.2 那半句"epoch 请回来"的触发器）——不许静默降级。

## 11. 文档同步义务（实施波内完成，不是 afterthought）

- **CLAUDE.md**：项目状态（M2 两波进度）、「LoadPlan 是手写形（D12）」改写为定形、按线基数表、tidy TU/抑制基数、
  规矩 6 附近"子串是契约"表述的收窄注记（5.3 迁移族）、示例/目录清单若波及。
- **wiki 勘误/修订**（提案被实现细化，挂账不改史体例沿 m0-m1 spec 头部先例）：
  §5.1 `LoadPlan.Entry` 补 `BinaryPath` 与回退语义；§5.6 报告 Status 结构化形状；§3.2 六型 + `enum` 留 M2b；
  §10 树 `Include/Vase/Config/`（D31 两条偏离）。
- **技能 `vase-cpp-engineering`**：若"图执法 Ok、环境 Err"这条边界要进 abi-boundary / plugin-lifecycle 分面，随波同步；
  受规矩 7 管辖（不复制基数与命令）。

## 12. M2b 接口预留与开放问题（只立不答）

1. `Solve` 必须能产出与 M2a 手写完全同形的 `LoadPlan`（D12 的"零返工"由此闭环）。
2. **`Solve` 看不见宿主服务**——阶段 0 注册的服务在枚举期不存在，宿主提供 `Vase.World` 而插件 `requires` 它会被静态求解误判缺失。
   解法（`LoadRequest` 带宿主服务清单，或 `SkipReason::kMissingDependency` 让位于装配预检的降级规则）**M2b 定案**。
3. 清单↔二进制逐字段比对、Adopt 就地重读的接口缝（JSON 只在 Catalog 层 vs Host 消费清单数据的方向问题）——M2b 定案。
4. `enum` 的 choices 表示与 `FieldInfo` 布局的关系——M2b 定案（D22）。
5. `ConfigInfo` 非空时"清单 config 数组 ↔ `kFields`"的逐字段比对（含 `Min/Max/Label`）——M2b 比对机器覆盖。
6. VaseConsole 与 Samples 的接线时机与 plan 格式（`file install` 一族的语义扩展）——M2b。
7. blob 未知键的处置（warn / 拒绝）随 Preset 校验规则一起定——M2b（3.3 已记 M2a 形态为无声忽略）。
8. 级联拆除空壳的**中途卸货**（三档证据、镜像出架）——M2b/M3（3a 同地，「拆未活成员」的 shape）；
   **M2a 形态已由 D42 钉死**：Eject 一律拒绝（`not in pod`）、反查遇空壳不炸、`DestroyPod` 统一收。
9. 环的**点名**（求解失败项 / 专门的 SkipReason）——M2b `Solve` 拓扑排序的本职输出（D40）。
10. **声明 Provides 与注册实况的一致性检测**（X 声明却 OnLoad 没给全 → 归因升级）——M3 诊断层候选（D37，与完整属主追踪器同住）。

**M2a-T11 收口波追记（2026-09-24，浮出即立、只立不答）：**

11. deps 族 console 现拼行（`eject refused: in use by [ … ]`）缺端到端回放对——本波补上 adopt 族后 console 用例 25→27 量级，deps 族两半补钉归 M2b bench 波。
12. `CmdAdopt` 与 `CmdSwap` 的 adopt 拒绝话术逐字重复（两文件各一份 Status 分支）——可提炼共享打印，归 M2b。
13. `CollisionRef` 的 adopt-host 归因支现与 T8 装载侧共享证人——可接受；M2b 若 adopt 路径引入宿主提供面对象，再单钉。
14. 版本键两态缝（T8 评审续挂项，见 SDD ledger）——随 M2b 清单定案一并处置。

## 13. 风险与已知债

| 风险 | 缓解 |
|---|---|
| 递归拆除的波及闭包算错（漏 optional-resolved / 多算 optional-未用） | 判定用例按 9 节三形态分立钉死；Lifecycle 归零断言兜底 |
| 空壳条目 + `FailedBinaries` + `SkipRecords` 三张表的一致性（Eject ②' 要认识所有形态，级联空壳是 M2a 新增的第三形态） | HotSwap 回归全选择子覆盖；D42 已钉「干净拒绝不解引用」并配用例；计划阶段先盘 M1 空壳路径再动刀 |
| `VASE_CONFIG` 宏的门禁摩擦（tidy 对宏体各检查、clang-format 对参数内花括号） | 沿 `VASE_PLUGIN` 先例：宏体内代码级出路优先、确无出路就地 NOLINT（新口径见 CLAUDE.md）；探针任务先行（沿 spec 3.3(1) 体例） |
| 忘写 `.Config = FieldsOf<T>` 的静默点 | §13.3 既受决定，不补救；在本 spec 与头注释各留一句指向 |
| 判据 5 过不了 | 文档预案：那是"低估装配期复杂度、epoch 请回来"的证据——先让证据说话，不现场改架构 |

**M2a-T11 收口波追记（2026-09-24）：**

| 风险 | 缓解 |
|---|---|
| `CreateConfig()` 返 null（分配失败形）与「未声明配置」共用同一条 `declares no config` ProgrammerError 消息，误导归因 | 现状接受（两态皆终止，恢复语义无分别）；M2b 若引入可恢复路径再拆消息 |
| `ReportMissingRequiredService`（Context.cpp）M2a 后无死亡用例锚——CreatePod 路径被预检墙前置，可达支仅剩运行中违约与 Adopt⑤ | 深度防御现状（R5-3 裁定）；墙本身由 `RequiresMissingConsumer` 系用例钉住，函数本体留白 |
| §3.3「消费点当场取用、不外带指针」被实现证伪（R-F1，终审修复波）：CreatePod 供计划条目、Adopt 供函数局部拷贝，类型化结构体的 kString 借用指针随供体先死 | 供给源收编为 `slot->Replays` 单源（深拷入槽，生命期覆盖全部实例）；`ConfigApply.HostSuppliedStringConfigOutlivesPlanDonor` 钉住 |

## 14. 改动面清单（给 writing-plans 的输入）

**新增**：`Include/Vase/Config/*.h`（2–3 个文件，计划定夺切分粒度）、`Include/Vase/Host/ConfigBlob.h`、`Source/Host/ConfigBlob.cpp`、
9 节测试文件与 fixture 插件。
**修改**：`Include/Vase/PluginDescriptor.h`（`PluginMeta` + bump）、`Include/Vase/Plugin.h`（转出 Config）、
`Include/Vase/Host/LoadPlan.h`（定形）、`Include/Vase/Host/Evidence.h`（5.2 字段）、`Include/Vase/Host/PluginHost.h`（注释边界）、
`Include/Vase/Pod/Pod.h` / `Source/Pod/Pod.cpp`（SkipRecords 与活集合语义）、`Include/Vase/Pod/Context.h` / `Source/Pod/Context.cpp`
（`DeclaredInRequires` 扩展、`Config<T>()`、类注释「M2 把它提前到求解期硬拒」按现实改写——D33/D43 落在装配层兑现一半）、
`Source/Host/PluginHost.cpp`（结构性校验、预检与 D41 归因、递归拆除、Strict、配置应用与 D34 回放表、Adopt 双向执法、KnownBinaries）、
`Samples/Embedding/main.cpp`（预计**零改动**——designated init 下新字段取默认已验证；若动只为演示，计划定夺）、
`Samples/Hello*`（补 `.Config`/`OptionalRequires` 示例形态，计划定夺）、`Source/Host/CMakeLists.txt`、
既有 HotSwap/Abi 断言迁移涉及的文件。
**不动**：三工具链文件、两承重 flag、`Cmake/VaseThirdParty.cmake`、`vcpkg.json`、`ThirdParty/cli`、
**`Tools/VaseConsole`**（其 plan 构造点 `Console.cpp:211` 系 designated init，新字段取默认 = 现行为，已核不炸编译；plan 格式语义扩展归 M2b/D19）。

---

*本 spec 批准后的下一步：writing-plans 出实施计划（预计 12–16 任务量级，含宏探针任务与基数重测收口任务）。*
