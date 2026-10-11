# Vase 的 Codex 项目指引

## 工程规则

开始项目工作时，先阅读根目录的 [CLAUDE.md](CLAUDE.md)。它是 Claude Code 与 Codex 共用的工程规则，项目状态、构建命令、按平台测试基数与七条规矩都以该文件为准。文件较长时分段读取，确认没有因输出截断漏掉规则；保留其节名，现有技能按节名引用它。

设计、实现、修改或评审 C++、CMake target、插件或测试前，使用 `vase-cpp-engineering` 技能。Codex 的发现入口在 `.agents/skills/vase-cpp-engineering/SKILL.md`，实际规则和 references 共用 `.claude/skills/vase-cpp-engineering/`。

交流与文档中文优先；语言、注释、提交信息的完整约定见 `CLAUDE.md`「语言约定」。验证命令与判据见「构建与测试」「静态检查与格式」「核这些门禁时，退出码单独用是不够的」。

## 代码定位

查找或理解代码前，阅读 [.claude/CLAUDE.md](.claude/CLAUDE.md) 的 CodeGraph 约定。根目录有 `.codegraph/` 时优先使用 CodeGraph MCP 的 `codegraph_explore`；工具不可用时用 `codegraph explore "<问题或符号>"`（Windows PowerShell 用 `codegraph.cmd`）。若工具无法运行，说明原因后再用 `rg` 和文件读取；没有索引时直接用 `rg`。

## 配置位置与环境

Codex 的项目配置放在 `.codex/`，项目技能入口放在 `.agents/skills/`，随仓库管理。共用的工程规则继续在 `CLAUDE.md` 与现有 C++ 技能中维护；操作说明见 [docs/codex-setup.md](docs/codex-setup.md)。

Windows PowerShell 中用 `codex.cmd` 与 `codegraph.cmd`，避免 npm 的 `.ps1` 启动入口被执行策略拦截。`CLAUDE.md` 的 Windows 构建命令按 Git Bash 编写；按该节指定的 shell 运行，不要把 Bash 语法直接交给 PowerShell。
