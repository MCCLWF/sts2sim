# 无色牌直接效果批次执行报告

分支：`feat/m2-completion`  
日期：2026-09-25

## 已接通

九张单人可用无色牌已接入：Dramatic Entrance、Eternal Armor、Impatience、Production、Prolong、Prowess、Shockwave、Ultimate Defend、Ultimate Strike。无色池生成表共 65 张，模型可用性表由原来的 5 张提升到 14 张；这只是该池的当前覆盖率，不表示无色牌整体完成。

`Impatience` 按源码只在手牌没有攻击牌时抽牌。其它效果复用 DamageCmd、CreatureCmd、PowerCmd、PlayerCmd 和 CardPileCmd，牌的费用、关键词、升级值来自生成内容表。

## 验证

第一幕 `NIBBITS_WEAK` 遭遇中，每张牌在 A0/A10、两种子、两策略、基础/升级两级共 16 场 Oracle L3 对拍全部零 `Unported`、零分歧；九张共 144 场。完整 `scripts/check.sh` 随后通过。

## 边界

其余 51 张无色牌仍缺实现，其中 `Splash` 涉及战士战斗中获取其他角色卡牌，继续明确保持 `Unported`；没有用战士卡池替代。多人专用无色牌也未纳入本批单人目标。
