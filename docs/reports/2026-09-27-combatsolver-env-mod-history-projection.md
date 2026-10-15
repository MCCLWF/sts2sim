# CombatSolver 风险历史投影迁入派生 Mod

日期：2026-09-27。仓库分支 `feat/combatsolver-env-backend`，记录所依据的前置提交为 `a2fa0c3d01667195c0e1d09a80b9c02b6c074165`。固定 upstream `external/CombatSolver` 为 `d231e9e51a0e58d6bfa1373c6265cce13ffd9a45`；本次没有修改 upstream。本文记录事实层与 Mod 历史适配器的阶段性结果，不表示完整搜索后端或最终 Mod 已完成。

## C++ Env 记录执行事实

`CombatPredictionEventKind` 现在以 `CardEffectStarted` 代替政策层 `Risk`；执行边界不再含 `CombatPredictionRiskReason`，也不按卡牌 ID 分派 Rampage/VoidForm 风险。历史 v7 的每条事件为 **20 列**（索引 0–19）；play facts 为 **10 列**，新增 `target_type`。v7 的导入、导出和 hash domain 同步升级。位置：`engine/sim/combat_prediction_boundary.h:20-26,55-66`、`env/combat_prediction.cpp:91-96,152-169,400-409,473-501`、`game/Combat/History/CombatHistory.h:23-33,167-174`。

`CardModel::OnPlayWrapper` 对每次真实 OnPlay 迭代依次记录 `CardPlayStarted`、`CardEffectStarted`，然后调用 `OnPlay`；OnPlay、附魔和 Affliction 处理后才记录 `CardPlayFinished`（战斗未提前结束时）。因此效果开始事实先于 OnPlay 内产生的伤害，并处在 Started 与 Finished 之间。重复播放时，这组记录按实际 play iteration 重复。位置：`game/Models/CardModel.cpp:443-465,467-479`。`CardEffectStarted` 保存卡牌/owner、卡牌快照及实际 play facts，包括游戏传入的目标、资源、自动打牌标志、play index/count 和目标类型；事实层不把 Self 的空游戏目标改写成搜索策略目标。

## C# Mod 还原原历史和已知风险

派生 C# 的 `EnvSearchHistoryProjection` 将 20 列事实投影成固定 upstream 使用的 **21 列**历史行，最后一列恢复原历史的 Risk reason。普通事实行末尾写 null；遇到 kind 9 的 `CardEffectStarted`，适配器按模型将事实替换为原历史的 `Risk`：

- `RAMPAGE` → `MethodMirrorIncomplete`。
- `VOID_FORM` → `MethodNotMirrored`。

此映射只覆盖固定 upstream 的默认 inference 设置：原 registry 默认 `AllowInference = true`；推断出的 handler 在调用前记 `MethodMirrorIncomplete`，不支持的 override 记 `MethodNotMirrored`。适配器在 `AllowInference` 关闭时拒绝投影。位置：`external/CombatSolver/src/Engine/Common/Mirrors/MethodMirrorRegistry.cs:68,77-88,214-230`、`tools/CombatSolverEnv/Search/EnvSearchHistoryProjection.cs:22-25,47-63,115-127`。

风险行在事实行的原始时间点替换 `CardEffectStarted`，故 Rampage/VoidForm 的映射都位于对应 `CardPlayStarted` 之后、该 OnPlay 的后续伤害之前、`CardPlayFinished` 之前。原执行代码在每次 OnPlay 调用前记录事实；原预测器的 inference registry 在运行推断 handler 前记录风险。位置：`game/Models/CardModel.cpp:461-465,475-479`、`external/CombatSolver/src/Engine/Common/Mirrors/MethodMirrorRegistry.cs:222-230`、`tools/CombatSolverEnv/Search/EnvSearchHistoryProjection.cs:49-62`。

游戏记录的 Self 目标可能为空，而原 CombatSolver 的策略历史要求目标指向持有者的 Creature。play facts 现在同时保留实际 `target` 与 `target_type`；Mod 只在目标为空且类型为 Self 时，按 owner 映射回 Creature 引用。缺少 owner 映射时拒绝投影，不猜目标。位置：`game/Combat/History/CombatHistory.h:167-174`、`tools/CombatSolverEnv/Search/EnvSearchHistoryProjection.cs:76-89`。

