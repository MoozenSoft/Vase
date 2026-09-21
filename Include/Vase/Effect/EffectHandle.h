#pragma once

// 索引 + 代际句柄（D9）。代际一次解决两件事：§7.2 的 Dispose 幂等
// （过期句柄 Release 无副作用）与 §7.3 的「从账上移除」。可平凡拷贝——
// 拷贝出去的第二份句柄在槽位被回收并复用后自动失效（代际不匹配）。
//
// **句柄不得活得比它的 EffectScope 久**：Scope 是非拥有裸指针，Release()/IsValid() 都解引用它。
// 代际只护「槽位复用」那一侧，护不了「Scope 已死」——陈旧句柄安全不是全保。
//
// 标 VASE_POD_API：两个成员函数的定义在 VasePod（要 EffectScope 完整类型），
// 不导出则本库外的调用方链接期即失败（实测 lld-link: undefined symbol）。

#include "Vase/Detail/Export.h"

#include <cstdint>

namespace vase
{

class EffectScope;

struct VASE_POD_API EffectHandle
{
    static constexpr std::uint32_t kInvalidSlot = 0xFFFFFFFFU;

    EffectScope* Scope = nullptr;
    std::uint32_t Slot = kInvalidSlot;
    std::uint32_t Generation = 0;

    [[nodiscard]] bool IsValid() const;
    // 立即回收 + 从 Scope 账上移除——两件事原子，缺一不可（§7.3）。
    // 对失效句柄调用无副作用（幂等）。const 是准确的：它改的是 Scope，不是句柄本身。
    void Release() const;
};

} // namespace vase
