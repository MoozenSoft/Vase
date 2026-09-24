---
description: 查看定时任务（CronList 实时清单 + 本机事件日志）
---

台账查看命令。执行流程（不要复述本段，照做后直接输出结果）：

1. 跑 `CronList`——**当前活着的任务一律以它为准**；
2. 从磁盘读事件日志 `.claude/scheduled-jobs.md`（不存在 = 本机还没有历史，报空即可；
   勿信任何注入快照，以磁盘为准）；
3. 输出：CronList 实时清单 + 事件日志；某 job 只有「创建」且 CronList 里已无 → 报一行悬空提示。
   本命令**只报告，不改写**日志。

分工：本命令文件进版本库；数据住在 `.claude/scheduled-jobs.md`——被根 .gitignore 忽略，
本机历史不随仓库分发，与调度器自己的 `scheduled_tasks.json`（运行时自管，勿手改）同处 `.claude/` 根。
