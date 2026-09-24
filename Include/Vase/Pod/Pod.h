#pragma once

// Pod——一次运行的边界（§2.1）。实例级一切的宿主：根 Context、服务表、事件总线、
// 每插件一个 EffectScope。DestroyPod 后本对象整体析构——「销毁即归零」不是修辞，
// 是字面意义的 unique_ptr 释放。
//
// 成员与访问器撞名时成员让位（Root() ↔ RootContext，Failures() ↔ FailureRecords）。
// 引用成员（Pool / Counters）配「拷贝与移动全删」：Pod 由 Host 以 unique_ptr 独占，
// 值语义从不存在。

#include "Vase/Detail/Counters.h"
#include "Vase/Detail/Export.h"
#include "Vase/Detail/Result.h"
#include "Vase/Effect/EffectScope.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/PluginDescriptor.h"
#include "Vase/Pod/Context.h"
#include "Vase/Pod/DependencyLedger.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vase::detail
{
class ServiceRegistry;
class EventBus;
class ScopePool;
struct BinaryRecord; // T10：FailedBinaries 存的是它的**非拥有**指针（Loader 表持有实体）
} // namespace vase::detail

namespace vase
{

// —— 以下报告类型住在 Pod.h（spec 3.1 的归属：SessionHandle/SessionReport 同款）。
// 链接方向是 Host→Pod 单向，FailedPluginRecord 等要能被 Pod 存储，就不能定义在
// Host 层——类型归属跟着**最底层的消费者**走。

struct DiagnosticSnapshot
{
    std::uint64_t Effects = 0;
    std::uint64_t Services = 0;
    std::uint64_t Subscriptions = 0;
    std::uint64_t PluginInstances = 0;
    std::uint64_t Scopes = 0;
};

struct FailedPluginRecord
{
    std::string Id;
    Phase Stage = Phase::kLoad;
    std::string Message;
};

// §4.4 两类跳过，从类型上就不许混（D29）。住 Pod.h 的理由与 FailedPluginRecord 同款：
// 装配期生成、Pod 自持，链接方向不许它上 Host 头。
enum class SkipClass : std::uint8_t
{
    kStatic,
    kRuntime,
};

struct SkippedRecord
{
    std::string Id;
    SkipClass Class = SkipClass::kStatic;
    std::string Cause;    // 静态：SkipReason 的名字化；运行时：missing service <k>@<v> / cascade from <id>
    std::string CausedBy; // 运行时归因（D41）；序缺陷与静态为空串
};

struct ResidualEntry
{
    std::string OwnerLabel;
    std::uint64_t Count = 0;
};

struct PodReport
{
    bool HandleWasStale = false; // §5.1：句柄失效是预期内，不是错误
    std::uint32_t PodIndex = 0;
    std::vector<FailedPluginRecord> Failures; // 运行时失败（§5.2 的轻量诊断记录）
    std::vector<SkippedRecord> Skips;
    std::vector<std::string> HotSwapLog;  // §9.2 v3「中途进出过谁」（T10/T11 填充）
    DiagnosticSnapshot CountersDiff;      // 相对基线的差分（基线 = 本 Pod 创建时）
    std::vector<ResidualEntry> Residuals; // #10 归属（M1 形态见类内注释）

    // VASE_POD_API 只加在这个成员函数上，不给整个 struct 加：定义在 VasePod 里，
    // 本库外调用不导出即 lld-link undefined symbol（与 EffectHandle 同一笔账）；
    // 而 struct 级别的导出会把 STL 成员带进 C4251 的射程。
    [[nodiscard]] VASE_POD_API bool Clean() const; // 差分五项全零且无 Residuals
};

struct PodHandle
{
    // Generation 从 1 起（见 PluginHost.cpp 的槽分配）：0 是默认构造的句柄，
    // 它不该解析到任何槽——否则 `PodHandle{}` 会冒充第一个 Pod。
    std::uint32_t Index = 0;
    std::uint32_t Generation = 0;

