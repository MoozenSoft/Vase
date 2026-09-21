#pragma once

// §9.2 的五项诊断计数。放 Detail 是因为 VasePod（Effect/Service 侧）与
// VaseHost（持有者）都要见到同一布局。**不做原子、不加锁**——依据是 §1.4 的
// 绑定线程契约；谁日后为「多线程装配」拆 1.4，谁就得连这里一起重做（§9.2 明文）。
// 始终维护（Release 也要写进 PodReport），断言只在 Debug——分工不是取舍。

#include <cstdint>

namespace vase::detail
{

struct DiagnosticCounters
{
    std::uint64_t Effects = 0;
    std::uint64_t Services = 0;
    std::uint64_t Subscriptions = 0;
    std::uint64_t PluginInstances = 0;
    std::uint64_t Scopes = 0;
};

} // namespace vase::detail
