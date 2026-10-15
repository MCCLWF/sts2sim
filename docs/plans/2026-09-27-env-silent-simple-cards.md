# 通用 Env：静默猎手五张基础牌

日期：2026-09-27。状态：五张牌、Sidestep 所需 Power 与指定有限 L3 均已完成。

## 目标

逐字移植原版 v0.111.0 的 Prepared、Escape Plan、Adrenaline、Sidestep、Expose 五张卡牌行为，供通用 Env 内容覆盖继续推进。真值仅取自 `../Slay the Spire 2/src/Core/Models/Cards/` 与相关命令源码。

## 范围

- 新增对应的五个 `game/Models/Cards/*.h` 与 Sidestep 必需的 `game/Models/Powers/EnergyNextTurnPower.h`，以及本计划和执行报告。
- 不触碰 CombatSolver、生成文件、共享测试、主计划或路线图；不运行生成器和 CMake 构建。
- 类内 `OnUpgrade` 只改 DynamicVar 的部分以已有生成 delta 表达，不重复覆写。
- 允许丢弃的 VFX/动画/hover tip 必须是纯表现调用；实际卡牌、费用、选择与 Hook 状态写入照常保留。

## 行为核对

| 卡牌 | 原版效果 | C++ 对应 |
|---|---|---|
| Prepared | 先取 `Cards.IntValue` 并抽取该数，再从手牌选取同数并弃掉；不足时选取现有手牌 | `CardPileCmd::Draw`、`CardSelectCmd::FromHandForDiscard`、`CardCmd::Discard` |
| Escape Plan | 抽一张；仅当抽到牌为 Skill 时获得 Block | `CardPileCmd::Draw` 后检查该牌类型，再 `CreatureCmd::GainBlock` |
| Adrenaline | 获得 Energy，再按 Cards.BaseValue 抽牌；关键词 Exhaust | `PlayerCmd::GainEnergy`、`CardPileCmd::Draw`；关键词由生成内容表提供 |
| Sidestep | 对自身施加 EnergyNextTurnPower，量为 Energy.IntValue；下回合重置能量后返还此量并移除 Power | `PowerCmd::Apply(ENERGY_NEXT_TURN_POWER, ...)`；Power 的 `AfterEnergyReset` 先加能量后移除 |
| Expose | 目标失去其当前全部 Block；若仍有 Artifact 则移除 1 层；再施加 Power 量易伤；关键词 Exhaust | `CreatureCmd::LoseBlock` → 条件 `PowerCmd::Remove` → `PowerCmd::Apply(VULNERABLE_POWER, ...)` |

逐类检查所有源码覆写：五类的 `CanonicalVars` 均由既有生成内容表提供；Escape Plan 的 `GainsBlock` 需要行为覆写；Adrenaline 与 Expose 的 Exhaust `CanonicalKeywords` 已在生成表中；三张牌的 `ExtraHoverTips` 仅用于界面展示；所有 `OnUpgrade` 只改变已由升级 delta 表达的 DynamicVar。Adrenaline 的全屏 VFX，以及 Expose 的动画和 VFX 仅接触表现节点，可省略。

## 验收

1. 对五份原版源码逐项复核效果顺序、变量访问、选择边界和目标处理。
2. 确认生成内容表中所有升级值对应原版 `OnUpgrade`：Prepared Cards 1→2，Escape Plan Block 3→5，Adrenaline Energy 1→2，Sidestep Energy 1→2，Expose Power 2→3。
3. 主线程统一生成/构建后，使用正版 oracle L3：两个 seed、两个策略、A0/A10、自定义含目标牌及基础牌组，并覆盖可升级牌变体。保留 trace 核实目标牌已打出、Prepared 进入牌选、Escape Plan 两个条件分支都被触达。期望值不得由模拟器自举。

## 文件

- `game/Models/Cards/Prepared.h`
- `game/Models/Cards/EscapePlan.h`
- `game/Models/Cards/Adrenaline.h`
- `game/Models/Cards/Sidestep.h`
- `game/Models/Cards/Expose.h`
- `game/Models/Powers/EnergyNextTurnPower.h`（Sidestep 的原版必要依赖）
- 本计划与对应执行报告
