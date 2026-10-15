# 通用 Env：Whispering Earring 自动出牌执行报告

日期：2026-09-27。分支：`feat/generic-env-rf-api`。依据：v0.111.0 `Models/Relics/WhisperingEarring.cs`、`VakuuCardSelector.cs`、`Commands/CardSelectCmd.cs`、`Rewards/CardReward.cs` 和正版 `Sts2Oracle`。计划：[Whispering Earring](../plans/2026-09-27-env-whispering-earring.md)。

## 前置与实现

Vakuu 接入后，四条高生命值铁甲战士三幕路线中两条完整，另两条在 `Relics.WhisperingEarring.ModifyMaxEnergy` 停止，之前已对齐 2513/3520 决策且无分歧。

- `ModifyMaxEnergy` 仅给遗物主人增加原版 EnergyVar 的基础值。首回合 Late Hook 最多自动出牌十三次；每次按原版顺序检查战斗结束、准备结束回合、回合编号和手牌首张可出牌，再计算目标、支付资源并以 `skipXCapture=true` 自动打出。
- 目标为可命中敌人首项、符合条件的其他玩家随机项或主人自己；其他目标类型为空。Flash/Talk 是演出，不影响 Env 状态。
- `VakuuCardSelector` 的临时选择策略由 `GameState` 的 POD 模式保存：卡牌选择取原顺序前 `maxSelect` 张；嵌套卡牌奖励选第一张，无牌则不选。作用域退出时恢复先前模式；快照、Hash 和导入校验覆盖模式。

## 验证

| 检查 | 结果 |
|---|---|
| `python3 tools/gen/gen_state_fields.py --check`；`python3 tools/gen/gen_dispatch.py --check`；`git diff --check` | 通过 |
| 聚焦 `test_relics`、`test_snapshot` 和 `sts2sim_cli` 构建 | 通过；选择器的前项、嵌套、异常恢复、Hash 与卡牌奖励分支有聚焦检查 |
| `DOTNET=~/.dotnet/dotnet python3 tests/l4/l4_run.py --seeds SEED0002,SEED0003 --policy-seeds 1,2 --no-neow --hp 10000 --stop victory --verify-snapshots --require-complete` | 两条完整一致；两条越过 Whispering Earring 后在 `Events.GraveOfTheForgotten.GenerateInitialOptions` 受控停止；2668/3520 决策已对齐，零分歧。命令因 `--require-complete` 在该新缺口退出 1 |
| `scripts/check.sh` | 全部通过：15/15 CTest；L2 200局零分歧；L3 80战、1611状态零分歧；默认 L4 80局、4089决策零分歧；聚焦遗物428决策；严格二幕3246决策；三幕胜利1138决策；Pael's Wing 219决策。Oracle 构建有既有可空性警告，0 错误 |

## 边界

当前两条受阻路线只验证了 Grave of the Forgotten 之前的实际后继；它们不足以证明 Whispering Earring 在所有卡牌、目标和十三次自动出牌组合下都正确。聚焦测试与快照检查提供局部语义证据，后续仍要扩大原版差分。Grave of the Forgotten 是下一处独立内容缺口。CombatSolver 保持冻结，RF/真实档案/API 完整验收仍未完成。
