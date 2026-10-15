# 通用 Env：Symbiote 与 Darv 战士路线

日期：2026-09-27。分支：`feat/generic-env-rf-api`。阶段 E3；CombatSolver 保持冻结。

## 前置证据

上一批 `SEED0007/0008` 高生命值铁甲战士三幕对拍零分歧，分别在 `Events.Symbiote.GenerateInitialOptions` 的 332/1371 决策和 `Events.Darv.GenerateInitialOptions` 的 247/589 决策明确停止；其它六条定向三幕路线已完整。完整 `scripts/check.sh` 通过。

## 目标与工作

- 按游戏 v0.111.0 源码移植 Symbiote 全部选项与 Corrupted 附魔；验证事件选择、变牌、附魔、CardSelect 前缀与 RNG 顺序。
- 按源码移植 Darv 的候选遗物组、过滤、预构建选项、洗牌、Dusty Tome 分支和先古对话/profile 行为。需要的遗物效果保持具体 `Unported`，不得空实现。
- 扩大铁甲战士真实三幕路线，使用 oracle 逐决策对拍并恢复快照。每个新出现的内容或容器阻断单独登记，不因前缀匹配而宣称完整。
- 完整运行 `scripts/check.sh`，更新报告与路线图。

## 当前进度

Symbiote 与 Corrupted 已按源码完成；选项、牌筛选、变牌 RNG、附魔伤害和自伤已通过
oracle 对拍。SEED0007 已越过原 Symbiote 阻断，当前下一处阻断是 ChoicesParadox；Darv
仍待实现。专项结果见 `docs/reports/2026-09-28-symbiote-corrupted.md`。

## 验收

两条路线应越过原首停；能到终点的路线按严格完整门禁统计。所有新机制保留 POD/确定性快照，原版状态、选项和 RNG 无分歧。提交前全门禁通过。
