# 无色牌批次 8 报告

分支：`feat/m2-completion`。计划：[`../plans/2026-09-25-colorless-batch-8.md`](../plans/2026-09-25-colorless-batch-8.md)。前置条件：M2 与 M3 接口已验收。

## 实现

- Alchemize：使用战斗药水生成 RNG 获取随机药水；升级降低费用。
- Automation：施加 AutomationPower，每抽 10 张牌获得能量；计数初始化保存在可复制的 PowerState 中。
- Hidden Gem：从抽牌堆选择可 Replay 的牌，优先攻击、技能和能力牌，增加其 Replay 次数；不可在战斗中生成。
- Jack of All Trades：从单人无色池中过滤可战斗生成的候选牌，洗牌后取不同卡牌加入手牌。

无色牌池已有 40/65 张对应模型。生成效果仍可产生剩余未移植的无色牌或 `ColorlessPotion`；这些效果的后续使用仍会受控停在 `Unported`。战士获取其他角色牌仍为 `Unported`。

## 验证

`tests/test_colorless.cpp` 增加对应真实战斗状态测试。完整 `scripts/check.sh` 通过：8/8 CTest；L2 200/200；L3 两组均 40/40、状态分别 784/784 和 827/827；L4 两组均 40/40、决策分别 1958/1958 和 2131/2131，0 diverged、0 unported。L3/L4 现有路线未专门强制抽到本批新牌，本批效果的直接证据来自专项测试与源码核对。
