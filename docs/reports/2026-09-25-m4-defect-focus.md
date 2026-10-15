# M4 故障机器人 Focus 与球遗物批次执行报告

计划：[`../plans/2026-09-25-m4-defect-focus.md`](../plans/2026-09-25-m4-defect-focus.md)。分支：`feat/m4-defect-focus`。前置：M2 已完成、M3 暂跳过；OrbQueue 批次 `4565678` 已通过完整检查。

## 接入内容

- `FocusPower` 修正球的被动和唤起数值，数值下限为零；`BiasedCognitionPower` 在每个玩家回合开始递减 Focus。
- `Defragment`、`Biased Cognition`、`Focused Strike`、`Hotfix`、`Hyperbeam` 五张牌；后三张的临时 Focus 能力在回合结束撤销，并覆盖同名能力叠加。
- `Data Disk`（进战斗加 Focus）、`Gold-Plated Cables`（队首球多触发一次）、`Runic Capacitor`（首回合加槽）、`Symbiotic Virus`（首回合充能 Dark）四件遗物。
- L4 Oracle 与回放器接入相同的 `--deck`、`--relics` 入口；在奖励流程可达路径中继续移植 `Leap`、`Go for the Eyes`、`Beam Cell`、`Uproar`、`Momentum Strike`、`Barrage`、`Lightning Rod` 七张牌及 `LightningRodPower`。
- 修正怪物死亡后 `NextMove` 的定义保留：原版清除行动状态机但仍允许读取 `NextMove.Intents`。`Go for the Eyes` 在致死伤害后继续检查目标意图，原模拟器此处报错。

## 对拍结果

所有以下测试均用 `tests/l3/l3_combat.py --characters DEFECT --encounter MAWLER_NORMAL --policy-seeds 1,2`，每组 8 个种子；具体牌组、参数与原始输出分别在日志中。

| 情形 | A0 | A10 |
|---|---|---|
| 四遗物、Focus 两牌与球牌混合 | 16/16 场，195/195 状态（`/tmp/m4_focus_mawler_a0.log`） | 16/16 场，201/201 状态（`/tmp/m4_focus_mawler_a10.log`） |
| 上述混合牌组全升级 | — | 16/16 场，177/177 状态（`/tmp/m4_focus_upgrade_a10.log`） |
| 无额外遗物、Biased Cognition 长战斗 | 16/16 场，646/646 状态（`/tmp/m4_focus_negative_a0.log`） | — |
| 三张临时 Focus 牌与球牌混合 | 16/16 场，262/262 状态（`/tmp/m4_focus_temp_a0.log`） | 16/16 场，266/266 状态（`/tmp/m4_focus_temp_a10.log`） |
| 上述临时 Focus 牌组全升级 | — | 16/16 场，229/229 状态（`/tmp/m4_focus_temp_plus.log`） |
| 临时 Focus 同名能力叠加 | — | 16/16 场，205/205 状态（`/tmp/m4_focus_stack_a10.log`） |
| 七张可达牌与球牌混合 | 16/16 场，212/212 状态（`/tmp/m4_focus_cards_final_a0.log`） | 16/16 场，223/223 状态（`/tmp/m4_focus_cards_final_a10.log`） |
| 上述可达牌组全升级 | — | 16/16 场，166/166 状态（`/tmp/m4_focus_cards_upgrade_a10.log`） |

合计 **176/176 场、2782/2782 状态**，0 `Unported`、0 分歧。Oracle 原始轨迹 `/tmp/m4_focus_oracle.jsonl` 显示四遗物组合的球槽容量为 6、四种球均出现、Focus 为 1–7，Dark 唤起值最高 82；`/tmp/m4_focus_negative_oracle.jsonl` 最长至第 26 回合，Focus 最低 -15。`/tmp/m4_focus_temp_oracle.jsonl` 中三张临时 Focus 牌分别打出 21、16、15 次，对应能力均出现在快照中。`/tmp/m4_focus_cards_oracle.jsonl` 中七张新牌分别被打出 10–24 次。

自定义 L4 使用相同的 Defect 牌组和四件遗物、种子 `M4L4FOCUS001`–`008`、策略 `1,2`、跳过 Neow，并启用 `--require-complete`：A0 **16/16 条、554/554 决策**；A10 **16/16 条、412/412 决策**，0 `Unported`、0 分歧（`/tmp/m4_focus_l4_verified_a0.log`、`/tmp/m4_focus_l4_verified_a10.log`）。

完整 `scripts/check.sh` 通过，原始日志 `/tmp/m4_focus_final_check.log`。

## 剩余范围

第一幕首领 `LagavulinMatriarchBoss.GenerateMonsters` 仍未移植，长战斗改用已移植的 `MAWLER_NORMAL`；该首领属于仍待完成的 M4 内容。其他 Defect 牌与遗物、Osty、后两幕、事件与先古仍在 M4 范围内，M4 尚未完成。
