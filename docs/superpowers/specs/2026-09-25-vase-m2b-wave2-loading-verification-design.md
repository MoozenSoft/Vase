# Vase M2b 第二波：加载期执法与 enum 定案（spec）

**日期**：2026-09-25 · **状态**：brainstorming 定稿（需求方四问四点全裁：期望 POD 进参 / 全字段比对两槽不收 / enum class + label / Console 双入口；§1–§5 分节确认）· **grilling 两轮收敛**（Q1–Q6 全裁：§3.3 勘误限缩 / 严格等值零规范化 / D89 静默点见光 / 清单换件命令面 / label→value 唯一转换点 / 证人族名；Round 2 前沿空） · **实施计划**：spec 复审通过后由 writing-plans 另立 · **分支**：`m2b-wave2`

**上游权威**：`wiki/vase-architecture.md` v3 的 §3.3–3.4（清单形态与权威性、两个版本号）/ §4.4（静态求解与「枚举期不加载二进制」）/ §5.6（进出判定流，①「就地重读」）/ §8.2–8.3（档三证据链、HeaderVersion）/ §11.2（热重载链）/ §12（判据 4、8、12、13）；
`2026-09-24-vase-m2b-catalog-solving-design.md` 的 §10 边界句与第 0 节「第一波不做」表——那六行「M2b 第二波」即本文范围；
`2026-09-23-vase-m2a-assembly-foundation-design.md` 的 D19 / D22 / D34 与延期表；
以及波 1 终审留给本文的 handoff 账四条（Issue-1 两段制、preset 根级 unknown 键、`Directory()`/Plan 再绑定用例、`PluginMeta` 收不收两槽）。
历史决定 D1–D66 不改史，本文决定从 **D67** 续号。

**本文的地位**：兑现波 1 §10 立界不预告的那些形状。凡本文与磁盘代码冲突处以磁盘为准（仓库惯例）；本文与架构文档冲突处以本文为准并回头挂勘误（第 8 节清单）。

> **[计划期改判注（2026-09-25，需求方逐条裁可；见 plan「偏离登记」）]** ① `ManifestExpectation` 为**拥有值形**（string/vector 成员，宿主面不过 ABI 界）——D67 骨架、D68/D84 的搬运与借用叙述按此读，`Solve` 无 scratch 存储；② choices 表由**作者侧 `inline constexpr std::array` 声明**、`Meta{.Choices}` 传引用，D77 的「宏生成 per-field 数组」不可行（取件器无法在 Meta 逗号下判 4/5 元），配对执法移 `CheckedChoices` consteval；③ 单点读**复用现成公开 `ParseManifestFile`**，§3.1 的 `LoadManifestFile` 一名并入它。**落地勘误（终审写回，2026-09-26）**：② 末句的终态形——配对执法是 `VASE_CONFIG_DETAIL_CHECK` 在宏展开处用 `MetaChoiceCount` 立 static_assert（宏实参在 consteval 形参内不是常量式，执法进不了体内），搬运 consteval 定名 `MetaChoices`，`CheckedChoices` 一名未落盘。

---

## 0. 范围

**第二波做**（Host 动、Catalog 动、配置面动、前端改接；描述符布局变更随 bump）：

1. `ManifestExpectation`：Host 侧公开 POD 期望形，`Solve` 产计划自动填、`AdoptPlugin` 必收（§2、§3）。
2. 加载期逐字段比对：`CreatePod`（带期望则比）与 `AdoptPlugin`（必比），域 = 全字段，含 `schemaVersion` 加载期闸的兑现形态（§2、D72、D74）。
3. `AdoptPlugin` 改收 `AdoptRequest`；`KnownBinaries` 代偿表连注释退役，兄弟导入执法的兄弟集换源（§2，D69/D71）。
4. `enum` 配置型与 choices schema：`ValueKind::kEnum`、`FieldInfo` 布局追加、`VASE_CONFIG` 作者面、`ConfigBlob` 新 alternative、manifest/Preset 两面的 label 形（§4）。
5. **`kHeaderVersion` 2→3**（D22 还债，唯一理由 = `FieldInfo` 布局）（§4，D76）。
6. D19 前端：Console 双入口改接（`catalog` 组 + `pod new <目录> [preset]` + `pod new-raw`），Samples 补 `DependentPlugin` / `FailingPlugin`，Hello 家族加 `Mood` 字段（§5）。
7. handoff 账四条：Solve 归因两段制（D82）、Preset 根级 unknown 键（D83）、`Directory()`/Plan 再绑定用例（§7）、`PluginMeta` 不收两槽（D73）。
8. §12 判据 4 的后半句：注入运行时失败，断言静态预测与实际装配的分歧恰在那里（§7）。

**第二波不做**（各归其位，不许顺手）：

| 不做 | 归属 |
|---|---|
| `VaseCli scan`（从描述符**生成**清单的构建期工具——比对二副本的「生成端」） | M5；波 2 的清单材料仍是手写 |
| 清单/文件目录监视 | 宿主的活儿（§4.4 明文） |
| `LoadRequest.HostProvided` 在 Console 的宿主声明 | 暂传空集；有 Stage0 宿主服务需求时再开 |
| Adopted 插件的配置回放语义（`slot->Replays` 三层合并、D34） | 原样保留，本文零改动 |
| C ABI 边界收缩（§8.4）、`VasePack`（§8.6）、macOS/移动端 | 提案，无波次移动 |
| 级联热替换（档 ②，§13.4 已否决） | 不做 |
| `enum` 进 `Meta` 之外的第二 authoring 面（如区间枚举/位标志） | 无此需求，不开 |

---

## 1. 决定表

