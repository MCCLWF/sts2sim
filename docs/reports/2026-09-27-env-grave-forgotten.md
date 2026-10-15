# 通用 Env：Grave of the Forgotten 执行报告

日期：2026-09-27。分支：`feat/generic-env-rf-api`。原版依据：v0.111.0 `GraveOfTheForgotten.cs`、`SoulsPower.cs`、`ForgottenSoul.cs`、`CardCmd.cs`，以及正版 DLL 的 L4 oracle。计划：[Grave of the Forgotten](../plans/2026-09-27-env-grave-forgotten.md)。

## 前置与实现

Whispering Earring 接入后，另两条铁甲战士高生命值三幕路线在 `Events.GraveOfTheForgotten.GenerateInitialOptions` 受控停止；此前 2668/3520 决策已对齐、零分歧。

- Grave of the Forgotten 提供 Confront/Accept，并按主人牌组中是否存在可附魔牌锁定 Confront。Confront 先把 Decay 加入牌组，再从 SoulsPower 有效候选中选择一张施加附魔，随后结束事件；Accept 取得 ForgottenSoul 后结束事件。
- SoulsPower 的资格条件包括普通附魔的牌种、牌组中不可打出、既有附魔限制和本地 Exhaust；首次施加时按原版移除 Exhaust。此内容规则进入 `CardCmd::Enchant`，不是事件内的临时伪效果。
- ForgottenSoul 的耗尽 Hook 只处理主人卡牌，以主人 `CombatTargets` RNG 从可命中敌人选目标，使用自带 `DamageVar(1, Unpowered)` 造成伤害。源代码中的预览、音效与 VFX 未进入无头 Env。

## 验证

| 检查 | 结果 |
|---|---|
| `cmake --build build --target sts2sim_cli -j2` | 通过 |
| `cmake --build build --target test_relics -j2 && ./build/test_relics` | ForgottenSoul 与 SoulsPower 聚焦测试通过；完整门禁再次执行 `test_relics` 并通过 |
| `DOTNET=~/.dotnet/dotnet python3 tests/l4/l4_run.py --seeds SEED0002,SEED0003 --policy-seeds 1,2 --no-neow --hp 10000 --stop victory --verify-snapshots --require-complete` | 两条完整一致；另两条越过 Grave 后停在 `Events.Reflections.GenerateInitialOptions`；2674/3520 决策已对齐、零分歧，命令因新 `Unported` 退出 1 |
| `scripts/check.sh` | 通过：16/16 CTest、L2 200 局、L3 80 场/1611 状态、默认 L4 80 局/4089 决策、严格二幕 3246 决策、既有胜利 1138 决策、新四路线 3520 决策、Pael's Wing 219 决策；所有定向严格路线零分歧零 Unported |

## 限制

两条路线只覆盖它们实际选到的 Grave 选项，不证明 Confront/Accept 的所有卡牌条件和后续遗物战斗组合均已通过原版差分；SoulsPower 和 ForgottenSoul 的聚焦测试保护具体源码不变量。后续 Reflections、长战斗容量与 The Architect 已另批接入，四条扩展三幕路线最终 3520/3520 决策对齐。全 Env、真实档案与 RF API 尚未完成；CombatSolver 保持冻结。
