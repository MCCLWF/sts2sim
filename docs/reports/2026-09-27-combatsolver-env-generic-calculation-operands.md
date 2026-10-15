# 通用计算公式输入与 Mod 终局计算政策

日期：2026-09-27。分支 `feat/combatsolver-env-backend`，前置 `63b3a53`。正版游戏源码 `../Slay the Spire 2/src/Core` 为 v0.111.0，固定原 CombatSolver 子模块为 `d231e9e51a0e58d6bfa1373c6265cce13ffd9a45`；本次未修改子模块。

## 边界变化

正版 `CalculatedVar.Calculate` 只在战斗进行且牌仍属于战斗时调用静态公式，否则乘数为零。通用 C++ `DynamicVar::Calculate` 维持这一条件；另有 `RawCalculationMultiplier` 提供公式在 owned 分支状态上的原始乘数，无搜索政策开关。已删除 C++ 的 `CalculateForCombatPrediction` 布尔政策分支。

`sts2env.combat_calculated_vars` 查询升为 v2。每个已审计计算变量输出正版 `game_value`、CalculationBase/Extra 的 decimal bits 和每个目标的 `raw_multiplier`；不再输出 `prediction_value`、原预测器语义标识或政策完整度。查询只在私有副本上运行；源 hash 与全部游戏状态不变。遇到未移植的计算公式时拒绝查询。

C# 派生 Mod 的 `EnvCombatCalculatedVars` 对这 13 个已审计模型采用固定原 `CalculatedVarSpecRegistry` 的 `Base + Extra * multiplier` 规则，在终局继续读取保留分支输入。公开 `GetValue(..., Game / OriginalPrediction)` 双语义保留，但 `OriginalPrediction` 由 Mod 计算，不从 C++ 接收搜索结果。未知模型或 v1/混入旧搜索字段的文档拒绝。

## 验收记录

| 命令 | 结果 |
|---|---|
| `cmake --build build -j 4` | `/tmp/mcc-generic-calculation-build.log`，构建通过 |
| `~/.dotnet/dotnet build tools/CombatSolverEnv/CombatSolverEnv.csproj -c Release` | `/tmp/mcc-generic-calculation-mod-build.log`，0 warning、0 error |
| `python3 tools/EnvQueryChecks/check.py --out /tmp/mcc-generic-calculation-queries` | `/tmp/mcc-generic-calculation-queries.log`：三组 TestMode=false 完整 live 根，各 13 计算变量、52 个 target 值；v2 文档中游戏值/原公式乘数逐 decimal 位与正版相同，Mod 原值逐位等于固定原 `CalculatedVarSpecRegistry`；源/Clone 纯度与未移植拒绝通过。另有 Play 牌堆边界夹具。 |
| `~/.dotnet/dotnet run --project tools/EnvCombatPredictionChecks -c Release -- --out /tmp/mcc-generic-calculation-beam` | `/tmp/mcc-generic-calculation-beam.log`：16 个原 Beam 场景实际 owned Env 分支通过，BodySlam 终局 4 个值的 game/Mod 原语义不同且各自逐位对齐。仅为有限语义投影。 |
| `build/test_combat_prediction /tmp/mcc-generic-calculation-abi-fixtures`，随后 `python3 tools/CombatSolverEnv/check_prediction_abi.py /tmp/mcc-generic-calculation-abi-fixtures` | `/tmp/mcc-generic-calculation-abi.log`：ABI、终局、选择重播、owned 句柄通过，v2 仅 game 语义。 |
| `~/.dotnet/dotnet run --project tools/EnvSessionChecks -c Release` | `/tmp/mcc-generic-calculation-api.log`：64 并行预测、16 专用分支、源隔离、失败恢复、Dispose、协议门禁通过。 |
| `scripts/check.sh` | `/tmp/mcc-generic-calculation-full.log`：完整结果见下。 |
| `python3 tools/CombatSolverEnv/run_smoke.py --mode both --expect-profile-root-rejection --memory-reservation-mib 1792 --out /tmp/mcc-generic-calculation-game` | `/tmp/mcc-generic-calculation-game.log`：derived smoke passed；完整证据在同名前缀目录。 |

完整回归的原始结尾：CTest 10/10；L2 200 runs、0 mismatch；L3 A0/A10 各 40/40 combat、784/784 与 827/827 状态；L4 两组各 40/40 run、1958/1958 与 2131/2131 决策。Oracle 构建含既有 1 项 CS8604 warning、0 errors；日志结尾为 `all checks passed`。

隔离实机使用最新版 DLL 和 `libsts2env_c.so` 已通过加载/运行；当前实际档案 run/player 均 `runs=0, missing_seen_encounters=85`，触发可见 `ENV_UNAVAILABLE_WARNING shown=true`。日志同时记录 `env_imported=false env_used=false native_search=continues`，Native 扩展 100 节点、335 次转移后正常结束。这验证拒绝与回退，尚未证明实机 Env 执行或两种模式的等预算性能。

## 尚未完成

本次没有新增实机 Env 接受结果。原完整 state key、其他策略/风险历史和评分尚未完成，正式同算法 Env 执行与等预算 Native/Env 性能比较仍未开启。游戏 v0.111.0 之外的公式、附魔/污染和原 `CalculatedVarSpecRegistry` 更广泛支持列表未在本查询覆盖；遇到未知内容继续明确拒绝。最终放入 mods 的双后端成品仍待验收。