| # | 决定 | 理由 |
|---|---|---|
| D67 | 比对与「就地重读」的接口骨架 = **Host 侧期望 POD 进参**：Host 定义 `ManifestExpectation`（公开头），Catalog 生产它，调用方搬运它。否决两案：① `PluginHost` 构造期注入回调「按 Id 读清单」——数据谁供给变隐式，借用窗/失败归因都要新契约，且 KnownBinaries 的路径与兄弟集仍另找来源；② Catalog-only 便利包装、Host 签名不动——Host 内执法退化为可绕过的约定，§3.4「不一致→拒绝加载」的权威点离开 Host，与 §5.6 判定流对不上 | D45 链接方向（Catalog→Host，Host 看不见 Catalog 类型）与 D55 JSON 封口（nlohmann 不落 `Source/Catalog/` 以外）夹出来的唯一顺方向的形：类型住低位、构造住高位、搬运在调用方 |
| D68 | `LoadPlanEntry` 加 `std::optional<ManifestExpectation> Expected;`（**拥有值形**，计划期改判见 D67 末注）。**有值 = CreatePod 在 `InspectBinary` 闸后、碰任何注册表之前逐字段比对；空 = M2a 行为原样** | D12 双生产者的承诺保住：手写计划继续可直喂 Host（Host 预检/碰撞执法的测试材料零改动），「带清单就执法」是数据在场时的行为，不是新通道。拥有形令 plan 拷贝/移动自包含——`Solve` 可被反复调用（编辑器每击重算），借用形要么让 catalog 养一块无上界的 scratch、要么让旧 Outcome 的指针悬垂，两条都是自造雷 |
| D69 | `AdoptPlugin` 签名改 `AdoptPlugin(PodHandle, const AdoptRequest&)`，`AdoptRequest{ std::string Id, path BinaryPath, const ManifestExpectation* Expected, std::vector<std::string> SiblingBinaries }`——`Expected` **语义必需**（指针形只是给 T8 过渡双轨留位，终态 `nullptr` = `Err`；§5.6 判定流 ① 是单点操作的定义性步骤，Adopt 无「不比对」一档） | 路径账（KnownBinaries）一退役，「Id → 二进制在哪」就只剩调用方能答；签名把这一点摊开在桌面上，比注入回调诚实。兄弟集同理由（见 D71）；拥有形下兄弟集用值 vector，借用窗议题不复存在 |
| D70 | 比对失败的通道：`CreatePod` = **Failed 记录**（`Phase::kLoad`，与 HeaderVersion 不符同位，级联语义复用 5.5）；`Adopt` = **`Err`**（身份/环境家族——D21 通道边界原文）。消息点名字段与两侧值。`AdoptReport` 加 `bool ManifestVerified`，与 `ImportEnforcementPassed` 对位（成功面要能证「比过」） | 身份类走 Err 是 M1 定的家族律；CreatePod 侧本就有 Failed 记录这条更诚实的通道（响亮且不炸整局）。`ManifestVerified` 让回放表面对「比对生效」有字段可断言，不逼读者解析消息 |
| D71 | `KnownBinaries` 整体退役（成员、CreatePod 注册点、Adopt ① 查表、Eject 侧「路径账保留」注释段——`PluginHost.cpp` 注释预告的「连注释一起删」兑现）。**兄弟导入执法（Adopt ④，§8.7）的兄弟集 = 调用方传入**（`AdoptRequest.SiblingBinaries`，前端应喂快照全量插件二进制名）。信任模型明记：旧表只知「CreatePod 见过的」，从未加载的兄弟插件本就是盲区；快照全量**严格强于**旧形，但欠报防不住——这是调用方纪律，不是 Host 执法 | 铁律与分层都不禁止 Host 记路径账，但那张表的存在理由（M1 无清单）已被清；留着它就是第二条真相来源。欠报风险要在 spec 里点破而不是藏起：它的另一面是「Host 无从自证文件系统真相」 |
| D72 | 比对域 = **全字段**：`Id`/`DisplayName`/`Version`（字串等值）；`requires`/`optionalRequires`/`provides`（**(name,version) 多重集等值，序不敏感**，含条数）；`config`（条目按 key 对齐后逐比 Kind / Default / Min / Max / Label / Choices，**choices 序敏感**）。服务数组与 choices 一个按集合一个按序，分界是「有无展示序语义」：服务序无（求解输出由拓扑定序），choices 序有（编辑器按下拉顺序展示，M5 生成器抄 `kFields` 源序，换序即漂移）。**比对 = 严格等值、零规范化**——「清单未写 displayName → parser 缺省填 id」对上「描述符 `.DisplayName` 空串」也算漂移，不为任何角设特例（grilling Q2 裁） | §3.4「两个副本 + 一次比对，分歧只有拒绝」的全字段兑现；「哪个字段有判据力」不设第二套规则（需求方裁）。特例是第二套规则的开始，而本仓比对哲学就一句「分歧只有拒绝」|
| D73 | `binary` / `enabledByDefault` **不进 `PluginMeta`**，记为「清单独有字段：免生成、免比对」。§3.4「全部由代码生成」对这两字段挂勘误 | binary 是部署事实（`VASE_PLUGIN` 无从知道产物文件名，作者在代码里手写第二份文件名正是漂移温床）；enabledByDefault 是编辑器缺省语义，无代码对应物。收进描述符要求 M5 把构建期事实反灌回代码声明，与「代码是唯一权威」的生成方向拧着（需求方裁） |
| D74 | `schemaVersion` 的「加载期闸」兑现形态 = **读取处即闸**：`LoadManifestFile` / `Refresh` 同一 parser 同一硬闸（major≠1 即 `Err`），Host 比对面**不**含此字段（`ManifestExpectation` 不设 `SchemaVersion` 成员——描述符侧无对位物；手写期望旁路则根本没有 JSON 层可闸） | 「扫描期与加载期同一规则」（§3.3/§11.1）由「独一 parser + 在场字段全闸」构造性成立；在期望形上留一个恒真字段是给读者设第二处要核对的账 |
| D75 | `ValueKind` 尾部追加 `kEnum`（既有 7 值序号不动）；`KindOf<T>` 对 `std::is_enum_v<T>` 返回 `kEnum`，且 **static_assert `underlying_type_t<T>` 恰为 `std::int32_t`**——`enum class Mood : std::int32_t` 是唯一合法写法。`Value::From` / `GetAs` 各加枚举分支（走 `Bits` 低 32 位；`GetAs` 的 else 白名单 static_assert 同步扩） | 与「六型精确匹配」同一家族风格：宁窄勿含混。底层型放宽到「可无损落进 int32 的兼容集」要另写一程合法集定义、比对面多一条转换规则——不值（需求方在两案中裁此） |
| D76 | **`enum` 字段必须带 choices**（编译期强制；「无 choices 的 enum」不开）；`FieldInfo` **尾部追加** `const ChoiceInfo* Choices = nullptr; std::uint32_t ChoiceCount = 0;`（全零 = 非枚举/未设，与「全零 = 未设」盘面一致；`ChoiceInfo{ std::int32_t Value; const char* Label; }` 住 `FieldInfo.h`，POD）。**`kHeaderVersion` 2→3**，理由一句话：`FieldInfo` 布局新增两槽 | D22 的兑现：「未定案的东西不焊进布局」现在定案了，bump 是该付的账（M2 内第二次递增，外部无债）。树内 fixture/Sample 重编自动跟上；旧二进制被拒消息现成（「HeaderVersion mismatch: binary 2, host 3」） |
| D77 | choices 的存储形（计划期改判注 ②）：**作者侧 `inline constexpr std::array<ChoiceInfo, N>` 命名表**（随枚举类型导出），`Meta{.Choices = ChoicesOf(表)}` 传引用，`FieldInfo` 存其 `.data()` + `.size()`；宏体经 `CheckedChoices<FieldT>` consteval 守「enum 必带 / 非 enum 禁带」配对。不走 initializer-list 成员/临时量，也不走「宏生成 per-field 数组」（取件器在 Meta 的 brace 逗号下无法判 4/5 元，已核不可行）。**落地勘误（终审写回，2026-09-26）**：守配对的不是带模板参的 consteval——终态是 `VASE_CONFIG_DETAIL_CHECK` 在宏展开处用 `MetaChoiceCount` 立 static_assert；`MetaChoices` 只作槽位搬运（模板参已去） | `MetaArray` 时代已探针定死的雷：constexpr 表里存指向 initializer-list 的指针 = 悬垂；「表是 static constexpr、指针要活到运行期」只有命名存储一条稳路——命名这一半保住，「谁命名」交给作者（表随枚举共享比按字段复制更对） |
| D78 | `ConfigBlob::Storage` 加**独立 alternative** `EnumStored{ std::int32_t Value; }`，不折进 `int32_t`；`Set` / `ToView` 的 switch 补分支 | 折进 int32 则 blob 面上 `kEnum` 与 `kInt32` 不可分——D32 与 Solve 的类型核对全废。variant 的替代位就是类型本身，加类型必须加替代（今天 `Set(kEnum)` 掉 `default` 的 ProgrammerError，即本条的第一条红测） |
| D79 | enum **成员执法点 = `MakeInstance` 的 D32 位**：blob 的 `kEnum` 值 ∉ 目标 field 的 choices → `Err`（与 kind mismatch 同通道同形态：宿主给的数据错，可恢复）；`ApplyToImpl` 仍只兜 Kind 二线（ProgrammerError），不查成员 | 既有格局是「装配点 Err、宏生成面 ProgrammerError 兜底」；成员=数据错可恢复，走 Err 不开第二规则 |
| D80 | manifest/Preset 的 enum 形：`type:"enum"` 必带 **`choices` = 非空 `{value,label}` 对象数组**（value 为 JSON 整数、value 唯一、label 唯一；D64 同族硬闸）；`default` = **label 字串**且 ∈ choices；`min`/`max` 与 `enum` 组合 → 拒（值域与区间互斥语义）。Preset/override 同写 label 字串；`Solve` 的 Coerce 点换 value、核对成员——**D51 的 error 族**，报错点名合法集；D66 作用域照吃 | 人写 Preset 看得见摸得着（需求方裁 label 方案）；「写整数值」的 B 方案被否：人手面对魔数、label 改动与 value 改动在文本上无法区分。`schemaVersion` 仍 1——独一 parser 认识新词汇（D49 原文预留） |
| D81 | `PluginCatalog` 公开面增：单点读复用现成 `ParseManifestFile`（计划期改判注 ③）；`BuildExpectation(const ManifestEntry&) → ManifestExpectation`（**拥有值返回**）；`AdoptInto(PluginHost&, PodHandle, std::string_view id) → Result<AdoptReport>`（快照定位子目录 → **重读该文件** → 收集快照兄弟名 → 新 `AdoptPlugin`；id 不在快照 → `Err`，归因在 Catalog 侧）。Console 与测试走 `AdoptInto`，不亲手搬期望 | §5.6 ①「不吃旧快照」的精确落点：路径定位吃快照（目录事实），**内容重读磁盘**（§11.2「Adopt 必须见到刚 scan 的那份」）。便利层把三步绑死，前端不可能忘喂兄弟集或喂过期内容 |
| D82 | **Issue-1 两段制**：`Solve` ② 的不动点迭代只判定归属（load/skip）；收敛后对终态表跑**归因 pass**——每个 skip 的 `SkipReason` 与 `SolveNote`（含 `Cause`）按终态产生，级联归因停在**直接提供方**（提供方被跳/被禁 → 消费者 `kMissingDependency` + `Note{kProviderSkipped, Cause=提供方 Id}`，无论提供方自身因何被跳）。Notes 按 (Kind,PluginId,Key,Cause) 字典序去重排序（承接 D53 确定性） | 现状读的是迭代中间态：提供方在本轮临时缺席与终态被禁产出同样的 Reason，归因抖动且非幂。终审裁定「两段制修（SkipReason/Note 终态语义）」的兑现形 |
| D83 | Preset **根级 unknown 键 → 结构错**：根只收 `schemaVersion`/`displayName`/`overrides` 三键。`PresetJsonTests` 钉一条 | 波 1 终审记的不对称：清单任何层级 unknown 皆拒（D49），preset 根级却放行——同一份「两边结论一致」的承诺，不该有一处双标 |
| D84 | `Solve` 的 ⑦ 路径装配步同时填 `Entry.Expected = BuildExpectation(快照条目)`（**拥有深拷，无借用窗**——与 `Id` 的 D61 窗就此解耦，计划期改判见 D67 末注）——**批装配吃快照的内容、Adopt 吃重读的内容**恰成 §5.6 两档的天然形态，前端不可能在主链上漏喂期望 | 让期望「在场」成为 Solve 的构造性保证而非调用方纪律；旁路（D68 空期望 / 手写期望）仍是显式动作，出问题时归因清晰；元数据量级的每条目一次深拷，换掉整类悬垂 |
| D85 | Console **双入口**：主链 `pod new <目录> [presetFile]`（Refresh→Solve→CreatePod，比对自动生效）+ 旁路 `pod new-raw <planFile>`（老 plan 格式与解析整体移入，Expected=nullptr）。新命令组 `catalog refresh/solve`（solve 逐行打计划+Notes = §4.4 卖点展示面；`HostProvided` 暂传空集）。`adopt` 仅 catalog 来源的局可用（raw 局响亮拒绝「adopt requires a catalog-backed pod」）。`file stage/install/show` 的路径单 accessor：raw 吃 Entry 表、catalog 吃快照 | 验证台独有价值 = 「绕过 Solve 直接喂 Host」的能力——判据 4 的分歧证人与 Host 执法材料都吃这条旁路（需求方裁；全面替换与 catalog 仅展示两案被否，死因见其处）。adopt 无第三条路径来源（D69 期望必需） |
| D86 | Samples 补 `DependentPlugin` / `FailingPlugin`（架构 §10 树的两格空位自此填上——随本文请需求方过目）。`DependentPlugin`：`Requires` Vase.Hello v1、`Provides` Vase.Farewell、OnStart 解析并调 Hello——依赖链演示位与点名消费者证人。`FailingPlugin`：OnStart 返回 `Err`、不 Provides——判据 8 的 Sample 面兼判据 4 后半的注入材料。都经 `vase_add_plugin_fixture`（规矩 5），各带 plugin.json fixture 与手写 plan 线材料 | D19 的原兑现面（M2a 延期行）；§12 判据 8 的验证方式一栏本就写着 `Samples/FailingPlugin`。测试 fixture 群（FailingStart 等）继续供 Unit/Integration 用，Samples 版专供 console 回放——演示与穷举是两个面，不合并 |
| D87 | Hello 家族 `VASE_CONFIG` 加 `Mood`（`enum class Mood : std::int32_t`，默认「安静」），Prime v2 默认「响亮」——`play` 打 label、换件后打新 label：「六型→七型」的肉眼证人，顺带是前端比对链的 Sample 侧证人。**HotSwap 换件位（`InstallPrime` 一族）与 console 换件链（`file install` + `file install-manifest` 双覆盖）必须同步覆盖 manifest**，否则新比对拒换件——此为 §11.2 链路的本意，不是缺陷；「只换二进制」保留为被拒的反面材料（§7 证人族）。**落地勘误（T14 实测，T14c 随动）**：「换件后打新 label」不在 adopt 同局兑现——adopted 实例按 D34 回放建局时 blob（本行第 1 节「回放语义原样保留」的兑现），局内换件腿仍打 `[安静]`（`prime v2` 半句证字节真换），`[响亮]` 出自拆局重建后的新 plan；synced 证人族据此立两级、各吃脚本自造态（`…ReplayReason` 局内 / `…Reason` 重建局，`hotswap.txt` 尾段加重建腿） | 配置面演进的展示位现成（M2a 的 `Repeats x1` 同一角色）；「换件不换清单 → 拒」是 §3.4 一致性链的正面证人，回放脚本把它演出来 |
| D88 | 测试子串契约迁移：`unknown plugin id` 自 Adopt 的 `Err` 面**退役**（Host 无路径账，见 D69/D71）。替代契约：request `Id != Expected.Id` → 新误用文本 `adopt request/expectation id mismatch`；文件不存在 → `EnsureResident` 原文透传。CLAUDE.md 规矩 6 括号段的子串清单随验收波同步改写 | 「identity 类子串是契约」的纪律不变，变的是子串集合——退役与新增都要留文书，否则下个读者按旧账找 |
| D89 | **描述符 `.Config` 全零（忘写的静默点）配带 `config` 数组的清单 → 比对拒**（D72 的 key 集双向等值天然成立），钉为显式语义 | §13.3「静默点照旧」自此挂限缩：忘写 `.Config` 的代价从「配置被忽略」变成「主链上加载被拒」——对插件作者的行为改变要见光，不等人自己撞（grilling Q3 裁） |

