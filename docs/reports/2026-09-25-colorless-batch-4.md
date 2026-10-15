# 无色牌批次 4 报告

分支：`feat/m2-completion`。计划：[`../plans/2026-09-25-colorless-batch-4.md`](../plans/2026-09-25-colorless-batch-4.md)。前置条件：M2 和 M3 接口验收已完成。

## 实现

- Equilibrium：获得格挡并让手牌保留一回合；升级增加 3 点格挡。
- Purity：选择最多 3 张手牌消耗；升级把上限提升到 5。
- Salvo：单体攻击并让手牌保留一回合；升级增加 4 点伤害。
- Scrawl：抽至 10 张手牌；升级获得 Retain。

无色牌池当前 27/65 有对应可用模型。跨职业取牌和 `ColorlessPotion` 仍为 `Unported`。

## 验证

`tests/test_colorless.cpp` 增加上述战斗状态及 Scrawl 升级测试。完整门禁 `scripts/check.sh`：待记录。
