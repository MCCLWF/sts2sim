# 无色牌批次 6 报告

分支：`feat/m2-completion`。计划：[`../plans/2026-09-25-colorless-batch-6.md`](../plans/2026-09-25-colorless-batch-6.md)。

## 实现

- **Anointed**：从抽牌堆随机取稀有牌直到填满手牌；升级获得 Retain。
- **Catastrophe**：从抽牌堆稳定随机选择并自动打出两张牌；升级增加一张。
- **The Bomb / TheBombPower**：施加 3 回合延迟爆炸，在计时结束时对所有可命中敌人造成 40 点无属性伤害；升级伤害 +10。

无色牌池当前为 32/65。跨职业取牌和 `ColorlessPotion` 仍为 `Unported`。

## 验证

`tests/test_colorless.cpp` 覆盖三张牌的战斗执行和 The Bomb 的计时状态。

完整 `scripts/check.sh`：8/8 CTest 通过；L2 为 200/200；L3 为 40/40、784/784 状态和 40/40、827/827 状态；L4 为 40/40、1958/1958 决策和 40/40、2131/2131 决策，均 0 diverged、0 unported。
