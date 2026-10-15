# CombatSolver Env：原能力实例键输入

日期：2026-09-27。分支：`feat/combatsolver-env-backend`。前置提交 `cce9cd6`，固定 upstream `d231e9e51a0e58d6bfa1373c6265cce13ffd9a45` 无修改。计划：[M5 能力键输入](../plans/2026-09-27-m5-power-key-inputs.md)。

## 结果与范围

能力实例只读键输入与实际原 `SimulatedCombatState.GetPowerFingerprint` 编码在三组 TestMode=false 真实根一致，每组 11 个能力；原 Beam 12 场有限后继通过新增个体核对。公开 managed/C ABI 读取不修改源、兄弟、完整 GameplayState 或历史，支持克隆、终局与 choice scope；未知/Unported 明确拒绝。

这不是完整原状态键。未提供 EffectivePowers 的聚合次序、退休 root 槽位以及其余策略/生命周期 maps。`original_state_key_complete=false`、`OriginalStateKeyReady=false`，历史/政策/威胁和正式 Env 执行门禁仍关闭。没有改变候选、评价、剪枝、预算，没有启用原 Beam 的 Env 后端，工作区外旧开发包未更新。

## 真值与实现

- 固定原方法依次编码 owner/applier/target 的 CombatId（null 为 UInt32.MaxValue）、Id.Entry、Amount、语义 AmountOnTurnStart、可选 duration `'d'`、Ritual flag、Surrounded direction，再加入 semantic DynamicVars 的 count 与两路 unordered sum。decimal 四字按原 signed int 编码，复用原 StateFingerprintBuilder/MixFirst/MixSecond。
- AmountOnTurnStart 只对 DrawCardsNextTurn/SummonNextTurn/HelloWorld 有意义；duration skip 只对 Weak/Vulnerable/Frail 有意义。真实根覆盖 DrawCardsNextTurn 非零字段及 Weak true/false，同时 Strength 的非零回合字段和 true skip 必须被原 key 忽略。SummonNextTurn/HelloWorld 未移植，仍不能导入为可用根，本批未提供其真实运行证据。
- Ritual 来源分别指向两个真实敌人；其他实例来源为 null/真实玩家。Surrounded 原枚举为 Right=0/Left=1。Panache CardsLeft 为 `-1.2500m`，Weak/Vulnerable 保留真实 decimal scale；Strength 正/负，Ritual flag 与 Surrounded 两方向都来自 native 对象字段。
- 新 `sts2env.combat_power_key_inputs` v1 查询在私有 C++ clone 读取实际 POD，完整 gameplay hash 前后相同；忽略 amount=0，与原 AppendFingerprint 一致。CalculatedVar 系列按原 SemanticStateFieldPolicy 排除；只排除固定原列明的 presentation StringVar，其他字符串明确 Unported。当前 live mapper 仍拒绝未支持字符串，不能把查询排除规则解释为已完成 live 字符串捕获。
- C ABI `sts2_combat_prediction_export_power_key_inputs` 和公开 `CapturePowerKeyInputs()` 返回 owned JSON，验证版本/hash、非零实例、唯一 ref、nullable 特殊字段、CombatId 范围、变量名与合法 decimal 四字。EnvSearchBranch 缓存分支专有输入/指纹，释放后不可调用。
- 原 Beam 诊断用同程序集 partial wrapper 直接调用原 private encoder，以 model/owner/实际指纹多重集匹配，不改变 upstream 方法。独立真实根工具反射调用同一原方法作为参照，逐 JSON 字段比较 C++ 和 managed 输出，不能用 Env 自己的输出作为期望值。

## 验收

| 命令 | 实际结果 |
|---|---|
| `cmake --build build -j 4` | 首次 `/tmp/mcc-power-key-build.log` 编译失败：PowerModel.extra 为 protected。核对现有 POD 接口后改读 `power.st().extra[]`；`/tmp/mcc-power-key-build-final.log` 所有目标成功，没有玩法/schema 改动 |
| `python3 tools/EnvQueryChecks/check.py --out /tmp/mcc-power-key-queries` | `/tmp/mcc-power-key-queries.log`：三组各 `powers=11; actual_original_encoder=true; full_key=false`；静态 C++/公开 managed JSON、源/hash/clone 纯度与 Unported 拒绝通过；既有 52 计算值、22 卡牌指纹、6 球指纹、getter/政策核心与增长降级通过 |
| `~/.dotnet/dotnet run --project tools/EnvCombatPredictionChecks -c Release -- --out /tmp/mcc-power-key-beam` | `/tmp/mcc-power-key-beam.log`：12 场通过新能力指纹检查，包括出牌、药水、额外回合、poison、终局；仍 `full_state_comparison=false; full_history_payload_comparison=false` |
| `~/.dotnet/dotnet run --project tools/EnvSessionChecks -c Release` | `/tmp/mcc-power-key-api.log`：64 普通并行预测、16 专用分支；新查询 concurrent/clone/source/owned/Dispose 和失败恢复通过 |
| `python3 tools/CombatSolverEnv/check_prediction_abi.py --library build/libsts2env_c.so /tmp/mcc-calculated-abi-fixtures` | 同一日志：raw C ABI 新查询协议/hash/clone、terminal/choice、source purity、diagnostic/null 拒绝通过，复用同 schema 的当前合同 fixture |
| `scripts/check.sh` | `/tmp/mcc-power-key-full.log`：`all checks passed`，CTest 10/10；L2 200/0 mismatched；L3 两组 40/40，784/784 和 827/827 states；L4 两组各 40 完整路线，1958/1958 和 2131/2131 decisions，0 Unported/分歧 |
| `python3 tools/CombatSolverEnv/run_smoke.py --mode both --expect-profile-root-rejection --memory-reservation-mib 2048 --out /tmp/mcc-power-key-game` | `/tmp/mcc-power-key-game.log`：`derived smoke passed`；game.log 显示当前部分 profile 明确拒绝、`env_imported=false/env_used=false`、warning shown=true，Native expanded=100/transitions=335；launcher Passed 且隔离实例已移除。Godot 退出仍有 CanvasItem/ObjectDB 警告，此门禁不证明节点泄漏验收或正式 Env 接受 |
| `git diff --check`、`python3 tools/gen/gen_state_fields.py --check`、子模块状态 | 通过；没有 gameplay POD/schema 和固定 upstream 改动 |

没有测试分歧后改真值或放宽比较。首次编译错误只修复 C++ 访问接口，不调整字段含义或原参照。

## 后续要求

原个体键通过不证明移除/替换能力的完整对象历史、EffectivePowers 次序或 retired root slots 闭包。完整原 key/策略账本/评分、禁用 Native 预测路径的 Env 执行 seam、同根等预算性能、真实 UI 切换/取消/连续回合/选牌/部署和最终 `mods/CombatSolverEnv` 仍须完成。
