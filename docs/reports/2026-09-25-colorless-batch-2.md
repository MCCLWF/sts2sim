# 无色牌批次 2 报告

分支：`feat/m2-completion`。计划：[`../plans/2026-09-25-colorless-batch-2.md`](../plans/2026-09-25-colorless-batch-2.md)。

## 实现

- **Dark Shackles** 与 **Dark Shackles Power**：对敌人施加临时力量降低，敌方回合结束恢复。
- **Discovery**：从持有者本职牌池生成三选一牌，本回合免费。
- **Enlightenment**：将手牌费用临时降至 1；升级后持续至本场战斗结束。
- **Master of Strategy**：抽 3 张牌并耗尽。
- **Secret Technique**：从抽牌堆取一张技能牌到手牌。
- **Secret Weapon**：从抽牌堆取一张攻击牌到手牌。
- **Thinking Ahead**：抽 2 张牌，再选择一张放回抽牌堆顶部。

无色牌池当前为 20/65 可用。`Discovery` 只查询战士自身牌池；战士战斗中取得其他角色牌的路径仍保持 `Unported`。

## 验证

新增 `tests/test_colorless.cpp` 并注册为 `colorless_m4`，覆盖 Dark Shackles、Master of Strategy、Enlightenment 和 Discovery 的真实战斗状态。完整 `scripts/check.sh` 作为本批门禁。
