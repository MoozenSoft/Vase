#pragma once

// VASE_CONFIG：字段列表写一次、展开两次（成员 + kFields），一致性是构造出来的（spec 3.2）。
// 作者形态（元组 4 项，类型可含 ::；Meta 的 brace-init 由元组外层括号保护）：
//   VASE_CONFIG(CombatConfig,
//       (float, CriticalMultiplier, 2.0f, vase::Meta{.Label = "暴击倍率",
//                                                    .Min = vase::Value::From<float>(1.0f),
//                                                    .Max = vase::Value::From<float>(10.0f)}),
//       (bool, FriendlyFire, false, vase::Meta{.Label = "友军伤害"}));
// enum 字段必带表：Meta{..., .Choices = vase::ChoicesOf(命名数组)}——表须 inline constexpr（D76/D77）。
// 展开物 = struct Type{ 成员(默认值 = 元组第3项) ; D76 配对 CHECK ; static constexpr kFields ; 配对工厂 }。
// 宏参数内花括号不受 Allman 管辖（与 VASE_PLUGIN 同例外）。至少 1 个字段；无配置就别用本宏。

#include "Vase/Config/ConfigInfo.h"
#include "Vase/Config/FieldInfo.h"
#include "Vase/Config/Value.h"

#include <array>
#include <cstdint>
#include <memory>

// 机制宏的定义体不参与 Allman 折叠，与 VASE_PLUGIN 头注释同一处置。
// clang-format off
// NOLINTBEGIN(cppcoreguidelines-macro-usage) C++20 无变参计数的模板替代，机制即宏，无代码级出路。
// 拆元组 = 变参取件器（T1/T2/…）：Meta 的 brace-init 里有裸逗号（花括号不护预处理器
// 实参定界），所以消费宏一律变参吸收整个元组、各取所需。「展开后再分裂逗号」的
// 预扫描形态在 clang-cl 实测不成立（名字先过词、括号后生成不回填），勿改回去。
#define VASE_CONFIG_DETAIL_T1(A, ...) A
#define VASE_CONFIG_DETAIL_T2(A, B, ...) B
#define VASE_CONFIG_DETAIL_STR2(A, B, ...) #B
#define VASE_CONFIG_DETAIL_T3(A, B, C, ...) C
#define VASE_CONFIG_DETAIL_TAIL(A, B, C, ...) __VA_ARGS__

#define VASE_CONFIG_DETAIL_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16,               \
                                 _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32,      \
                                 _33, N, ...)                                                                        \
    N
#define VASE_CONFIG_DETAIL_RSEQ_N()                                                                                  \
    33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5,   \
    4, 3, 2, 1
#define VASE_CONFIG_DETAIL_NARG_(...) VASE_CONFIG_DETAIL_ARG_N(__VA_ARGS__)
#define VASE_CONFIG_DETAIL_NARG(...) VASE_CONFIG_DETAIL_NARG_(__VA_ARGS__, VASE_CONFIG_DETAIL_RSEQ_N())

#define VASE_CONFIG_DETAIL_CAT_(A, B) A##B
#define VASE_CONFIG_DETAIL_CAT(A, B) VASE_CONFIG_DETAIL_CAT_(A, B)

#define VASE_CONFIG_DETAIL_FE_1(M, S, T) M(S, T)
#define VASE_CONFIG_DETAIL_FE_2(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_1(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_3(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_2(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_4(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_3(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_5(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_4(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_6(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_5(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_7(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_6(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_8(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_7(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_9(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_8(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_10(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_9(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_11(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_10(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_12(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_11(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_13(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_12(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_14(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_13(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_15(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_14(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_16(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_15(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_17(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_16(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_18(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_17(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_19(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_18(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_20(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_19(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_21(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_20(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_22(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_21(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_23(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_22(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_24(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_23(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_25(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_24(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_26(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_25(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_27(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_26(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_28(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_27(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_29(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_28(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_30(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_29(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_31(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_30(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FE_32(M, S, T, ...) M(S, T) VASE_CONFIG_DETAIL_FE_31(M, S, __VA_ARGS__)
#define VASE_CONFIG_DETAIL_FOR_EACH(M, S, ...) VASE_CONFIG_DETAIL_CAT(VASE_CONFIG_DETAIL_FE_, VASE_CONFIG_DETAIL_NARG(__VA_ARGS__))(M, S, __VA_ARGS__)

