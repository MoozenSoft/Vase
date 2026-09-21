#include "Vase/Pod/DependencyLedger.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

namespace
{

// cookie 只作身份用：栈上对象的地址即可（账本不拥有它们，只借来配对两端）。
vase::detail::LedgerEdge Edge(std::uint32_t podIndex, const void* consumer, const void* provider,
                              std::string_view consumerId)
{
    return vase::detail::LedgerEdge{
        .PodIndex = podIndex,
        .Consumer = consumer,
        .Provider = provider,
        .ConsumerId = consumerId,
        .ProviderId = "Vase.Provider",
        .ServiceName = "Vase.Test.Shared",
        .ServiceVersion = 1,
    };
}

std::vector<std::string> ConsumerIdsOf(const std::vector<const vase::detail::LedgerEdge*>& edges)
{
    std::vector<std::string> ids;
    ids.reserve(edges.size());
    for (const vase::detail::LedgerEdge* edge : edges)
    {
        ids.emplace_back(edge->ConsumerId);
    }
    return ids;
}

TEST(Ledger, RecordAndReverseLookup)
{
    // 反查（§5.6 规则③）：EdgesTo(T) 要点名**每一个**消费者——Eject 的拒绝报告靠它。
    vase::detail::DependencyLedger ledger;
    int provider = 0;
    int consumerA = 0;
    int consumerB = 0;
    ledger.Record(Edge(0, &consumerA, &provider, "Vase.ConsumerA"));
    ledger.Record(Edge(0, &consumerB, &provider, "Vase.ConsumerB"));

    EXPECT_EQ(ConsumerIdsOf(ledger.EdgesTo(&provider)), (std::vector<std::string>{"Vase.ConsumerA", "Vase.ConsumerB"}));
    EXPECT_TRUE(ledger.EdgesTo(&consumerA).empty()); // 反查只看 provider 那一列
    EXPECT_EQ(ConsumerIdsOf(ledger.EdgesFrom(&consumerA)), (std::vector<std::string>{"Vase.ConsumerA"}));
    EXPECT_EQ(ledger.EdgeCount(), 2U);
}

TEST(Ledger, RemoveByInstanceClearsBothSides)
{
    // 边随实例死（§5.6 规则②）：同一实例既是一条边的消费者、又是另一条边的提供方，
    // 一次 RemoveByInstance 要两向都摘——只摘一向就是热卸时漏网的死边。
    vase::detail::DependencyLedger ledger;
    int shared = 0;
    int upstream = 0;
    int downstream = 0;
    ledger.Record(Edge(0, &shared, &upstream, "Vase.Shared"));       // shared 作消费者
    ledger.Record(Edge(0, &downstream, &shared, "Vase.Downstream")); // shared 作提供方
    ASSERT_EQ(ledger.EdgeCount(), 2U);

    ledger.RemoveByInstance(&shared);
    EXPECT_EQ(ledger.EdgeCount(), 0U);
    EXPECT_TRUE(ledger.EdgesFrom(&shared).empty());
    EXPECT_TRUE(ledger.EdgesTo(&shared).empty());
}

TEST(Ledger, ClearPodOnlyTargetPod)
{
    // ClearPod 整批清零只打自己那一局（§5.6）：交错落账，清 Pod 1 后 Pod 0 的边原封不动。
    // 同一 cookie 在两局里各有一条边——按 PodIndex 过滤与按 cookie 过滤在这里会给出不同答案。
    vase::detail::DependencyLedger ledger;
    int providerA = 0;
    int providerB = 0;
    int consumerA = 0;
    int consumerB = 0;
    ledger.Record(Edge(0, &consumerA, &providerA, "Vase.ConsumerA"));
    ledger.Record(Edge(1, &consumerB, &providerB, "Vase.ConsumerB"));
    ledger.Record(Edge(0, &consumerB, &providerA, "Vase.ConsumerB"));

    ledger.ClearPod(1);
    EXPECT_EQ(ledger.EdgeCount(), 2U);
    EXPECT_TRUE(ledger.EdgesTo(&providerB).empty()); // Pod 1 的那条没了
    EXPECT_EQ(ConsumerIdsOf(ledger.EdgesTo(&providerA)),
              (std::vector<std::string>{"Vase.ConsumerA", "Vase.ConsumerB"}));
    // consumerB 在 Pod 0 的边留下、Pod 1 的边走掉：剩下的那条必须仍归属 Pod 0。
    const std::vector<const vase::detail::LedgerEdge*> remaining = ledger.EdgesFrom(&consumerB);
    ASSERT_EQ(remaining.size(), 1U);
    EXPECT_EQ((*remaining.begin())->PodIndex, 0U);
}

} // namespace
