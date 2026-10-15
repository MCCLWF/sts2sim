# 战斗外死亡未结束整局：首因与修复

分支 `fix/out-of-combat-death`，基线 `4b3013f`。小修复，无单独计划；在
[NoSL Q1 任务](../plans/2026-10-03-nosl-q1-foundation.md) 的基线测量中发现。

## 现象

`nosl_run --policy random --split dev --first 51 --count 1`（Silent A10，种子 `NOSLDEV0051`）
永不返回，单线程 100% CPU。逐步跟踪的最后三步：

```text
step 47 map   floor 2 hp 6/70  -> MapMove 1,2
step 48 event floor 3 hp 6/70  -> EventOption 1      (ROOM_FULL_OF_CHEESE / SEARCH, will_kill_player=true)
step 49 map   floor 3 hp 0/70  -> MapMove 1,3        (挂起)
```

gdb 栈：`RunFlow::ChooseMapPoint → RunManager::EnterMapCoord → CombatManager::StartCombat →
RunAfterEndTurnSignal → EndPlayerTurnPhaseOneInternal`。死亡玩家在 `StartTurn` 里被直接标成
ready-to-end-turn，回合循环没有出口。

## 首因

原版 `Commands/CreatureCmd.cs` 的 `Kill`：所有玩家死亡时，先在战斗中 `LoseCombat()`，然后**无论是否在战斗中**
都调用 `RunManager.Instance.OnEnded(isVictory: false)` 并显示结算界面。`RunState.IsGameOver` 就是
“所有玩家死亡”。

移植版 `CreatureCmd::Kill` 只保留了 `LoseCombat()`，整局结束由 `RunFlow::AfterCombatAction` 在战斗动作后设置。
战斗外的致死（事件扣血）没有任何地方结束整局，Env 回到地图并继续接受动作。

L4 对拍没有发现：oracle 采集策略只在不致死的选项里选（`WillKillPlayer`），自然轨迹不会走到这里。

## 修复

- `RunFlow::SettleGameOver`：不在战斗中且玩家已死亡时置 `is_victory=false`、`phase=Ended`。
- `Env::Run` 在每个顶层动作/操作执行完后调用一次。等待中的 Choice 不受影响。
- 未改 RNG、未改任何已有对拍字段。

## 验证

| 检查 | 结果 |
|---|---|
| `tests/test_env.cpp` 新增用例：Room Full of Cheese，HP 1 时选 Search | 修复前 `test_env` 退出 41；修复后退出 0 |
| `nosl_run --policy random --split dev --first 51 --count 1` | 合入后在 Q1 分支复测，结果写入 Q1 报告 |
| `STS2_SRC='…/Slay the Spire 2/src/Core' scripts/check.sh` | 退出 0，末行 `all checks passed`；59/59 CTest；L3 40+40 场完整战斗一致；L4 全部路线 identical，含 4 条到最终胜利的整局（3163/3163 决策） |

期望值来自原版源码（`Kill → OnEnded(isVictory: false)`），不是模拟器自己的输出。

## 限制

- **oracle 差分已补（2026-10-03 晚）。** oracle 新增按给定动作执行的 `run --script`
  （见 [E01 报告](2026-10-03-nosl-e01-oracle-replay.md)）。把 `NOSLDEV0051` 那条随机轨迹去掉第 42 步的
  战斗外丢药水后交给正版执行：正版在 Room Full of Cheese 选 Search 后以 `death` 结束（HP 0/70，第 3 层，房间 Event），
  `tests/l4/l4_run.py --trace` 的结果是 `identical`，45/45 个决策一致。产物在 `artifacts/out-of-combat-death/oracle/`。
  没有用修复前的二进制重跑这条对拍，所以“修复前会在这里分歧”是推断，不是实测。
- 同一个动作内“先致死、再进入战斗”的事件流程没有排查；原版此时已显示结算界面，移植版仍会执行后续代码。
  目前没有已知事件这样做。
- 其他战斗外致死来源（遗物获取、药水等）走同一检查，但只有事件扣血有用例。

门禁原始日志：`artifacts/out-of-combat-death/check-full.log`（不提交），SHA256 `fa3ebef794d4301af306d9b25f96fbc4a3707756b9ef8e89843657c24dfe5229`。
