#include "Cli.h"

#include "Doctor.h"
#include "Plan.h"
#include "Scan.h"
#include "Validate.h"
#include "Vase/PluginDescriptor.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
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
           "       VaseCli validate <插件目录> [--host-provides <name>@<version>]...\n"
           "       VaseCli plan <插件目录> [presetFile] [--host-provides <name>@<version>]...\n"
           "       VaseCli doctor <插件目录>\n";
}

// "name@version"：版本必须 ≥1（与 D64 的解析期闸同口径）；名字不得为空。
std::optional<vase::ServiceRef> ParseHostProvided(std::string_view text)
{
    const std::size_t at = text.rfind('@');
    if (at == std::string_view::npos || at == 0U || at + 1U >= text.size())
    {
        return std::nullopt;
    }
    std::uint32_t version = 0;
    const std::string_view digits = text.substr(at + 1U);
    // 指针形态照 Console::ParseIndex（data() 先落命名变量再进 from_chars；data()+size() 吃 pro-bounds）。
    const char* const begin = digits.data();
    const char* const end = std::next(begin, static_cast<std::ptrdiff_t>(digits.size()));
    const auto parsed = std::from_chars(begin, end, version);
    if (parsed.ec != std::errc{} || parsed.ptr != end || version < 1U)
    {
        return std::nullopt;
    }
    return vase::ServiceRef{.Name = text.substr(0, at), .Version = version};
}

} // namespace

namespace tools::cli
{

bool ParseTrailingHostProvides(const std::vector<std::string>& args, std::size_t firstFlag,
                               std::vector<vase::ServiceRef>& out, std::string_view usageLine, std::ostream& err)
{
    // 一次吃「flag + 值」两格：步进放 header 的 std::advance(it, 2)（双 ++it 触发 -Wfor-loop-analysis）。
    for (auto it = std::next(args.begin(), static_cast<std::ptrdiff_t>(firstFlag)); it != args.end();
         std::advance(it, 2))
    {
        if (*it != "--host-provides" || std::next(it) == args.end())
        {
            err << usageLine;
            return false;
        }
        const std::optional<vase::ServiceRef> provided = ParseHostProvided(*std::next(it));
        if (!provided.has_value())
        {
            err << "invalid --host-provides value: " << *std::next(it) << " (expected <name>@<version>=1)\n";
            return false;
        }
        out.push_back(*provided);
    }
    return true;
}

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
    if (command == "plan")
    {
        return RunPlan(args, out, err);
    }
    if (command == "doctor")
    {
        return RunDoctor(args, out, err);
    }

    err << "unknown command: " << command << '\n';
    PrintUsage(err);
    return kExitUsage;
}

} // namespace tools::cli
