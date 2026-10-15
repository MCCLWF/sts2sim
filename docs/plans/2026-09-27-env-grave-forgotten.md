# 通用 Env：Grave of the Forgotten

日期：2026-09-27。分支：`feat/generic-env-rf-api`。阶段 E1/E3，继续扩展铁甲战士真实三幕路线。

## 目标

按 v0.111.0 原版接入 Grave of the Forgotten 的选项、诅咒、SoulsPower 附魔和 ForgottenSoul 遗物。原路线在 `GenerateInitialOptions` 受控停止；本批需越过它并保持正版逐决策差分为零。

## 工作

- 让 SoulsPower 的资格检查按基础附魔限制和卡牌本地 Exhaust 关键字判定；附魔首次施加时移除 Exhaust，使用通用 `CardCmd::Enchant` 路径。
- 事件提供 Confront/Accept 两项；无可附魔牌时锁定 Confront。Confront 先加 Decay，再从有效牌中选一张附魔，随后结束事件；Accept 获得 ForgottenSoul 后结束事件。
- ForgottenSoul 按原版 `AfterCardExhausted` 检查卡牌主人，使用 `CombatTargets` 在可命中敌人中随机选目标并造成遗物 `DamageVar` 的未强化伤害。
- 通过生成 dispatch、聚焦测试、原版四条三幕路线和完整 `scripts/check.sh` 验证；后继若出现其它未移植内容，以新的首停点记录。

## 边界

只模拟影响游戏状态的事件、附魔和遗物规则；悬停提示、演出和视觉预览不进入 Env。现有其它附魔仍按各自移植状态处理，本批不声称已完成通用附魔体系。
