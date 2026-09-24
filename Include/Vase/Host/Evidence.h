#pragma once

// §8.2 三档证据的载荷。字段设计的唯一原则：**判据力跟着平台走，写进结构**。
// Eject/Adopt 报告是 §0.2「每次进出必须有账可查」的纸面凭证——缺一个平台字段
// 的「成功」比失败更危险。

#include "Vase/Detail/Export.h"
#include "Vase/Pod/DependencyLedger.h" // Host→Pod 方向合法（VaseHost 链 VasePod）

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vase
{

struct LedgerEdgeRef // 账本边的一次性快照：报告是返回值，必须拥有（LedgerEdge 的 string_view 不外带）。
{
    std::string ConsumerId;
    std::string ProviderId;
    std::string Service;
    std::uint32_t Version = 0;
};

inline LedgerEdgeRef SnapshotEdge(const detail::LedgerEdge& edge)
{
    return LedgerEdgeRef{
        .ConsumerId = std::string(edge.ConsumerId),
        .ProviderId = std::string(edge.ProviderId),
        .Service = std::string(edge.ServiceName),
        .Version = edge.ServiceVersion,
    };
}

enum class EjectStatus : std::uint8_t
{
    kRejectedConsumers,
    kEjected,
};

struct EjectReport
{
    std::string PluginId;
    EjectStatus Status = EjectStatus::kRejectedConsumers; // 默认悲观：成功态必须被显式置上（spec 5.2）
    std::vector<LedgerEdgeRef> Consumers;                 // kRejectedConsumers：入边逐条点名（D21，3b 兑现）
    std::vector<LedgerEdgeRef> RemovedEdges;              // kEjected：拆除时账上出边清单（「解析记录」Eject 侧）

    // 档一 · 实例归零（三平台同，§8.2）：账无反向边 + Scope 空 + 存活对象计数 0。
    bool LedgerHadNoIncomingEdges = false;
    bool ScopeEmptied = false;
    bool CrossPodInstancesZeroed = false; // 多开同插件时这是「能不能卸货」的闸

    // 档二 · 映射解除：Linux 主判 = 条目消失；Windows 主判 = 文件可写开，**辅助**地位
    // （改名替换骗得过它，§8.2 v3 修订——最终防假成功的是档三）。
    bool MappingRemoved = false;
    bool ReopenWritable = false;
    bool MappingRemovalIsObservable = false;
    bool ReopenWritableIsMeaningful = false;
    bool BinaryActuallyUnloaded = false; // false 且无错误 = 其他 Pod 实例仍活，货留在架上（§8.1）

    // §9.1 v3：Eject 自动重置登记的进程级状态。**M1 恒空**——.ProcessState 登记
    // 属 M3（12.3 里程碑行），字段先立住让报告形状稳定，M3 填实现。
    std::vector<std::string> ProcessStatesReset;

    std::string HotSwapNote; // 「failed-record ejected」/「kept resident: other pod holds instances」等
};

struct RequirementRef // 拥有型需求引用。描述符侧 ServiceRef 是 string_view 借用，报告活不过镜像驻留期。
{
    std::string Service;
    std::uint32_t Version = 0;
};

struct CollisionRef // D43：碰撞报告要多一个现提供者（与 RequirementRef 的差别仅此）。
{
    std::string Service;
    std::uint32_t Version = 0;
    std::string ProvidedBy; // 活集合中当前提供者 Id；宿主提供时为 "host"
};

enum class AdoptStatus : std::uint8_t
{
    kRejectedDependencies,
    kRejectedCollision,
    kAdopted,
};

struct AdoptReport
{
    std::string PluginId;
    AdoptStatus Status = AdoptStatus::kRejectedDependencies; // 默认悲观：成功态必须显式置上
    std::vector<RequirementRef> Missing;                     // kRejectedDependencies：缺哪条（§5.6③「报告缺哪条」兑现）
    std::vector<CollisionRef> Collisions;                    // kRejectedCollision：碰撞服务 + 现提供者（D43）
    std::vector<LedgerEdgeRef> Outgoing;                     // kAdopted：本次入局新落账边清单；OutgoingEdges = .size()
    bool ReusedResidentImage = false;                        // §5.6 判定流②：复用分支同样必须 IdentityVerified
    bool IdentityVerified = false;                           // 档三 · 内存特征 == 磁盘特征
    bool ImportEnforcementPassed = false;                    // §8.7：导入表不含兄弟插件
    std::size_t OutgoingEdges = 0;                           // 装配后账本上的出边数（「每次解析落账」的凭证）
};

} // namespace vase
