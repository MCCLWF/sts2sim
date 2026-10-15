# 通用药水第一批执行报告

分支：`feat/m2-completion`  
日期：2026-09-25

## 已接通

14 个药水模型已通过生成分发接入 `PotionModel::OnUse`：

`BlockPotion`、`EnergyPotion`、`FirePotion`、`StrengthPotion`、`DexterityPotion`、`VulnerablePotion`、`WeakPotion`、`PoisonPotion`、`SwiftPotion`、`FocusPotion`、`LiquidBronze`、`HeartOfIron`、`BloodPotion`、`FruitJuice`。

它们复用现有 `CreatureCmd`、`PlayerCmd`、`PowerCmd` 和 `CardPileCmd`，药水在 wrapper 中按游戏顺序移出槽位、执行 Hook、应用效果并完成清理。`FruitJuice` 同步保留不可在战斗中生成的规则。

## 验证

`tests/test_potions.cpp` 为每个药水建立新的真实战斗，验证药水被消耗以及对应的生命、格挡、能量、抽牌、Power 或最大生命变化。专用测试通过；随后完整 `scripts/check.sh` 通过，既有 L2/L3/L4 路线无分歧。

## 尚未覆盖

攻击/技能/能力药水的牌池生成和三选一选择、依赖未移植 Power 的药水、事件专属药水仍保持 `Unported`。跨职业牌池边界（例如无色 `Splash`）不因本批药水而改变。
