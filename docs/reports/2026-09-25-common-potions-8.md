# 通用药水批次 8 报告

分支：`feat/m2-completion`。计划：[`../plans/2026-09-25-common-potions-8.md`](../plans/2026-09-25-common-potions-8.md)。

## 实现

**Orobic Acid** 现在按持有者角色牌池分别生成一张攻击牌、技能牌和能力牌，三张牌本回合免费并加入战斗手牌。战士路径使用其自身牌池，不跨职业取牌。

当前共享药水为 44/45；唯一剩余共享药水是 `ColorlessPotion`，它涉及跨职业无色牌池，继续保持显式 `Unported`。

## 验证

`tests/test_potions.cpp` 检查使用后手牌增加三张；完整 `scripts/check.sh` 作为本批门禁。
