# 无色牌批次 8

- 分支：`feat/m2-completion`。
- 范围：Alchemize、Automation、Hidden Gem、Jack of All Trades 及 AutomationPower；继续补足战士可接触的单人无色牌。
- 真值：游戏 v0.111.0 反编译源码的同名卡牌、Power、PotionFactory 和 CardFactory。
- 验收：专项战斗测试覆盖药水获取、每抽 10 张牌得能量、抽牌堆卡牌 Replay 增加，以及无色牌生成过滤；完整 `scripts/check.sh` 通过。
- 边界：生成效果仍可产生尚未移植的无色牌或 ColorlessPotion，其后续使用维持明确 `Unported`；战士获取其他角色牌同样未实现。
