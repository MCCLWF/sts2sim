# CombatSolver Env：计算型卡牌变量与原预测查询

日期：2026-09-27。分支：`feat/combatsolver-env-backend`。计划：[M5 计算变量](../plans/2026-09-27-m5-calculated-card-queries.md)。前置提交 `8655f92`，固定 upstream `d231e9e51a0e58d6bfa1373c6265cce13ffd9a45` 无修改。玩法真值为正版 v0.111.0 源码/oracle；搜索政策真值为固定 upstream。

## 结果与交付状态

已移植目前支持内容中 13 个模型的 CalculatedVar 家族公式，并公开 owned 分支查询。三组真实根各 13 个变量、52 个目标值，与正版 getter 和原预测器的计算结果逐 decimal 位相同；原卡牌指纹各 22 张一致。六组真实整根的实际出牌和连续两次 EndTurn 全字段一致。原 Beam 12 个场景的有限语义投影通过，包括新增 BodySlam 终局下的两种查询语义。

完整原状态键、事件/风险、政策/威胁/评分、原算法 Env 执行及游戏内同根等预算性能验收仍未完成。所有完整度标志和正式 Env 执行门禁保持关闭。此次未更新工作区外旧开发包，未将其作为最终 mods 成品。

## 实现与真值

1. 原版 `CalculatedVar.Calculate(target)` 只有在 CombatManager.IsInProgress 且卡牌仍附于战斗时调用 multiplier，随后执行 `CalculationBase.BaseValue + extra.BaseValue * multiplier`。CalculatedDamage 的 extra 是 ExtraDamage，其余为 CalculationExtra。CalculatedVar 自己的 BaseValue 是派生缓存，修改 operands 后不能代替计算。
2. C++ `DynamicVar` 按类型识别并实现集中公式：AshenStrike、BodySlam、Bully、GoldAxe、MindBlast、PerfectedStrike、Rend、Unleash、DemonicShield、ExpectAFight、Barrage、CompileDriver、TearAsunder。未知活动公式明确 Unported。原 `AttackCommand` 的 DynamicVar 重载现在在每次命中、BeforeDamage 后、按实际 singleTarget（多目标为 null）计算，普通变量仍返回 BaseValue。
3. 这些卡牌效果读取同一公式；Thrash 选择计算型伤害时使用正版 `Calculate(null)`。Barrage/TearAsunder 在完整 decimal 加乘后才转整数；DemonicShield 保留先失血再计算及真实 Props。此次没有新增 POD 字段或改变 schema。
4. Live mapper 保存实际 operands/缓存值，检查 exact 变量类型、Props、IsFromOsty、实际 owner、canonical 静态 multiplier delegate 的 Method/Target 身份。改变 delegate 或 owner 的负例明确拒绝并恢复原对象；没有在 native 预先计算未来值后灌入 Env。
5. 独立 `sts2env.combat_calculated_vars` v1 查询覆盖五牌堆每个计算变量及 null/每个 creature ref；每行保留缓存 BaseValue、game_value 和 prediction_value 的四个 decimal 字。C ABI、managed `CaptureCalculatedVars()` 和 `EnvSearchBranch` 都返回 owned 结果，并检查源 hash/私有副本完整状态纯度。
6. 固定原 `CalculatedVarSpecRegistry.TryCalculate` 直接读取预测分支的倍率，终局仍会读取保留状态。C++ 提供独立 `CalculateForCombatPrediction`，没有把游戏的 IsInProgress 条件错误加进原评分读取。BodySlam 原 Beam 终局用例逐位验证 4 个值，确认 game 与 original prediction 结果不同。游戏 inactive getter 通过临时设置真实 manager 标志、调用原 Calculate、finally 恢复进行独立验证；它不是生成 Env 后继的路径。

## 差分范围

- 三组完整 live 根为 baseline/empty_resources/blocked_hook；另保留明确的 native silent Play 牌堆边界夹具。真实 Strike 出牌两次与玩家伤害两次建立 GoldAxe/TearAsunder 的非零历史；不是伪造统计数。基础 operands 为 `-1.2500m` 与 `0.8750m`，保留旧计算缓存；Block=13，Strength 为 3/-3，敌人实际 Vulnerable/Weak 使目标公式非零。
- 实际 OrbCmd.Channel 生成 Lightning 两个、Frost 和 Dark 各一个，Barrage 倍率为 4、CompileDriver 不同 ID 数为 3，全部经过 production inventory capture。Unleash 当前只覆盖缺少 Osty 的零倍率，未验证非零宠物分支、宠物完整 live 捕获或跨回合闭包。
- 六组 full-live 为 Strike、Rampage、Thrash、Thrash+BodySlam、BodySlam、Barrage。Barrage 使用三个真实 FrostOrb 和 operands `-0.2500m`/`1.1250m`，覆盖完整 decimal 运算后截断的命中次数、出牌及后续球触发。全部比较完整 state 文档，没有忽略字段。移除能力引用仍依赖已观察 PowerApplied/PowerRemoved 的固定 roster 夹具，不能泛化为生产历史池完成。
- 原 Beam 12 场保留所有历史条目顺序（包括 Risk）、已覆盖 payload/六计数、风险原因、结构与九流 RNG 对拍。仍不是完整 shadow state 或全部历史 payload/政策风险验收。

