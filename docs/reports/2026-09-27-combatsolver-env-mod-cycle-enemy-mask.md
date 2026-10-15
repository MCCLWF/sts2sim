# Mod 循环键的敌人位图

日期：2026-09-27。分支 `feat/combatsolver-env-backend`，前置 `af8a143`。固定 CombatSolver `d231e9e51a0e58d6bfa1373c6265cce13ffd9a45` 未修改。

## 实现范围

原 `CycleShapeKey` 按顺序编码循环牌堆形状两个半字、原 `KnownEnemies` 的存活位图、药水库存键两个半字和搜索边界。`KnownEnemies` 的根顺序来自原战斗敌人表；普通死亡后仍保留原位置，所以不能使用当前 Env 活动 roster 的下标。

Mod 在打开 owned Env 根时保存敌人引用、战斗 ID、模型及原顺序，并在 Fork/Predict 共享只读身份。分支读取 C++ 通用快照的当前敌人表和 HP，在名单稳定、活跃敌人正生命且不含 Waterfall Giant 的情形计算位图，再由 `CaptureCycleShapeKey(SearchBoundaryReason)` 合并此前已验证的牌堆与药水键。搜索边界作为 C# 政策参数显式传入；C++ Env 不知道原 Beam 的循环键。

当前缺少原搜索的复活/Steam 和动态名单账本，因此新敌人、逃跑、敌人池槽变化/复用、活跃敌人零 HP、Waterfall Giant 都明确拒绝。它只覆盖一个可证明的子集；其他怪物/战斗及拒绝分支尚未逐场差分。`OriginalStateKeyReady=false` 与正式 Env 执行门禁保持关闭。

## 验证

- 派生 Mod Release：`/tmp/mcc-cycle-mask-mod-build.log`，0 warning、0 error。
- TestMode=false 的 16 个 Cultists 原 Beam owned 后继：`/tmp/mcc-cycle-mask-beam.log`，固定原 `SimulationSnapshot.CycleShapeKey` 与 Mod `CaptureCycleShapeKey(SearchBoundaryReason.None)` 逐位相同。夹具在比较前验证原边界确实为 `None`；另继续对拍无序牌堆、循环牌堆部分、药水键和状态键牌堆片段。
- 预测 API：`/tmp/mcc-cycle-mask-api.log`，64 并行、16 专用分支、协议、源隔离和 Dispose 通过。
- 完整 `scripts/check.sh`：`/tmp/mcc-cycle-mask-full.log`，CTest 10/10；L2 200 局零差异；L3 A0/A10 各 40/40 场、784/784 与 827/827 状态；L4 两组各 40/40 局、1958/1958 与 2131/2131 决策。Oracle 构建有既有一项 CS8604 warning、零 error。
- 隔离实机 `run_smoke.py --mode both --expect-profile-root-rejection --memory-reservation-mib 1792`：`/tmp/mcc-cycle-mask-game.log` 和同名前缀目录，`derived smoke passed`。当前档案 run/player 均 `runs=0, missing_seen_encounters=85`，日志显示 `ENV_UNAVAILABLE_WARNING shown=true`、`env_imported=false env_used=false native_search=continues`，Native 展开 100 节点、335 次转移。这仅验收可见拒绝与 Native 继续，不是实机 Env 接受。

这不是原 `BuildStateKey`、评分或完整政策，不能据此开启 Env 模式、宣称两种后端同算法速度差异或交付最终 `mods/` 成品。
