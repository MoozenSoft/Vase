#pragma once

// plan 文件：CLI 私有的「Id ↔ 二进制路径」文本。M1 它代替还不存在的 Catalog；T12 起
// KnownBinaries 与旧轨 Adopt 已退役，它退居 **raw 旁路**（`pod new-raw`，D68/D85）：
// 无清单来源 → 局不带期望，adopt/清单换件命令对它响亮拒绝。
// **不是**架构文档 §5.1 的清单格式。

#include "Vase/Detail/Result.h"

#include <filesystem>
#include <string>
#include <vector>

namespace tools::console
{

struct Entry
{
    std::string Id;
    std::filesystem::path BinaryPath;
};

// 一行一条：首个空白之前是 Id，其余整段（含其中的空白）都是路径。
// `#` 起始行与空行忽略。数组序 = 文件序 = LoadPlan::Ordered 序。
vase::Result<std::vector<Entry>> ParsePlanFile(const std::filesystem::path& file);

} // namespace tools::console
