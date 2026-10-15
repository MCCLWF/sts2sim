# M4 Silent 攻防与次回合抽牌批次报告

计划：[`../plans/2026-09-25-m4-silent-attack-block.md`](../plans/2026-09-25-m4-silent-attack-block.md)。分支：`feat/m2-completion`。M2 已完成严格 L4 与卡牌矩阵验收，M3 暂跳过。

## 已接入

- 新增 `Backstab`、`Dash`、`Predator`、`Footwork` 四张 Silent 卡；已有的 `Slice` 一并对拍。
- `DrawCardsNextTurnPower`：回合开始根据当时的能力数量增加抽牌，然后移除能力。

## 验证

- `tools/gen/gen_dispatch.py`：分发生成通过；`cmake --build build -j2`：通过。
- Silent 自定义牌组 `SLICE,BACKSTAB,DASH,PREDATOR,FOOTWORK,STRIKE_SILENT,DEFEND_SILENT,NEUTRALIZE,SURVIVOR`，`TOADPOLES_WEAK`，种子 `M4SILB001`—`M4SILB008` × 策略 `1,2`：A0 **16/16 场、197/197 状态**，A10 **16/16 场、199/199 状态**；均 0 `Unported`、0 分歧。
- A0 Oracle 轨迹：`Backstab` 出牌 15 次、`Slice` 25 次、`Dash` 14 次、`Predator` 16 次、`Footwork` 11 次；`DrawCardsNextTurnPower` 出现在 21 个决策点状态中。本机轨迹 `/tmp/m4_silent_b_trace.jsonl`。
- `scripts/check.sh`：构建、2/2 单元测试、冒烟、L2 200/200、L3 A0/A10 各 40/40、L4 带/不带 Neow 各 40/40 全部通过；本机日志 `/tmp/m4_silent_b_check.log`。

## 限制

本批仅覆盖列出的卡牌与能力。Silent 其余牌及 M4 的后两幕、OrbQueue 和 Osty 仍待移植。
