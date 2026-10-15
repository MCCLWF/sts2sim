# 无色牌批次 10：Bolas 与 Thrumming Hatchet

计划：[`../plans/2026-09-25-colorless-batch-10.md`](../plans/2026-09-25-colorless-batch-10.md)。

## 实现

- Bolas 与 Thrumming Hatchet 已加入 C++ 模型：分别造成 3/11 点 Move 伤害，升级后为 4/14 点。
- 为卡牌 POD 状态增加 `last_played_turn`。`CombatHistory::CardPlayFinished` 写入完成打出时玩家的回合号；两张牌在该玩家下一回合的 `BeforeHandDraw` 中检查该值，并在自身不在手牌时加入手牌。这复现了游戏对 `CardPlayFinishedEntry.HappenedLastPlayerTurn` 的查询，不添加动态历史集合。
- 此改动将无色牌模型头文件覆盖从 44/65 提升到 46/65。多人专用卡未纳入单人覆盖统计；`Splash` 跨职业获取路径仍显式 `Unported`。

## 验证

- `python3 tools/gen/gen_dispatch.py`：通过，672 个已移植模型。
- `cmake --build build -j2 --target test_colorless && ./build/test_colorless`：通过。
- 针对性测试覆盖两张牌的伤害与升级、上回合回手、同回合和超过一回合时不回手，以及牌已在手牌时的幂等处理。
- `./scripts/check.sh`：通过。C++ 构建成功，ctest 8/8；冒烟无崩溃/卡死；L2 200/200；L3 两组各 40/40（784/784、827/827 状态）；L4 两组各 40/40（1958/1958、2131/2131 决策），0 分歧、0 `Unported` 停止。
- Oracle 构建有 1 条 nullable 警告（`tools/Sts2Oracle/OracleRun.cs:234`），0 错误。

## 边界

本批只补两张无色攻击牌及它们需要的回合历史字段。余下无色牌、多人专用牌和跨职业获取仍按当前路线处理；M4 尚未完成。