---

## 2. Host 侧：`ManifestExpectation` 与加载期执法

### 2.1 形状（新公开头 `Include/Vase/Host/ManifestExpectation.h`，header-only）

```cpp
struct ExpectedService     { std::string Name; std::uint32_t Version = 0; };
struct ExpectedChoice      { std::int32_t Value = 0; std::string Label; };
struct ExpectedConfigField {
    std::string Key;
    ValueKind Kind = ValueKind::kNone;    // 含 kEnum（§4）；kind 漂移在此现形
    std::optional<ConfigBlob::Storage> Default, Min, Max; // 拥有 variant（含 EnumStored）；nullopt = 未设
    std::string Label;
    std::vector<ExpectedChoice> Choices;  // 按序（D72 末段）
};
struct ManifestExpectation {              // 拥有值形（计划期改判注）：宿主面不过 ABI 界，零借用窗
    std::string Id, DisplayName, Version;                  // 字串等值比对
    std::vector<ExpectedService> Requires, OptionalRequires, Provides;
    std::vector<ExpectedConfigField> Config;               // key 对齐比对
};
struct AdoptRequest {
    std::string Id;
    std::filesystem::path BinaryPath;                      // 同 LoadPlanEntry：Host 内部绝对化
    const ManifestExpectation& Expected;                   // 必需（D69）；调用期内读，被调方不持有
    std::vector<std::string> SiblingBinaries;              // 快照全量插件二进制名（D71）
};
```

