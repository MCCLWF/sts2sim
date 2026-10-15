# CombatSolver Env：真实根诊断入口与原卡牌指纹

日期：2026-09-27；分支：`feat/combatsolver-env-backend`。上游固定为 `d231e9e51a0e58d6bfa1373c6265cce13ffd9a45`，没有改动子模块。执行依据为[同政策适配计划](../plans/2026-09-26-m5-combatsolver-env-same-policy-adapter.md)的 P0/P3 读取边界；完整原算法执行后端仍未完成。

## 实现

### 游戏诊断使用真实根

派生 Mod 将 `EnvLiveSnapshotProvider` 绑定到主线程 `CaptureLiveSnapshot`。它冻结七个现有组件，完成 profile/房间/字段覆盖门禁，再从 blank template 组装 owned JSON。schema session 只提供运行时怪物注册表绑定，组装后的 gameplay 字段来自真实组件。

`SolveImportedSnapshot` 在 worker 导入这份完整文档，核对结构根，然后调用现有 C++ reference Beam。原生已选路线的诊断重放也改用同一文档的独立 session。导入/预测失败释放所有句柄，显式返回 `UnportedReason` 或 `ErrorReason`；捕获失败不转到种子重建路径。独立 `Sts2EnvCompare` 没有 provider 时仍保留其明确的种子研究入口。

捕获失败的日志使用 `root_source=live_capture env_imported=false`，与实际导入成功的 `live_snapshot_import` 区分。`ShowUnavailable` 在 Label 设置后记录提示证据；Native 继续搜索。该主线程捕获尚未绑定原 coordinator 的同一个请求 root/token，因而不能视为正式同根性能比较。

### 保留原卡牌指纹编码

getter 协议升为 `sts2env.combat_prediction_queries` v2，新增每张卡的 `fingerprint_input` / `fingerprint_input_error`。普通卡牌输入包含升级、重播、临时 Exhaust/Sly/Retain、deck-version/removed 标志及语义动态变量的精确 decimal bits；已有查询提供本地费用和本地关键词。

`EnvSearchCardFingerprintProjection` 直接复用固定 upstream 的 `StateFingerprintBuilder`，按 `CaptureCardStateFingerprintForTesting` 的顺序编码。动态变量使用同一 unordered sum/mix 规则，decimal 四字按有符号 int 编码，保留负数和 scale。它只消费 owned Env 数据，不调用原生卡牌 getter。分支缓存同一份不可变查询与指纹；释放后仍先检查 session 生命周期。历史计数也改为只读集合，避免通过数组强转改动投影。

Claw、GeneticAlgorithm、Maul、MadScience、Rampage、TheScythe 的特殊字段、附件和未分类字符串变量明确标记缺失，指纹投影拒绝整个读取。尤其 Rampage 原字段是 decimal，不能用现有整数 extra slot 冒充。CalculatedVar 家族按原政策排除，但生产 live mapper 仍拒绝这些特殊变量，本轮没有把它们列为已支持。

原 Beam 的开发后继核对现在逐牌堆、逐位置对比原生预测卡牌指纹与 Env 指纹。差异或未映射字段停用该根的 Env 核对，保留 Native 搜索结果；它没有改变候选政策。

## 验证

```bash
scripts/check.sh --quick
~/.dotnet/dotnet build tools/CombatSolverEnv -c Release
~/.dotnet/dotnet run --project tools/EnvLiveCardChecks -c Release
python3 tools/EnvQueryChecks/check.py --out /tmp/mcc-original-card-fingerprints-final
~/.dotnet/dotnet run --project tools/EnvCombatPredictionChecks -c Release -- --out /tmp/mcc-card-fingerprint-branches
~/.dotnet/dotnet run --project tools/EnvSessionChecks -c Release
build/test_combat_prediction /tmp/mcc-card-fingerprint-abi-fixtures
python3 tools/CombatSolverEnv/check_prediction_abi.py /tmp/mcc-card-fingerprint-abi-fixtures
python3 tools/CombatSolverEnv/run_smoke.py --mode both --expect-profile-root-rejection \
  --memory-reservation-mib 2048 --out /tmp/mcc-live-root-game-smoke-final
```

结果：

- Quick：10/10 CTest 通过，smoke 通过；派生程序集 Release 构建 0 warnings / 0 errors。
- TestMode=false 整根夹具通过新增的 imported-root reference 搜索和 Native 计划重放，源 hash 与所提供根保持一致；已有 Strike 和两次完整 EndTurn 的全状态/RNG 对拍继续通过。
- 三组完整 live 根各有 7 张牌，原指纹与 Env 指纹逐卡相同。显式设置升级、`Damage=-3.50m`、临时费用、重播 2 次、Exhaust/Sly/Retain 和 deck-version 引用；同时覆盖 Unplayable、Whirlwind X 与临时星费。第四组独立的 silent Play 牌堆 fixture 验证 getter 文档。静态 C++ 查询另验证 Rampage 缺失标记和 source/sibling purity。
- 10 个原 Beam 后继场景均保留 owned Env 分支并通过新增指纹核对，包括跨回合、额外回合、药水、胜负终局；每个场景仍明确 `original_key_ready=False`。现有比较仍是语义投影，非全状态或完整历史 payload 对拍。
- Managed owned session、并行隔离/恢复/释放，以及 raw C ABI 借用缓冲区、错误清零、终局/选择重放合同通过。
- 隔离游戏的当前部分档案明确拒绝：实际组件已捕获，`env_imported=false`，UI Label 已设置，Native 完成 100 个展开节点、335 次转移，Env 检查转移为 0，场景 Passed 并清理实例。它是加载与拒绝检查，不是 Env 接受或可见游戏性能验收。

中途检查也保留了失败证据：首次 4096 MiB 游戏实例因宿主资源预留满未启动；初版新提示没有旧 `ENV_BRANCH_WARNING` 标记，改为检查实际新 Label 的 `ENV_UNAVAILABLE_WARNING`；尝试 BodySlam 后生产 mapper 在 `CalculationBaseVar` 门禁拒绝，未扩大支持范围。C ABI 的首次命令传错了 fixture 目录，随后用 `test_combat_prediction` 生成专用输入重跑通过。

## 未完成与下一步

`OriginalStateKeyReady=false` 和正式 Env execution gate 保持关闭。普通卡牌指纹只是完整状态键的一部分；仍缺特殊卡牌/附件、orb/战略账本、完整政策事件及风险、Hook 后威胁/评分、原 Replay/choice/round/annotation 的 Env 执行适配、切换时旧请求取消与部署所有权。

当前 reference Beam 的评分、预算和原 CombatSolver 不同，禁止用其耗时宣称同算法加速。最终 `mods` 成品、任意支持 live 根接受、连续回合/选择/药水部署及等预算性能仍待完成，本报告不宣称用户最终目标已达成。
