# 通用 Env：静默猎手留牌与 Shiv 生成牌

日期：2026-09-27。状态：实现完成，等待主线程统一生成、构建与定向差分验收。

## 目标

按原版 v0.111.0 移植 Expertise、Hidden Daggers、Leading Strike，补齐静默猎手通用 Env 内容覆盖中的抽牌留牌和 Shiv 生成行为。规则依据仅为 `../Slay the Spire 2/src/Core/Models/Cards/` 及相关命令源码。

## 范围

- 新增 `game/Models/Cards/Expertise.h`、`HiddenDaggers.h`、`LeadingStrike.h`，以及本计划和执行报告。
- 仅沿用现有 `CardPileCmd`、`CardSelectCmd`、`CardCmd`、`DamageCmd` 与 `CombatManager`；若发现基础命令缺口，只记录给主线程。
- 不改 CombatSolver、生成文件、CMake、其他共享文件；本批不运行生成器、构建或长测试，不提交。
- 只省略纯视觉的攻击特效与等待；规则命令、选择结果、生成事件、升级与执行顺序照源码保留。

## 原版方法对照

| 原版方法 | 本批移植行为 | 对应 API 与语义 |
|---|---|---|
| `Expertise.OnPlay` | 先按 `Cards.IntValue` 抽牌，等待 `Draw` 返回实际抽牌集合，再逐张施加单回合 Retain | `CardPileCmd::Draw(ctx, Dec(cardCount), Owner())`（`fromHandDraw` 默认 `false`），随后 `CardCmd::ApplySingleTurnRetain` |
| `Expertise.OnUpgrade` | Cards 增加 1；其数值变化已由内容等级表提供，无需重复覆写 | `CardData` 的等级差分 |
| `HiddenDaggers.OnPlay` | 先按 Cards 数量从手牌多选并弃牌，再按 Shivs 数量创建新 Shiv 并一次性加入手牌；升级版逐个升级这些新生成对象 | `FromHandForDiscard(ctx, Owner(), CardSelectorPrefs(count), nullptr, *this)` → `Discard` → `CreateCard` × count → `AddGeneratedCardsToCombat` → 条件 `Upgrade` |
| `LeadingStrike.OnPlay` | 先对指定敌人造成伤害，再按 Shivs 数量逐张生成并加入手牌 | `DamageCmd::Attack(...).FromCard(...).Targeting(...).Execute(ctx)`，之后循环调用单张创建及 `AddGeneratedCardToCombat` |
| `Shiv.CreateInHand` | 保留零数量和战斗结束/未进行时不生成的边界；批量路径先创建全部对象再加入，单张路径每次加入一个新对象 | 现有 `CombatManager::IsOverOrEnding` 与 `CardPileCmd` 组合；不复用牌组/其他牌的既有 CardModel，避免把同一对象重复塞入新生成命令 |

Expertise 和 Leading Strike 的升级数值由现有内容等级表处理：Expertise Cards 2→3，Leading Strike Damage 3→6。Hidden Daggers 升级不改变自身变量，而是在弃牌与生成后升级 `Shiv.CreateInHand` 返回的实际新卡对象。

## 定向 L3 验收建议

主线程统一生成及构建后，使用正版 oracle 对 Silent 的单卡自定义牌组做有限 L3：两个 seed、两个策略种子、A0/A10；三张目标牌各覆盖普通版和升级版，并包含若干普通攻防牌以触发抽牌与弃牌交互。保存 trace 以确认：

- Expertise 的抽牌结果在 `Draw` 完成后逐张获得 retain，包含抽牌被阻止或牌堆不足的路径。
- Hidden Daggers 先出现 FromHandForDiscard 选择与弃牌，随后生成 Shiv；升级版生成对象立即升级，普通版不升级；覆盖手牌不足指定数量时的选择边界。
- Leading Strike 先伤害指定目标，再分别生成每张 Shiv；增加能击杀最后敌人的样例，确认战斗结束后不生成 Shiv。
- 每张目标卡普通版及升级版均实际打出；以逐状态 oracle 差分为准，不从模拟器输出自举期望值。

## 涉及文件

- `game/Models/Cards/Expertise.h`
- `game/Models/Cards/HiddenDaggers.h`
- `game/Models/Cards/LeadingStrike.h`
- 本计划与 `docs/reports/2026-09-27-env-silent-retain-shivs.md`
