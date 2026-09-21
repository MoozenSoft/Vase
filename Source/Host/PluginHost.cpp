#include "Vase/Host/PluginHost.h"

#include "Vase/Detail/Counters.h"
#include "Vase/Detail/Fail.h"
#include "Vase/Detail/ImageInspect.h"
#include "Vase/Detail/RegistryBus.h"
#include "Vase/Detail/Result.h"
#include "Vase/Effect/EffectScope.h"
#include "Vase/Host/Evidence.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/Loader.h"
#include "Vase/PluginDescriptor.h"
#include "Vase/Pod/Context.h"
#include "Vase/Pod/DependencyLedger.h"
#include "Vase/Pod/Pod.h"
#include "Vase/Service/Service.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

// —— 零开销条约登记（.claude/skills/cpp20-zero-overhead 第 8 组）——
// 本文件引入的全部付费点都在「装配 / 拆除」上，每个都每 Pod 或每插件发生常数次，
// 不在任何循环或稳态重复路径里（条约 4.2 的临界路径是每帧 / 每对外事件 / 稳态每入口）。
// 本任务未做阶梯 1/2/3/4 的任何测量，故不主张任何性能结论；下面写的是「付了什么」
// 与「为什么值」。复核触发：CreatePod / DestroyPod / EjectPlugin / AdoptPlugin 若进入
// 每帧或每事件的路径，或插件数进入三位数，回来做阶梯 2 的分配计数。
//
//   位置                          付了什么                      为什么值
//   Pod::Pod ×1/Pod               4 次堆分配（注册表 / 总线 /    §5.1 的「销毁即归零」就是这
//                                 根 Scope / 根 Context）        批 unique_ptr 的所有权；
//                                                               合并分配会牺牲类型清晰与析构序
//   CreatePodImpl ×1/新建槽       1 次槽分配（**复用空槽走      槽位稳定是「句柄 = 索引 + 代际」
//                                 :165-171，是 0 次**）          的前提
//   CreatePodImpl ×N/插件         3 次堆分配（LiveInstance /    每插件一个 Context/Scope 是 §2.1
//                                 Scope / Context）+ OwnerLabel  的实例边界；两份串都不可省——
//                                 一份 string 拷贝 + 一次         拥有型拷贝是「别赌字面量生命
//                                 `std::string id(entry.Id)`     周期」，id 是入口要的 NUL 结尾
//                                                               形态（string_view 不保证）
//   CreatePodImpl ×N/插件         KnownBinaries 一次插入：1 个   §5.6「M1 无清单，以计划登记
//                                 结点分配，Id 超过 15 字符时     代替」；短 Id 走 SSO，长 Id 再
//                                 另加一份键 string              加一次分配
//   CreatePodImpl ×1/Pod          1 次 std::function 间接调用    PodOptions::Stage0 由宿主提供，
//                                                              类型编译期不可知（条约 1.3）
//   CreatePodImpl 失败路径        每处失败约 4 次 std::string     §5.2「保留记录不保留实例」——
//                                 构造（Id 拷贝、消息拷贝、       失败的代价本来就在诊断上；
//                                 消息拼接、to_string 临时量）   不进稳态路径
//   DestroyPod ×1/Pod             PodReport 按值返回：Failures   §5.1：DestroyPod 永不失败，
//                                 与 Residuals 的 vector 拷贝     报告是唯一出口（HotSwapLog
//                                 （HotSwapLog 是 move）          是 std::move，不拷贝）
//   EjectPlugin ×1/被卸插件   EdgesTo 的一张入边 vector      §5.6 的 ①②③ 判定流本体。
//                                  （M1 边数十条，线性扫即   EdgesTo 的 vector 是账本既有
//                                  正确复杂度）＋ 1 次 Scope   API（T9）的形状——本任务不新增
//                                  Dispose ＋ 全局闸扫活槽      索引；全局闸不分配
//                                  与实例（不分配）
//   AdoptPlugin ×1/被领回插件     档三 + §8.7 各读**一遍整个文件**  判据来自文件字节，没有
//                                 （FileIdentity / 导入解析，各一次  「流式验新」的既有 API 可
//                                 vector<uint8_t> 按文件大小分配）  复用；M1 的插件是几百 KB
//                                 ＋ 2 次 ImageIdentity 的 Bytes    级，且 Adopt 是人工进出的
//                                 ＋ 出边/兄弟名两组 vector；       动作、不在稳态路径上
//                                 装配同 CreatePodImpl 的 3 次分配
//   ~PluginHost ×1/进程           AllResident() 拷贝整张驻留表    一次，退出路径

