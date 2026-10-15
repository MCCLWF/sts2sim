# 无色牌批次 12：Mayhem

计划：[`../plans/2026-09-26-colorless-batch-12-mayhem.md`](../plans/2026-09-26-colorless-batch-12-mayhem.md)。

## 实现

- Mayhem 已接入，打出时给予 1 层 `MAYHEM_POWER`；费用与升级减费由内容表应用。
- Mayhem Power 在所属玩家进入自动打出阶段时，调用现有 `CardPileCmd::AutoPlayFromDrawPile`，按层数从抽牌堆顶自动打出卡牌，不强制消耗。
- 单人无色牌模型头文件覆盖升至 48/65。

## 验证

- `python3 tools/gen/gen_dispatch.py`：通过，675 个已移植模型。
- `cmake --build build -j2 --target test_colorless && ./build/test_colorless`：通过；测试验证获得能力、顶牌被自动打出、结果进入弃牌堆且未被强制消耗。
- Mayhem 自定义牌组 Oracle L3 对拍（4 个游戏种子 × 2 个策略种子）：**8/8 场、183/183 状态**一致，0 `Unported`、0 分歧。
- `./scripts/check.sh`：通过。ctest 8/8；冒烟无崩溃/卡死；L2 200/200；L3 两组各 40/40（784/784、827/827 状态）；L4 两组各 40/40（1958/1958、2131/2131 决策），0 分歧、0 `Unported` 停止。
- Oracle 构建有 1 条 nullable 警告（`tools/Sts2Oracle/OracleRun.cs:234`），0 错误。

## 边界

本批只覆盖 Mayhem 及其自动打牌能力。无色池其余模型和 M4 其余内容仍待补全。