**拥有与借用（契约，写进头注释）**：`ManifestExpectation` 全树无借用——它是拥有值形，`Solve` 逐条目深拷一份进 `LoadPlanEntry.Expected`（D84），`AdoptInto` 构函数局部量后同步传 `AdoptPlugin`（D69）。`LoadPlan` 因此拷贝/移动自包含；**`Plan.Ordered[i].Id` 的 D61 借用窗原样不变**（那条借的是快照串，不在此形内）。比对读侧一律 `string_view`/`Storage` 化，函数签名不带生命周期魔法。`ManifestExpectation` 不含 `SchemaVersion`（D74）。

`VASE_HOST_API`：纯 POD 无行为，不需要导出宏修饰（与 `LoadPlan.h` 同格——它也没挂）；`AdoptPlugin`/`CreatePod` 已是导出类的成员。

### 2.2 比对点与执法序

**`CreatePod`**（`InspectBinary` 返回 `desc` 之后、任何注册表/账本动作之前；**只跑 `kLoad` 条目**——`kSkip` 条目没有二进制，无从比对）：`Entry.Expected` 非空 → `CompareDescriptor(*Entry.Expected, *desc)` → 失败则按 §5.2 落 `FailedPluginRecord`（`Phase::kLoad`，`Message` = 点名字段与两侧值，见 §6）+ 下游级联（5.5 机器原样复用），本条目不进装配。空 → 行为与 M2a 逐字节一致。

