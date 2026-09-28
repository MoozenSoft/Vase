#pragma once

#include "Vase/PluginDescriptor.h"

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace tools::cli
{

struct ValidateOptions
{
    std::filesystem::path PluginDirectory;
    std::vector<vase::ServiceRef> HostProvided; // --host-provides 喂进 LoadRequest（D131，Task 12 解析）
};

// 四项检查（spec §3.2）：① 清单↔二进制一致 ② Id 重复 ③ 依赖图/环/版本 ④ 服务命名前缀。
// 退出码见 Cli.h；② 失败时快照不建成，只报该项（D130）。
int RunValidate(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

// 供用例直接调：跳过 argv 解析。
int ValidateDirectory(const ValidateOptions& options, std::ostream& out, std::ostream& err);

} // namespace tools::cli
