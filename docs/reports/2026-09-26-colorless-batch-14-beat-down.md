# 无色牌批次 14：Beat Down

计划：[`../plans/2026-09-26-colorless-batch-14-beat-down.md`](../plans/2026-09-26-colorless-batch-14-beat-down.md)。

## 实现

- Beat Down 从弃牌堆筛选攻击牌，排除 Unplayable，按 `CardModel.CompareTo` 排序后使用 RunState Shuffle RNG 稳定洗牌，再取最多 3 张自动打出；升级数量为 4。
- AnyEnemy 攻击牌逐张使用 CombatTargets RNG 选择目标，其他牌按源码传空目标。
- 将 `CardPileCmd::StableShuffle` 开放为共享辅助函数，原洗牌入口仍复用同一实现。
- 无色牌模型头文件覆盖升至 50/65。

## 验证

- `python3 tools/gen/gen_dispatch.py`：通过，678 个已移植模型。
- `cmake --build build -j2 --target test_colorless && ./build/test_colorless`：通过；测试覆盖升级后打出 4 张攻击牌、攻击造成伤害和 Unplayable 卡留在弃牌堆。
- Beat Down 自定义牌组 Oracle L3 对拍（4 个游戏种子 × 2 个策略种子）：**8/8 场、156/156 状态**一致，0 `Unported`、0 分歧。
- `./scripts/check.sh`：通过。ctest 8/8；冒烟无崩溃/卡死；L2 200/200；L3 两组各 40/40（784/784、827/827 状态）；L4 两组各 40/40（1958/1958、2131/2131 决策），0 分歧、0 `Unported` 停止。
- Oracle 构建有 1 条 nullable 警告（`tools/Sts2Oracle/OracleRun.cs:234`），0 错误。

## 边界

本批只覆盖 Beat Down 与复用稳定洗牌。其他无色牌、跨职业获取和 M4 其余内容仍待继续补全。
