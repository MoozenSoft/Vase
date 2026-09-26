// Mood.h —— Hello 家族 enum 配置型的旁生头（D87 肉眼证人）：Greeter 是服务接口不背配置型。
// 枚举与 choices 表同头共用——两插件的 VASE_CONFIG 引同一张表（swapdemo 的 D25 布局同形亦覆盖它）。
#pragma once

#include "Vase/Config/FieldInfo.h"

#include <array>
#include <cstdint>
#include <string>

namespace samples
{

// NOLINTNEXTLINE(performance-enum-size) D75 钉 int32 基型（与 ConfigMacroTests 的 Mood 同因）。
enum class Mood : std::int32_t
{
    kQuiet = 0,
    kLoud = 1,
};

// 作者侧 choices 表（D77 命名存储：inline constexpr，两侧插件共享）。
inline constexpr std::array<vase::ChoiceInfo, 2> kMoodChoices = {
    {
        {.Value = 0, .Label = "安静"},
        {.Value = 1, .Label = "响亮"},
    },
};

// 问候串尾缀的单一真值（两插件 OnLoad 共用；Prime 换件仍含 "prime v2" 钉串）。
// 解构取件而非下标：.at() 在本仓被禁（_HAS_EXCEPTIONS=0），裸 operator[] 过不了 tidy。
[[nodiscard]] inline std::string MoodLabel(Mood mood)
{
    const auto [quiet, loud] = kMoodChoices;
    return mood == Mood::kLoud ? loud.Label : quiet.Label;
}

} // namespace samples
