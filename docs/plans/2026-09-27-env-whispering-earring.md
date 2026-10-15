# 通用 Env：Whispering Earring 自动出牌

日期：2026-09-27。分支：`feat/generic-env-rf-api`。阶段 E1/E3；衔接 Vakuu 后真实铁甲战士三幕路线。

## 目标

按 v0.111.0 原版实现 Whispering Earring 的能量增加和首回合最多十三张自动出牌，包括资源支出、目标、停止顺序及 `VakuuCardSelector` 的临时选择语义。作为通用 Env 规则实现，不能用针对两条测试路线的硬编码绕过。

## 工作

- 接入 `ModifyMaxEnergy` 和 `AfterAutoPrePlayPhaseEnteredLate` Hook；前者仅对遗物主人增加 `EnergyVar(1)`，后者只在其首回合执行。
- 依次检查战斗结束、准备结束回合、回合编号改变和首张可出牌；先 `SpendResources`，再 `CardCmd::AutoPlay`，保持原版 `skipXCapture=true`。
- 对敌人目标取可命中敌人首项；对其他玩家目标使用 `CombatTargets.NextItem`；自己目标直接用主人。仅演出用 Flash/Talk 不进入模拟状态。
- 提供作用域内的原版第一项卡牌选择器，进入嵌套选择时不向外暴露人工决策；退出后恢复先前选择语义。它的状态须是 POD 且可快照恢复。
- 用聚焦测试和原版 L4 路线验证；如果后继暴露其它独立缺口，准确记录首次停点，不以停点后无对拍数据宣称完整。

## 验收

生成字段和 dispatch 检查、相关 CTest、`scripts/check.sh` 全部通过；`SEED0002,SEED0003` × policy 1,2 的严格三幕路线至少越过原 `ModifyMaxEnergy` 停点并保持零分歧。结果写入同名执行报告。
