# M4 亡灵契约师 Osty 与起始链路执行报告

计划：[`../plans/2026-09-25-m4-necrobinder-osty.md`](../plans/2026-09-25-m4-necrobinder-osty.md)。分支：`feat/m4-necrobinder-osty`。前置：M2 已完成，M3 暂跳过。

## 接入内容

- Osty 宠物：创建、生命上限增长、死亡后保留与复活；`DieForYouPower` 代挡敌方有力量加成的攻击；`BoundPhylactery` 开战与后续回合召唤。
- 起始牌 `Bodyguard`、`Unleash`；额外加入 `Afterlife`、`Defile`、`Fear`、`Drain Power`、`Debilitate`、`Enfeebling Touch`，以及 `DebilitatePower` 和 `EnfeeblingTouchPower`。
- L3 战斗快照增加宠物 ID、战斗 ID、生命、格挡、能力与下个动作，避免只对拍玩家和敌人时漏掉 Osty 差异。修正幻象死亡时临时能力保留规则，和原版 `ITemporaryPower` 行为一致。

## 验证

- 起始牌组 `M4OSTY001`–`020`、策略 `1,2`：A0 **40/40 场、985/985 状态**；A10 **40/40 场、993/993 状态**。
- 混合牌组 `M4OSTYC001`–`008`、策略 `1,2`、`MAWLER_NORMAL`：A0 **16/16 场、330/330 状态**；A10 **16/16 场、344/344 状态**。全升级牌组：A0 **16/16 场、219/219 状态**；A10 **16/16 场、231/231 状态**。原版轨迹实际打出 `Bodyguard` 23 次、`Unleash` 17 次、`Afterlife` 10 次、`Defile` 14 次、`Fear` 21 次、`Drain Power` 17 次、`Debilitate` 21 次、`Enfeebling Touch` 9 次；宠物快照共 274 个，最大生命 1–20。
- `Unleash` 单卡长战斗 `M4OSTYD001`–`004`、策略 `1,2`：**8/8 场、112/112 状态**，覆盖至第 7 回合和 Osty 的生命增长、重置。L3 合计 **152/152 场、3214/3214 状态**，0 未移植、0 分歧。
- 自然奖励 L4 `M4OSTYR001`–`008`、策略 `1,2`、跳过 Neow、`--require-complete`：A0 **16/16 条、578/578 决策**；A10 **16/16 条、476/476 决策**，均为 0 未移植、0 分歧。
- 完整 `scripts/check.sh` 门禁通过，日志 `/tmp/m4_osty_check.log`。

## 仍待完成

该批仅覆盖 Osty 起始链路和少量奖励牌。Necrobinder 其余牌及宠物交互、其他职业剩余内容、第二/三幕、Underdocks、事件、先古、附魔、苦难与修饰器仍属 M4。
