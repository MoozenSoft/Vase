#pragma once

// 命令表 ↔ ThirdParty/cli 的桥。本头文件**不出现任何 cli 类型**：认识 cli 的只有
// Shell.cpp，Commands 那一侧只看见 CommandSpec。

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

namespace samples::shell
{

struct CommandSpec
{
    std::string Group;               // 空 = 挂在根菜单上；非空 = 挂在同名子菜单下（如 "pod" / "file"）
    std::string Name;                // 子命令名，单 token（"new" 而不是 "pod new"）
    std::vector<std::string> Params; // 参数名，只为 usage/help 服务；值一律 string
    std::string Help;
    std::function<void(std::ostream&, const std::vector<std::string>&)> Handler;
};

// 循环为什么结束。**不是**「跑得好不好」——累计判据在命令层（spec §5.2）。
enum class ShellStop : std::uint8_t
{
    kByExitCommand,
    kEndOfInput,
    kInputStreamError,
};

// 未匹配输入必须走 onUnmatched：cli 的默认处置只在会话流打一行文案、**不经任何命令层判据**——
// 不接管则脚本敲错一条线仍可能整场 rc=0。这条通道**比「命令名没匹配上」宽**：cli 内建命令
// （`help` / `!`）的 arity 不对（`help pod`、裸 `!`）也从这里走，故同样判非零（`help` / 空行不判）。
// 签名同 cli::Cli::WrongCommandHandler；空 = 维持 cli 默认。
ShellStop RunShell(const std::vector<CommandSpec>& commands, std::istream& in, std::ostream& out,
                   const std::function<void(std::ostream&, const std::string&)>& onUnmatched);

} // namespace samples::shell
