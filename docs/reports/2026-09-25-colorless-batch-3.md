# 无色牌批次 3 报告

分支：`feat/m2-completion`。计划：[`../plans/2026-09-25-colorless-batch-3.md`](../plans/2026-09-25-colorless-batch-3.md)。

## 实现

- **Hand of Greed**：攻击并在击杀目标时获得 20 金币；不可在战斗中生成。
- **Panic Button**：获得 30 点格挡，并在接下来 2 个敌方回合禁止牌源格挡。
- **Fasten**：获得 Fasten Power，使带有 Defend 标签的牌额外获得 4 点格挡。

当前无色牌池为 23/65；这些效果只依赖战斗伤害、金币、格挡和现有 Power 管线，没有跨职业牌池行为。

## 验证与边界

`tests/test_colorless.cpp` 覆盖击杀金币、No Block 层数和 Fasten 对防御牌的格挡修饰。跨职业取牌和 `ColorlessPotion` 仍明确保持 `Unported`。
