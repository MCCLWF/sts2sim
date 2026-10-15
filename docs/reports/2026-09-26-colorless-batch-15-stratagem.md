# 无色牌批次 15：Stratagem

计划：[`../plans/2026-09-26-colorless-batch-15-stratagem.md`](../plans/2026-09-26-colorless-batch-15-stratagem.md)。

## 实现

- Stratagem 打出后获得 1 层 `STRATAGEM_POWER`，升级减费到 0 由内容表处理。
- 每次玩家抽牌堆完成洗牌后，能力闪烁并从当前抽牌堆按能力层数执行固定数量的选择；选中的牌从抽牌堆进入手牌。
- 复用现有 `AfterShuffle` 分发、`CardSelectCmd::FromCombatPile` 和选择前缀机制。
- 无色牌模型头文件覆盖升至 51/65。

## 验证

- `python3 tools/gen/gen_dispatch.py`：通过，680 个已移植模型。
- `cmake --build build -j2 --target test_colorless && ./build/test_colorless`：通过；测试覆盖升级费用、洗牌触发、选择前缀消费、牌从抽牌堆进入手牌。
- Stratagem 自定义牌组 Oracle L3 对拍（4 个游戏种子 × 2 个策略种子）：**8/8 场、197/197 状态**一致，0 `Unported`、0 分歧。
- `./scripts/check.sh`：通过。ctest 8/8；冒烟无崩溃/卡死；L2 200/200；L3 两组各 40/40（784/784、827/827 状态）；L4 两组各 40/40（1958/1958、2131/2131 决策），0 分歧、0 `Unported` 停止。
- Oracle 构建有 1 条 nullable 警告（`tools/Sts2Oracle/OracleRun.cs:234`），0 错误。

## 边界

本批只覆盖 Stratagem 及其洗牌后选牌能力。其余无色牌、跨职业获取和 M4 其余内容仍待补全。
