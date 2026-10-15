# CombatSolver Env 搜索分支所有权与只读投影

日期：2026-09-27。固定 upstream：`external/CombatSolver` 的 `d231e9e51a0e58d6bfa1373c6265cce13ffd9a45`。

## 已完成

- 原 `CombatBeamSolver` 验证节点的 `SimulationSnapshot.EnvBranch` 改为 `EnvSearchBranch`，内部只持有专用 `EnvCombatPredictionSession`。它不调用普通 run Env 的战后清理/奖励语义。
- 根、兄弟、动作后继、终局和异常路径均有独立 Clone/Dispose；预测前后检查 source hash 不变。
- C ABI 新增专用结构检查：在私有 Env clone 上导出 `prediction_hash`、`state_hash`、decision、terminal 和结构化 combat root。它不改变源分支。
- `EnvSearchProjection` 复制 turn/phase/energy/stars、卡槽、玩家/敌人生命/格挡、九路战斗 RNG、历史事件数量和六项计数。
- `EnvSearchPolicyProjection` 对齐 Env getter 与 `CanPlayCard_core` 探针的卡引用、牌堆位置、费用、`can_play` 和目标合法性。
- `EnvSearchThreatProjection` 对齐 roster 的怪物 entry、move ID、stun/攻击标记、多段次数、状态数量和原始 decimal damage bits。
- 所有投影都是不可变 scalar 副本；状态键、评分和政策 Hook 仍缺字段时固定返回不完整并拒绝正式执行，不使用 Env hash 或空集合冒充原 Beam 键。
- `EnvSearchBranch.RequireExecutionReady()` 现在是执行入口的硬契约：按历史账本、原状态键、候选政策和威胁投影顺序拒绝任何未完成适配。它不影响 Both 模式的只读核对，但防止未来调用者绕过门禁直接运行部分投影。

## 验收命令

```bash
scripts/check.sh --quick
~/.dotnet/dotnet build tools/CombatSolverEnv/CombatSolverEnv.csproj -c Release
~/.dotnet/dotnet run --project tools/EnvCombatPredictionChecks -c Release -- --out /tmp/mcc-env-threat-checks
python3 tools/CombatSolverEnv/check_prediction_abi.py /tmp/mcc-p2-source-fixtures
~/.dotnet/dotnet run --project tools/EnvSessionChecks -c Release
```

结果：quick 检查的 10/10 CTest 和 smoke 通过；CombatSolverEnv 编译无警告；十个 TestMode=false 同根场景均报告 `owned_prediction_branch=true`、`original_key_ready=false`，并完成结构/终局/历史对拍；C ABI 和并行/恢复/释放检查通过。

## 尚未完成

`OriginalStateKeyReady=false` 是有意的硬门禁。原搜索仍缺卡牌动态模型指纹、orb 与战略账本、完整 RiskEntry/生成/选择事件、Hook 后威胁与评分特征、连续回合的完整政策转移，以及最终路线部署。因此设置页的 Env 选项仍不能把原 Beam 切换为 Env 执行，Native/Env/Both 仍是安全的 companion/验证模式；本报告不宣称已经交付可替代原生搜索的最终 mods 包。
