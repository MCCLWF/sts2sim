# 实机已完成事件状态进入通用 Env

日期：2026-09-27  
分支：`feat/combatsolver-env-backend`

## 这次完成的内容

正版 `EventSynchronizer` 在离开事件房间时不会清空 `_events`；下一次 `BeginEvent` 才会清理或替换旧记录。此前 live mapper 看到一条记录就把完整根停用，无法区分“已完成但保留的 Neow”和“仍有选项/异步任务的事件”。

本次把普通战斗根允许的边界收窄为可证明的已完成 Neow：

- 通用 `RunData` 增加可复制的 `EventSynchronizerState`，保存单人事件数量、canonical event、玩家投票槽、当前投票和页码；`EventState` 仍保存事件本身的 RNG、描述键、动态变量、古代事件字段和选项。
- C++ `BeginEvent`/选项路径按单人语义更新同步器；快照校验拒绝多事件、空状态残留 canonical、无槽投票和非零页码等不变量破坏。
- Mod 主线程冻结同步器的 `_events`、canonical、投票、页码、待处理任务和事件战斗状态。只接受精确 `Neow` 类型、已完成、已清理、无当前选项、无动态变量/修饰器/调试选项、无事件战斗和无待处理任务的记录。
- 完成事件的 RNG、描述定位键、`HealedAmount`、`CustomDonePage`、事件 ID 和事件槽写入 blank-template 组装的 live root；事件数组不再被模板空值覆盖。其他事件类型、未完成事件、共享事件页、异步选项和未知闭包继续 fail-closed。

## 验证

以下命令均通过：

```text
cmake --build build -j8
~/.dotnet/dotnet build tools/CombatSolverEnv/CombatSolverEnv.csproj -c Release -v:q
~/.dotnet/dotnet run --project tools/EnvSessionChecks -c Release
~/.dotnet/dotnet run --project tools/EnvLiveCardChecks -c Release
scripts/check.sh
```

`EnvSessionChecks` 通过 64 路并行分支、快照导入/克隆、部分档案动作拒绝和新增同步器字段的越界拒绝。`EnvLiveCardChecks` 的 TestMode=false 完整根夹具及完成 Neow 事件夹具通过；六个普通战斗整根场景仍逐字段恢复并完成两次后续回合。`scripts/check.sh` 结果为 CTest 10/10、L2 200/0、L3 两组 40/40 且 784/784 与 827/827 状态一致、L4 两组 40/40 且 1958/1958 与 2131/2131 决策一致。

真实隔离游戏复验：

```text
python3 tools/CombatSolverEnv/run_smoke.py --mode both \
  --expect-event-bookkeeping-rejection --memory-reservation-mib 1792 \
  --out /tmp/mcc-event-game-final
```

通过。真实启动点的 Run/Player 档案已经被捕获；该启动器在没有先选择 Neow 的情况下直接进入战斗，因此正版同步器仍有一条未完成事件，日志明确为 `live_event_pending:1`。Env 在根导入前停用，UI 显示 Env 不可用，Native 继续完成 100 次展开和 335 次转移。日志保存在 `/tmp/mcc-event-game-final/game.log`。

## 当前边界

这次工作证明了“已完成并清理的 Neow 记录可以进入通用状态”的组件合同，但没有宣称实机已经接受任意 live 根。真实游戏复验仍使用绕过 Neow 选择的隔离启动点，所以它正确地停在未完成事件边界；不能把该记录丢弃或填回模板空值。要让实际 Env 替换搜索，还需要用完成 Neow 的真实存档/启动路径再次验收，并继续完成部分档案规则、完整 state key、评分/威胁投影以及与原 Beam 相同的执行后端。当前 UI 的 Native/Env/Both 仍是参考核对和 fail-closed 回退，不是原搜索已切换到 Env。
