# 通用药水第二批

分支：`feat/m2-completion`。第一批直接效果药水已通过真实战斗测试，本批继续补不需要牌池选择或未移植 Power 的直接效果。

## 范围

`BeetleJuice`、`Fortifier`、`FyshOil`、`MazalethsGift`、`PotionOfBinding`、`PotionShapedRock`、`ShipInABottle`、`StarPotion`。

## 验收

每个模型都通过生成分发接入 `PotionModel::OnUse`，并由 `tests/test_potions.cpp` 在真实战斗夹具中验证药水消耗及 Power、格挡、伤害或星星变化。随后运行完整构建、单测、烟测和 L2/L3/L4 门禁。
