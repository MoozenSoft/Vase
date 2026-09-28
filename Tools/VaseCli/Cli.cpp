#include "Cli.h"

#include "Scan.h"
#include "Validate.h"

#include <iterator>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

std::vector<std::string> Arguments(int argc, const char* const* argv)
{
    std::vector<std::string> args;
    for (int index = 1; index < argc; ++index)
    {
        args.emplace_back(*std::next(argv, index)); // std::next 形态：pro-bounds 两关的既有惯例（同 Solve 的 At）
    }
    return args;
}

void PrintUsage(std::ostream& err)
{
    err << "usage: VaseCli scan <插件目录>\n"
           "       VaseCli validate <插件目录> [--host-provides <name>@<version>]...\n";
}

} // namespace

namespace tools::cli
{

// NOLINTNEXTLINE(misc-const-correctness) char** 是契约：main 的 argv 原样直通（Cli.h 同签名），不可收窄。
int Run(int argc, char** argv, std::ostream& out, std::ostream& err)
{
    const std::vector<std::string> args = Arguments(argc, argv);
    if (args.empty())
    {
        PrintUsage(err);
        return kExitUsage;
    }

    const std::string_view command = args.front();
    if (command == "scan")
    {
        if (args.size() != 2U)
        {
            PrintUsage(err);
            return kExitUsage;
        }
        return RunScan(*std::next(args.begin(), 1), out, err);
    }
    if (command == "validate")
    {
        return RunValidate(args, out, err);
    }

    err << "unknown command: " << command << '\n';
    PrintUsage(err);
    return kExitUsage;
}

} // namespace tools::cli
