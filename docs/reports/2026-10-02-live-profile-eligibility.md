# 全解锁实机档案误拒绝修复

2026-10-02，Linux / 正版 v0.111.0。前置条件：main 已有实际 profile 字段、完整战斗捕获与本地部署；本任务修正 D-001 支持范围的错误判定。

## 根因与修复

用户报错 `Env: snapshot: only fully unlocked veteran profiles are supported`。当前保存对局的玩家档案实际为57个已揭示时代、46局、75个见过的遭遇，12个 discovery Boss 已全部见过。旧桥接对比 `UnlockState.all._encountersSeen` 的全集，native 也要求它的85位默认掩码。没有见过普通怪或事件战斗变体，被错误当成不满足全解锁老档案。

真值为 `UnlockState.cs` 与 `ActModel.ApplyDiscoveryOrderModifications`：遭遇历史只有各幕的 `BossDiscoveryOrder` 影响 veteran 假设；`Overgrowth`、`Underdocks`、`Hive`、`Glory` 共12个 Boss。全部57个时代及正跑局数的门槛继续保留。桥接列出 `missing_discovery_bosses`，组装器核验来源与实际字段，native 按生成的各幕 Boss 列表检查位图。普通/事件遭遇无需全部见过，其真实位图完整导入，不覆写为测试 sentinel。

此前22,763个捕获比较点及 Godot 测试使用 `UnlockState.all`；它们证明当时限定输入的字段传输，没有覆盖真实已解锁档案。这次 managed fixture 支持 `IRONCLAD --run-save <path>`，直接读取原版对局保存的 `unlock_state`。默认 fixture 和 Godot `ENV-COMPARISON` 都改为57时代、46局、只见过12个 discovery Boss，防止测试 sentinel 再次掩盖误拒绝。

状态字段、schema、版本号、搜索预算与搜索算法不变。

## 验证

原始结果在忽略的 `artifacts/live-profile-fix/`，不提交玩家存档。

- 未修改行为源码的失败基线：managed tool 使用当前对局的实际 `unlock_state`，退出134，复现同一 snapshot 错误，见 `baseline.log`。
- `cmake --build build --target sts2solver test_snapshot -j8` 后，`build/test_snapshot` 通过。仅12个 Boss 的 profile 能精确恢复/继续；零跑局、缺时代、伪造兼容但缺 Boss、player 不支持标志均拒绝。构建曾因仅修改状态声明的注释触发 schema 来源检查而失败，撤销无必要的声明文件改动后通过，未更改 schema。
- `dotnet build tools/CombatSolverModChecks/CombatSolverModChecks.csproj -c Release` 通过；沿用 OracleRun 的一项已有 nullable 警告。
- managed tool `IRONCLAD --run-save <实际对局存档>` 退出0：`PROFILE_INPUT runs=46 epochs=57 seen=75`；导入保留 `seen_low=bf7f7fefdcf7dfd6`、`seen_high=5efddff` 和真实跑局数。开局及实际打出 Strike 后的搜索均得到经过重放验证的胜利，工具断言 live 未变；见 `actual-profile.log`。这不是实际存档整战复盘，而是使用实际档案字段的原版战斗夹具。
- Mod Linux Release 0警告/错误；`tools/verify-refactor-boundaries.sh` 退出0。
- 原生 Godot `ENV-COMPARISON` runId `77c16364c5e843f0a340ed02f45e78b5`，Passed，23.10秒。断言 `Runs=46:Epochs=57:Seen=12`、两份 profile 的见闻位图和跑局数与 live 一致。非零遗物计数、串行双后端、live 不变、英中英切换与取消均通过。启动器成功删除整个隔离实例。完整命令与请求/结果见 `godot.log` 和 `godot/`。
- `scripts/check.sh` 退出0，`all checks passed`：59/59 CTest；L2 200局0分歧；L3 40完整战斗、784/784状态；L4 111局、25,143/25,143决策。修掉新测试常量分支的无效移位编译警告后，目标 snapshot 重新构建并执行通过；行为源码未再更改。
- 本地部署成功：刚完成且验证过的 `CombatSolver.dll`、`libsts2solver.so`、manifest 与两份许可证覆盖已确认的 Linux 游戏 `mods/CombatSolver`，见 `deployment.json`。Mod 子仓库提交 `ce56a6bb` 已 fast-forward 合入main、删除任务分支；Env同批合入main。

## 边界

仅支持57时代、正跑局数及12个 discovery Boss 已见的单人原版老档案。缺少必需 Boss 的首次排序规则仍未开放；未知第三方模型与未知字段仍拒绝。未启动可见 Steam、未改玩家存档、未上传发布渠道。
