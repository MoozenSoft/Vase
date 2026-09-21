// T13 §7.6（承重条款）的常驻证伪者：Eject 之后的事件风暴不许有任何一次派发落进已卸代码。
//
// 为什么必须常驻：违反它的表现**不是立刻失败**，而是「某个随机时刻跳进已解除映射的代码页」
// ——本轮没崩不代表下一轮不崩（地址空间布局、页重用、其他 Pod 的映射顺序都在掺和）。
// 所以这里赌的不是「这次没崩」，而是两条**可证伪**的量：
//   ① 进程活着走完整场噪声（真·悬垂回调在没有 ASan 的门禁里就是当场崩溃，崩溃即红）；
//   ② 邻居 B 的精确心跳数——它排除「拍子根本没派出去」这个让 ① 假绿的世界。
#include "Vase/Host/PluginHost.h"

#include "Vase/Detail/Result.h"
#include "Vase/Host/Evidence.h"
#include "Vase/Host/LoadPlan.h"
#include "Vase/Pod/Context.h"
#include "Vase/Pod/Pod.h"
#include "fixtures/BCommon.h"

#include <gtest/gtest.h>

namespace
{

TEST(HotSwap, NoCallbackFiresIntoEjectedCode)
{
    vase::PluginHost host;
    vase::LoadPlan plan;
    plan.Ordered.push_back({.Id = "Vase.Timer", .BinaryPath = VASE_FIXTURE_TIMER});
    plan.Ordered.push_back({.Id = "Vase.NeighborB", .BinaryPath = VASE_FIXTURE_NEIGHBORB});
    const vase::PodHandle h = host.CreatePod(plan).Value();
    vase::Pod* const pod = host.Resolve(h);

    const vase::Result<vase::EjectReport> eject = host.EjectPlugin(h, "Vase.Timer");
    ASSERT_TRUE(eject.IsOk());
    // 本用例把崩溃当红，而这只在「镜像真被解映射」的世界里成立——先把那个前提钉住：
    // 货留在架上（`BinaryActuallyUnloaded == false`）时，本用例就不再是 §7.6 的证伪者。
    // 只钉前提，不预言去掉前提后噪声会怎样（T13 实验二实测：悬垂订阅崩的根因是实例已被
    // 销毁，模块在不在架上都救不了——所以别把「留在架上就会安静变绿」写成事实）。
    EXPECT_TRUE(eject.Value().BinaryActuallyUnloaded);
    for (int seq = 0; seq < 100; ++seq)
    {
        pod->Root().Emit(samples_fixture::TickEvent{seq});
    }
    // 噪声之后单独再派一拍：Timer 的订阅随它自己的 EffectScope 一同消亡，B 的那一份
    // 不受牵连——**取一次而不是跨 Eject 握着指针**：B 的实例确实没被碰过，但握过 Eject
    // 的指针要读者自己论证这件事，而重新 Get 一次不需要任何论证。
    pod->Root().Emit(samples_fixture::TickEvent{999});
    // 100 拍 + 这 1 拍 = 101：拍子真的派到了订阅者手上，上面那 100 次不是空转
    // ——这一条才让「没崩」有意义（它排除「事件根本没派出去」那个假绿世界）。
    EXPECT_EQ(pod->Root().Get<samples_fixture::IHeart>().Beats(), 101);
    EXPECT_TRUE(host.DestroyPod(h).Clean()); // 五计数不因中途进出而移动基线锚点
}

} // namespace
