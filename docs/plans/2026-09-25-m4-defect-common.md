# M4 故障机器人常见牌扩充

分支：`feat/m4-defect-common`。前置：M2 已验收、M3 暂跳过；Defect 球和 Focus 两批已通过逐状态 L3 与自定义 L4。

## 目标

扩大 Defect 常见牌及其直接依赖的能力。使用更多种子的真实 L4 记录暴露首个可达 `Unported`，按 v0.111.0 源码移植，直到本批固定种子集完整对拍。遇到跨系统复杂缺口时记录真实范围，优先移植能闭环的牌及其依赖。

首批固定种子 `M4L4FOCUS009`–`020` 的停止点：`Refract`、`Ice Lance`、`Compile Driver`、`Sunder`、`Synthesis`、`Gunk Up`。其中 `Refract` 需要 Glass 球的可变被动值，`Synthesis` 需要 `FreePowerPower`。
第二轮回放继续抵达 `Null`，同批接入。

## 验收

1. 生成分发、构建、`ctest`、完整 `scripts/check.sh` 通过。
2. 新牌的自定义 L3 A0/A10 逐状态一致；同一内容的自定义 L4 A0/A10 逐决策一致。报告列出实际打出的牌、种子、状态数、决策数与仍未接入项。
3. 更新 README 和路线图，M4 未完成范围继续明确标记。
