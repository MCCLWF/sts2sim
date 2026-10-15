# 无色牌批次 7 报告

分支：`feat/m2-completion`。计划：[`../plans/2026-09-25-colorless-batch-7.md`](../plans/2026-09-25-colorless-batch-7.md)。前置条件：M2 和 M3 接口已验收。

## 实现

- Prep Time：每次己方回合开始施加 Vigor；升级提高施加量。
- Rolling Boulder：每次己方回合开始对所有可命中敌人造成无属性伤害，随后提高下一次伤害；升级提高初始伤害。
- Panache：累计打牌次数并在达到阈值时对所有可命中敌人造成伤害，每回合结束重置计数。
- The Gambit：获得高额格挡，若随后受到未被格挡的有属性攻击伤害则死亡。

无色牌池当前为 36/65。跨职业取牌和 `ColorlessPotion` 仍为 `Unported`。

## 验证

`tests/test_colorless.cpp` 增加四张牌及 Power 的真实战斗状态检查。

完整 `scripts/check.sh`：8/8 CTest 通过；L2 为 200/200；L3 为 40/40、784/784 状态和 40/40、827/827 状态；L4 为 40/40、1958/1958 决策和 40/40、2131/2131 决策，均 0 diverged、0 unported。
