#pragma once

// doctor（M6/D143/D144/D152）：收窄四项环境诊断。收集序 ①③④→②（② 唯一装载步置后），
// 输出按 check 1–4 固定序。

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace tools::cli
{

int RunDoctor(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

// 供用例直调：跳过 argv 解析。退出码同一律三档（spec §3.5）。
int DoctorDirectory(const std::filesystem::path& pluginDirectory, std::ostream& out, std::ostream& err);

} // namespace tools::cli
