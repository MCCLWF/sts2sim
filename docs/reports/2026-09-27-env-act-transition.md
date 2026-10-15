# 通用 Env 第一幕跨幕实测

日期：2026-09-27。分支 `feat/generic-env-rf-api`，基线 `8a93c1f` 后的未提交集成批次。计划：[跨幕与最终胜利](../plans/2026-09-27-env-act-transition.md)。真值游戏 v0.111.0；CombatSolver 保持冻结。

## 已接通

- 正常 Boss 奖励的 Proceed 退出当前房间，调用 `EnterNextAct/EnterAct`，进入新幕地图；同幕双 Boss 首场保留本幕推进。
- 跨幕时按原版重置幕层数及地图/遭遇权重，沿用预选 RoomSet，执行进入幕 Hook。Oracle 和 L4 现在能记录/比较真实 Boss 后边界及全部 run/player RNG 状态。
- 长 Boss 战正版轨迹曾产生497张战斗牌、单堆421张，暴露原固定容量不足。当前卡池1023可用槽、单堆512、临时 CardList 对齐卡池上界，仍是有限容量；溢出需明确失败，不能截短原版状态。

## 正版证据

| 语料 | 严格重放与恢复 | 终点 |
|---|---|---|
| 无 Neow，SEED0001..SEED0004 × policy1,2 | 8/8、2903/2903决策零分歧；`--verify-snapshots --require-complete` 通过 | 7条实际进入第二幕地图，1条第一幕战死 |
| 有 Neow，SEED0002/SEED0003 × policy1,2 | 4/4、1003/1003决策零分歧；同上通过 | 4条实际进入第二幕地图 |

终点对拍包括地图和全部 run/player RNG counter/state0–3；公共终点查询12/12一致。逐决策恢复还比较导入Hash/合法动作，以及恢复根和普通根执行同一真实动作后的Hash。原始日志：`/tmp/mcc-env-snapshot-stun-l4.log`、`/tmp/mcc-env-snapshot-shared-neow-l4.log`；恢复修复细节见[专门报告](2026-09-27-env-snapshot-stun-shared-page.md)。轨迹使用Ironclad A0、hp10000定向采集，不能代表自然生命值完整胜利。

完整 `scripts/check.sh` 已通过：13/13 CTest；L2 200局零分歧；L3 80战/1611状态零分歧；L4 80局/4089决策零分歧。日志 `/tmp/mcc-env-snapshot-full-check.log`。默认L4仍在第一幕Boss前停止且允许Unported；上表额外轨迹才验证了实际跨幕。

## 尚未接通

继续第二幕的原版轨迹 `/tmp/mcc-env-act2-oracle.jsonl` 在第244/564决策停止于 `Events.Orobas.GenerateInitialOptions is not ported`；原版该选项在更后的事件边界实际给出 ElectricShrymp、Driftwood、ArchaicTooth，行动选择Driftwood。Orobas 的选项生成与候选遗物效果必须按原版补齐，不能替换候选或跳过 RNG。其余二幕/三幕、TheArchitect/WinRun、事件战斗返回父事件、真实档案和游戏内捕获仍缺。按用户优先级，先沿战士整局轨迹处理这些阻塞，再扩展其他角色的新内容；完整 Env/RF 尚未验收。
