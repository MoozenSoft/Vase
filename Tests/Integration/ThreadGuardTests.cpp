#include "Vase/Host/LoadPlan.h"
#include "Vase/Host/PluginHost.h"
#include "Vase/Pod/Pod.h"

#include <gtest/gtest.h>
#include <thread>

namespace
{

TEST(ThreadGuard, VaseApiFromForeignThreadTerminates)
{
    // 12 节 #16（§1.4）：绑定线程之外的调用当场终止——响的，不拖到随机时刻。
    EXPECT_DEATH(
        {
            vase::PluginHost host; // 绑定在 death-test 子进程的主线程
            std::thread foreign([&host] { static_cast<void>(host.CreatePod(vase::LoadPlan{})); });
            foreign.join();
        },
        "not the bound thread");
}

TEST(ThreadGuard, SameThreadSequentialHostsAllowed)
{
    // §1.4 的「唯一」是「同时只一个」：串行建销合法（测试基建依赖这条）。
    for (int i = 0; i < 2; ++i)
    {
        vase::PluginHost host;
        EXPECT_EQ(host.Resolve(vase::PodHandle{0, 0}), nullptr); // 空 Host 上失效句柄 = nullptr，不崩（守卫未误伤）
    }
    SUCCEED();
}

} // namespace
