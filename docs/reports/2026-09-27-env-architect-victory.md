# 通用 Env：The Architect 胜利闭环执行报告

日期：2026-09-27。分支：`feat/generic-env-rf-api`。计划：[The Architect 胜利闭环](../plans/2026-09-27-env-architect-victory.md)。原版依据：v0.111.0 `TheArchitect.cs`、`AncientDialogueSet.cs`、`RunManager.cs`、`CreatureCmd.cs` 与正版 DLL 的 L4 oracle。

## 实现

- 事件通过 `TheArchitectEventEncounter` 预建展示用布局，保留怪物对象与原版 RNG 顺序。对话依据 `SaveManager.Progress.GetStatsForCharacter(...).TotalWins`，在通用 `NewRunOptions`/POD `RunData` 中单独保存当前角色胜场数；没有借用先古访问次数。
- 首胜铁甲战士的实际路径为 `THE_ARCHITECT.dialogue.0` → `PROCEED`。单例对话选择仍抽一次事件 RNG。`PROCEED` 调用 `WinRun`，先记录胜利，再按原版强制杀死玩家；终局仍保留 Event 房间与完成的事件页。
- 普通 run query 增加 `is_victory`，CLI 区分 victory/death；终局快照校验允许原版展示布局保留。`is_victory` 是 Env 的结果事实，不是客户端评分策略。

## 验证

| 检查 | 原始结果 |
|---|---|
| `cmake --build build --target test_architect_event sts2sim_cli -j2 && ./build/test_architect_event` | 通过；聚焦首胜对话、单例 RNG、事件页、胜利、HP=0、run query 和终局快照恢复 |
| `DOTNET=~/.dotnet/dotnet python3 tests/l4/l4_run.py --seeds SEED0003 --policy-seeds 1 --no-neow --hp 10000 --stop victory --verify-snapshots --require-complete` | `1 runs: 1 identical, 0 stopped at unported content, 0 diverged; 1041/1041 decisions matched` |
| `DOTNET=~/.dotnet/dotnet python3 tests/l4/l4_run.py --seeds SEED0002,SEED0003 --policy-seeds 1,2 --no-neow --hp 10000 --stop victory --verify-snapshots --require-complete` | `4 runs: 4 identical, 0 stopped at unported content, 0 diverged; 3520/3520 decisions matched`；逐决策快照恢复开启 |
| `scripts/check.sh` | 通过：16/16 CTest、L2 200 局、L3 80 场/1611 状态、默认 L4 80 局/4089 决策、严格二幕 3246 决策、既有胜利 1138 决策、新四路线 3520 决策、Pael's Wing 219 决策；所有定向严格路线零分歧零 Unported |

## 限制

这四条路线有三条战死、一条真实胜利；高生命值是内容/长战斗压力条件，不证明正常血量胜率。角色胜场数可由 C++ `NewRunOptions` 提供，但当前 C ABI/Python 重置入口未接受非零值。其它角色对话与真实存档胜场配置只有源码实现，未完成对应原版多路线差分。完整 Env、RF 功能矩阵和 M5 规模性能验收仍未完成；CombatSolver 保持冻结。
