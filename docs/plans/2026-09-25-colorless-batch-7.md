# 无色牌批次 7

- 分支：`feat/m2-completion`。
- 范围：Prep Time、Rolling Boulder、Panache、The Gambit 及对应 Power，继续补齐战士可用的单人无色牌。
- 真值：游戏 v0.111.0 反编译源码 `Models/Cards` 和 `Models/Powers` 中同名模型。
- 验收：专项战斗测试覆盖回合开始的 Vigor、持续递增的全体伤害、Panache 打牌计数、The Gambit 受击致死；完整 `scripts/check.sh` 通过。
- 边界：跨职业取牌和 `ColorlessPotion` 保持 `Unported`。