**`AdoptPlugin`** 的序（⓪–⑥ 编号对齐现实现注释）：

```text
⓪ 身份唯一（含 request.Id == Expected.Id 守卫，D88）
① 档三验新（已驻留分支，原文原样）→ EnsureResident
② InspectBinary（HeaderVersion 闸在前）
③ 期望比对（新增，序见 D67/D69 论证）→ 失败 = Err
③' Provides 碰撞（D43 原样）
④ 兄弟导入执法（兄弟集来源换 AdoptRequest，判定体不动）
⑤ 声明绑齐（原样）→ ⑥ 装配（原样，含 Replays 回放 D34）
```

比对实现 = `Source/Host/ManifestCompare.cpp`（新 TU），纯函数 `Result<void> CompareDescriptor(const ManifestExpectation&, const PluginDescriptor&)`——CreatePod 与 Adopt 共一处（「同一套规则只有一份」的仓库惯例）。全字段规则见 D72；消息面见 §6。

`AdoptReport` 加 `bool ManifestVerified = false;`（成功路径置 true，与 `IdentityVerified` 同段）；`CreatePod` 侧的「比过且过」不另设字段（Failed 记录缺席即证——比对在装配前，无记录 = 无分歧，读侧用 `ReportedSkips` 那类的既有断言形）。

### 2.3 `KnownBinaries` 退役与迁移

删除：成员声明（`PluginHost.h:120-121`）、`CreatePodImpl` 注册点、`AdoptPlugin` ①（连 `PluginHost.cpp:872-881` 的「M2 还债」注释与 :835-837 的「路径账保留」注释段）。`Adopt ⓪` 的「already in pod」判定不依赖它（走 `pod.Instances`/`FailureRecords`），原样。

调用点迁移全清单（实施计划按此派活）：`Tools/VaseConsole`（17+ 处经 `AdoptInto`）、`Tests/HotSwap/*`（经 `CatalogSandbox` 扩出的期望形装配 helper）、`Tests/Abi`、`Samples/Embedding`（D19「仅适配形状」）。每处迁移顺手钉上「快照→期望」的断言材料。

---

## 3. Catalog 侧：单点读、期望构造、开闸与还账

### 3.1 新公开 API（`Include/Vase/Catalog/PluginCatalog.h` / `ManifestView.h`）

- 单点读**不新立函数**（计划期改判注 ③）：波 1 已公开 `VASE_CATALOG_API Result<ManifestEntry> ParseManifestFile(path, subdirectoryName)`，`Refresh` 与「就地重读」共用它——法律唯一本就成立，本文其余处提及 `LoadManifestFile` 均读作此函数。
- `VASE_CATALOG_API ManifestExpectation BuildExpectation(const ManifestEntry&);`
  拥有式值转换（入参只读，返回自包含）。`ManifestConfigField → ExpectedConfigField`：标量转进 `Storage` variant；**enum 的 default 过唯一共享 helper 做 label→value**（`ManifestChoice` 表驱动，`Solve` 的 Coerce 调同一份——类型核对与比对共用咽喉，「同一套规则只有一份」，grilling Q5 裁）。期望形里**只存 value 形**，label 只活在 manifest/Preset 两面。
- `class PluginCatalog` 成员 `Result<AdoptReport> AdoptInto(vase::PluginHost&, vase::PodHandle, std::string_view id);`
  定位（`Find(id)` → 子目录，无则 `Err "adopt: id not in catalog snapshot"`）→ `LoadManifestFile`（重读磁盘，不吃快照内容）→ `BuildExpectation`（临时 entry，调用期）→ 收集快照全量 `LibraryFileName(binary)` 为兄弟名 → `Host.AdoptPlugin`。Catalog 链 Host（D45）使这条编排合法。
