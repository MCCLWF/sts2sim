# 通用药水第一批

分支：`feat/m2-completion`。M3 环境层已合入；本批只处理通用 run 内容中的药水，不扩展其他角色。

## 目标

1. 接通最常见的直接效果药水，使用时不再落到 `PotionModel.OnUse` 的 `Unported`。
2. 保留源码中的目标类型、动态变量和战斗命令管线，使用生成分发接入具体模型。
3. 为每个已接通效果建立独立战斗夹具，检查药水消耗和实际状态变化。

## 范围

`BlockPotion`、`EnergyPotion`、`FirePotion`、`StrengthPotion`、`DexterityPotion`、`VulnerablePotion`、`WeakPotion`、`PoisonPotion`、`SwiftPotion`、`FocusPotion`、`LiquidBronze`、`HeartOfIron`、`BloodPotion`、`FruitJuice`。

攻击/技能/能力药水的三选一生成，以及依赖尚未移植 Power 的药水，留在后续批次；不在本批静默替换。

## 验收

- `test_potions` 在真实战斗状态中逐个使用药水，验证目标生命、格挡、能量、抽牌、Power、最大生命和药水槽位。
- `ctest`、`scripts/check.sh` 通过。