#define VASE_CONFIG_DETAIL_MEMBER(S, T)                                                                                 \
    VASE_CONFIG_DETAIL_T1 T VASE_CONFIG_DETAIL_T2 T = VASE_CONFIG_DETAIL_T3 T;

// D76 配对执法与 D80 侧「默认 ∈ choices」闸（账①）的落点：条件在展开处是 manifestly-constant
// 表达式（Meta 实形 + consteval KindOf / constexpr MetaChoiceCount），所以 static_assert 放这里
// 而不是 MetaChoices 体内（形参非常量式）。
#define VASE_CONFIG_DETAIL_CHECK(S, T)                                                                                \
    static_assert(vase::KindOf<VASE_CONFIG_DETAIL_T1 T>() != vase::ValueKind::kEnum ||                                \
                      vase::MetaChoiceCount(VASE_CONFIG_DETAIL_TAIL T) > 0U,                                          \
                  "enum config field must carry choices (D76)");                                                      \
    static_assert(vase::KindOf<VASE_CONFIG_DETAIL_T1 T>() == vase::ValueKind::kEnum ||                                \
                      vase::MetaChoiceCount(VASE_CONFIG_DETAIL_TAIL T) == 0U,                                         \
                  "choices only legal on enum fields (D76)");                                                         \
    static_assert(vase::DefaultInChoices<VASE_CONFIG_DETAIL_T1 T>(VASE_CONFIG_DETAIL_T3 T,                            \
                                                                 vase::MetaChoices(VASE_CONFIG_DETAIL_TAIL T)),        \
                  "enum config field default must be one of its choices (D80)");

#define VASE_CONFIG_DETAIL_FIELD(S, T)                                                                                  \
    vase::FieldInfo{.Name = VASE_CONFIG_DETAIL_STR2 T,                                                                \
                    .Kind = vase::KindOf<VASE_CONFIG_DETAIL_T1 T>(),                                                  \
                    .Default = vase::Value::From<VASE_CONFIG_DETAIL_T1 T>(VASE_CONFIG_DETAIL_T3 T),                   \
                    .Min = vase::MetaMin(VASE_CONFIG_DETAIL_TAIL T),                                                  \
                    .Max = vase::MetaMax(VASE_CONFIG_DETAIL_TAIL T),                                                  \
                    .Label = vase::MetaLabel(VASE_CONFIG_DETAIL_TAIL T),                                              \
                    .Apply = vase::kApplyTo<&S::VASE_CONFIG_DETAIL_T2 T>,                                             \
                    .Choices = vase::MetaChoices(VASE_CONFIG_DETAIL_TAIL T).Items,                                    \
                    .ChoiceCount = vase::MetaChoices(VASE_CONFIG_DETAIL_TAIL T).Count},

#define VASE_CONFIG(Type, ...)                                                                                        \
    struct Type                                                                                                       \
    {                                                                                                                 \
        VASE_CONFIG_DETAIL_FOR_EACH(VASE_CONFIG_DETAIL_MEMBER, Type, __VA_ARGS__)                                     \
        VASE_CONFIG_DETAIL_FOR_EACH(VASE_CONFIG_DETAIL_CHECK, Type, __VA_ARGS__)                                      \
        static constexpr std::array<vase::FieldInfo, VASE_CONFIG_DETAIL_NARG(__VA_ARGS__)> kFields =                  \
            {{VASE_CONFIG_DETAIL_FOR_EACH(VASE_CONFIG_DETAIL_FIELD, Type, __VA_ARGS__)}};                             \
        static void* VaseConfigCreate() { return std::make_unique<Type>().release(); }                                \
        static void VaseConfigDestroy(void* raw) { const std::unique_ptr<Type> owning{static_cast<Type*>(raw)}; }     \
    }
// NOLINTEND(cppcoreguidelines-macro-usage)
// clang-format on
