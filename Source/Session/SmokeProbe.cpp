#include "Vase/Detail/SmokeProbe.h"

#include <cstdint>

namespace vase
{

std::uint32_t SessionSmokeProbe()
{
    // 取值本身无意义；用一个非平凡常量，避免被优化成一眼可判定的常数。
    return 0x56415345U; // "VASE"
}

} // namespace vase