namespace
{

// §1.4：进程内同时只允许一个 PluginHost 存活。守卫在构造置位、析构释放——
// 测试之间串行重建是合法的，「两个 Host 同时在世」才是要拒的形态。
// 包一层函数而不是裸命名空间级变量：cppcoreguidelines-avoid-non-const-global-variables
// 报的正是后者。进程唯一性不变——函数内 static 也是进程一份。
bool& HostAlive()
{
    static bool alive = false;
    return alive;
}

// §8.1 的唯一一跳：插件的唯一导出入口。签名与 VASE_PLUGIN 生成的 VasePlugin_GetPlugin 一致。
using PluginEntryPoint = const vase::PluginDescriptor* (*)(const char*);

// §8.1 唯一一跳的**除装配外**全过程：入口符号 → 描述符 → HeaderVersion 闸。CreatePod 的
// 宽松装载与 Adopt 的拒绝共用它，三个失败点的原文因此只有一份（CreatePod 另按 §5.2 落记录）。
vase::Result<const vase::PluginDescriptor*> InspectBinary(const vase::detail::BinaryRecord& record,
                                                          std::string_view pluginId)
{
    const vase::Result<void*> symbol = vase::detail::Loader::Symbol(record, "VasePlugin_GetPlugin");
    if (!symbol.IsOk())
    {
        return vase::Result<const vase::PluginDescriptor*>::Err(symbol.GetError());
    }

    // void* → 函数指针只有 reinterpret_cast 一条路：static_cast 这对类型不合法，
    // memcpy 与 std::bit_cast 都被 bugprone-bitwise-pointer-cast 拒（两者均实测）。
    // 与 LoaderWindows.cpp 的 GetProcAddress 那处同类、同理由。
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto getPlugin = reinterpret_cast<PluginEntryPoint>(symbol.Value());
    const std::string id(pluginId); // 入口收 const char*：string_view 不保证以 '\0' 结尾
    const vase::PluginDescriptor* desc = getPlugin(id.c_str());
    if (desc == nullptr)
    {
        return vase::Result<const vase::PluginDescriptor*>::Err(
            vase::Error{"VasePlugin_GetPlugin returned null for the requested id"});
    }

    // 描述符的**第一个**字段先读：拦的是「插件与宿主 Vase 头版本不一致」（§3.1/§8.3，12 节 #12）
    if (desc->HeaderVersion != vase::kHeaderVersion)
    {
        return vase::Result<const vase::PluginDescriptor*>::Err(
            vase::Error{"HeaderVersion mismatch: binary " + std::to_string(desc->HeaderVersion) + ", host " +
                        std::to_string(vase::kHeaderVersion)});
    }
    return vase::Result<const vase::PluginDescriptor*>::Ok(desc);
}

// §8.7 的文件名比对大小写不敏感：Windows 的导入表保留创建时大小写，而链接器来源不一
// （LoaderTests 的 ProbeImportsVasePod 为此把 expected 写成小写形）。手写 ASCII 而不引
// <cctype>：std::tolower 的 char 重载要先转 unsigned char，为几个字母不值当。
std::string LowerAscii(std::string_view text)
{
    std::string lowered(text);
    for (char& letter : lowered)
    {
        if (letter >= 'A' && letter <= 'Z')
        {
            letter = static_cast<char>(letter - 'A' + 'a');
        }
    }
    return lowered;
}

// §0.2「每次进出必须有账可查」：Eject / Adopt 的每条拒绝都要带「谁、哪一步」。ErrorContext
// 在指定初始化下四个字段必须全写（-Wmissing-designated-field-initializers 是 error），
// 故把构造收到这一处。
[[nodiscard]] vase::Error Refusal(std::string_view pluginId, vase::Phase stage, std::string message)
{
    vase::ErrorContext context;
    context.PluginId = std::string(pluginId);
    context.Stage = stage;
    return vase::Error{std::move(message), std::move(context)};
}

vase::DiagnosticSnapshot Snapshot(const vase::detail::DiagnosticCounters& counters)
{
    return vase::DiagnosticSnapshot{
        .Effects = counters.Effects,
        .Services = counters.Services,
        .Subscriptions = counters.Subscriptions,
        .PluginInstances = counters.PluginInstances,
        .Scopes = counters.Scopes,
    };
}

vase::DiagnosticSnapshot Difference(const vase::detail::DiagnosticCounters& counters,
                                    const vase::DiagnosticSnapshot& baseline)
{
    // 进程级计数单调递增，故差分 = 现值 - 基线（§9.2）。**不是绝对值**。
    return vase::DiagnosticSnapshot{
        .Effects = counters.Effects - baseline.Effects,
        .Services = counters.Services - baseline.Services,
        .Subscriptions = counters.Subscriptions - baseline.Subscriptions,
        .PluginInstances = counters.PluginInstances - baseline.PluginInstances,
        .Scopes = counters.Scopes - baseline.Scopes,
    };
}

} // namespace

