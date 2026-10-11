---
name: vase-cpp-engineering
description: 在 Vase 仓库设计、实现、修改或评审 C++、CMake target、插件和测试时使用。读取项目既有的架构、生命周期、ABI、无异常与跨平台验证约束。
---

# Vase C++ 工程：Codex 入口

先读取 [共用工程技能](../../../.claude/skills/vase-cpp-engineering/SKILL.md)，按它的路由表读取本次任务相关的 references，并遵循其工程流程与验证要求。

共用技能中的 `references/...` 均相对于仓库根目录下的 `.claude/skills/vase-cpp-engineering/`，例如 `references/verification.md` 的实际位置是 `.claude/skills/vase-cpp-engineering/references/verification.md`。其中的根 `CLAUDE.md` 指仓库根目录文件。

本入口只负责 Codex 发现与路径适配；工程规则在共用技能、根 `CLAUDE.md` 和源码注释中维护。
