# 通用 Env 小批：Driftwood 重掷奖励报告

日期：2026-09-27。分支：feat/generic-env-rf-api。计划：[Driftwood 重掷奖励](../plans/2026-09-27-env-driftwood.md)。状态：生成、Release 构建、有限 L4 与公共查询差分已通过；执行 Reroll 的完整原版差分仍待补。

## 实现

- 新增 `game/Models/Relics/Driftwood.h`，照 v0.111.0 `Driftwood.cs` 实现 `TryModifyRewardsLate`：非持有者返回 `false`；持有者只将 `RewardType::Card` 的 `can_reroll` 设为 `true`，随后返回 `true`。
- 没有覆盖已有 `CanReroll` 查询或新增 setter；没有添加策略字段、Random Foreseer 专用预测或 CombatSolver 依赖。
- `Ancient` 稀有度已存在于 `data/content.json`，按 `RelicModel::Rarity()` 内容表读取。
- `tools/Sts2Oracle/OracleRun.cs` 的 `RewardJson` Card 对象增加 `can_skip`、`can_reroll`、`has_been_rerolled`。前两项读正版公开属性；`has_been_rerolled` 通过反射读取正版私有 `_hasBeenRerolled` 字段。
- `GetSelectedCardReward` 的 `decision.options` 增加原样、保序的 `alternatives`（`OptionId`）数组和 `can_reroll`（数组包含 `REROLL`）。原有 `can_skip=true`、`min`、`max`、选择动作及 RNG 调用保持不变。
- 键名按源码保留为 `Skip` 和 `REROLL`。本改动只序列化已有事实，没有修改 Oracle 策略或选择结果。

## TestMode / headless 检查

- 源码检查：`Driftwood.TryModifyRewardsLate`、普通卡牌奖励的 `CanReroll`、`CardReward.Reroll` 及 `CardRewardAlternative.Generate` 的重掷选项判断没有 TestMode 分支。
- `RewardsSet.Offer` 会在 TestMode 下走测试选择器分支；`OracleRun.RunOne` 为 L4 安装 `RewardsSet.testSelector`，因此定向 run 不会落入“没有 selector 时自动选完全部奖励”的路径。生产模式走 UI 屏幕，选择器实现不是生产 UI 的完整替身。
- `Reward.CreateIcon` 的 TestMode 检查只省略奖励图标节点。当前 `ProductionBranches` 对商店价格、Calling Bell 与 Cauldron 的恢复与 Driftwood 无关。
- 此检查支持把 Oracle L4 作为有限 headless 全流程路径证据，不支持全面 TestMode Off 或真机结论。

## 验收状态

| 项目 | 状态 |
|---|---|
| 正版 `Driftwood.cs` 语义核对 | 已完成 |
| `Driftwood.h` 实现 | 已生成分发；静态 no-stub 库存增加 1 |
| 生成器、Release 构建 | root 统一执行通过 |
| Oracle L4 定向差分 | 8 条普通生命值路线及 8 条高 HP+Courier 路线，2038/2038 决策对齐，0 Unported |
| Oracle 直接 `CanReroll`/`REROLL` 观测字段 | 公共查询与原版比较，观察到 5+36 个 REROLL 候选列表 |
| 全面 TestMode Off 或真机验收 | 未执行 |

## 定向配置与证据限制

实际使用 `SEED0001..SEED0004` × policy seeds `1,2`、Ironclad A0、no-Neow、`--relics DRIFTWOOD`；额外配置使用 `--hp 1000 --relics DRIFTWOOD,THE_COURIER`。普通 8 条都战死，高 HP 8 条都停在第一幕 Boss 前。L3 `combat` 路径不经过常规房间主 `RewardsSet`，未用作本项直接验收。

Oracle trace 现在在 rewards decision 输出实际 `can_reroll` 等字段，并在 card-select 输出原版 alternative 数组。C++ replay 已同步；独立 `tools/check_run_queries.py` 比较公共接口的相同事实，严格拒绝缺失/Unported，有限矩阵通过。

root 统一构建与验收。命令、日志和完整门禁见[集成报告](2026-09-27-env-run-query-results.md)。普通 OracleRun 策略会选卡，尚未执行 REROLL action；C++ 合同夹具执行了 Reroll、检查标志与恢复，但不构成该分支的原版差分。原版 `CardReward.OnSelect` 保存最初的 alternatives；Reroll 后 UI 刷新与 headless selector 保存列表的区别仍需专门验证，未凭 `CanReroll=false` 擅自改变原版选择循环。
