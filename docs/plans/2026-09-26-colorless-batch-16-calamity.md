# 无色牌批次 16：Calamity

## 目标

移植 Calamity：玩家打出攻击牌后，从本角色攻击牌池生成同数量攻击牌。

## 范围

- Calamity：3 费 Power，升级后费用降到 2，给予 1 层 `CALAMITY_POWER`。
- `BeforeCardPlayed` 为本玩家攻击牌记录触发时的能力层数；`AfterCardPlayed` 消耗该牌实例的记录，按角色攻击牌池和战斗卡牌生成 RNG 有放回生成攻击牌到手牌。
- 为每张牌实例增加 POD 待处理层数槽，支持播放结束后按原始层数结算。
- Splash 的跨职业卡池和选择仍保持 `Unported`。

## 验收

- 测试打出 Calamity 后，攻击牌触发生成攻击牌，技能牌不触发，升级费用正确。
- Oracle 自定义战斗牌组逐状态对拍，并运行完整 `scripts/check.sh`。
