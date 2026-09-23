#pragma once

// plan 文件：CLI 私有的「Id ↔ 二进制路径」文本，M1 用来代替还不存在的 Catalog。
// **不是**架构文档 §5.1 的清单格式，M2 起与 KnownBinaries 一起退场（spec §8.2）。

#include "Vase/Detail/Result.h"

#include <filesystem>
#include <string>
#include <vector>

namespace samples::plan
{

struct Entry
{
    std::string Id;
    std::filesystem::path BinaryPath;
};

// 一行一条：首个空白之前是 Id，其余整段（含其中的空白）都是路径。
// `#` 起始行与空行忽略。数组序 = 文件序 = LoadPlan::Ordered 序。
vase::Result<std::vector<Entry>> ParsePlanFile(const std::filesystem::path& file);

} // namespace samples::plan
