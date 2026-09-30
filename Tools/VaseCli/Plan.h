#pragma once

// plan（M6/D141/D145）：Refresh → LoadPreset(可选) → Solve → 打印。零装载的全工具唯一预览腿。

#include "Vase/PluginDescriptor.h" // ServiceRef

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace tools::cli
{

struct PlanOptions
{
    std::filesystem::path PluginDirectory;
    std::optional<std::filesystem::path> PresetFile;
    std::vector<vase::ServiceRef> HostProvided; // D60：Name 借 argv 串
};

// 退出码同一律三档见 Cli.h 与 spec §2.3（D142）。
int RunPlan(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

// 供用例直调：跳过 argv 解析（同 ValidateDirectory 形）。
int PlanDirectory(const PlanOptions& options, std::ostream& out, std::ostream& err);

} // namespace tools::cli