投影重建原历史的六个计数，顺序为 FinishedPlays、EtherealPlays、LightningChannels、UnblockedHitsReceived、CardsDrawn、CardsGenerated；计算条件在 `EnvSearchHistoryProjection.cs:94-102`。Beam fixture 将投影计数与原预测器 `GetCounters(owner)` 对拍。

## deferred history Index 的含义

upstream 历史的 `Index` 是事件追加到有序时间线时分配的序号：`Record` 设为当前 `EntryCount`（`external/CombatSolver/src/Engine/InCombat/Simulation/CombatPredictionHistory.cs:429-443`）。抽牌开始 (`CardDrawn`) 和生成开始 (`CardGenerated`) 各自产生一个原始条目；对应的 `CardDrawResolved` / `CardGenerationResolved` 通过 `OriginalEntry` 对象引用配对，导出到事实协议时用第 9 列 `originalIndex` 表示那个开始条目的历史序号。upstream 对象模型和完成逻辑见 `CombatPredictionHistoryEntry.cs:129-162`、`CombatPredictionHistory.cs:269-285,332-352,450-469`；C++ 导入校验其指向之前未完成的正确事件类型，见 `env/combat_prediction.cpp:193-205`。

这个序号不是卡牌 ID，也不是对局中的手牌索引。原时间线会用 `Index` 选出相关条目中的最后一项并聚合风险（`CombatPredictionHistory.cs:220-246,472-485`）；历史切片按事件序号读取，延续/choice 克隆会保留 Index 并重新关联 OriginalEntry（`CombatPredictionHistory.cs:140-151,429-469`、`CombatPredictionHistory.CardContinuation.cs:73-99`、`CombatPredictionHistory.ExecutionContinuation.cs:60-88`）。所以 C# 过滤 CardEffectStarted、插入 Risk 后，不能原样复制 deferred 的 raw `originalIndex`。适配器建立 raw→policy index 表；解析条目只允许指向此前仍映射、类型正确的 draw/generated 开始条目，再把字段 9 改为投影时间线序号（`EnvSearchHistoryProjection.cs:34-37,64-75`）。对拍显式检查原预测器的 Index 和两种 `OriginalEntry.Index`（`tools/EnvCombatPredictionChecks/Program.cs:242-255`）。

## 已记录的验收结果

以下结果来自当前工作区已有日志；此报告没有重新构建或运行检查。

| 命令 | 对应记录 |
|---|---|
| `scripts/check.sh > /tmp/mcc-mod-history-full.log 2>&1` | `/tmp/mcc-mod-history-full.log` |
| `build/test_combat_prediction /tmp/mcc-mod-history-abi-fixtures` | ABI fixtures；后接下行 ABI 检查 |
| `python3 tools/CombatSolverEnv/check_prediction_abi.py /tmp/mcc-mod-history-abi-fixtures` | `/tmp/mcc-mod-history-abi.log` |
| `~/.dotnet/dotnet run --project tools/EnvSessionChecks -c Release` | `/tmp/mcc-mod-history-api.log` |
| `python3 tools/EnvQueryChecks/check.py --out /tmp/mcc-mod-history-queries` | `/tmp/mcc-mod-history-queries.log` |
| `~/.dotnet/dotnet run --project tools/EnvCombatPredictionChecks -c Release -- --out /tmp/mcc-mod-history-beam-final` | `/tmp/mcc-mod-history-beam-final.log` |
| `python3 tools/CombatSolverEnv/run_smoke.py --mode both --expect-profile-root-rejection --memory-reservation-mib 1792 --out /tmp/mcc-mod-history-game-retry` | launcher、memory 与 game logs：`/tmp/mcc-mod-history-game-retry/`；摘要：`/tmp/mcc-mod-history-game-retry.log` |

