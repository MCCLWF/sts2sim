# Turret Operator Weak 遭遇移植

## 目标

按 v0.111.0 源码补齐 `TURRET_OPERATOR_WEAK`，包括 Living Shield、Turret Operator 与 Rampart 能力，并以 oracle L3 决策状态对拍。

## 任务

- 移植两只怪物的初始血量、招式状态机和实际动作。
- 移植 Rampart：玩家侧正常回合开始时给场上的 Turret Operator 增加 25 点无来源格挡；额外玩家回合不触发。
- 注册遭遇、怪物和能力的 C++ 模型分发。
- 使用 A0/A10 多种子 L3 对拍，并确认遭遇进入成功。
- 更新总遭遇报告、roadmap 与 README 覆盖数。

## 验收

- `TURRET_OPERATOR_WEAK` 能经 Env 初始化并生成两只原版怪物。
- L3 每个决策点完整战斗状态与 oracle 一致；对拍覆盖队友存活分支、5 段攻击、Reload 力量及 Rampart 格挡。
- `scripts/check.sh --quick` 通过。

## 进度记录（2026-09-26）

- ✅ 遭遇、Living Shield、Turret Operator 与 Rampart 均已移植并注册。
- ✅ A0 4/4 战斗、69/69 状态；A10 2/2 战斗、35/35 状态，L3 与 oracle 一致。
- ✅ C++ 模型构建、Env/CLI 构建及 `scripts/check.sh --quick` 通过。
- ⏳ 完整全仓库 L2/L3/L4 门禁仍未运行；全遭遇目标仍有 26 个正式遭遇未实现。