- `Solve` ⑦ 步填 `Entry.Expected`（D84）：期望构自快照条目本体——`Plan` 里 skip 条目也填（信息一致，读侧自选用不用）。

### 3.2 两条还账（行为修正，零新 API）

- **Issue-1 两段制**（D82）：`Solve` ② 的不动点循环里**不产出** Reason/Notes；收敛后归因 pass 统一产。验收证人见 §7「Solve 归因终态」格。
- **Preset 根级 unknown 键**（D83）：`PresetJson` 的 `OnlyKeys(root, {"schemaVersion","displayName","overrides"})`——现缺口是根级没跑这道闸。

### 3.3 `Directory()`/Plan 再绑定（用例钉，API 不变）

`D61` 的窗口纪律补一条执行证人：`Refresh(目录A) → Solve → Refresh(目录B)` 后，断言 ① 新 `SolveOutcome` 的 `Plan` 条目与 `Find` 全指新快照（Id 集/期望内容按 B 断言）、② `Directory()` 等于 B、③ 旧 plan 未被跨窗使用（纪律断言：证人自身遵守即成立，不做 also-dangling 检测）。落 `CatalogScanTests` 扩格（§7）。

---

## 4. `enum` 与 ABI：值面、宏面、清单面

### 4.1 值面与宏面（`Include/Vase/Config/`）

```cpp
// Value.h —— 追加
kEnum,                                  // 尾部，既有 7 值序号不动
// KindOf: std::is_enum_v<T> → static_assert(underlying == int32) → kEnum（D75）
// From<Enum>/GetAs<Enum>: Bits 低 32 位往返，GetAs 白名单 static_assert 同步扩
```

```cpp
// FieldInfo.h —— 追加（ChoiceInfo 住此头，POD 平凡拷贝）
struct ChoiceInfo { std::int32_t Value; const char* Label; };
struct FieldInfo { /* 现有七槽原序不动 */
    const ChoiceInfo* Choices = nullptr; std::uint32_t ChoiceCount = 0; };   // 尾追加（D76）
```

`Meta` 增 `.Choices`（`std::initializer_list<ChoiceInfo>` 传值**只作宏内解析材料**，存储走 D77 的命名数组）。`ConfigMacros` 的 `_DETAIL_FIELD` 扩：字段 tuple 第 5 项（可选）= choices 花括号列；`KindOf==kEnum` 而 tuple 无第 5 项 → `static_assert` 编译失败；非 enum 而写了 choices → 同点失败。生成物：`static constexpr std::array<ChoiceInfo, N> kChoices_<Field>` + FieldInfo 引其 `.data()/.size()`。`kApplyTo` 经 `GetAs<Field枚举类型>` 自然成立（宏面零特例）。

`PluginDescriptor.h`：`kHeaderVersion = 3U` + 注释一行（`1→2（M2a-T3）`句式续 `2→3`）。头注释里「enum 暂不提供（D22）」的过期句在 `Value.h`/`FieldInfo.h` 各处顺手收（存量注释随改随收，不专项回扫）。

### 4.2 拥有面（`ConfigBlob`）与装配执法

`Storage` 加 `struct EnumStored { std::int32_t Value; };`（D78）。`Set`/`ToView` 补 `kEnum` 分支；`FromDefaults`（走 `Set(field.Name, field.Default)`）自然覆盖。`MakeInstance` 的 D32 核对点扩义（D79）：kind 等值后，`kEnum` 再查 `Value ∈ FieldInfo.choices`，∉ → `Err`（消息形态见 §6）。

### 4.3 清单/Preset schema（`Source/Catalog`，`schemaVersion` 仍 = 1）

| 项 | 规则 |
|---|---|
| `type:"enum"` | 合法值域自本波开闸（D49 预留兑现）；**必带 `choices`**，非空数组 |
| `choices` | 元素恰 `{value,label}` 双必填对象；value = JSON 整数（∈ int32 范围，复用 T8 的越界拒绝通道）、label = 非空字符串；**value 唯一、label 唯一**（D64 同族）；条目 `type ≠ enum` 而带 `choices` → 拒 |
| `default`（enum） | label 字串且 ∈ choices；非 label 形（写整数/写错字）→ 拒 |
| `min`/`max` | 与 `type:"enum"` 组合 → 拒（D80） |
| Preset/override | 值为 label 字串；`Solve` Coerce 换 value（成员核对 = D51 error 族、D66 全量作用域；报错点名合法集） |

`ManifestConfigField` 增 `std::vector<ManifestChoice>`（`{int32 Value; std::string Label;}`，拥有形——自借用面与 `DefaultsRaw` 同条目纪律）。比对面：`Choices` 按序逐项（D72 末句）。

**中间形态（D59 既有机制对 enum 的兑现，不是新账）**：enum 覆盖在 `LoadPreset` 段以 **kString（label）** 入 `ConfigBlob`，过 `Solve` 的 Coerce 才成 `EnumStored`——三层合并全程按 label 整体替换，最终形由 Solve 定；raw plan 手写 `EnumStored` 的旁路同样合法（D78 的 `Set` 分支）。

### 4.4 Sample 证人（D87）

`Samples/HelloCommon` 的 `HelloConfig` 加 `Mood` 字段（choices：安静/响亮）；`HelloPlugin` 默认「安静」、`HelloPluginPrime` 默认「响亮」；`Embedding`/`HelloPlugin` 的 `OnStart` 打 label（「play 打出的 `… x1` 同位证人」）。两插件的 manifest fixture 随带同步（比对域含 choices）。

---

## 5. 前端（D19）：Console 双入口与 Samples

### 5.1 命令面（改接清单；老 plan 格式本体不动）

