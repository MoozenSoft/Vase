#pragma once

// PluginHost——进程级二进制层（§1、§1.4）：进程唯一、绑定创建线程、
// 拥有 Loader / ScopePool / 诊断计数 / 依赖账本（T9）/ 活 Pod 表。
// 铁律 §1.2 在本类持有的三个进程级容器上成立，且是**结构性**的——容器存的类型本身
// 就装不下实例级对象：二进制表只存 path+句柄（T6 的 Loader）、账本只存非拥有指针（T9）、
// 池只存空闲内存（T3）。**不是靠插入点断言拦住的**，别把这里读成「有断言兜底」。

#include "Vase/Detail/Counters.h"
#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"
#include "Vase/Detail/ScopePool.h"
#include "Vase/Host/Evidence.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/Loader.h"
#include "Vase/Pod/DependencyLedger.h"
#include "Vase/Pod/Pod.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace vase
{

// DiagnosticSnapshot / FailedPluginRecord / ResidualEntry / PodReport / PodHandle
// 全部定义在 Vase/Pod/Pod.h（T7 Step 2）——它们要能被下层的 Pod 存储，链接方向不允许
// 它们定义在本头文件里。本头文件只加 using 级别的引用都不需要：include 已到位。

class VASE_HOST_API PluginHost
{
public:
    PluginHost(); // §1.4：绑定当前线程；进程内同时只允许一个存活实例（守卫为全局标志，
                  // 析构释放——测试间串行重建合法，「两个 Host 同时在世」才是要拒的形态）
    // 活局未拆 → Debug 终止（§1.4/9.2 断言）；驻留镜像统一在此卸下；
    // 宿主契约「先 DestroyPod 后毁 Host」（R6-1 可见性，终审修复波补记）。
    ~PluginHost();

    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;
    PluginHost(PluginHost&&) = delete;
    PluginHost& operator=(PluginHost&&) = delete;

    Result<PodHandle> CreatePod(const LoadPlan& plan, const PodOptions& options = {});
    PodReport DestroyPod(PodHandle handle); // 永不失败（§5.1）
    Pod* Resolve(PodHandle handle);         // 失效 → nullptr（预期内）

    // §5.6 判定流左列：执法拒绝（有消费者）= **Ok + Status=kRejectedConsumers + Consumers 逐条点名**
    // （D21，3b 兑现）；成功拆除 = Ok + Status=kEjected + RemovedEdges。Err 只剩误用：
    // stale handle / not-in-pod——只有后者的子串是契约（回放证人钉它，前者无匹配方）。
    Result<EjectReport> EjectPlugin(PodHandle handle, std::string_view pluginId);

    // §5.6 判定流右列：Adopt——「就地重读清单 → 档三验新 → 声明全绑 / Provides 不碰 → 装配」。
    // 执法拒绝（声明不齐 / Provides 碰撞，D21/D43）= Ok + Status + Missing / Collisions 逐条点名，
    // 成功 = kAdopted + Outgoing 解析记录；Err 只剩误用与环境/身份类，判据子串是契约：
    // 未知 Id / 身份不符 / 特征缺失（T6 原文）/ 兄弟导入 / already in pod。
    Result<AdoptReport> AdoptPlugin(PodHandle handle, std::string_view pluginId);

    detail::DiagnosticCounters& ForTestCounters() { return Counters; } // 测试缝：
    // Lifecycle 判据直接读五项计数。交出去的是**可写引用**——测试要能读，也要能构造残留
    // 来验报告（#10/D14）。公开 API 保证的是另一件事：**不提供绕过 Effect 渠道的注册入口**，
    // 账本只由 Context 的 Provide/On 与 Effect 回收改。

    // 测试缝（T9，只读）：账本边数。生产消费者是 T10 的 Eject 反查（§5.6 规则③）。
    [[nodiscard]] std::size_t ForTestLedgerEdgeCount() const { return Ledger.EdgeCount(); }

private:
    struct PodSlot
    {
        std::unique_ptr<Pod> Inner;
        std::uint32_t Generation = 0;
        bool Alive = false;
        DiagnosticSnapshot Baseline;         // §9.2：基线是「Pod 创建时」快照，不是进程启动值
        std::vector<std::string> HotSwapLog; // §9.2 v3 进出事件流（T10/T11）；
                                             // Failed 记录由 Pod::FailureRecords 自持（§5.2「保留记录不保留实例」）
        std::unordered_map<std::string, ConfigBlob> Replays; // D34：计划灌入的 Id→ResolvedConfig，Adopt 回放
    };

    // 非绑定线程 → 终止（§1.4/#16）。**消息必须含子串 `not the bound thread`**——
    // T8 的 death test 按它匹配（见计划 T8 的 Interfaces 行），漏了这句 T8 必红。
    void AssertBoundThread(const char* api) const;
    Result<PodHandle> CreatePodImpl(const LoadPlan& plan, const PodOptions& options);

    // CreatePod 与 Adopt 共用的装配件（§5.3 的两阶段机器本身不在这里——**回调留在调用方**：
    // CreatePod 必须「先全 OnLoad 再全 OnStart」，合并进 helper 会把那个序压掉）。
    // 装配件：造配置对象 → Apply → 建实例挂 Context。配置键/类型不匹配 = 宿主给的数据错，
    // 可恢复 → Err（D32；不走 ProgrammerError）。blob 由调用方给，但供给单源 = slot->Replays
    // （R-F1）：CreatePod 条目在装配前灌表、Adopt 按 Id 查表（D34），查无 = 空 blob = 全默认。
    [[nodiscard]] Result<std::unique_ptr<Pod::LiveInstance>> MakeInstance(Pod& pod, detail::BinaryRecord& record,
                                                                          const PluginDescriptor* desc,
                                                                          const ConfigBlob& resolvedConfig);
    // 失败即时回收（§5.6 规则②：边先死 → Scope 逆序 → 实例销毁）。不碰 pod.Instances——
    // 残留条目的去留由调用方定：CreatePod 留作 Failed 证据，Adopt 直接丢弃。
    void DiscardInstance(Pod& pod, Pod::LiveInstance& live);

    // §5.2 判据 5：OnStart 失败的递归拆除。波及 = strict 声明边 ∪ 账本实边的传递闭包（D28），
    // 闭包在**拆之前**一次算全（拆除会改账本，判定必须看完整初始态）；拆按数组序倒序 = 逆拓扑序。
    [[nodiscard]] static std::vector<Pod::LiveInstance*> ComputeDownstreamClosure(Pod& pod, Pod::LiveInstance& origin);
    void TearDownLiveInstances(Pod& pod, const std::vector<Pod::LiveInstance*>& doomed, std::string_view causedById);

    // 句柄 → 槽（失效 = nullptr）。DestroyPod / Resolve / EjectPlugin 共用一份：
    // 这段「索引范围 + Alive + 代际相等」的校验写三遍就是漏一处的温床，而漏掉代际
    // 比较意味着失效句柄能改到新 Pod。
    [[nodiscard]] PodSlot* FindSlot(PodHandle handle);

    std::thread::id ThreadId;
    detail::ScopePool CountersPool; // **池与计数分开**：池是内存机器，计数是账本
    detail::DiagnosticCounters Counters;
    detail::Loader Loader;
    // §5.6 账本：进程级、跨 Pod 存活（随宿主死）。铁律 §1.2 在它身上是**结构性**的——
    // 只存非拥有 cookie 与借用字符串，装不下实例级对象。
    detail::DependencyLedger Ledger;
    std::vector<std::unique_ptr<PodSlot>> Slots;                          // deque 语义：槽位稳定（句柄=索引+代际）
    std::unordered_map<std::string, std::filesystem::path> KnownBinaries; // Id→path：
    // CreatePod 注册、Adopt 查用（T11；§5.6「M1 无清单，以计划登记代替」）。
};

} // namespace vase
