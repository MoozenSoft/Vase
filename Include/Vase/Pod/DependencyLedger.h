#pragma once

// §5.6 账本。两个判定共用它：Eject 前 EdgesTo(T) 非空即拒（反查）；实例消失
// 时 RemoveByInstance（正查无必要——摘边发生在被摘方自己的回收点）。
// 单线程契约（§1.4）下 vector 线性扫就是正确复杂度（M1 的边数以十计；别预防性建索引）。
//
// 边的生命周期 ⊆ 两端实例的生命周期：非拥有 cookie + 借用字符串。两端任一消失即删
// （§5.6 规则②），所以 string_view 指进描述符字面量是安全的（镜像驻留 ⊇ 实例存活，§8.1）。

#include "Vase/Detail/Export.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace vase::detail
{

struct LedgerEdge
{
    std::uint32_t PodIndex = 0;
    const void* Consumer = nullptr; // 插件实例 cookie（永不空——宿主解析不落边）
    const void* Provider = nullptr; // 插件实例 cookie（永不空——kHost 提供方向不记）
    std::string_view ConsumerId;
    std::string_view ProviderId;
    std::string_view ServiceName;
    std::uint32_t ServiceVersion = 0;
};

class VASE_POD_API DependencyLedger
{
public:
    void Record(LedgerEdge edge); // Debug 断言：Consumer/Provider 均非空（§1.2 属主不变式在账本上的投影）

    // 返回的指针指进账本内部存储：**下一次改动**（Record / RemoveByInstance / ClearPod）即
    // 失效。调用点都是「查到就当场读」（Eject 的拒绝报告、拆除断言），别把它们存过界。
    [[nodiscard]] std::vector<const LedgerEdge*> EdgesTo(const void* provider) const;
    [[nodiscard]] std::vector<const LedgerEdge*> EdgesFrom(const void* consumer) const;

    void RemoveByInstance(const void* instance); // 进出两向一起摘（边随实例死）
    void ClearPod(std::uint32_t podIndex);       // DestroyPod 整批清零（§5.6）

    // 粗粒度刻意如此：一次瞬态解析留下的边也拦 Eject（§5.6）——「调用一次就丢引用」照样落账。
    [[nodiscard]] std::size_t EdgeCount() const { return Edges.size(); }

private:
    std::vector<LedgerEdge> Edges;
};

} // namespace vase::detail
