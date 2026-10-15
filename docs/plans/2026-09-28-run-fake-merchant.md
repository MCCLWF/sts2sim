# Fake Merchant 完整 run 路径

## 目标

依据 v0.111.0 源码将 Fake Merchant 的定价、采购、Foul Potion 开战、库存奖励以及十件关联遗物的逻辑纳入单人通用 Env。避免为追平某一条种子而作状态/RNG 特判。

## 工作项

1. 对照 `FakeMerchant.cs`、`NFakeMerchant.cs`、`MerchantRelicEntry.cs`、`FoulPotion.cs` 与十个 Fake 遗物源码，列出纯 UI 和游戏状态差异。
2. 使用已有 POD MerchantState 承载六件库存；按事件 RNG 洗牌，再按玩家 Shops RNG 逐件定价。事件房间内开放采购与离开，Foul Potion 触发事件战斗并把未售遗物连同地毯加入战斗奖励。
3. 逐件实现 Fake 遗物和其依赖的 ConfusedPower，生成派发。保留源代码中同名真遗物的独立数值与时序。
4. 增加定向测试和能够触达该路径的正版 oracle 差分；更新内容审计、路线图、README 与执行报告。

## 验收

- 商店库存、定价、采购、Foul Potion 触发战斗与奖励清单有源码对应的定向证据；假遗物游戏效果有直接 oracle 或明确未验收记录。
- `scripts/check.sh`、生成清单和 `git diff --check` 通过；未得到的原版 UI/整局证据不冒充为已通过。
