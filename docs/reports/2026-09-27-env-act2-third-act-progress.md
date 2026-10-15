# 通用 Env 二幕及第三幕入口实证进度

日期：2026-09-27。分支：`feat/generic-env-rf-api`。游戏与源码固定 v0.111.0。此报告覆盖当前工作树；不把未提交代码或有限轨迹当作完整 Env。

## 当前结果

第一幕此前已在 12 条原版战士轨迹、3906 个决策点完成差分和逐点快照恢复，其中 11 条进入第二幕，1 条战死。本批沿真实二幕路径补充先古、事件、遗物、跨职业卡、奖励 alternative 与生成过滤，使用原版游戏逻辑采集决策，再让通用 Env 按同一动作序列重放。

| 原版配置 | 已通过的决策 | 实际终点 |
|---|---:|---|
| `SEED0002` policy 1，A0，无 Neow，HP 10000 | 564/564 | 二幕 Boss 战死 |
| `SEED0002` policy 2，同上 | 747/747 | 二幕 Boss 战死 |
| `SEED0003` policy 1，同上 | 603/603 | 第三幕入口；Kaiser 表现层无头补丁后新采集的有效轨迹 |
| `SEED0003` policy 2，同上 | 595/595 | 第三幕入口；Biiig Hug、Soot 与 Metamorphosis 路径 |
| `SEED0004` policy 1，同上 | 737/737 | 第三幕入口 |
| `SEED0002` policy 1，另带 `PAELS_WING` 并强制两次献祭 | 219/219 | 第一幕结束，确实选择两次 `SACRIFICE` |

以上均开启逐决策快照导出、导入、合法动作及后继比较；定向路径使用测试用高生命值，不能证明自然生命值下的相同终点。最初 SEED0003 policy 1 的旧轨迹在 Crusher/Rocket 死亡时遭无头音频空引用，记录到中途动作；修正表现层补丁后 603/603 一致。旧轨迹不是有效的行为真值，见 [Ranwid 报告](2026-09-27-env-ranwid-the-elder.md)与 [O-015](../audits/deviations.md)。

第三幕 `SEED0004` policy 1 的原版轨迹共 1123 决策；本批接入 Fur Coat、TinkerTime 和 Mad Science 的 Attack/Sapping 分支后，Env 通过前 814 个决策，随后明确停在 `Events.BattlewornDummy.GenerateInitialOptions is not ported`。TinkerTime 的两次原版 RNG 选择、`ChosenCardType` 事件状态、`MAD_SCIENCE` 的 CardState 配置字段及事件结束后的牌组状态逐点一致；本条路线在该阻塞前未出现 Mad Science 出牌，因此 Attack/Sapping 的出牌效果另需专门夹具。Fur Coat 的拾取、地图标记、保存坐标、原版内容 RNG、标记战斗的 HP 钩子和逐点快照恢复没有分歧；第三幕尚未贯通，最终 TheArchitect/WinRun 及真实胜利也未验收。另两条二幕语料已知真实阻塞有 `DEATHS_DOOR`、`ITERATION`。这些 Unported 路径没有伪装成成功。

## 本批通用规则

新增/完善的路径包括 Orobas、Tezcatara、Spirit Grafter、Ranwid、Pael、Infested Automaton、Colorful Philosophers、Nonupeipe，以及 Archaic Tooth、Sand Castle、Wongo's Mystery Ticket、Lost Wisp、Pael's Tears/Wing、Toasty Mittens、Biiig Hug、Soot、Metamorphosis、Reflex、The Hunt 和 Fur Coat。Fur Coat 的标记坐标是可快照的 POD 状态，使用 `Rng(owner, FUR_COAT, 0)` 与列主序不稳定洗牌，地图生成、战斗开始和后续敌人加入均沿原版 Hook 顺序执行；`run_queries` 现在公开当前地图点及其标记，供通用客户端读取。内容对应原版模型；涉及的公共选择、奖励 alternative、生成过滤、遗物移除 Hook、战斗期费用和快照引用校验在通用 Env 中实现。每项具体原版条件与验收范围记录在同日分项报告。尚未用原版动作触发的分支继续视为未验证。

## 下一阶段判断

`python3 tools/coverage/env_inventory.py` 当前静态库存为卡牌 261/596 个具体头文件、遗物 191/299、普通事件 52/59、先古 5/9；五角色卡池依次为 90/90、32/91、31/91、8/91、10/91。正式遭遇头文件 90/90。头文件和静态 `IsUsable` 均不能证明每个方法、条件分支及组合已被原版验收。

继续做通用 Env 有价值，现有确定性基础、选择重放和逐点差分已经能贯通多幕真实路线。当前不能宣称可完全替代游戏内搜索或 Random Foreseer：第三幕与其他角色内容、真实档案和 live 捕获、类型化悬停操作、可选停止边界、完整来源/因果 trace、统一失败与容量语义、独立 C#/Python SDK 均未完成。默认 L4 仍允许 Unported 并在第一幕 Boss 前停；新加的严格门禁只覆盖上表的有限路线。

继续按通用 Env + RF 公共 API 主线推进；原版规则、RNG、Hook 和结果事实由 Env 提供，搜索政策、评分、风险偏好和 UI 属于客户端。CombatSolver 旧移植保持冻结，完整后也不自动恢复。

## 验收边界

完整 `scripts/check.sh` 已复跑通过，日志 `/tmp/mcc-env-act2-full-check-final.log`：13/13 CTest，L2 200次开局零差分，L3 80战/1611状态零分歧，默认L4 80条/4089决策零分歧；Sand Castle 172/172、Wongo ticket 256/256；五条严格二幕路线3246/3246决策；Pael's Wing两次献祭219/219。严格路线开启逐点快照恢复。额外的 Fur Coat/TinkerTime 第三幕路线命令为 `DOTNET=~/.dotnet/dotnet python3 tests/l4/l4_run.py --seeds SEED0004 --policy-seeds 1 --no-neow --hp 10000 --stop victory --verify-snapshots --require-complete`，通过 814/1123 后在 Battleworn Dummy 明确停止；这是当前第三幕推进证据，不是完整成功。冒烟仍有6条Unported；现有门禁通过不代表全内容完成。分项命令和范围见各报告及 `scripts/check.sh`。
