# 通用药水批次 7 报告

分支：`feat/m2-completion`。计划：[`../plans/2026-09-25-common-potions-7.md`](../plans/2026-09-25-common-potions-7.md)。

## 实现

- **Droplet of Precognition**：从抽牌堆选择一张牌加入手牌。
- **Entropic Brew**：持续生成随机药水直到填满药水槽。
- **Liquid Memories**：从弃牌堆选择一张牌，本回合免费并加入手牌。
- **Snecko Oil**：抽 7 张牌并为本回合设置随机能量费用。
- **Touch of Insanity**：选择一张手牌，使其本场战斗免费。

这些效果复用 `CardSelectCmd`、`CardPileCmd`、药水工厂和现有能量费用状态。当前共享药水为 43/45 可用；剩余共享缺口为 `ColorlessPotion`（跨职业无色牌边界）和 `OrobicAcid`。

## 验证与边界

`tests/test_potions.cpp` 覆盖五种药水的真实战斗状态。完整 `scripts/check.sh` 作为本批验收门禁。跨职业牌池路径仍明确保持 `Unported`，没有用战士牌池替代。
