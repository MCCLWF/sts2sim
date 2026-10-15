# CombatSolver Env 部分事件账本

日期：2026-09-27。固定原搜索：`external/CombatSolver` 的 `d231e9e51a0e58d6bfa1373c6265cce13ffd9a45`。本报告只证明列出的事件种类、六计数和已实现字段，不表示完整搜索可用。

预测会话拥有独立于 `GameState` 的 scalar 历史。C++ 在真实出牌、逐项受伤、攻击完成、抽牌开始/完成、生成开始/完成、Orb channel 时发出条目。每条由 session Clone 复制；Answer 时与游戏状态一同回滚到动作起点并重放。`sts2env.combat_prediction` v4 快照保存当前历史与选择动作基线，独立 hash 覆盖两者；导入检查六计数、deferred 引用、攻击命中数、选择重放一致性。普通 Env 的快照和普通出牌历史保护保留原行为。

`tools/EnvCombatPredictionChecks` 在 **TestMode=false** 的十个同根场景，将 Env 条目种类顺序、六项 `CombatHistoryCounters`、卡牌 ID/持有者/升级/类型、出牌目标/资源/重复次序、抽牌类型、逐项伤害属性与攻击的所有命中结果和标志与原预测器对比，均通过。`strike` 的两次攻击各为 Started→Damage→Attacked→Finished；`fire_potion` 包括致命药水伤害；玩家中毒下一回合包括五对 Drawn/Resolved。为此修复预测作用域在最后一名敌人被击杀时漏记终击的缺口。原预测器对 Self 卡的 `CardPlay.Target` 记录玩家本身，游戏执行传空目标；预测账本按原政策记录玩家身份，游戏执行参数保持原样。`forced_end` 中原预测器另有 VoidForm 的 `MethodNotMirrored` RiskEntry；当前 C++ 游戏执行层不会产生这一政策条目，测试仅将这一项作为**明确记录的缺口**排除。原始证据可通过以下命令重建：

```bash
cmake --build build -j 4 --target test_combat_prediction sts2env_c
build/test_combat_prediction /tmp/mcc-p2-trace-fixtures
python3 tools/CombatSolverEnv/check_prediction_abi.py /tmp/mcc-p2-trace-fixtures
~/.dotnet/dotnet run --project tools/EnvCombatPredictionChecks -c Release -- --out /tmp/mcc-p2-trace-native-env
```

v4 为伤害新增来源 kind/ID：执行时的卡、怪物攻击、中毒、荆棘、DemisePower 与 MercuryHourglass 使用显式来源作用域；没有原政策对应信息的 FirePotion 保留 Unknown，不根据模型类型猜测。十个 TestMode=false 场景中的伤害来源与固定原预测器逐项一致，Clone/Answer/导出恢复均覆盖新字段，hash 同步覆盖。当前事件字段仍为部分投影：生成缺结果分类和选项，其他原条目与评分账本也未覆盖；未测来源还可能是 Unknown。测试尚未证明完整历史 payload，也未比较原搜索全部状态。v4 始终标记 `history_projection_complete=false`，原同算法 Env 搜索门禁继续关闭。最终可安装 Mod、游戏内真实双模式性能对比与 Env 全面替代尚未验收。

后续接入：`EnvSearchBranch` 现在包住 `EnvCombatPredictionSession`，Beam 的验证节点从根、动作、连续回合到终局都使用独立预测句柄，并通过私有 clone 的结构视图检查 source purity。十个 TestMode=false 夹具均报告 `owned_prediction_branch=true`；这只证明执行所有权和有限结构对拍，不能证明完整 Snapshot/评分/状态键等价，也没有打开正式 Env 后端。
