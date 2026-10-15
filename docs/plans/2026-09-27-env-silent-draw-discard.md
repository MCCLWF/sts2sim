# 通用 Env：静默猎手抽弃牌与多段攻击牌

日期：2026-09-27。状态：实现完成，等待主线程生成和构建后做定向 Oracle L3。

## 目标

按原版 v0.111.0 补齐 Outmaneuver、Flechettes，并用原版 Oracle 验证 Dagger Throw 已有移植；保留 Flechettes 在计算变量 `Calculate` 中“基值 + extra × 倍率”的通用语义。

## 原版行为

| 卡牌 | 原版行为 | 移植要点 |
|---|---|---|
| Outmaneuver | 对自身施加 EnergyNextTurnPower，量为 `Energy.IntValue` | 使用现有 PowerCmd 与 EnergyNextTurnPower；升级值从生成的 level delta 读取 |
| Dagger Throw | 先造成 Damage，再抽 1 张，再从手牌选择 1 张弃掉；没有可弃牌时不做弃牌 | 现有头文件已按该顺序实现；选择数量为 1，选择 Prompt 仅 UI |
| Flechettes | `CalculatedHits` 为 CalculationBase(0) + CalculationExtra(1) × 当前手牌 Skill 数；以该计算值作为攻击次数 | `OnPlay` 调用 `DynamicVar.Calculate(target)`；倍率分支只读取出牌后仍在手牌中的 Skill 数，不直接代替公式写入 hits |

## 范围

- 新增 `game/Models/Cards/Outmaneuver.h` 与 `game/Models/Cards/Flechettes.h`。
- 在 `game/Localization/DynamicVars/DynamicVarSet.cpp` 仅补 Flechettes 的 `CalculatedHits` multiplier 分支。
- 复核现有 `game/Models/Cards/DaggerThrow.h`，若与原版一致则保持文件不变。
- 仅新增本计划与执行报告；不改生成文件、基础 Cmd/Hook、Python、主计划或共享测试；不运行生成器、CMake 构建或全量门禁。

## 验收

1. 逐项核对 v0.111.0 原版卡牌、DynamicVar 和相关命令顺序。
2. 确认升级数值由现有生成内容 delta 表达：Dagger Throw Damage +3，Flechettes Damage +2，Outmaneuver Energy +1。
3. 主线程生成和 Release 构建后，只运行 `tests/l3/l3_combat.py` 定向矩阵；初始矩阵用 seeds `SDRAW001,SDRAW002`、policy seeds `13,29`、A0/A10、普通/升级版、`CULTISTS_NORMAL` 与 Silent 基础牌。Dagger Throw 牌组为 4 张目标牌、4 张 Strike、7 张 Defend；Outmaneuver 为 5 张目标牌、4 张 Strike、6 张 Defend；Flechettes 为 3 张目标牌、6 张 Defend、6 张 Strike，以争取观察 0/1/多 Skill 分支。
4. 保留并读取 Oracle trace：Dagger Throw 有真实出牌和弃牌选择；Outmaneuver 下回合增加并消耗能量 Power；Flechettes 实际出牌覆盖 0、1、多张手牌 Skill，且出牌后公式仍以剩余手牌计算。
5. 报告区分有限路径差分与完整游戏证明，并标出 headless TestMode/ProductionBranches 边界。

## 文件

- `game/Models/Cards/Outmaneuver.h`
- `game/Models/Cards/Flechettes.h`
- `game/Localization/DynamicVars/DynamicVarSet.cpp`（单一 Flechettes multiplier 分支）
- `game/Models/Cards/DaggerThrow.h`（只读复核；预计不改）
- `docs/plans/2026-09-27-env-silent-draw-discard.md`
- `docs/reports/2026-09-27-env-silent-draw-discard.md`
