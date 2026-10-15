# M4 故障机器人常见牌扩充执行报告

计划：[`../plans/2026-09-25-m4-defect-common.md`](../plans/2026-09-25-m4-defect-common.md)。分支：`feat/m4-defect-common`。前置：M2 已完成、M3 暂跳过；Defect 球和 Focus 批次已提交且通过完整门禁。

## 接入内容

- `Refract`、`Ice Lance`、`Compile Driver`、`Sunder`、`Synthesis`、`Gunk Up`、`Null` 七张牌；均来自扩展 L4 记录的实际可达停止点。
- Glass 球：可变被动伤害、逐次衰减、全体伤害与 Focus 修饰；`FreePowerPower`：下一张手牌或打出区中的能力牌免能量并消耗层数。
- 状态池增加 Glass 的原始被动值，保持 `GameState` 可平凡复制；已有 L3 快照的球唤起值会逐状态比较其变化。

## 验证

- 自定义 L4 使用 Defect Focus 批次的牌组及四件遗物，种子 `M4L4FOCUS009`–`020`、策略 `1,2`、跳过 Neow，并启用 `--require-complete`：A0 **24/24 条、784/784 决策**；A10 **24/24 条、625/625 决策**，均为 0 `Unported`、0 分歧。日志：`/tmp/m4_defect_common_l4_a0.log`、`/tmp/m4_defect_common_l4_a10.log`。
- 七张新牌的自定义 L3 牌组，种子 `M4COMMON001`–`008`、策略 `1,2`、`MAWLER_NORMAL`：A0 **16/16 场、170/170 状态**；A10 **16/16 场、180/180 状态**。全升级牌组 A10 **16/16 场、150/150 状态**。日志：`/tmp/m4_defect_common_l3_a0.log`、`/tmp/m4_defect_common_l3_a10.log`、`/tmp/m4_defect_common_l3_upgrade.log`。
- Glass 长战斗：A10 **16/16 场、543/543 状态**；Oracle 快照覆盖唤起值 8、6、4、2、0，最长至第 17 回合。日志：`/tmp/m4_defect_glass_long.log`，原始轨迹：`/tmp/m4_defect_glass_oracle.jsonl`。
- `Synthesis` 与能力牌组合：A10 **16/16 场、220/220 状态**；Oracle 打出 `Synthesis` 27 次、记录到 `FreePowerPower` 43 个状态，31 个状态同时有零费用能力牌在手。日志：`/tmp/m4_defect_synthesis_a10.log`，原始轨迹：`/tmp/m4_defect_synthesis_oracle.jsonl`。
- L3 合计 **80/80 场、1263/1263 状态**，0 `Unported`、0 分歧。混合牌组的 Oracle 轨迹实际打出七张新牌各 4–15 次（`/tmp/m4_defect_common_oracle.jsonl`）。完整 `scripts/check.sh` 通过，日志 `/tmp/m4_defect_common_check.log`。

## 仍待完成

这批固定 L4 种子只覆盖可达的 Defect 牌与第一幕部分房间。Defect 余牌、其他职业、Osty、第二/三幕、事件与先古仍属于 M4，未据此宣称 M4 完成。
