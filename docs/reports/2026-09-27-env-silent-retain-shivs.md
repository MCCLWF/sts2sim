# 静默猎手留牌与 Shiv 生成牌移植报告

日期：2026-09-27。状态：已接入生成和构建，有限定向 L3 通过；完整条件路径尚未验收。

## 前置条件

- 已完整阅读 `AGENTS.md` 与 `docs/audits/pitfalls.md`。
- 当前分支：`feat/generic-env-rf-api`。
- 原版依据：`../Slay the Spire 2/src/Core/Models/Cards/Expertise.cs`、`HiddenDaggers.cs`、`LeadingStrike.cs`、`Shiv.cs` 与 `../Slay the Spire 2/src/Core/Commands/CardPileCmd.cs`、`CardSelectCmd.cs`、`CardCmd.cs`，版本 v0.111.0。
- 子任务只编辑三个目标卡头文件、本计划和本报告；主线程随后修正 CombatState 类型限定、生成分发、构建并运行下述定向矩阵。尚未提交，CombatSolver 保持冻结。

## 实施

- `Expertise` 先调用 `CardPileCmd::Draw` 并保存其实际返回的 `CardList`，等抽牌及相关命令返回后再逐张调用 `ApplySingleTurnRetain`。按源码读取 `Cards.IntValue()`，转换为现有命令需要的 `Dec`；`fromHandDraw` 保留默认 `false`。
- `HiddenDaggers` 先用 `FromHandForDiscard` 按 Cards 变量选择并调用 `Discard`。之后参照 `Shiv.CreateInHand(owner, count, combatState)` 创建全新的 Shiv 模型，全部创建后一次调用 `AddGeneratedCardsToCombat(..., Hand, Owner())`。升级版对这些生成对象逐个调用 `CardCmd::Upgrade`。
- `LeadingStrike` 先执行目标伤害，再按源码的循环对每一张 Shiv 单独创建、单独调用 `AddGeneratedCardToCombat`，使每张卡的生成 Hook 在下一张生成前完成。纯视觉的攻击特效和等待已省略。
- `Shiv.CreateInHand` 在 `count == 0` 或 `CombatManager.IsOverOrEnding` 时不创建对象。批量与逐张生成路径都在创建 Shiv 前检查结束状态；这也避免在战斗已结束时进入现有 `AddGeneratedCardToCombat` 的非进行中分支。
- Expertise Cards 2→3 和 Leading Strike Damage 3→6 来自现有内容等级表，未在卡牌行为中重复实现升级差值。Hidden Daggers 自身升级通过升级实际生成的新 Shiv 来表达。

## 原版方法核对

| 源码方法 | 核对结论 |
|---|---|
| `Expertise.OnPlay` / `OnUpgrade` | `Draw(ctx, Cards.IntValue(), Owner())` 返回的集合完成后逐张 Retain；升级仅增加 Cards 1，等级表已表达 |
| `HiddenDaggers.OnPlay` | `FromHandForDiscard` → `CardCmd.Discard` → `Shiv.CreateInHand`；升级版仅升级 helper 返回的生成对象 |
| `LeadingStrike.OnPlay` / `OnUpgrade` | 目标攻击先执行；随后每轮单独调用单张 `Shiv.CreateInHand`；升级仅增加 Damage 3，等级表已表达 |
| `Shiv.CreateInHand` | `count == 0`、战斗结束时返回空；批量路径先创建所有新对象再批量加入 Hand，creator 默认 owner；Leading Strike 则每次创建并加入一张 |

## 验收结果

主线程生成 Dispatch/AllModels 后构建通过；定向工具 `python3 tools/check_silent_retain_shivs.py --trace-dir /tmp/mcc-env-silent-shivs-final` 退出0，日志 `/tmp/mcc-env-silent-shivs-final.log`。

矩阵为3卡 × 原版/升级 × A0/A10 × SHIVCHECK01/02 × policy1/2，Cultists遭遇，牌组含2张目标卡、4StrikeSilent、4DefendSilent、2Neutralize。48场/990状态全部与正版一致，零Unported，记录101次原版实际目标卡出牌。覆盖统计按state/action事件定位，不将嵌套选牌动作错误地当成下一战斗状态。

此为有限配置验收。dump未直接包含全部SingleTurnRetain内部标志；满手、NoDraw、所有生成Hook、致死与真实捕获尚未完整覆盖。计划的完整条件路径标准仍未完成。2026-09-27勘察核对了上述记录，重建当前源并通过12/12 CTest，没有再次采集/重跑这48场。
