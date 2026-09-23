// VaseConsole —— 交互式热插拔验证台（spec §1）。同一套命令既能敲也能 --script 回放，
// 退出码即「全程是否 Clean」。
#include "Console.h"
#include "Shell.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string_view>

int main(int argc, char** argv)
{
    std::istream* input = &std::cin;
    std::ifstream script;

    if (argc > 1)
    {
        const std::string_view flag{*std::next(argv, 1)};
        if (flag != "--script" || argc < 3)
        {
            std::cerr << "usage: VaseConsole [--script <file>]\n";
            return 2;
        }
        script.open(std::string{*std::next(argv, 2)}.c_str(), std::ios::binary);
        if (!script.is_open())
        {
            // 打不开脚本是「这次判定没发生」，不是干净运行。
            std::cerr << "cannot open script: " << *std::next(argv, 2) << "\n";
            return 2;
        }
        input = &script;
    }

    tools::console::Console console;
    const tools::console::ShellStop stop = tools::console::RunShell(
        console.Commands(), *input, std::cout,
        [&console](std::ostream& out, const std::string& cmd) { console.UnmatchedCommand(out, cmd); });
    // 任何停止原因都先拆局再判定：exit 语义是「拆净所有局」（spec §3.3），而
    // kEndOfInput/kInputStreamError 两支若留着活局走到 ~Console，Debug 下先撞 ~PluginHost 断言。
    console.TearDownAll(std::cout);
    return console.Verdict(stop);
}
