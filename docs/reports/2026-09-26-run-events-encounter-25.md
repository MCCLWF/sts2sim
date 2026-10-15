# M4 run 内容批次 25：Bugslayer 与事件奖励牌

计划：[批次 25](../plans/2026-09-26-run-events-encounter-25.md)。本批接入 Bugslayer 事件、Exterminate、Squash 和 Bad Luck。

## 实现

- Bugslayer 两个选项分别向牌组加入 Exterminate 或 Squash，并正确结束事件。
- Exterminate 完成全体敌人四次攻击及伤害升级。
- Squash 完成单体攻击、易伤及升级数值。
- Bad Luck 完成不可生成、不可升级、永恒不可打出及回合结束掉血。

## 验收

- 生成器和构建通过，709 个模型接入分发。
- `test_colorless` 覆盖事件两项选择、两张事件牌的升级/战斗行为及 Bad Luck 的关键词/掉血行为。
- 完整 `scripts/check.sh`：通过；CTest 8/8，smoke death=94/unported=6、death=100、boss=3/death=97；L2 200/200；L3 两组 40/40（784/784、827/827）；L4 两组 40/40（1958/1958、2131/2131），0 diverged、0 unported。Oracle 构建 0 errors，仅保留既有 `OracleRun.cs(234,84) CS8604` 警告。

## 边界

本批没有实现其他角色卡牌或跨职业卡池路径。
