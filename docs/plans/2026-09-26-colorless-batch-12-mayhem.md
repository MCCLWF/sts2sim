# 无色牌批次 12：Mayhem

## 目标

移植 Mayhem 与其能力：玩家回合自动打出阶段，根据层数从抽牌堆顶自动打出牌。

## 范围

- Mayhem：2 费 Power，升级后费用减至 1，给予 1 层 Mayhem Power。
- Mayhem Power：仅在所属玩家进入自动打出阶段时触发；从抽牌堆顶打出层数对应数量的牌，不强制消耗。
- 复用现有 `CardPileCmd::AutoPlayFromDrawPile`，沿用自动目标选择与牌本身的结果牌堆逻辑。

## 验收

- 测试获得能力、抽牌堆顶牌确实被打出且未被强制消耗。
- Oracle 自定义战斗牌组逐状态对拍，并运行完整 `scripts/check.sh`。