```text
catalog refresh <pluginDir>        新：Refresh + 打 Ids/Warnings；失败打错并 MarkFailed
catalog solve [presetFile]         新：以最近一次 refresh 的快照跑 Solve，
                                   逐行打（序、decision、SkipReason、Notes 的 Kind+Cause）；
                                   HostProvided 传空集（第 0 节）
pod new <pluginDir> [presetFile]   新主链：refresh（总是重跑，新鲜度归命令面）→ solve → CreatePod
                                   （条目自动带期望，加载期比对在主链默认生效）
pod new-raw <planFile>             老 `pod new` 的 plan 解析与行为整体移入（Expected=nullptr）
adopt <id>                         catalog 局 → AdoptInto；raw 局 → 响亮拒绝（D85 文本）
eject / swap                         判定不变；swap 每轮内的 adopt 同走 AdoptInto
file stage / install / show          二进制线行为不变；登记路径 = 单 accessor
                                   （raw：Entry 表；catalog：快照 BinaryPath 的父目录 + filename）
file stage-manifest <id> <src>       新：清单字节进暂存槽——Staged 扩为**按型双槽**
                                   （binary/manifest 各一，install 只消费自己那型，错配即无路）
file install-manifest <id>           新：暂存清单落至 <id> 的清单位；两条新命令 raw 局同样响亮拒绝（随 D85 形）
```

`LivePod` 记账扩：来源两态（planPath 或 catalogDir + presetPath）；`Staged` 由单槽扩为**按型双槽**（binary/manifest 各一——install 系命令只消费自己那型，grilling Q4 裁）。**catalog 随局归属**：`pod new` 为该局**自持一份 `PluginCatalog`**（refresh 自己的目录），`adopt` 用 Active 局的那份——会话级 catalog 归 `catalog` 预览命令组专用。否则第二局 `pod new` 一换目录，第一局的 `adopt` 就在别人的快照里找 Id。退出码规则（全程 Clean）不动。回放脚本（ctest 的 `VaseConsole*` 与 wiki 素材）随命令面重排：老 `pod new <plan>` 证人改指 `pod new-raw`，新增主链证人（含比对生效与 catalog solve 输出面）。`wiki/vase-console-use.md` 整节改写列入文书义务（§8），它是实有工具文档不是提案。

### 5.2 Samples 两员（D86）与 `Embedding`

`Samples/{DependentPlugin,FailingPlugin}/`（经 `vase_add_plugin_fixture`，只链 VasePod；`VASE_PLUGIN` 描述符按 §3.1 既有形）+ 各自的 manifest fixture 材料（`Tests/Integration/fixtures/manifests/` 扩目录）。`Embedding`：`AdoptPlugin` 调用点若存在则迁 `AdoptRequest` 形，无则零改动（实施时核对，D19 口径「仅适配形状」）。

---

## 6. 错误与诊断通道（含子串契约迁移）

- 比对失败消息（CreatePod 记录 / Adopt Err 同一格式器，住 `ManifestCompare.cpp`）：`manifest/binary mismatch for "<id>": <字段路径> — manifest "<v1>" != binary "<v2>"`（字段路径形如 `provides[Vase.World v1] missing in binary` / `config "mood".choices[2].label` / `displayName`）。**子串契约**：`manifest/binary mismatch` 一个 token 即够（判据不逼读者解析全消息，但字段细节留给人）；测试按「Err 有 / Status 对 / 子串含 token」三层断言。
- 新误用契约（D88）：`adopt request/expectation id mismatch`。
- 退役：`unknown plugin id`（Adopt 面）。既有钉它的用例（HotSwap/Abi 若干）迁移至新契约或「not in catalog snapshot」（走 `AdoptInto` 的调用改钉 Catalog 侧文本）。
- Solve 的 enum 报错（成员/类型）：沿用 D51 error 族通道，消息点名 `<plugin>.<key>: "<label>" is not one of [安静, 响亮]`；断言按 `Result` 与最小子串（不新增子串族——字段化不了的通道才用串）。
- `PresetJson` 根级 unknown：结构错文本与既有条目级同形（`preset "<path>": unknown field "<key>"`）。**落地勘误（终审写回，2026-09-26）**：两面 parser 实发 `unknown top-level field "<key>" (D83)`（清单根级同族 `… (D49)`）——`top-level` 入消息以别条目级。

## 7. 测试分层与验收面

| 簇 | 层 | 覆盖 |
|---|---|---|
| `LoadTimeComparisonTests` | Integration | 判据 13 逐字段：`displayName`/`version`/三类服务集的缺员·超员·版本错位·config 的 key·Kind·Default·Min·Max·Label·Choices（value/label/序）各一条篡改证人；「带期望且一致 → 装载成功」「Expected=nullptr → M2a 行为」两条基线；手写期望旁路一条（D68 的 raw 面） |
| `AdoptManifestTests` | HotSwap | 就地重读三态：改清单不改二进制 → Err mismatch；换件不更清单 → Err；**DLL+清单同步覆盖 → kAdopted**（D87 正面证人）；`ManifestVerified` 字段断言；request/期望 Id 不匹配误用格（D88） |
| `EnumConfigValueTests` / 宏面 | Unit | Kind 往返（From/GetAs）、`kEnum` 进 `ConfigBlob` 的 Set/ToView、宏产出 `kChoices_<Field>` 表正确性、两种编译期失败（enum 无 choices / 非 enum 带 choices——编不过的测试以「形状反例不进编译面」处理，钉文档说明）、`underlying ≠ int32` 同此 |
| `ManifestJson`/`PresetJson` 扩 | Unit | §4.3 闸表逐条：choices 非空/双必填/唯一、default label 形、min·max×enum 拒、value 越 int32 拒（复用 T8 通道）；根级 unknown（D83） |
| `SolveTests` 扩 | Integration | Coerce label→value、成员 error 族点名合法集、D66 照吃、**Issue-1 终态归因**：级联链 A←B←C 禁 A → B、C 的 Reason/Notes 只出终态（两段制证人）、双跑幂等（D53 延用） |
| `CatalogScanTests` 扩 | Integration | D84 的 Expected 自动填（拥有深拷；断言「换 Refresh 后旧 plan 的 Expected 仍完好、`Id` 窗纪律另在 §3.3」）、§3.3 再绑定三断、`ParseManifestFile` 直调与 `Refresh` 同闸（同一函数，构造性成立） |
| `ConfigApplyTests` 扩 | Integration | `MakeInstance` 成员执法：手工 blob 灌非法 enum 值 → Err（D79）；合法 → Apply 后读回枚举 |
| Abi/头版本 | Abi | HeaderVersion=3 树内生效证人 + 伪造 2 的旧二进制拒（判据 12 现形扩展，不新造手法） |
| Console 套件 | （ctest -R VaseConsole） | 命令面重排后的全链：catalog 主链、new-raw 旁路、solve 展示、raw 局 adopt/清单 install 拒绝，外加新证人族——**`VaseConsoleManifestMismatch*`**（`…IsFailure` 钉退出码 + `…Reason` 钉 `manifest/binary mismatch` 子串，随 `VaseConsoleEjectBlocked*`/`AdoptBlocked*` 先例形）与正面半句 **`VaseConsoleSwapManifestSynced`**（`file install` + `file install-manifest` 双覆盖 → adopt 通过、回放打新 label，D87 的肉眼证人入 ctest 面）。**落地勘误（终审写回，2026-09-26）**：两族实名带 Adopt 前缀——`VaseConsoleAdoptManifestMismatch*` / `VaseConsoleAdoptManifestSynced*`（T14 更名：前缀使全族被规矩 6 窄选择子拉进热插拔验证；Synced 族另含 Replay/Show/Adopt 三条兄弟用例） |
| `Embedding` 回放 | 既有位 | 新 Sample 进 play/swapdemo 素材（§5.2）；`Mood` 换件前后 label 证人 |

