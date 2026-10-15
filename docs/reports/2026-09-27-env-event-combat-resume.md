# 通用 Env 事件战斗返回与 Battleworn Dummy：执行报告

日期：2026-09-27。分支：`feat/generic-env-rf-api`。游戏真值：v0.111.0 `src/Core` 与直接运行游戏 DLL 的 `Sts2Oracle`。计划：[事件战斗续接](../plans/2026-09-27-env-event-combat-resume.md)。

## 前置状态

原严格三幕路线 `SEED0004` / policy 1 / 铁甲战士 / `--no-neow --hp 10000` 在 814/1123 决策后因 `Events.BattlewornDummy.GenerateInitialOptions` 停止。旧 C++ Beam/预测 ABI 已归档，但 CombatSolver managed 派生工具仍处于活动 `tools/` 下，并被 MSBuild 目标拒绝构建。

## 改动和原版依据

- 旧 CombatSolver managed 适配器、专用对拍和双求解器脚本移到 `archive/combatsolver/managed-tools/`；归档目录的 `Directory.Build.targets` 继续明确拒绝构建。活动 `tools/` 不再保留这些私有项目。`env/game_queries.cpp` 的诊断文字去掉旧 “combat prediction” 称呼；原版 getter、owned 查询和事实字段仍属通用 API，未作无证据的规则回退。
- 事件战斗记录 `should_resume_parent_event_after_combat`，战斗结束按原版 `NCombatUi.OnCombatWon` 先检查 `Encounter.ShouldGiveRewards`。需要恢复父事件时，先退出子战斗房间，再调用 `EventModel.Resume`；退出房间值保留遭遇 ID、超时状态和额外奖励。`CombatData` 新字段进入完整 POD 快照/Hash/恢复。
- Battleworn Dummy 实现三档选择。其怪物的时间能力在逃跑前记录 `RanOutOfTime`；恢复时超时进入失败页，胜利的 V1 用玩家 Rewards RNG 选药水，V2 用事件 RNG 稳定洗牌并升级两张可升级牌，V3 从遗物袋取奖励；原版顺序是先结束事件、再计算奖励。
- Headless oracle 原来对所有结束的战斗都直接调用 `CombatRoom.OfferRoomEndRewards`，违背原版 `NCombatUi.OnCombatWon` 对 `ShouldGiveRewards=false` 的跳过分支，导致 Battleworn 虚构普通卡牌奖励。现在该分支调用游戏自己的 `RunManager.ProceedFromTerminalRewardsScreen`，让父事件实际恢复。修改仅修正测试驱动的流程；奖励、事件和战斗规则仍由正版 DLL 执行。

## 验证结果

| 命令/检查 | 原始结果 |
|---|---|
| `bash -n scripts/check.sh`；`python3 tools/gen/gen_state_fields.py --check` | 通过 |
| `scripts/check.sh` | 全部通过；CTest 14/14；L2 200局零分歧；L3 80战、1611状态零分歧；默认 L4 80局、4089决策零分歧；聚焦遗物 428决策；严格二幕 3246决策；新增严格三幕胜利 1138决策；Pael's Wing 献祭 219决策 |
| `DOTNET=~/.dotnet/dotnet python3 tests/l4/l4_run.py --seeds SEED0004 --policy-seeds 1 --no-neow --hp 10000 --stop victory --verify-snapshots --require-complete` | 1局完整胜利，1138/1138 决策一致；每个决策恢复后后继也一致；0 `Unported`、0 分歧 |
| `ctest --test-dir build -R '^battleworn_timeout$' --output-on-failure` | V1/V2/V3 的非参与回合、两次递减、超时先置位再逃跑及快照往返通过 |
| 显式 `dotnet build archive/combatsolver/managed-tools/CombatSolverEnv/CombatSolverEnv.csproj -c Release` | 按预期被归档目录的 `Directory.Build.targets` 拒绝 |
| `nm -D build/libsts2env_c.so` 与活动目录检索 | 29 个 `sts2_env_` 导出；无旧 `sts2_combat_prediction_*`/Beam 导出；活动 `tools/`、`env/`、`engine/`、`game/` 无 CombatSolver 私有代码引用 |

扩大到 `SEED0002,SEED0003`、policy 1/2、相同高生命值三幕胜利目标：4局中2局完整一致，2局在 `Events.Vakuu.CalculateVars` 明确停止；已对齐 2509/3520 决策，0 分歧。该未移植先古事件是下一处真实 run 层阻塞。

## 限制

上述三幕胜利使用 `--hp 10000` 测试辅助；它证明这一条原版操作路径和快照恢复一致，不证明自然 HP 的获胜率、全部铁甲战士可达内容或五角色完整 Env。Battleworn V2 超时分支经过真实路线；V1/V3 胜利奖励和 V2 胜利升级仍需独立原版差分。Random Foreseer 的类型化局外操作、完整结果追踪、真实 live 捕获和独立 SDK 仍未达到完成门槛。CombatSolver 保持冻结。
