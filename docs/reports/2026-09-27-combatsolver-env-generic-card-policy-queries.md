# 通用卡牌查询与 Mod 可打出政策

分支 `feat/combatsolver-env-backend`，前置 `b58f6a6`。固定原搜索为 `external/CombatSolver` 的 `d231e9e51a0e58d6bfa1373c6265cce13ffd9a45`，未修改子模块。目标接续用户要求的职责边界：Env 保留游戏实现，搜索专用规则在 C# Mod。

## 修改

- 游戏 getter 协议升为 v4：`card_state` 返回真实 upgrade/replay/retain/sly/deck/removed 状态、附魔/污染身份、全部动态变量的 `is_calculated` / `is_string` 和 BaseValue decimal bits，以及已实现的 Rampage/Thrash 增长值。C++ 不再输出 `fingerprint_input` 或搜索覆盖错误，也不按 SemanticStateFieldPolicy 筛掉计算变量。
- `EnvSearchCardFingerprintProjection` 在 Mod 中保持原编码和 unordered variable mix；排除原 Derived 分类，拒绝未分类字符串、附件和缺少原特殊实例值的 Claw/GeneticAlgorithm/Maul/MadScience/TheScythe。Rampage 使用真实增长值；原键没有 Thrash 特殊 case，因此不会额外加入它的增长值。移除 C++ 筛选不代表这些缺项已支持。
- 删除 `ExportCombatPredictionPolicyProbe` 和 `sts2_combat_prediction_export_policy_probe`，包括托管 P/Invoke。公共 `CapturePolicyProbe` 改由 Mod 的 `EnvCardPlayPolicy` 从 owned game queries 计算，输出 `combatsolver.env_policy_probe`。Enthralled 优先拒绝、Unplayable 零返回费用、X 使用持有者资源、能量转星数和 Smog 拒绝均在 C#；Hook/card logic 读取游戏原拒绝原因位，AnyAlly 留给目标验证，与固定原核心规则一致。没有调用原生未来效果。
- getter 不再含 C++ 的原政策/威胁完整度字段；Mod 的公共完整度仍为 false。球查询 v2 仅返回 `query_semantics=game`、实际队列与 getter 值，删除 C++ 的原状态键完整度字段和 AppendOrbs 语义标识。原球键编码仍由 Mod 完成。

真值核对：游戏 `CardModel.CanPlay` 会独立累积 Hook/card logic 的拒绝原因，即使资源或同伴检查失败；原 `CombatPredictionSimulator.CanPlay` / `SimulatedCombatState.CanPlayCard` 分别定义未转换费用、Enthralled 和 Smog 政策。原六类特殊字段见 `CombatBeamSolver.StateEvaluation.cs` 的 `GetCardFingerprint`；当前卡牌内容无 StringVar，未知 StringVar 仍按原字段政策拒绝。Luna（xhigh）完成只读清单和拒绝测试，我复核实现与结果。

## 验收

| 命令 | 记录 |
|---|---|
| `cmake --build build -j 4` | `/tmp/mcc-generic-card-policy-build.log` |
| `~/.dotnet/dotnet build tools/CombatSolverEnv/CombatSolverEnv.csproj -c Release` | `/tmp/mcc-generic-card-policy-mod-build.log`，0 warnings / 0 errors |
| `python3 tools/EnvQueryChecks/check.py --out /tmp/mcc-generic-card-policy-queries` | `/tmp/mcc-generic-card-policy-queries.log` |
| `~/.dotnet/dotnet run --project tools/EnvSessionChecks -c Release` | `/tmp/mcc-generic-card-policy-api.log` |
| `build/test_combat_prediction /tmp/mcc-generic-card-policy-abi-fixtures`，随后 `python3 tools/CombatSolverEnv/check_prediction_abi.py /tmp/mcc-generic-card-policy-abi-fixtures` | `/tmp/mcc-generic-card-policy-abi.log` |
| `~/.dotnet/dotnet run --project tools/EnvCombatPredictionChecks -c Release -- --out /tmp/mcc-generic-card-policy-beam` | `/tmp/mcc-generic-card-policy-beam.log` |
| `scripts/check.sh` | `/tmp/mcc-generic-card-policy-full.log` |

已通过：三组真实完整 live 根与一组明确的 Play 牌堆夹具，TestMode=false。全 getter 文档由游戏对象直接读取，和 C++ 静态实现逐字段比较；每组原卡牌指纹 22 张、原 CanPlayCard/返回费用逐卡一致。13 个计算变量/52 个目标值、6 个球/容量 8、11 个能力的既有核对仍通过。基线另外检查 Mod 对 STEADY、SMOG、CLAW 和未分类字符串的四种投影拒绝；这些是 owned 输入的拒绝测试，不是特殊 live 内容接受测试。C++ 附件查询保留真实身份并保持源不变。

原 Beam 16 场诊断均 `observed=True owned_prediction_branch=True reason=none`，全部已有卡牌/球/能力指纹核对保留。API 的 64 并行预测与专用 16 分支通过，新增 Mod policy 的 Clone/并发等值检查；Dispose 后读取明确失败。C ABI 明确断言原 policy 符号不存在，并验证 v4/v2 与所有终局/选择范围查询、源隔离和错误恢复。

完整 `scripts/check.sh` 通过：CTest 10/10，L2 200 runs / 0 mismatch；L3 A0/A10 各 40/40 combats、784/784 与 827/827 states；L4 两组各 40/40 runs、1958/1958 与 2131/2131 decisions。Oracle 构建保留既有一项 CS8604 warning，无错误；最终 `all checks passed`。未放宽游戏或原预测器期望。

## 限制与后续

此阶段未增加实机 Env 接受证明；上一阶段实际游戏进程加载与档案拒绝已有报告，不能替代 Env 执行验收。特殊子类实例字段、附件、原政策 Hook 风险和全历史/策略账本仍不完整。

`CalculateForCombatPrediction` 与计算查询的原终局保留状态语义仍在 C++，下一步需改为通用公式事实并由 Mod 派生原政策。原完整 state key/评分/同算法 Env 执行与等预算性能、连续回合和最终 mods 成品验收仍未完成，正式门禁保持关闭。