- **规矩 6 全触发**：`-R 'HotSwap|Eject|Adopt'` 双平台各自留证据；承重 flag 未动但 Adopt 路径动了大手术，T12 主循环重跑。
- **环境义务**：六线删树重配全量（`win-verify.cmd` + `linux-verify.sh`；`z-applocal` 假红先单线重跑再判）；tidy 三线各跑、正文双 0（TU 基数上跳：`ManifestCompare.cpp` + 各测试新源，验收记）；format 门（新文件先 `git add`）。
- **基数**：六线现值 194/193/194/193/196/195 上预计 **+35±10**（判据 13 逐字段 ~10、判据 4 后半 2–3、enum 各簇 15–20、handoff 3–6、console ~5）；增量成因记本文，**数只住 CLAUDE.md**（规矩 7）。debug/release −1 与 Linux +2 两处差值成因不变（新用例无一受 `#ifndef NDEBUG` 或平台门控；HotSwap 新格若需文件锁语义按规矩 6 双平台同跑，不引入新平台门）。

## 8. 文书义务（验收时同步；规矩 7 口径：只住 CLAUDE.md 的真值不复制进技能）

- **CLAUDE.md**：项目状态行（M2b 第二波完成；「尚未确定的事项」表相应行同步）；「磁盘上是这些」清单 + `ManifestExpectation.h` + Samples 两目录；六线基数表；**规矩 6 括号段**——判定子串迁移后的新契约清单（D88）；「M2b 波 1 新增测试文件」段续写波 2 增量。
- **wiki/vase-architecture.md 勘误回挂**：§3.4「全部由代码生成」+ `binary`/`enabledByDefault` 免生成免比对（D73）；§5.6 判定流 ① 的落点句（单点重读 = `LoadManifestFile`，Host 吃期望形，D67/D81）；§12 判据 4 后半与 13 标已落、判据 8 的 Samples 落地；§10 Samples 树补两目录（该节标「已确认」——**本条随 spec 评审一并请需求方过目**，D86）；§3.3 两版本号的加载期闸补「兑现形态 = 读取处即闸」（D74）；**§3.3「version……没有任何一处消费它／不参与任何判断」限缩为「不参与求解判断；§3.4 的一致性比对是其唯一消费点」，「插件升级不需要求解逻辑参与」句补「但需重生成清单」**（grilling Q1 裁）；§13.3「静默点照旧」挂「波 2 后主链上不再静默」（D89）。
- **wiki/vase-console-use.md**：命令表 + catalog 组 + 双入口语义 + `file stage-manifest`/`file install-manifest` 两条换件清单命令 + 回放资产清单 + 比对生效说明（plan 格式节保留，改标题为 `pod new-raw`）。
- **技能**：不搬运基数/文件清单（规矩 7）；`references/abi-boundary.md` 补一行「描述符布局变更现例 = M2b 波 2 的 FieldInfo choices 槽（v2→v3）」——判据与理由类，允许两边各写。

## 9. 风险与预期中的行为性红

1. **换件不重生成清单 → Adopt 被比对拒**（§11.2 本意）：`InstallPrime` 族与 console `file install` 的换件位必须覆盖 manifest——不随带即证人自拒，实施第一波会撞到。
2. **波 1 手写 manifest fixture 的展示字段校准**：`displayName:"探针"` 类与描述符抄不齐的值在比对生效后即拒——`Tests/Integration/fixtures/manifests/` 与沙箱材料全数按描述符校订，列实施任务。
3. **Hello 加字段 → 描述符变**：Hello 系 manifest 材料随带 `Mood` 的 choices/默认值同步抄写（比对域含 config）。
4. **兄弟集欠报防不住**（D71 信任模型）：调用方纪律明记在头注释与本文；不是 Host 执法破口（严格强于旧账），但要在验收记录里点明它属「无法自动化验证」族（§12 判据 19 同列）。
5. **迁移面大**：Adopt 调用点 17+ 与判据 13 的新证人群同时落地——规矩 6 的 HotSwap 双平台证据在**每个动 Adopt 的任务波**之后即时收，不留到波尾（M2a 教训「任务波即跑 cl 线」的同类节奏要求）。
