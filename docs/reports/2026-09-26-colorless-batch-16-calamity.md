# 无色牌批次 16：Calamity

计划：[`../plans/2026-09-26-colorless-batch-16-calamity.md`](../plans/2026-09-26-colorless-batch-16-calamity.md)。

## 实现

- Calamity 打出后给予 1 层 `CALAMITY_POWER`；升级降费至 2 由内容表处理。
- `BeforeCardPlayed` 为本玩家攻击牌在牌实例 POD 状态中记录当前 Calamity 层数；`AfterCardPlayed` 取出并清空快照，按角色牌池用 `CombatCardGeneration` RNG 有放回生成等量攻击牌到手牌。
- 无色牌模型头文件覆盖升到 52/65；单人可用牌为 52/53。唯一单人牌池例外是 `Splash`，其跨职业获取与选择仍为 `Unported`；12 张多人专用牌不属于单人范围。

## 验证

- `python3 tools/gen/gen_dispatch.py`：通过，682 个已移植模型。
- `cmake --build build -j2 --target test_colorless && ./build/test_colorless`：通过；测试覆盖升级费用、能力层数、攻击牌触发生成、技能牌不触发以及快照被消费。
- Calamity 自定义牌组 Oracle L3 对拍（4 个游戏种子 × 2 个策略种子）：**8/8 场、125/125 状态**一致，0 `Unported`、0 分歧。
- `./scripts/check.sh`：通过。ctest 8/8；冒烟无崩溃/卡死；L2 200/200；L3 两组各 40/40（784/784、827/827 状态）；L4 两组各 40/40（1958/1958、2131/2131 决策），0 分歧、0 `Unported` 停止。
- Oracle 构建有 1 条 nullable 警告（`tools/Sts2Oracle/OracleRun.cs:234`），0 错误。

## 边界

本批只覆盖 Calamity 及其攻击牌触发历史。`Splash` 跨职业获取继续显式未移植；多人专用牌、其他 M4 内容仍待按范围处理。