## 验收命令与原始结果

日志与 JSON 位于临时目录，不提交原始大体积产物。

| 命令 | 结果 |
|---|---|
| `scripts/check.sh --quick` | `/tmp/mcc-calculated-quick-first.log`：CTest 10/10，整局 smoke 通过 |
| `scripts/check.sh` | `/tmp/mcc-calculated-full.log`：`all checks passed`；L2 200 runs/0 mismatched；L3 两组各 40/40，784/784 与 827/827 states；L4 两组各 40 完整路线，1958/1958 与 2131/2131 decisions，0 Unported/分歧 |
| `~/.dotnet/dotnet build tools/CombatSolverEnv/CombatSolverEnv.csproj -c Release` | `/tmp/mcc-calculated-build.log`：0 Warning/0 Error；查询与隔离游戏工具后续也重新构建派生程序集 |
| `python3 tools/EnvQueryChecks/check.py --out /tmp/mcc-calculated-queries-orbs` | `/tmp/mcc-calculated-queries-orbs.log`：三组各 `cards=13; target_values=52; game_and_original_prediction_exact_bits=true`、22 张原指纹一致；所有 getter、原 CanPlayCard core、静态 C++/公开 managed 查询、源/clone 纯度、Unported、增长降级通过 |
| `~/.dotnet/dotnet run --project tools/EnvLiveCardChecks -c Release` | `/tmp/mcc-calculated-live-orbs.log`：六组整根 seed-free 导入/实际出牌/两次完整 EndTurn 全字段通过，TestMode=false，installed_game_acceptance=false |
| `~/.dotnet/dotnet run --project tools/EnvCombatPredictionChecks -c Release -- --out /tmp/mcc-calculated-beam` | `/tmp/mcc-calculated-beam.log`：12 场通过；BodySlam `values=4; game_and_original_prediction_exact_bits=true; distinct_semantics=true`；full_state_comparison=false、full_history_payload_comparison=false |
| `~/.dotnet/dotnet run --project tools/EnvSessionChecks -c Release` | `/tmp/mcc-calculated-api.log`：64 并行普通预测、16 专用预测分支、计算查询缓存/owned disposal、失败恢复、严格协议/恢复通过 |
| `build/test_combat_prediction /tmp/mcc-calculated-abi-fixtures`、`python3 tools/CombatSolverEnv/check_prediction_abi.py --library build/libsts2env_c.so /tmp/mcc-calculated-abi-fixtures` | C++ 合同与 raw C ABI 通过；新查询源不变、协议/hash、终局/选择 scope、空/诊断句柄错误及独立 owner 均通过 |
| `python3 tools/CombatSolverEnv/run_smoke.py --mode both --expect-profile-root-rejection --memory-reservation-mib 2048 --out /tmp/mcc-calculated-game` | `/tmp/mcc-calculated-game.log`：`derived smoke passed`；game.log 记录 live capture/profile 拒绝、`env_imported=false env_used=false native_search=continues`、UI warning shown=true；原 native_expanded=100、native_transitions=335；隔离实例已清理 |
| `git diff --check`、`python3 tools/gen/gen_state_fields.py --check`、固定子模块状态 | 通过，无 upstream 修改 |

隔离游戏只验证加载、当前部分档案的明确拒绝、UI Label 赋值和 Native 继续，尚非玩家可见切换/Env 接受/完整替换验收。headless 的缺视觉节点日志仍存在，不替代可见游戏流程测试。

## 实施中发现的失败

- 第一次派生构建的 C# pattern 局部名 `actual` 与外层 Props pattern 冲突。改为 actualBinding/expectedBinding 后通过；公式身份检查保留。
- 第一次查询工具缺 PredictedCard 命名空间，且直接使用 InvokeCalculate 会要求额外 Ritsu IComputed 重载程序集。补原 Common 引用，并调用实际固定原 `CalculatedVarSpecRegistry.TryCalculate`；没有重写原计算政策。
- 先前 BodySlam Unported 合同在公式移植后已过时，改用真正未移植 PreciseCut 验证未知公式拒绝与源/兄弟隔离；没有保留错误预期或放宽比较。

## 后续

继续补完整原 state key、其余政策/风险历史和威胁/评分读取，接原 concrete simulator 的 Env Fork/Replay/round/choice 执行路由。非零 Osty、复杂动态变量/附件及一般 live 对象闭包仍须补证据。正式门禁完成后才能验收游戏内切换、取消、选择、药水、部署和同根等预算性能，并生成最终 `mods/CombatSolverEnv` 文件夹。
