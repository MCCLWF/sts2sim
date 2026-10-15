# CombatSolverEnv：通用能力事实与 Mod 搜索账本

## 前置状态与边界修正

分支 `feat/combatsolver-env-backend`，基线 `542febc`，固定 CombatSolver `d231e9e51a0e58d6bfa1373c6265cce13ffd9a45`。用户明确要求 Env 保持通用，原搜索复刻在 C# Mod 内。基线已验证个体能力键；完整原 key、策略、评分与正式 Env 执行仍未完成。

撤回上一批尚未提交的 C++ root/retired 槽位实现。`CombatPredictionBoundary` 现在只新增能力 amount/attach/remove 事实，记录实例、owner、CombatId、模型、前后 amount、当时是否挂载；普通 GameState 不包含搜索账本。外层预测协议 v6 和选择重放基线保存这些事实并纳入 prediction hash。旧 v5 明确拒绝；普通游戏 v2 状态/schema 未改变。

C++ `sts2env.combat_power_queries` v1 提供游戏 hook 顺序的原始能力字段、全部动态变量的数值与 calculated/string 类型标志；不选择原 fingerprint 语义字段、不丢弃零 amount、不输出退休集合。字符串表现内容仍未存储，查询仅提供其实际 numeric BaseValue。C ABI/managed API 入口改为 `export_power_queries` / `CapturePowerQueries()`。这是尚未交付版本的协议变更，DLL 与 native library 必须成套构建。

C# `EnvSearchPowerFingerprintProjection` 负责原生命周期字段、零 amount/derived/presentation 筛选、个体键及有序 P 与 unordered r 编码。`EnvSearchPowerLedger` 共享不可变根 owner/model 单实例集合，从每条独立 Env 分支的事实计算退休集合；Fork/Predict 不调用原生效果模拟。重新获得不会擦除退休记录。验证路径调用固定原 `GetPowerFingerprint`，聚合诊断调用原 EffectivePowers / AddCreatureTypeSet。

## 验证

- `cmake --build build -j4`，`dotnet build tools/CombatSolverEnv/CombatSolverEnv.csproj -c Release`：通过；Mod 0 warnings / 0 errors。原始日志 `/tmp/mcc-generic-powers-build.log`、`/tmp/mcc-generic-powers-mod-build.log`。
- `python3 tools/EnvQueryChecks/check.py --out /tmp/mcc-generic-power-queries`：通过；三组 TestMode=false 完整 live 根各 11 个能力，原始游戏字段、精确 decimal 位、原个体/聚合键一致；源与 clone 查询不变。另有 Play 牌堆、卡牌 getter/计算值和增长降级回归。日志 `/tmp/mcc-generic-power-queries.log`。
- `dotnet run --project tools/EnvSessionChecks -c Release`：通过；64 普通并发预测、16 owned combat 分支、恢复、失败、销毁和查询不变性。日志 `/tmp/mcc-generic-power-api.log`；最终 managed 字段校验微调后 Mod 构建与 API 重跑均通过，日志 `/tmp/mcc-generic-power-final-mod-build.log`、`/tmp/mcc-generic-power-final-api.log`。
- `build/test_combat_prediction /tmp/mcc-generic-power-abi-fixtures` 与 `python3 tools/CombatSolverEnv/check_prediction_abi.py --library build/libsts2env_c.so /tmp/mcc-generic-power-abi-fixtures`：通过；严格协议、choice replay、terminal restore、borrowed buffer 与 null-on-failure。日志 `/tmp/mcc-generic-power-abi.log`。
- `dotnet run --project tools/EnvCombatPredictionChecks -c Release -- --out /tmp/mcc-generic-power-beam`：14 场通过；新增 `power_retire` / `power_retire_reacquire` 使用默认 FlexPotion，根 Strength=-5、Dexterity=1，验证归零移除、重新获得后的 P 顺序与持续 r 记录。所有场景要求实际挂载 owned Env 分支且 FailureReason 为空，拒绝不能伪报通过；全状态/全历史 payload 仍不完整。日志 `/tmp/mcc-generic-power-beam.log`。
- `scripts/check.sh`：全部通过；CTest 10/10，L2 200 局 0 mismatched；L3 A0/A10 各 40/40，784/784 与 827/827 states；L4 两组各 40/40，1958/1958 与 2131/2131 decisions。日志 `/tmp/mcc-generic-power-full.log`。
- 非空能力事实的 Discovery choice 克隆/导入/回答重放：通过，回答不重复/丢失已有事实。`test_combat_prediction` 与 raw ABI 在新增选择夹具后重跑通过；构建日志 `/tmp/mcc-generic-power-choice-build.log`。
- `python3 tools/CombatSolverEnv/run_smoke.py --mode both --expect-profile-root-rejection --memory-reservation-mib 2048 --out /tmp/mcc-generic-power-game`：隔离实机加载及清理通过；实际部分档案明确 `env_imported=false/env_used=false` 并显示 UI 提示，Native 继续。这不是自然根接受证明。日志 `/tmp/mcc-generic-power-game.log` 及目录中的 game/launcher evidence。
- `git diff --check`、`python3 tools/gen/gen_state_fields.py --check`：通过；固定 upstream 未修改。

## 已知限制与后续迁移

这一批完成能力字段筛选与退休账本迁移；通用化尚未全部完成。既有 C++ OnPlay 风险映射、card fingerprint 输入筛选、CanPlayCard 策略 probe 与原 terminal calculated-var 查询仍带搜索语义，后续改为事实/通用 getter，由 Mod 编码、计算风险/策略及评分。orb 数值已经来自游戏 getter，其原搜索命名和 completeness 字段仍需清理。通用终局保留/选择重放接口继续提供模拟分支能力。

重复根能力、离场生物的原 listener 顺序等尚未全面覆盖；本批场景通过不构成完整 key/评分证明。`OriginalStateKeyReady=false` 与正式 Env 执行门禁继续关闭。最终 mods 成品、实机 live 根接受、同预算速度对比、连续执行/取消/选择/部署尚未完成；未声称完全替代或性能提升。
