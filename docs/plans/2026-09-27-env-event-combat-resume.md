# 通用 Env：事件战斗返回父事件与 Battleworn Dummy

日期：2026-09-27。分支：`feat/generic-env-rf-api`。所属阶段：E1/E3，先完成铁甲战士 run 层闭环。

## 目标

按 v0.111.0 原版实现共享事件进入战斗、无普通奖励时的直接继续、战后返回父事件和 Battleworn Dummy 的三档结算。冻结 CombatSolver 派生工具，使通用 Env 构建与验收不依赖其私有策略。

## 工作与边界

- 将旧 CombatSolver managed 适配器与专用检查移到 `archive/combatsolver/managed-tools/`，继续禁止构建；通用查询保留原版 getter、owned 副本和事实字段。
- 在 POD 战斗状态中保存父事件是否应恢复与 Battleworn 超时标志；快照、Hash 和导入必须覆盖新字段。
- `RunFlow` 按 `Encounter.ShouldGiveRewards` 决定是否给普通战斗奖励，并按父事件恢复标志决定返回事件或地图。
- Battleworn Dummy 的选项、战斗、超时与 V1 药水、V2 升级、V3 遗物奖励按原版 RNG/调用顺序实现。
- 修复 headless oracle 错把 `ShouldGiveRewards=false` 的遭遇当普通奖励战斗处理的流程；对拍真值仍来自游戏 DLL、原版 `NCombatUi.OnCombatWon` 和 `RunManager.ProceedFromTerminalRewardsScreen`。

## 验收

1. `bash -n scripts/check.sh`、生成状态字段检查、CMake 构建、CTests 和完整 `scripts/check.sh` 通过。
2. `SEED0004` / policy 1 / Ironclad / `--hp 10000` 的三幕 `--stop victory --verify-snapshots --require-complete` 真实路线完整对齐，并纳入 `scripts/check.sh`。
3. Battleworn V1/V2/V3 的超时标志、逃跑顺序、快照往返由聚焦测试验证；胜利奖励的其它分支须后续增加原版差分，不能把本次路线当作全内容完成。
4. 归档项目的显式构建被拒绝，活动 `tools/` 不再含 CombatSolver 私有适配器；默认 C ABI 不恢复旧 prediction/Beam。

结果记录于[执行报告](../reports/2026-09-27-env-event-combat-resume.md)。
