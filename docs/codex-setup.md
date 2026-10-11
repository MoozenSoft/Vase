# 在 Vase 中使用 Codex

本仓库保留 Claude Code 的配置，并为 Codex 增加项目入口；两者共用工程规则。

| 文件 | 用途 |
|---|---|
| `AGENTS.md` | Codex 自动读取的项目指引，要求读取根 `CLAUDE.md` 与 CodeGraph 约定 |
| `CLAUDE.md` | 共用的项目状态、构建命令、规则与验证基数，继续在这里维护 |
| `.agents/skills/vase-cpp-engineering/SKILL.md` | Codex 可发现的技能入口，转到现有 `.claude/skills/vase-cpp-engineering/` |
| `.codex/config.toml` | 项目级 CodeGraph MCP 配置，按当前 Windows 环境启动 |
| `.mcp.json`、`.claude/` | 原有 Claude Code 配置，继续保留 |

## 启动与检查

本机已安装 Codex CLI。Windows PowerShell 在仓库根目录运行：

```powershell
Set-Location D:\Git\Vase
codex.cmd
```

新会话首次出现项目可信提示时，确认本仓库可信；Codex 仅在可信项目中加载 `.codex/config.toml`。已有会话需要重新打开项目或启动新会话，使配置与技能入口重新加载。

在终端检查服务器配置：

```powershell
codex.cmd mcp get codegraph
```

在 Codex CLI 会话中用 `/mcp` 检查实际连接，用 `/skills` 查看 `vase-cpp-engineering`。可以显式输入 `$vase-cpp-engineering`，也可以让 Codex 在相关 C++ 任务中自动选择它。`mcp get` 只证明配置可见，实际连接以 `/mcp` 或 MCP 握手结果为准。

Windows 使用 `.cmd` 入口避开 PowerShell 对 npm `.ps1` 启动脚本的限制，无需修改系统执行策略。CodeGraph 必须在启动 Codex 的环境的 PATH 中；本项目配置不写死用户目录。

## Linux / macOS

`AGENTS.md` 与技能入口跨平台共用。当前 MCP 启动项使用 Windows 的 `cmd.exe`；在 Linux/macOS 本地使用 CodeGraph 时，将 `.codex/config.toml` 中该服务器的启动项改为：

```toml
[mcp_servers.codegraph]
command = "codegraph"
args = ["serve", "--mcp"]
startup_timeout_sec = 30
tool_timeout_sec = 120
```

对应环境需已有 CodeGraph 和项目索引。共用工作树切换启动环境时，注意该配置会影响另一侧；远程环境的 MCP 连接需单独检查。

## Claude 配置的兼容范围

Claude 的 `.mcp.json` 不作为 Codex 项目 MCP 配置；对应服务器已在 `.codex/config.toml` 中声明。Claude 的 `settings.json`、`enabledPlugins`、`UserPromptSubmit` hook 与 `/moozen:*` 定时任务命令仍属于 Claude 环境，本次没有将这些格式直接复制给 Codex，也没有启用原定时任务。Codex 的 CodeGraph 检索行为由 `AGENTS.md` 明确引导。

模型、账号与审批策略继承当前 Codex 用户或受管设置；本仓库不写入凭据，不改全局配置。

官方依据：[项目指引 AGENTS.md](https://learn.chatgpt.com/docs/agent-configuration/agents-md)、[项目配置与可信项目](https://learn.chatgpt.com/docs/config-file/config-basic)、[MCP 配置](https://learn.chatgpt.com/docs/extend/mcp?surface=cli)、[技能](https://learn.chatgpt.com/docs/build-skills)。