namespace vase
{

PluginHost::PluginHost()
    : ThreadId(std::this_thread::get_id())
{
    if (HostAlive())
    {
        detail::ProgrammerError("a PluginHost is already alive in this process");
    }
    HostAlive() = true;
}

PluginHost::~PluginHost()
{
#ifndef NDEBUG
    // 宿主死时账没平 = 框架自己漏了——先于任何宿主受罚（§9.2）。
    assert(Counters.Effects == 0 && Counters.Services == 0 && Counters.Subscriptions == 0 &&
           Counters.PluginInstances == 0 && Counters.Scopes == 0);
    for (const std::unique_ptr<PodSlot>& slot : Slots)
    {
        assert(!slot->Alive && "PluginHost destroyed while a Pod is still alive");
    }
#endif

    // §8.1 末行：宿主退出时全部卸下，逆装载序（AllResident 按装载序返回）。
    // 承重：Loader 的析构**不**释放驻留镜像——漏掉这一段就是孤儿平台句柄
    // （模块仍然映射着，此后任何 Loader 实例都看不见它）。
    const std::vector<detail::BinaryRecord> resident = Loader.AllResident();
    for (const detail::BinaryRecord& record : std::views::reverse(resident))
    {
        const detail::UnloadEvidence evidence = Loader.Unload(record);
        // 档二证据只记不判：进程正在退出，日志即可——端到端断言推迟到 CI/M5
        // （gtest 测不了自家析构）。Linux 看 MappingRemoved，Windows 看 ReopenWritable。
        const bool unloaded = evidence.ReopenWritableIsMeaningful ? evidence.ReopenWritable : evidence.MappingRemoved;
        if (!unloaded)
        {
            detail::WriteStderr("Vase: binary still resident at host shutdown: ");
            detail::WriteStderr(record.Path.string());
            detail::WriteStderr("\n");
        }
    }

    HostAlive() = false;
}

void PluginHost::AssertBoundThread(const char* api) const
{
    // §1.4：生命周期动作全部串行于绑定线程。这里用 ProgrammerError 而不是裸 assert——
    // 「另一个线程动了装配」没有可恢复的形态，两个构建都必须终止（#16）。
    if (std::this_thread::get_id() != ThreadId)
    {
        std::string message{"PluginHost::"};
        message.append(api);
        // 子串 `not the bound thread` 是**契约**：T8 的 death test 按它匹配（见计划 T8 的
        // Interfaces 行）。改措辞可以，这三个词不能丢。
        message.append(" called from a thread that is not the bound thread");
        detail::ProgrammerError(message);
    }
}

PluginHost::PodSlot* PluginHost::FindSlot(PodHandle handle)
{
    if (handle.Index >= static_cast<std::uint32_t>(Slots.size()))
    {
        return nullptr; // 默认构造的 PodHandle{0,0} 也走这一支（Generation 从 1 起）
    }
    // 非常量下标 operator[] 过不了 cppcoreguidelines-pro-bounds-*，与 EffectScope 同写法。
    // 两道 *：Slots 存的是 unique_ptr，解一次拿到槽本身。
    PodSlot& candidate = **std::next(Slots.begin(), static_cast<std::ptrdiff_t>(handle.Index));
    if (!candidate.Alive || candidate.Generation != handle.Generation)
    {
        return nullptr; // §5.1：句柄失效是预期内，不是错误
    }
    return &candidate;
}

Result<PodHandle> PluginHost::CreatePod(const LoadPlan& plan, const PodOptions& options)
{
    return CreatePodImpl(plan, options);
}

Result<PodHandle> PluginHost::CreatePodImpl(const LoadPlan& plan, const PodOptions& options)
{
    // ① 绑定线程 + 槽：优先复用空闲槽，复用即推进代际——旧句柄从此解不开新 Pod（#15）
    AssertBoundThread("CreatePod");

    std::uint32_t index = 0;
    PodSlot* slot = nullptr;
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(Slots.size()); ++i)
    {
        // 非常量下标 operator[] 过不了 cppcoreguidelines-pro-bounds-*，与 EffectScope 同写法。
        // 两道 *：Slots 存的是 unique_ptr，解一次拿到槽本身。
        PodSlot& candidate = **std::next(Slots.begin(), static_cast<std::ptrdiff_t>(i));
        if (!candidate.Alive)
        {
            slot = &candidate;
            index = i;
            ++slot->Generation;
            break;
        }
    }
    if (slot == nullptr)
    {
        Slots.push_back(std::make_unique<PodSlot>());
        slot = Slots.back().get();
        index = static_cast<std::uint32_t>(Slots.size()) - 1U;
        slot->Generation = 1; // 0 留给默认构造的 PodHandle：它不该解析到任何槽
    }

    // 槽位复用前先清该序号的账：正常应已空（DestroyPod 清过），这是防漏摘的防御网。
    Ledger.ClearPod(index);
    // ② 基线快照 + Pod。**快照必须先取**：根 Scope 属于本 Pod，它的 +1 要落在差分里；
    // 取在 Pod 构造之后会让拆局归零时 Scopes 差分成 -1（实测：scopes=18446744073709551615）。
    slot->Baseline = Snapshot(Counters);
    // 裸 new 就地喂给 unique_ptr：构造函数私有，std::make_unique 的访问检查发生在
    // std 的上下文里、过不了（实测 clang-cl: calling a private constructor of class 'vase::Pod'）。
    slot->Inner = std::unique_ptr<Pod>{new Pod(CountersPool, Counters, &Ledger, index)};
    slot->Alive = true;
    slot->HotSwapLog.clear();

    Pod& pod = *slot->Inner;
    const auto recordFailure = [&pod](std::string_view pluginId, Phase stage, std::string_view message)
    {
        pod.FailureRecords.push_back(FailedPluginRecord{
            .Id = std::string(pluginId),
            .Stage = stage,
            .Message = std::string(message),
        });
    };
    // §5.6 四条补角的「Failed 可被 Eject」：失败插件的**记录 + 驻留镜像**。镜像照记是
    // 刻意的——拒的是实例，不是文件（§8.1 的进/出表里没有「HeaderVersion 不匹配就把文件
    // 踢出去」这一行）；不放它，T10 的 Eject 就永远解不开一个失败插件的货。
    // EnsureResident 自己失败那支**不**调用本函数：那种失败根本没有镜像可记。
    const auto recordFailedBinary = [&pod](std::string_view pluginId, detail::BinaryRecord* binary)
    { pod.FailedBinaries[std::string(pluginId)] = binary; };

