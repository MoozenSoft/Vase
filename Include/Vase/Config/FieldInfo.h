#pragma once

// FieldInfo 是**对外契约、布局即 ABI**（spec 3.2「把 FieldInfo 当接口对待」）：
// 任何布局改动 → kHeaderVersion 递增（D26）。Min/Max 无执法（D35）：纯展示元信息，
// 越界判定归 M2b 的 Preset 校验。

#include "Vase/Config/Value.h"
#include "Vase/Detail/Fail.h"

#include <cstdint>
#include <type_traits>

namespace vase
{

// 作者书写面。设计成**可整体传值的 constexpr 聚合**（D35 执法不进库内，所以只这三个成员）。
struct Meta
{
    const char* Label = "";
    Value Min{}; // kNone = 未设
    Value Max{};
};

// constexpr 身份 helper：展开式里不能对括号内 designated-init 直接取成员，走函数调用形。
constexpr const char* MetaLabel(Meta m) { return m.Label; }
constexpr Value MetaMin(Meta m) { return m.Min; }
constexpr Value MetaMax(Meta m) { return m.Max; }

struct FieldInfo
{
    const char* Name = "";
    ValueKind Kind = ValueKind::kNone;
    Value Default{};
    Value Min{};
    Value Max{};
    const char* Label = "";
    void (*Apply)(void* configStruct, const Value& v) = nullptr; // = kApplyTo<&T::Field>
};

static_assert(std::is_trivially_copyable_v<FieldInfo>);

template <typename MemberPtr>
struct MemberTraits;

template <typename Config, typename Field>
struct MemberTraits<Field Config::*>
{
    using Owner = Config;
    using Type = Field;
};

template <auto MemberPtr>
void ApplyToImpl(void* configStruct, const Value& v)
{
    using Traits = MemberTraits<decltype(MemberPtr)>;
    // 宏保证「表里 Kind == 成员类型」是构造的（同一次展开）；这里运行时兜底拦的是
    // 宿主灌错类型的 Value——那种正常路径在装配 Apply 点已被 D32 提前拦掉，走到这就是编程错误。
    if (v.Kind != KindOf<typename Traits::Type>())
    {
        detail::ProgrammerError("VASE_CONFIG Apply: Value kind mismatch");
    }
    auto* typed = static_cast<Traits::Owner*>(configStruct);
    (typed->*MemberPtr) = v.GetAs<typename Traits::Type>();
}

template <auto MemberPtr>
constexpr auto kApplyTo = &ApplyToImpl<MemberPtr>;

} // namespace vase
