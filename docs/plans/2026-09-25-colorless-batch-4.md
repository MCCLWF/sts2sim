# 无色牌批次 4

- 分支：`feat/m2-completion`。
- 范围：接入 Equilibrium、Purity、Salvo、Scrawl 的战斗效果和升级行为；继续专注战士可获得的通用内容。
- 真值：`../Slay the Spire 2/src/Core/Models/Cards/` 中对应 v0.111.0 类。
- 验收：专门战斗测试覆盖留牌、选牌消耗、抽至满手、伤害与升级；`scripts/check.sh` 全部通过。
- 涉及：`game/Models/Cards/`、生成派发、`tests/test_colorless.cpp`、README 和路线图。
