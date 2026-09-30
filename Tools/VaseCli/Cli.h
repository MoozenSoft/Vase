#pragma once

// 子命令分发（M5/D137）：手写 argv，不引 cli 库。退出码三档见主文件头。

#include "Vase/PluginDescriptor.h"

#include <cstddef>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace tools::cli
{

// 0 = 全过；1 = 有检查未过；2 = 用法或环境错误。不把四项编码进不同码位（D135）。
inline constexpr int kExitOk = 0;
inline constexpr int kExitCheckFailed = 1;
inline constexpr int kExitUsage = 2;

// 尾段成对消费 `--host-provides <name>@<version>`（validate 与 plan 共用，M6/D146）：
// 任一格错即点名并返回 false；args 存活须覆盖调用（D60 的借窗同律）。
bool ParseTrailingHostProvides(const std::vector<std::string>& args, std::size_t firstFlag,
                               std::vector<vase::ServiceRef>& out, std::string_view usageLine, std::ostream& err);

int Run(int argc, char** argv, std::ostream& out, std::ostream& err);

} // namespace tools::cli
