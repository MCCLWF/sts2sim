# 无色牌批次 15：Stratagem

## 目标

移植 Stratagem 与其洗牌后选牌能力。

## 范围

- Stratagem：1 费 Power，升级后费用降至 0，给予 1 层 `STRATAGEM_POWER`。
- 玩家抽牌堆洗牌后，从抽牌堆选择等同于能力层数的牌加入手牌。
- 复用 `CardSelectCmd::FromCombatPile` 与现有 `AfterShuffle` Hook；选择顺序由选择前缀决定。

## 验收

- 测试获得能力、洗牌触发、选牌进入手牌及升级费用。
- Oracle 自定义战斗牌组逐状态对拍，并运行完整 `scripts/check.sh`。