    bool operator==(const PodHandle&) const = default;
};

class PluginHost;
class PodTestPeer;

class VASE_POD_API Pod
{
public:
    Pod(const Pod&) = delete;
    Pod& operator=(const Pod&) = delete;
    Pod(Pod&&) = delete;
    Pod& operator=(Pod&&) = delete;
    // 析构**声明在这里、定义在 Pod.cpp**：默认在类内会让每个用到 unique_ptr<Pod> 的 TU
    // 都实例化 Pod 的成员析构，而它们要 ServiceRegistry / EventBus 的完整类型——
    // Host 侧于是被迫包含对它无用的 Vase/Detail/RegistryBus.h（实测 clang-cl:
    // invalid application of 'sizeof' to an incomplete type 'vase::detail::ServiceRegistry'）。
    ~Pod();

    Context& Root(); // 宿主 stage-0 与运行期入口（§1.3 推论：宿主服务每局重注册）

    [[nodiscard]] std::size_t PluginCount() const; // 活集合（§5.6「活集合」的 M1 形态）
    [[nodiscard]] bool HasPlugin(std::string_view id) const;
    [[nodiscard]] std::vector<std::string> PluginIds() const;
    [[nodiscard]] const std::vector<FailedPluginRecord>& Failures() const; // §5.5：失败清单随时可被宿主读出
    [[nodiscard]] const std::vector<SkippedRecord>& Skips() const { return SkipRecords; }

private:
    friend class PluginHost;
    friend class PodTestPeer;

    struct LiveInstance
    {
        const PluginDescriptor* Desc = nullptr;
        Plugin* Instance = nullptr; // 由 Desc->Create/Destroy 配对管理
        // 本实例的驻留镜像（T10）。Loader 用 unique_ptr 存记录，故**驻留期内稳定**：
        // 摘除只发生在 Unload 之后，而 Unload 前本实例必已拆净（全局闸把的关，§1.2 的账）。
        detail::BinaryRecord* Binary = nullptr;
        std::unique_ptr<EffectScope> Scope;
        std::unique_ptr<Context> Ctx;
        // 配置对象：镜像内 CreateConfig 造、DestroyConfig 毁（D25）；deleter 与指针成对，空 = 无配置。
        std::unique_ptr<void, void (*)(void*)> ConfigObject{nullptr, nullptr};
        std::string OwnerLabel; // Meta->Id 的拥有型拷贝（别赌字面量生命周期）

        enum class InstanceState : std::uint8_t
        {
            kLoading,
            kLoaded,
            kStarted,
            kFailed,
        };

        InstanceState State = InstanceState::kLoading;
    };

    Pod(detail::ScopePool& pool, detail::DiagnosticCounters& counters, detail::DependencyLedger* ledger,
        std::uint32_t podIndex); // ledger T9 起非空

    void TeardownInstancesAndRoot(); // 逆序回收全部实例 + 根 Scope（DestroyPod 的机器，§5.4）

    detail::ScopePool& Pool;
    detail::DiagnosticCounters& Counters;
    detail::DependencyLedger* Ledger;
    std::uint32_t PodIndex;

    std::unique_ptr<detail::ServiceRegistry> Registry;
    std::unique_ptr<detail::EventBus> Bus;
    std::unique_ptr<EffectScope> RootScope;
    std::unique_ptr<Context> RootContext;

    std::vector<std::unique_ptr<LiveInstance>> Instances; // 数组序 = 计划序
    std::vector<FailedPluginRecord> FailureRecords;       // §5.2：Failed 保留的是记录
    std::vector<SkippedRecord> SkipRecords;               // §5.1：跳过者无实例无边——不进活集合、不进 Clean（D38）

    // T10（§5.6 四条补角「Failed 可被 Eject」）：失败插件的**记录**在 FailureRecords，
    // 它的**驻留镜像**在这里。两张表分开是必须的——二进制根本没驻留（EnsureResident
    // 自己失败）也是一种失败，那种只有记录、没有镜像。Eject 靠这张表把镜像一并摘掉。
    // 值是**非拥有**指针：实体在 Loader 表里，且只要本表还引用它，全局闸就不许卸。
    std::unordered_map<std::string, detail::BinaryRecord*> FailedBinaries;

    // #10（D14）的最小验证形态：**测试注入**的未回收 Scope 清单。插件侧真泄漏通道
    // （绕 Effect 渠道的注册）是 9.3 契约束，其检测属 M3 完整属主追踪器——这里先把
    // 「报告要点名残留归属」的机制立住（PodTestPeer 注入，正常装配路径永不写入）。
    std::vector<std::unique_ptr<EffectScope>> LeakedScopesForTest;
};

} // namespace vase