    // ③ 阶段 0（§5.3）：宿主服务在**根 Context** 注册——来源由此判为 kHost（§6.1），
    //    且根上的注册不落账本边（T9）。
    if (options.Stage0)
    {
        options.Stage0(pod.Root());
    }

    // ④ 阶段 1（§5.3）：逐条目按数组序（= 拓扑序）装载。宽容语义：单条失败只落记录（§0.3-4）
    for (const LoadPlanEntry& entry : plan.Ordered)
    {
        KnownBinaries[std::string(entry.Id)] = entry.BinaryPath; // §5.6：M1 无清单，以计划登记代替

        const Result<detail::BinaryRecord*> resident = Loader.EnsureResident(entry.BinaryPath);
        if (!resident.IsOk())
        {
            recordFailure(entry.Id, Phase::kLoad, resident.GetError().Message());
            continue;
        }

        const Result<const PluginDescriptor*> inspected = InspectBinary(*resident.Value(), entry.Id);
        if (!inspected.IsOk())
        {
            recordFailure(entry.Id, Phase::kLoad, inspected.GetError().Message());
            recordFailedBinary(entry.Id, resident.Value());
            continue;
        }

        std::unique_ptr<Pod::LiveInstance> live = MakeInstance(pod, *resident.Value(), inspected.Value());
        const Result<void> loaded = live->Instance->OnLoad(*live->Ctx);
        if (!loaded.IsOk())
        {
            // §5.2 失败即时回收：Scope 先撤、实例后销毁；下游级联等 M2 依赖图。
            // 账本同样先摘（§5.6 规则②）：失败实例在 OnLoad 半途可能已落过边（Get 成功、
            // 随后才返回 Err 的形态）。它的 Instances 条目在 teardown 里被 continue 跳过，
            // 不在这里摘就只剩 DestroyPod 的 ClearPod 兜底——局内任何反查都会看见死边的 cookie。
            DiscardInstance(pod, *live);
            recordFailure(entry.Id, Phase::kLoad, loaded.GetError().Message());
            recordFailedBinary(entry.Id, live->Binary);
            // 失败**残留条目**留在 Instances 里（Instance 已置空）：§5.2 的「保留记录」在
            // 装配点上的形态，也是 Eject ②' 要摘的那条空壳（见 EjectPlugin 的注释）。
            pod.Instances.push_back(std::move(live));
            continue;
        }

        live->State = Pod::LiveInstance::InstanceState::kLoaded;
        pod.Instances.push_back(std::move(live));
    }

    // ⑤ 阶段 2（§5.3）：仅对已 Loaded 的实例跑 OnStart，仍按数组序；失败同败同治
    for (const std::unique_ptr<Pod::LiveInstance>& live : pod.Instances)
    {
        Pod::LiveInstance& instance = *live;
        if (instance.State != Pod::LiveInstance::InstanceState::kLoaded)
        {
            continue;
        }
        const Result<void> started = instance.Instance->OnStart(*instance.Ctx);
        if (!started.IsOk())
        {
            // 同上：OnStart 失败也可能已落过边（OnLoad 里的 Get 是在本实例 cookie 上落的）。
            DiscardInstance(pod, instance);
            pod.FailureRecords.push_back(FailedPluginRecord{
                .Id = instance.OwnerLabel,
                .Stage = Phase::kStart,
                .Message = started.GetError().Message(),
            });
            recordFailedBinary(instance.OwnerLabel, instance.Binary);
            continue;
        }
        instance.State = Pod::LiveInstance::InstanceState::kStarted;
    }

    // ⑥ §5.5 严格模式：任一失败即整局失败——整拆半成品后返回 Err。M1 是「无级联」的
    //    基本形（级联拆除等 M2 依赖图）；宽容模式（默认）只落记录，Pod 照常交付。
    if (options.Strict && !pod.FailureRecords.empty())
    {
        const FailedPluginRecord& first = pod.FailureRecords.front();
        ErrorContext context;
        context.PluginId = first.Id;
        context.Stage = first.Stage;
        const Error error{"strict pod creation failed: " + first.Message, context};
        pod.TeardownInstancesAndRoot();
        slot->Alive = false;
        slot->Inner.reset();
        return Result<PodHandle>::Err(error);
    }

    return Result<PodHandle>::Ok(PodHandle{.Index = index, .Generation = slot->Generation});
}

std::unique_ptr<Pod::LiveInstance> PluginHost::MakeInstance(Pod& pod, detail::BinaryRecord& record,
                                                            const PluginDescriptor* desc)
{
    std::unique_ptr<Pod::LiveInstance> live = std::make_unique<Pod::LiveInstance>();
    live->Desc = desc;
    live->Instance = desc->Create();
    live->Binary = &record; // T10：本实例的驻留镜像（全局闸与 Eject 都要它）
    ++Counters.PluginInstances;

    // OwnerLabel 取拥有型拷贝：Scope 只存 const char*，字面量的生命周期不可赌。
    live->OwnerLabel = std::string(desc->Meta->Id);
    live->Scope = std::make_unique<EffectScope>(CountersPool, &Counters, live->OwnerLabel.c_str());
    // Context 的构造同 Pod：私有构造，只能裸 new 就地喂给 unique_ptr
    live->Ctx = std::unique_ptr<Context>{new Context(*live->Scope, *pod.Registry, *pod.Bus, &Counters, desc->Meta)};
    // T9 的三个挂钩在装配点填：账本、消费方 cookie（本插件实例）、Pod 序号。
    live->Ctx->Ledger = pod.Ledger;
    live->Ctx->ConsumerCookie = live->Instance;
    live->Ctx->PodIndex = pod.PodIndex;
    return live;
}

