#include "Shell.h"

// 引号包含是**承重的**：cl.exe 线的 /external:anglebrackets 会把尖括号包含的整棵头树标为
// 外部头，连 /we4530 升出的 C4530 一起静音——那正是本 fork 「不使用异常」这条不变式
// 唯一的编译期把守。理由与被否写法见 Cmake/VaseThirdParty.cmake（spec §5.4）。
#include "cli/cli.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <istream>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace tools::console
{
namespace
{
// 不用 cli::CliFileSession：它把 history 大小钉成 1（clifilesession.h:46），而且它的
// Start() 只看 eof、拿不到「脚本读完却没走 exit」这个停止原因——那是退出码规则 ④ 的输入。
constexpr std::size_t kHistorySize = 100;
} // namespace

ShellStop RunShell(const std::vector<CommandSpec>& commands, std::istream& in, std::ostream& out,
                   const std::function<void(std::ostream&, const std::string&)>& onUnmatched)
{
    auto root = std::make_unique<cli::Menu>("vase");
    // 子菜单按 Group 惰性建立。实测：`pod new a b` 会路由到子菜单 new 且 args=["a","b"]，
    // 所以两段名不需要把空格塞进命令名（cli 先按空白切 token，名字里带空格永远匹配不上）。
    std::vector<std::pair<std::string, std::unique_ptr<cli::Menu>>> groups;
    for (const CommandSpec& command : commands)
    {
        cli::Menu* parent = root.get();
        if (!command.Group.empty())
        {
            const auto found =
                std::ranges::find_if(groups, [&command](const auto& entry) { return entry.first == command.Group; });
            if (found != groups.end())
            {
                parent = found->second.get();
            }
            else
            {
                auto menu = std::make_unique<cli::Menu>(command.Group);
                cli::Menu* raw = menu.get();
                groups.emplace_back(command.Group, std::move(menu));
                parent = raw;
            }
        }
        // 拷贝一份 handler：Insert 把它移进菜单持有的命令对象，而 command 是调用方的。
        const auto handler = command.Handler;
        parent->Insert(
            command.Name, command.Params,
            [handler](std::ostream& sessionOut, const std::vector<std::string>& args)
            {
                if (handler)
                {
                    handler(sessionOut, args);
                }
            },
            command.Help);
    }
    for (auto& [name, menu] : groups)
    {
        root->Insert(std::move(menu));
    }

    cli::Cli cli(std::move(root));
    if (onUnmatched)
    {
        // cli 把「命令不存在」与「参数形状不对」都归这条通道（CliSession::Feed 的调用点）。
        // 形参按值（setter 自己持有一份），故这里传引用副本即可。
        cli.WrongCommandHandler(onUnmatched);
    }

    bool exitRequested = false;
    cli::CliSession session(cli, out, kHistorySize);
    session.ExitAction([&exitRequested](std::ostream&) { exitRequested = true; });
    session.Enter();

    ShellStop stop = ShellStop::kEndOfInput;
    while (!exitRequested)
    {
        session.Prompt();
        std::string line;
        // 判据只认 bad/fail：末行不带换行时 getline 仅置 eofbit，那是「脚本自然读完」，
        // 归 kEndOfInput（规则 ④/⑤ 要区分这两类停止原因）。
        if (in.bad() || in.fail())
        {
            stop = ShellStop::kInputStreamError;
            break;
        }
        // 先 Feed 再分类：getline 在末行无换行时仍抽得出字符（只置 eofbit、不置 failbit），
        // 这条 exit 照喂不误；下一轮 getline 空手而归才落到 else 里按 eofbit 归 kEndOfInput。
        const bool gotLine = static_cast<bool>(std::getline(in, line));
        // cli 的 split 不认 \r 为分隔符：CRLF 脚本（file(GENERATE) 在 Windows 即产出 CRLF，
        // 实测）每行会带尾 \r 进 Feed，`exit` 变 `exit\r` 而匹配不上。在消费点剥掉。
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (gotLine)
        {
            session.Feed(line);
        }
        else
        {
            // bad 先于 eof：eofbit 与 badbit 同置的角落仍算「输入流出错」（规则 ④）。
            stop = (in.bad() || !in.eof()) ? ShellStop::kInputStreamError : ShellStop::kEndOfInput;
            break;
        }
    }

    if (exitRequested)
    {
        stop = ShellStop::kByExitCommand;
    }
    out << std::flush;
    return stop;
}

} // namespace tools::console
