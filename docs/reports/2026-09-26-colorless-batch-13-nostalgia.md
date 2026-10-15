# 无色牌批次 13：Nostalgia

计划：[`../plans/2026-09-26-colorless-batch-13-nostalgia.md`](../plans/2026-09-26-colorless-batch-13-nostalgia.md)。

## 实现

- Nostalgia 打出后获得 1 层 `NOSTALGIA_POWER`；费用降至 0 的升级由内容表处理。
- 结果牌堆 Hook 会在 `CardPlayStarted` 前读取当前计数：前 `Amount` 张攻击/技能牌若默认进入弃牌堆，就放到抽牌堆顶。
- `CombatHistory::CardPlayStarted` 将攻击/技能牌启动次数记录在 `CombatData` POD 字段中。普通玩家回合开始时清零；额外回合保留同一回合记录。
- 无色牌模型头文件覆盖升至 49/65。

## 验证

- `python3 tools/gen/gen_dispatch.py`：通过，677 个已移植模型。
- `cmake --build build -j2 --target test_colorless && ./build/test_colorless`：通过；测试覆盖升级费用、第一张攻击牌回到抽牌堆顶、第二张技能牌进入弃牌堆及启动计数。
- Nostalgia 自定义牌组 Oracle L3 对拍（4 个游戏种子 × 2 个策略种子）：**8/8 场、186/186 状态**一致，0 `Unported`、0 分歧。
- `./scripts/check.sh`：通过。ctest 8/8；冒烟无崩溃/卡死；L2 200/200；L3 两组各 40/40（784/784、827/827 状态）；L4 两组各 40/40（1958/1958、2131/2131 决策），0 分歧、0 `Unported` 停止。
- Oracle 构建有 1 条 nullable 警告（`tools/Sts2Oracle/OracleRun.cs:234`），0 错误。

## 边界

本批只覆盖 Nostalgia 和它依赖的攻击/技能牌历史计数。其余无色牌、跨职业获取与 M4 其余内容仍待继续补全。