void PluginHost::DiscardInstance(Pod& pod, Pod::LiveInstance& live)
{
    // 边先死（§5.6 规则②）：回收动作里任何查账都看得见一致状态。
    if (pod.Ledger != nullptr)
    {
        pod.Ledger->RemoveByInstance(live.Instance);
    }
    live.Scope->Dispose();
    live.Desc->Destroy(live.Instance);
    live.Instance = nullptr;
    --Counters.PluginInstances;
    live.State = Pod::LiveInstance::InstanceState::kFailed;
}

PodReport PluginHost::DestroyPod(PodHandle handle)
{
    // 永不失败（§5.1）：任何句柄都给报告，失效走 HandleWasStale 这一支
    AssertBoundThread("DestroyPod");

    PodReport report;
    report.PodIndex = handle.Index;

    PodSlot* slot = FindSlot(handle);
    if (slot == nullptr)
    {
        report.HandleWasStale = true; // §5.1：句柄失效是预期内，不是错误
        return report;
    }

    slot->Inner->TeardownInstancesAndRoot();

    // §5.6「活集合→空」的兜底网：正常路径下 TeardownInstancesAndRoot 已逐实例把边摘净
    // （Debug 线另有正查断言把关），这里再按 Pod 整批清零——槽位复用会让同一 index 的
    // 上一局残边拦下一局的 Eject，这道网防的正是「漏摘一条就永久不可解」。
    Ledger.ClearPod(handle.Index);

    // 差分而非绝对值：进程级计数本就单调递增（§9.2）。基线是本 Pod 创建时的快照。
    report.CountersDiff = Difference(Counters, slot->Baseline);
    report.Failures = slot->Inner->Failures(); // 拷贝：Pod 随即整体析构

    // #10（D14）的 M1 形态：测试注入的残留 Scope 逐条点名归属。它们此刻仍在计数里
    // （Scopes 差分因此非零），报告交出后随 Pod 一起回收。
    // Count 取 Scope 自己的 EffectCount，不写死——报告要说的是「泄漏了 N 个 Effect」。
    // LeakedScopesForTest 由 PodTestPeer 独占写入，生产路径恒空，该循环体不进入。
    for (const std::unique_ptr<EffectScope>& leaked : slot->Inner->LeakedScopesForTest)
    {
        report.Residuals.push_back(ResidualEntry{
            .OwnerLabel = std::string(leaked->OwnerLabel()),
            .Count = leaked->EffectCount(),
        });
    }

    report.HotSwapLog = std::move(slot->HotSwapLog);
    slot->HotSwapLog.clear();

    // 销毁即归零：Index 留下、代际不推进（推进发生在复用那一刻）
    slot->Alive = false;
    slot->Inner.reset();
    return report;
}

Pod* PluginHost::Resolve(PodHandle handle)
{
    AssertBoundThread("Resolve");
    const PodSlot* slot = FindSlot(handle);
    return slot == nullptr ? nullptr : slot->Inner.get();
}

