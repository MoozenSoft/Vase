// 无再入探针（T13 §7.6 承重条款的验证形态）：订阅 TickEvent，handler 只写自己的成员。
//
// 成员计数**外部不可见**是故意的：3c 判据的形状是「Eject 之后派发 100 拍，进程活着」，
// 不是「数到回调没来」。真·悬垂回调在没有 ASan 的门禁里以「踩进已解除映射的代码页」
// 现形——崩溃本身就是红；能数出来的计数反而会让人去写一条其实测不出东西的断言。
//
// M1 等价形：Vase 本体不提供调度服务（§0.1 边界），「10ms 定时器」的拍由宿主驱动，
// 这里的 TickEvent 就是那根拍子。
#include "Vase/Plugin.h"

#include "../BCommon.h"

namespace
{

class TimerPlugin final : public vase::Plugin
{
public:
    vase::Result<void> OnLoad(vase::Context& ctx) override
    {
        ctx.On<samples_fixture::TickEvent>(&TimerPlugin::OnTick, this);
        return vase::Result<void>::Ok();
    }

private:
    void OnTick(const samples_fixture::TickEvent& event)
    {
        static_cast<void>(event); // 只数拍子，载荷（Seq）在这里用不到
        ++Ticks;
    }

    int Ticks = 0;
};

} // namespace

// 匿名命名空间在 VASE_PLUGIN 之前闭上（全仓统一位形），但这与链接性无关：
// 宏的 extern "C" 导出即使包在其内也不会变内部链接——C 语言链接赢（链接实测），
// 宿主的 GetProcAddress / dlsym 两种写法都照样找得到它。
VASE_PLUGIN(TimerPlugin){
    .Id = "Vase.Timer",
    .DisplayName = "定时器探针",
    .Version = "0.1.0",
    .Requires = {},
    .Provides = {},
};
