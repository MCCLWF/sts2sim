# 实机解锁档案状态进入通用 Env

日期：2026-09-27。分支 `feat/combatsolver-env-backend`，前置 `4c99a23`。固定原 CombatSolver 子模块未修改。

## 变更

通用 `GameState` 现在分别保存 Run 与 Player 的真实 `UnlockState`：运行次数、57 个 `EpochModel.AllEpochIds` 的解锁位、按生成 `EncounterId` 编号的已见遭遇位和全解锁老档案兼容标志。这些 POD 字段进入完整快照、克隆及哈希。默认 seeded Env 仍使用全解锁档案；`ModelDb.AllEncounters` 实际包含 85 个 ID，生成枚举另有 5 个不属于该集合的 ID，默认位图与正版 `UnlockState.all` 对齐。

Mod 从冻结的 Run/Player 原始档案生成这些字段，assembler 检查源、快照字段和兼容汇总一致，不再在组装前拒绝部分档案。通用 Env 的动作入口、合法动作查询和内部重放对部分档案抛出 `Unported`，原 Beam 核对分支也明确拒绝。卡池、药水池、首局教程、已见遭遇和发现幕等规则仍未对拍，不会以全解锁逻辑执行部分档案。

## 验证

- `cmake --build build -j 8`：通过；POD 字段 visitor 和 schema 已重新生成。
- `~/.dotnet/dotnet run --project tools/EnvSessionChecks -c Release`：64 并行分支、完整快照导入、部分档案快照/克隆字段保持、动作拒绝不改源，以及伪造全解锁位图和越界遭遇位的导入拒绝通过。日志 `/tmp/mcc-profile-session-final.log`。
- `~/.dotnet/dotnet run --project tools/EnvLiveCardChecks -c Release`：TestMode=false 的全解锁实际对象与六个完整根夹具继续通过；部分档案捕获验证 Run/Player 编码位。日志 `/tmp/mcc-profile-live-final.log`。
- `scripts/check.sh`：CTest 10/10；L2 200 局零差异；L3 A0/A10 各 40/40 战斗、784/784 与 827/827 状态；L4 两组各 40/40 局、1958/1958 与 2131/2131 决策。日志 `/tmp/mcc-profile-full-final.log`，本次 Oracle 构建零 warning、零 error。
- 隔离实机 `run_smoke.py --mode both --expect-event-bookkeeping-rejection --memory-reservation-mib 1792`：`derived smoke passed`，场景启动和 Native 100 展开/335 转移通过；保留日志 `/tmp/mcc-profile-game-final/game.log`。首次运行档案 Run/Player 均 `runs=0, epochs=57, encounters_seen=0`，档案捕获通过，下一处拒绝为 `live_event_bookkeeping_pending:1`，UI 显示 Env 不可用且 Native 继续。最初运行旧检查器时，它仍期待旧档案拒绝理由而退出 1；更新检查器后重新启动实机并通过。

## 未完成与下一步

这次没有实机完整根导入。离开 Neow 后，正版 `EventSynchronizer._events` 保留 1 个事件记录；当前 room mapper 只接受空同步器。该记录及其投票/页面/选项等状态尚未证明可以丢弃，也未映射进通用 Env，不能把事件数组填成模板空值。下一项是精确冻结并映射该遗留记录及同步器状态，再重试实机导入；随后逐项移植和对拍部分档案的内容生成规则，才能允许预测。原 Beam 同算法 Env 执行适配、UI 等预算性能、选择/药水/连续回合和最终 `mods/` 验收仍未完成。
