#pragma once

// 子命令分发（M5/D137）：手写 argv，不引 cli 库。退出码三档见主文件头。

#include <iosfwd>

namespace tools::cli
{

// 0 = 全过；1 = 有检查未过；2 = 用法或环境错误。不把四项编码进不同码位（D135）。
inline constexpr int kExitOk = 0;
inline constexpr int kExitCheckFailed = 1;
inline constexpr int kExitUsage = 2;

int Run(int argc, char** argv, std::ostream& out, std::ostream& err);

} // namespace tools::cli
