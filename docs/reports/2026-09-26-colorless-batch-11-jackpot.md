# 无色牌批次 11：Jackpot

计划：[`../plans/2026-09-26-colorless-batch-11-jackpot.md`](../plans/2026-09-26-colorless-batch-11-jackpot.md)。

## 实现

- Jackpot 造成 25 点 Move 伤害，升级后 30 点。
- 新增单独的零费战斗牌生成入口，严格按源码顺序筛选玩家角色已解锁牌池：基础费用为 0 且非 X 费，再过滤不能在战斗生成的牌、基础/先古/事件稀有度和重复模型。
- 使用 `CombatCardGeneration` RNG 有放回抽 3 张；升级的 Jackpot 会升级可升级生成牌，然后逐张加入手牌。
- 无色牌模型头文件覆盖从 46/65 升到 47/65。跨职业牌池与 `Splash` 路径仍未接入。

## 验证

- `python3 tools/gen/gen_dispatch.py`：通过，673 个已移植模型。
- `cmake --build build -j2 --target test_colorless && ./build/test_colorless`：通过。
- Jackpot 自定义牌组 Oracle L3 对拍（5 张 Jackpot、`MAWLER_NORMAL`、4 个游戏种子 × 2 个策略种子）：**8/8 场、71/71 状态**一致，0 `Unported`、0 分歧。
- `./scripts/check.sh`：通过。ctest 8/8；冒烟无崩溃/卡死；L2 200/200；L3 两组各 40/40（784/784、827/827 状态）；L4 两组各 40/40（1958/1958、2131/2131 决策），0 分歧、0 `Unported` 停止。
- Oracle 构建有 1 条 nullable 警告（`tools/Sts2Oracle/OracleRun.cs:234`），0 错误。

## 边界

本批只增加 Jackpot 及其专用的候选筛选。其他角色卡池、其余无色牌与 M4 其余内容仍待继续补全。