- `/tmp/mcc-mod-history-full.log`：C++ 全目标构建完成；CTest **10/10** 通过；smoke 记录 `death=94 unported=6`、`death=100`、`boss=3 death=97`；Oracle build 成功，含 1 个 `CS8604` 警告、0 错误。L2 为 200 runs、0 mismatch；L3 两组均 40/40 combats identical、0 unported、0 diverged，状态匹配分别 784/784 与 827/827；L4 两组均 40 runs identical、0 stopped at unported、0 diverged，决策匹配分别 1958/1958 与 2131/2131。日志结尾为 `all checks passed`。
- `/tmp/mcc-mod-history-beam-final.log`：原 Beam 的 **16 个实际 owned 场景**均报告 `observed=True`、`owned_prediction_branch=True`、`reason=none`、`semantic_projection=true`，包括 `rampage`、`rampage_then_turn`、`rampage_repeat`、使用 VoidForm 的 `forced_end`。三种 Rampage 场景的 projection history 长度依次为 5、19、10；`forced_end` 为 15。日志另外明确记录 `Mod history: nondefault inference policy explicitly refused`。对拍范围仍不是完整状态或完整历史 payload：总结果写明 `full_state_comparison=false`、`full_history_payload_comparison=false`、所有场景 `original_key_ready=False`，且 `installed_game_acceptance=false`。
- `/tmp/mcc-mod-history-api.log`：64 个并行预测、源隔离、失败恢复、dispose、独立 hash domain、完整恢复、16 个并行分支、严格协议和 owned disposal 等 API checks 均报告通过。
- `/tmp/mcc-mod-history-abi.log`：C ABI 检查报告通过，包括 borrowed buffers、严格输入、失败时空结果、独立 owner、terminal/choice query scope、card effect facts restore、terminal restore 和 choice replay。
- `/tmp/mcc-mod-history-queries.log`：计算变量的原预测/游戏精确 decimal 对拍及部分原卡牌、Orb、Power 输入 fingerprint 对拍通过；同一日志仍标明 `policy_projection_complete=false`、`threat_projection_complete=false`、`complete_state_key=false` / `full_key=false`，`CanPlayCard` probe 仍不完整。

## 实际游戏进程的 derived smoke

`/tmp/mcc-mod-history-game/launcher.txt` 首次 admission 因 `host reservations full` 超时；没有启动游戏 PID，临时 instance 已清理。基于先前测得的 1,463,696 KiB 峰值，将 reservation 调到 1792 MiB 重试：`/tmp/mcc-mod-history-game-retry/launcher.txt` 记录 `HEADLESS_ADMITTED`，`/tmp/mcc-mod-history-game-retry.log` 记录 `derived smoke passed`。`/tmp/mcc-mod-history-game-retry/memory.json` 记录 reservation 为 1792 MiB，采样峰值 RSS 为 1,448,604 KiB。运行记录状态为 Passed，回合 1 至回合 1，原生 solver 达到 node limit，扩展 100 个节点、335 次转移。

这次游戏进程没有实际启用 Env：`/tmp/mcc-mod-history-game-retry/game.log` 记录 `ENV_MODE_UNAVAILABLE`，原因是 live profile 的 run/player 均为 `runs=0, missing_seen_encounters=85`；同时明确写出 `env_imported=false`、`env_used=false`、`native_search=continues`，且 `ENV_UNAVAILABLE_WARNING shown=true`。汇总为 `native_expanded=100 native_transitions=335 env_checked_transitions_root_cumulative=0 env_disabled=true`。所以这证明安装环境中的 native derived smoke 通过，不证明 live Env root 可导入、Env 搜索已跑通或安装版 Env 历史映射已验收。重试结束时 launcher 已清理 instance；`game.log` 还记录退出时 Godot renderer resources in use/leaked，未据此判断 smoke 结果。

## 未完成与待补

`EnvSearchHistoryProjection.Complete` 仍固定为 false，`RequireComplete()` 始终拒绝；`EnvSearchBranch` 正式执行入口还要求完整 history 与原 state key。C# 当前只覆盖默认 inference 下明确映射的 Rampage/VoidForm 和这些 fixtures 里的事实；Hooks、choices、限制策略等其他 Risk 分派/条目仍未迁完。`tools/CombatSolverEnv/Search/EnvSearchHistoryProjection.cs:8-20` 明确标注该边界。

前置 `a2fa0c3` 的原 state key 与 score 尚未完成；本次 history 投影没有补齐它们。全状态/完整历史对拍、完整搜索后端、最终可安装 Mod 都不应从以上结果推断为已完成。

### Env 实机 / 安装验收结果

实际游戏进程的 native derived smoke 通过，但 fixture 汇总仍记录 `installed_game_acceptance=false`。该次运行因 live profile completeness 校验禁用了 Env（见上节）；尚未实测 fully-unlocked live profile 下的 Env root import、双模式或 Env transition 对拍。
