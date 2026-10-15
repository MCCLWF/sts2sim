# 共享药水批次 9 报告

分支：`feat/m2-completion`。计划：[`../plans/2026-09-25-common-potions-9.md`](../plans/2026-09-25-common-potions-9.md)。前置条件：M2 与 M3 接口已验收；共享药水已有 44/45 个模型。

## 实现

- ColorlessPotion 从无色池的单人可战斗生成候选牌里随机展示 3 张，允许跳过；选中牌本回合免费并进入手牌。
- `CardFactory::GetForCombat` 增加可选的卡牌排除参数，供 Jack of All Trades 使用，默认行为与既有调用保持一致。

共享药水模型覆盖达到 45/45。无色牌仍有 25 张未移植；药水随机提供它们时，后续打出仍会受控停在 `Unported`。战士获取其他角色牌仍未实现。

## 验证

`tests/test_potions.cpp` 覆盖 ColorlessPotion 的选择、牌池和费用；`tests/test_colorless.cpp` 继续覆盖 Jack of All Trades。

完整 `scripts/check.sh` 通过：8/8 CTest；L2 200/200；L3 两组均 40/40、状态分别 784/784 和 827/827；L4 两组均 40/40、决策分别 1958/1958 和 2131/2131，0 diverged、0 unported。L3/L4 现有路线未强制用到 ColorlessPotion，药水行为的直接证据来自专项测试与源码核对。
