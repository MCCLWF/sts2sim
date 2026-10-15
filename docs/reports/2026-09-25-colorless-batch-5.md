# 无色牌批次 5 报告

分支：`feat/m2-completion`。计划：[`../plans/2026-09-25-colorless-batch-5.md`](../plans/2026-09-25-colorless-batch-5.md)。

## 实现

- Entropy：施加 EntropyPower；在己方回合开始选择固定数量手牌并按战斗牌池随机变换；升级获得 Innate。
- Restlessness：手中没有其他牌时抽牌并获得能量；升级同时提高抽牌和能量。

无色牌池当前为 29/65。跨职业取牌与 `ColorlessPotion` 仍明确为 `Unported`。

## 验证

`tests/test_colorless.cpp` 覆盖 EntropyPower 的选择与变换路径，以及 Restlessness 的条件触发。

完整 `scripts/check.sh`：8/8 CTest 通过；L2 为 200/200；L3 为 40/40、784/784 状态和 40/40、827/827 状态；L4 为 40/40、1958/1958 决策和 40/40、2131/2131 决策，均 0 diverged、0 unported。