Result<EjectReport> PluginHost::EjectPlugin(PodHandle handle, std::string_view pluginId)
{
    // §5.6 规则④：进出全程串行于绑定线程（回收动作里允许合法调用别人的服务，见 §7.6-2）。
    AssertBoundThread("EjectPlugin");

    PodSlot* slot = FindSlot(handle);
    if (slot == nullptr)
    {
        return Result<EjectReport>::Err(Refusal(pluginId, Phase::kEject, "eject on stale pod handle"));
    }
    Pod& pod = *slot->Inner; // friend：Instances / Ledger / FailedBinaries 都是 Pod 的私有成员
    const std::string id(pluginId);

    EjectReport report;
    report.PluginId = id;

    // ① 反查：本局有没有该 Id 的**活实例**（Failed 条目 Instance == nullptr，不算活）
    Pod::LiveInstance* live = nullptr;
    for (const std::unique_ptr<Pod::LiveInstance>& candidate : pod.Instances)
    {
        if (candidate->Instance != nullptr && candidate->OwnerLabel == pluginId)
        {
            live = candidate.get();
            break;
        }
    }

    const detail::BinaryRecord* binary = nullptr;
    if (live != nullptr)
    {
        // 规则③：哪怕一条声明未用的边也拒绝，且**没有强制模式**。账本是进程级的，但服务表
        // 按 Pod 隔离——同一个实例的入边只可能来自本局的消费者，故这里不按 PodIndex 过滤：
        // 多一层过滤只会把（本不该存在的）跨局边静默放行，而那正是「留下消费者却卸了货」
        // 的最坏形态，也正是 3b 点名要防的。
        const std::vector<const detail::LedgerEdge*> incoming = pod.Ledger->EdgesTo(live->Instance);
        std::string consumers;
        for (const detail::LedgerEdge* edge : incoming)
        {
            if (!consumers.empty())
            {
                consumers.append(", ");
            }
            consumers.append(edge->ConsumerId);
        }
        if (!consumers.empty())
        {
            std::string message{"eject refused: "};
            message.append(pluginId);
            message.append(" is provided by [");
            message.append(consumers);
            message.append("]");
            return Result<EjectReport>::Err(Refusal(id, Phase::kEject, std::move(message)));
        }

        // ② 拆实例（复用 T3/T7 的既有回收机器，不新建排序机器）：边先死（§5.6 规则②）
        // → Scope 逆序回收 → 销毁实例。档① 保证只碰被卸者自己的 Scope，不需要跨实例
        // 排序（§7.6-1）；出边存活义务由串行保证——Dispose 整体完成前不会有下一个生命
        // 周期动作，故回收动作里合法调用 U 的服务（§7.6-2）。
        binary = live->Binary;
        pod.Ledger->RemoveByInstance(live->Instance);
        live->Scope->Dispose();
        // ④ 档一复验：拆完之后现场再读一遍。Scope 的读数必须在条目析构**之前**取（下面的
        // erase_if 会连同 unique_ptr 一起销毁这个 LiveInstance）。
        report.ScopeEmptied = live->Scope->EffectCount() == 0 && live->Scope->IsDisposed();
        live->Desc->Destroy(live->Instance);
        live->Instance = nullptr;
        --Counters.PluginInstances;
        std::erase_if(pod.Instances,
                      [live](const std::unique_ptr<Pod::LiveInstance>& entry) { return entry.get() == live; });
        report.LedgerHadNoIncomingEdges = true; // ① 已当场证得 EdgesTo 为空
    }
    else
    {
        // ①' §5.6 四条补角的「Failed 可被 Eject」：失败插件没有活实例，**必无入边**
        // （实例早已在 CreatePod 的失败路径上拆净，边随它死），直接进 ②'。
        const auto failed = pod.FailedBinaries.find(id);
        if (failed == pod.FailedBinaries.end())
        {
            return Result<EjectReport>::Err(Refusal(id, Phase::kEject, "plugin not in pod: " + id));
        }
        binary = failed->second;
        // ②' 拆记录与驻留镜像。Instances 里的**残留条目**（OnLoad/OnStart 失败留下的
        // Instance == nullptr 条目）一并摘掉：Eject 的语义是「本局不再记得这个插件」，
        // 留一条空壳会让后续 Adopt 的 already-in-pod 判在死条目上。摘除不影响
        // TeardownInstancesAndRoot——它本来就 continue 掉 Instance == nullptr 的条目。
        pod.FailedBinaries.erase(failed);
        std::erase_if(pod.Instances, [pluginId](const std::unique_ptr<Pod::LiveInstance>& entry)
                      { return entry->OwnerLabel == pluginId; });
        std::erase_if(pod.FailureRecords,
                      [pluginId](const FailedPluginRecord& record) { return record.Id == pluginId; });
        report.LedgerHadNoIncomingEdges = true; // 无实例必无入边
        report.ScopeEmptied = true;             // 该插件此刻没有任何存活 Scope
        report.HotSwapNote = "failed-record ejected";
    }

    // ③ 全局闸：镜像「一文件一记录」（Loader 按绝对路径去重），只有**所有 Pod 都不再
    // 引用这个记录**时才卸货（§8.1）。持有者有两类：活实例（LiveInstance::Binary）与
    // Failed 记录（FailedBinaries 的值）。只数前者，会让后 Eject 的那局把先失败那局的
    // 裸指针留成悬垂——「Unload 前所有持有者已摘干」这条不变式得在这里执行。
    //
    // 判据是**记录指针相等**，不是 id 相等，这一条承重：两条**不同 Id、同一个库文件**的
    // 计划条目会拿到同一个 BinaryRecord（路径去重；`VasePlugin_GetPlugin` 对不匹配的 id
    // 返回 nullptr，那是**普通加载失败**、框架正常受理）。按 id 判，失败那条挂在自己的 id
    // 名下、Eject 另一个 id 时闸看不见它，于是 Unload 掉别人还指着的记录 —— 那个 Pod 的
    // FailedBinaries 从此悬垂，再 Eject 一次就是 `Unload(*悬垂)`（要读它的 Path/Raw）。
    // 按记录判，跨局与跨键两个洞一起堵住，且不需要任何额外存储。
    std::size_t crossPodInstances = 0;
    std::size_t crossPodFailedRecords = 0;
    for (const std::unique_ptr<PodSlot>& other : Slots)
    {
        if (!other->Alive || other->Inner == nullptr)
        {
            continue;
        }
        for (const std::unique_ptr<Pod::LiveInstance>& instance : other->Inner->Instances)
        {
            if (instance->Instance != nullptr && instance->Binary == binary)
            {
                ++crossPodInstances;
            }
        }
        for (const auto& entry : other->Inner->FailedBinaries)
        {
            if (entry.second == binary)
            {
                ++crossPodFailedRecords;
                break; // 同一局里两条不同 Id 指着同一记录只算一个持有者（只判零与非零）
            }
        }
    }
    report.CrossPodInstancesZeroed = crossPodInstances == 0;

    if (crossPodInstances > 0 || crossPodFailedRecords > 0)
    {
        // §8.1：拆的是实例，不是镜像——闸上还有持有者指着它。
        report.BinaryActuallyUnloaded = false;
        if (!report.HotSwapNote.empty())
        {
            report.HotSwapNote.append("; ");
        }
        report.HotSwapNote.append("kept resident: other pod still references this plugin");
        slot->HotSwapLog.push_back("eject(kept):" + id);
        return Result<EjectReport>::Ok(std::move(report));
    }

    // 「活实例 ⇒ Binary != nullptr」这条不变式（Pod.h 的注释与 CreatePodImpl 的填充点）
    // 在这里承重一次解引用。Debug-only 的免费护栏，release 行为不变。
    assert(binary != nullptr);
    // 档二证据原样搬进报告（§8.2：哪个字段在本平台有判据力由 UnloadEvidence 自己声明）
    const detail::UnloadEvidence evidence = Loader.Unload(*binary);
    report.MappingRemoved = evidence.MappingRemoved;
    report.ReopenWritable = evidence.ReopenWritable;
    report.MappingRemovalIsObservable = evidence.MappingRemovalIsObservable;
    report.ReopenWritableIsMeaningful = evidence.ReopenWritableIsMeaningful;
    // 不写死 true：Unload 交回的证据才是结论（Loader.cpp 明写「调用方必须读证据，不读=漏账」）。
    // 平台选择与 ~PluginHost 同形：本平台有判据力的那个字段作判。
    report.BinaryActuallyUnloaded =
        evidence.ReopenWritableIsMeaningful ? evidence.ReopenWritable : evidence.MappingRemoved;
    // KnownBinaries[id] 的**路径账保留**：§5.6「Adopt 就地重读」的 M1 前身——清单没有，
    // 路径留着，下一手可能反悔再 Adopt（12.1 的循环正是 Eject 之后立刻 Adopt）。
    slot->HotSwapLog.push_back("eject:" + id);
    return Result<EjectReport>::Ok(std::move(report));
}

