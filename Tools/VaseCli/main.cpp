// VaseCli —— 批处理工具（M5/§11.1）：scan 生成清单、validate 校验四项。
// 退出码三档（D135）：0 = 全过；1 = 有检查未过；2 = 用法或环境错误。
#include "Cli.h"

#include <iostream>

int main(int argc, char** argv) { return tools::cli::Run(argc, argv, std::cout, std::cerr); }
