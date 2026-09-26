#pragma once

// FieldInfo 是**对外契约、布局即 ABI**（spec 3.2「把 FieldInfo 当接口对待」）：
// 任何布局改动 → kHeaderVersion 递增（D26）。Min/Max 无执法（D35）：纯展示元信息，
// 越界判定归 M2b 的 Preset 校验。choices 的成员执法在 Solve 与装配点（D79/D80）加宏
// CHECK 点的默认值成员闸（账①），此处仍纯数据。

#include "Vase/Config/Value.h"
#include "Vase/Detail/Fail.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>

namespace vase
{

// choices 条目值形（D76）：POD，指向作者侧命名数组（寿命纪律见 ChoicesOf）。
struct ChoiceInfo
{
    std::int32_t Value;
    const char* Label;
};

// 作者面 Meta 与 FieldInfo 尾两槽间的搬运形。
struct ChoiceView
{
    const ChoiceInfo* Items = nullptr;
    std::uint32_t Count = 0;
};

// 作者书写面。设计成**可整体传值的 constexpr 聚合**（D35 执法不进库内）。
struct Meta
{
    const char* Label = "";
    Value Min{}; // kNone = 未设
    Value Max{};
    ChoiceView Choices{}; // = ChoicesOf(表)；enum 配对执法在宏 CHECK 点（D76）
};

// constexpr 身份 helper：展开式里不能对括号内 designated-init 直接取成员，走函数调用形。
constexpr const char* MetaLabel(Meta m) { return m.Label; }
constexpr Value MetaMin(Meta m) { return m.Min; }
constexpr Value MetaMax(Meta m) { return m.Max; }
constexpr std::uint32_t MetaChoiceCount(Meta m) { return m.Choices.Count; }

// D77：只接受命名存储（inline constexpr 数组，随枚举类型共享）——指针要活到运行期。
template <std::size_t N>
consteval ChoiceView ChoicesOf(const std::array<ChoiceInfo, N>& table)
{
    return ChoiceView{table.data(), static_cast<std::uint32_t>(N)};
}

// D76 的槽位搬运 consteval；配对执法（enum 必带 / 非 enum 禁带）在 ConfigMacros 的 CHECK 点。
consteval ChoiceView MetaChoices(Meta m) { return m.Choices; }

// D80 描述符侧补闸（账①）：enum 字段的默认值必须 ∈ 自己的 choices 表，否则该字段的值域自相矛盾。
// 非 enum 恒真——默认值形态各异（string 是字面量指针），不能无条件转 int32，故自护在体内；
// 条件仍在宏 CHECK 点展开（理由同 MetaChoiceCount：consteval 形参内不是常量式）。
template <typename FieldT>
constexpr bool DefaultInChoices([[maybe_unused]] FieldT value, [[maybe_unused]] ChoiceView choices)
{
    if constexpr (!std::is_enum_v<FieldT>)
    {
        return true;
    }
    else
    {
        const auto raw = static_cast<std::int32_t>(value);
        for (std::uint32_t index = 0; index < choices.Count; ++index)
        {
            if (std::next(choices.Items, static_cast<std::ptrdiff_t>(index))->Value == raw)
            {
                return true;
            }
        }
        return false;
    }
}

struct FieldInfo
{
    const char* Name = "";
    ValueKind Kind = ValueKind::kNone;
    Value Default{};
    Value Min{};
    Value Max{};
    const char* Label = "";
    void (*Apply)(void* configStruct, const Value& v) = nullptr; // = kApplyTo<&T::Field>
    // 尾两槽：v3 新增，全零 = 非枚举（D76；宏的 designated-init 按声明序，故 Choices 组在尾）。
    const ChoiceInfo* Choices = nullptr;
    std::uint32_t ChoiceCount = 0;
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