Result<AdoptReport> PluginHost::AdoptPlugin(PodHandle handle, std::string_view pluginId)
{
    // §5.6 规则④：进出全程串行于绑定线程（Adopt 会跑插件的 OnLoad/OnStart）。
    AssertBoundThread("AdoptPlugin");

    PodSlot* slot = FindSlot(handle);
    if (slot == nullptr)
    {
        return Result<AdoptReport>::Err(Refusal(pluginId, Phase::kAdopt, "adopt on stale pod handle"));
    }
    Pod& pod = *slot->Inner; // friend：Instances / FailureRecords / Registry 都是 Pod 的私有成员
    const std::string id(pluginId);

    // ⓪ 身份唯一：本局已有该 Id 的活实例或 Failed 记录 → 拒。失败**残留条目**也带
    //    OwnerLabel（Instance 已置空），故第一段同时覆盖「活实例」与「失败残留」；
    //    FailureRecords 那一路覆盖「EnsureResident 自己失败、连镜像都没有」的形态。
    for (const std::unique_ptr<Pod::LiveInstance>& candidate : pod.Instances)
    {
        if (candidate->OwnerLabel == pluginId)
        {
            return Result<AdoptReport>::Err(Refusal(id, Phase::kAdopt, "adopt refused: " + id + " is already in pod"));
        }
    }
    for (const FailedPluginRecord& failure : pod.FailureRecords)
    {
        if (failure.Id == pluginId)
        {
            return Result<AdoptReport>::Err(Refusal(id, Phase::kAdopt, "adopt refused: " + id + " is already in pod"));
        }
    }

    // ① §5.6①「清单就地重读」的 M1 代偿：路径账是 CreatePod 时记下的（KnownBinaries）。
    //    **M2 还债**：PluginCatalog 接手这张表，届时连同本条注释一起删。
    const auto known = KnownBinaries.find(id);
    if (known == KnownBinaries.end())
    {
        return Result<AdoptReport>::Err(
            Refusal(id, Phase::kAdopt,
                    "adopt refused: unknown plugin id " + id +
                        " — M1 requires prior registration through a LoadPlan (M2's Catalog "
                        "replaces this table)"));
    }
    // 绝对化与 Loader::EnsureResident 用**同一套写法**（Loader.cpp:23）：驻留表的查询键是
    // 字面比较，相对路径查不到那条。只算一次，下面两个 Loader 调用共用同一个值。
    std::error_code ec;
    std::filesystem::path abs = std::filesystem::absolute(known->second, ec);
    if (ec)
    {
        abs = known->second;
    }

    // ② 档三验新。FindResident 只用来**分流**（它决定 ReusedResidentImage、也决定要不要比对）；
    //    记录本体两分支都从 EnsureResident 取——已驻留时它按文档复用同一记录、不触发二次平台
    //    加载，于是这里不必把 const 访问器的结果 const_cast 回可变。
    const detail::BinaryRecord* resident = Loader.FindResident(abs);
    const bool reused = resident != nullptr;
    if (reused)
    {
        // §5.6 的强调句：**复用分支同样比对**（防「驻留=免检」）。特征缺失即拒绝——透传 T6
        // 的原文（它自带「补哪个链接标志」的指路），不做「跳过验新」的静默降级。
        const Result<detail::ImageIdentity> memory = detail::Loader::MemoryIdentity(*resident);
        if (!memory.IsOk())
        {
            return Result<AdoptReport>::Err(Refusal(id, Phase::kAdopt, memory.GetError().Message()));
        }
        const Result<detail::ImageIdentity> disk = detail::Loader::FileIdentity(abs);
        if (!disk.IsOk())
        {
            return Result<AdoptReport>::Err(Refusal(id, Phase::kAdopt, disk.GetError().Message()));
        }
        if (!(memory.Value() == disk.Value()))
        {
            // 档二看不见的那种替换：Linux 上旧 inode 仍映射、路径已被换血，只有特征比对分得出。
            return Result<AdoptReport>::Err(
                Refusal(id, Phase::kAdopt,
                        "adopt refused: resident image and file on disk differ — tier three caught a replacement "
                        "that tier two cannot see. escape: rebuild the pod / eject-all then retry"));
        }
    }
    const Result<detail::BinaryRecord*> ensured = Loader.EnsureResident(abs);
    if (!ensured.IsOk())
    {
        // T6：消息自带缺依赖诊断
        return Result<AdoptReport>::Err(Refusal(id, Phase::kAdopt, ensured.GetError().Message()));
    }
    detail::BinaryRecord* record = ensured.Value();

    // ③ 描述符 + HeaderVersion 闸——CreatePod 的同一份 InspectBinary（「v2 全套保留」）
    const Result<const PluginDescriptor*> inspected = InspectBinary(*record, id);
    if (!inspected.IsOk())
    {
        return Result<AdoptReport>::Err(Refusal(id, Phase::kAdopt, inspected.GetError().Message()));
    }
    const PluginDescriptor* desc = inspected.Value();

    // ④ §8.7 导入表执法读的是**文件声明**：运行期已解析的导入表会替隐式兄弟链拉边，而账面
    //    看不见的正是这种「文件里写着」的关系。命中即拒——破了这条，账本看不见的边会把引用
    //    计数焊死，Eject 当场假成功。
    const Result<std::vector<std::string>> imports = detail::Loader::ImportedLibraryNamesFromFile(abs);
    if (!imports.IsOk())
    {
        return Result<AdoptReport>::Err(Refusal(id, Phase::kAdopt, imports.GetError().Message()));
    }
    const std::string selfName = LowerAscii(abs.filename().string());
    std::vector<std::string> siblings;
    for (const auto& entry : KnownBinaries)
    {
        const std::string sibling = LowerAscii(entry.second.filename().string());
        if (sibling != selfName)
        {
            siblings.push_back(sibling);
        }
    }
    for (const std::string& imported : imports.Value())
    {
        const std::string lowered = LowerAscii(imported);
        if (lowered != selfName && std::ranges::find(siblings, lowered) != siblings.end())
        {
            return Result<AdoptReport>::Err(
                Refusal(id, Phase::kAdopt,
                        "adopt refused: binary imports sibling plugin '" + imported +
                            "' — an edge the ledger cannot see would weld the reference count (§8.7)"));
        }
    }

    // ⑤ 声明绑齐或整体拒绝（规则①：不带病入局）。提供方 kPlugin 与 kHost 都算——
    //    宿主服务活到 Pod 终老，对消费者而言与插件提供方无异。
    std::string unresolved;
    for (std::size_t index = 0; index < desc->Meta->Requires.Size(); ++index)
    {
        const ServiceRef& required = *std::next(desc->Meta->Requires.Begin(), static_cast<std::ptrdiff_t>(index));
        if (pod.Registry->Find(ServiceKey{.Name = required.Name, .Version = required.Version}) == nullptr)
        {
            if (!unresolved.empty())
            {
                unresolved.append(", ");
            }
            unresolved.append(required.Name);
        }
    }
    if (!unresolved.empty())
    {
        return Result<AdoptReport>::Err(
            Refusal(id, Phase::kAdopt, "adopt refused: unresolved declarations [" + unresolved + "]"));
    }

    // ⑥ 装配：CreatePod 的同一台两阶段机器。任一阶段失败 → 当场全拆（规则②：Adopt 失败
    //    零级联——本实例从未进 pod.Instances，边随它死，活人一根毛都不掉）→ Err。
    std::unique_ptr<Pod::LiveInstance> live = MakeInstance(pod, *record, desc);
    const Result<void> loaded = live->Instance->OnLoad(*live->Ctx);
    if (!loaded.IsOk())
    {
        DiscardInstance(pod, *live);
        return Result<AdoptReport>::Err(
            Refusal(id, Phase::kAdopt, "adopt failed at OnLoad: " + loaded.GetError().Message()));
    }
    live->State = Pod::LiveInstance::InstanceState::kLoaded;

    const Result<void> started = live->Instance->OnStart(*live->Ctx);
    if (!started.IsOk())
    {
        DiscardInstance(pod, *live);
        return Result<AdoptReport>::Err(
            Refusal(id, Phase::kAdopt, "adopt failed at OnStart: " + started.GetError().Message()));
    }
    live->State = Pod::LiveInstance::InstanceState::kStarted;

    AdoptReport report;
    report.PluginId = id;
    report.ReusedResidentImage = reused;
    // 走到这里 = 该比的那一支已经比过：驻留分支由 ② 的档三比对证得，未驻留分支由「刚由本
    // 进程从该文件装入」构造性地成立（内存即该文件的映射，没有第二个来源可比）。
    report.IdentityVerified = true;
    report.ImportEnforcementPassed = true;
    report.OutgoingEdges = pod.Ledger != nullptr ? pod.Ledger->EdgesFrom(live->Instance).size() : 0;

    // 追加**末尾**：逆拓扑序回收时它领自己的位（§5.6 四条补角「Adopted 节点没有特殊位置」）。
    pod.Instances.push_back(std::move(live));
    slot->HotSwapLog.push_back("adopt:" + id);
    return Result<AdoptReport>::Ok(std::move(report));
}

} // namespace vase
