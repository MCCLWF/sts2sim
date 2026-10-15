# Choices Paradox 整局阻断修复

## 目标

移植原版 `ChoicesParadox.AfterPlayerTurnStart`，使 `SEED0007` 的完整路线继续通过该遗物选择点。

## 范围

- 按 v0.111.0 源码在第一回合生成五张角色牌、逐张施加保留、选一张加入手牌。
- 使用已有 `CardFactory`、`CardSelectCmd` 和 `CardPileCmd` 通用机制，不引入专用搜索逻辑。
- 运行原版 oracle 的 L4 决策与快照差分，继续记录下一处首停。

## 验收

- 生成分发后构建通过。
- `SEED0007` 越过原本第 695 个决策且此前决策零分歧。
- 完整 `scripts/check.sh` 通过；失败如实记入报告。
