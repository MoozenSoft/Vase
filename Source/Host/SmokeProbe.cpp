#include "Vase/Detail/SmokeProbe.h"

#include <cstdint>

namespace vase
{

std::uint32_t HostSmokeProbe()
{
    // 这次调用发生在 VaseHost 内部：
    //   导入库没接上 → 链接错误；运行时找不到 VasePod 动态库 → 加载失败。
    return PodSmokeProbe();
}

} // namespace vase
