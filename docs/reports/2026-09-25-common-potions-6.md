# 通用药水批次 6 报告

分支：`feat/m2-completion`。计划：[`../plans/2026-09-25-common-potions-6.md`](../plans/2026-09-25-common-potions-6.md)。

## 实现

- **Ashwater**：耗尽目标持有者的整手牌。
- **Bottled Potential**：把手牌移回抽牌堆，洗牌后抽 5 张。
- **Clarity**：抽 1 张牌并获得 3 层 Clarity；每个持有者回合额外抽 1 张并递减。
- **Distilled Chaos**：从抽牌堆顶部自动打出 3 张牌。
- **Gambler's Brew**：选择要弃掉的手牌并等量抽牌。
- **Radiant Tincture**：获得 1 点能量，并获得 3 层回合结束时转化为能量的 Radiance。

本批还加入 `ClarityPower` 和 `RadiancePower`，复用现有抽牌、自动出牌、弃牌和能量重置管线。当前精确统计为共享药水 38/45，铁甲战士专属药水 2 个可用；跨职业 `ColorlessPotion` 仍未接入。

## 验证与边界

`tests/test_potions.cpp` 覆盖六种药水及两个新 Power 的真实战斗状态。完整 `scripts/check.sh` 作为本批验收门禁。`OrobicAcid` 等依赖角色牌池/更多内容的药水继续保持未实现，不通过静默替换绕过边界。
