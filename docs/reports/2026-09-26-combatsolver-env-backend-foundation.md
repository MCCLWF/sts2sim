# CombatSolver Env 后端：预测边界与健壮性核查

日期：2026-09-26  
分支：`feat/combatsolver-env-backend`  
基线：`0479913`（正式遭遇 90/90）；CombatSolver 子模块 `d231e9e5`。

## 本次实现

- C ABI 增加当前根只读检查与纯分支预测。预测检查动作合法性、复制 Env、执行一步；失败不返回后继，也不改动源根。`Unported` 单独返回状态码 `1`，普通错误为 `-1`。
- Python `Env.predict()` 返回独立后继，`inspect_current()` 读取当前状态；未移植内容由 `UnportedError` 明确传递。合法动作数量接口仍按非负数量处理，不混用状态码。
- 开发版双搜索 Mod 遇到 `Unported` 时输出 `ENV_UNPORTED` 并停止 Env 比较；界面说明 Env 未使用。Env 现在使用确定性的深度限制 Beam 后端，和 CombatSolver 共享 Beam 的展开/保留形状，但评价、候选集合、终局处理和预算单位仍未完全复用，因此仍保留两边原始毫秒数、工作量和首动作，不给出速度倍数。
- Mod 支持 `STS2ENV_COMPARE_MODE=env|native|both`（默认 `both`），以及 `STS2ENV_COMPARE_BEAM_WIDTH`；`native` 不启动 Env，`env` 不等待 native 结果，`both` 才输出双搜索行。
- 根预检新增四个牌堆的牌实例摘要：升级等级、局部费用、附魔与苦难 ID/数量。字段不同则不启动 Env 搜索。卡牌 UID、动态变量与其他内部状态仍未覆盖。

## 健壮性核查

1. `CombatRootSnapshot` 在游戏主线程捕获原生预测根；比较 Mod 原本只检查卡牌 ID 顺序和部分人物/敌人标量，本轮增加了部分牌实例字段。卡牌 UID、动态变量、内部计数、怪物状态机、RNG 和历史仍未导入 C++。结构匹配不能证明同根；后续回合通常被根预检拒绝。
2. 当前 C++ Env 的 `ReferenceBeam` 已改为 Beam 形状的确定性搜索，但与 CombatSolver `CombatBeamSolver` 的候选生成、评价、终局排序和预算仍不同。原始时长是两种任务的观察值，不能当成同算法的性能结论；完整策略复用仍待 CombatSolver 侧适配。
3. C++ 状态通过 POD 克隆，Env 的选择点靠前缀重放；失败后的原 Env 可能处于半完成状态。因此外部预测必须在分支上执行并丢弃失败分支，新接口对此提供了明确所有权边界。
4. Mod 通过反射读取 CombatSolver 的内部完成结果，依赖固定子模块版本；仍须在未来的 CombatSolver 派生 Mod 中建立稳定的搜索后端和结果接口。当前 Mod 只读结果，不部署 Env 动作。

## 本次验证

| 命令 | 结果 |
|---|---|
| `cmake --build build --target sts2env_c -j 4` | 通过 |
| `STS2SIM_ENV_LIB=$PWD/build/libsts2env_c.so python3 python/test_env.py` | 通过；预测与 clone+step 后继哈希一致，源哈希不变，非法动作被拒绝 |
| `dotnet build tools/Sts2EnvCompare/Sts2EnvCompare.csproj -c Release --no-restore -v quiet` | 通过；0 警告、0 错误 |
| `ctest --test-dir build --output-on-failure` | 8/8 通过 |
| `scripts/check.sh` | 通过；冒烟三组：`death=94 unported=6`、`death=100`、`boss=3 death=97`；L2 200/200 局零分歧；L3 A0/A10 各 40/40 战斗、784/784 与 827/827 状态一致；L4 两组各 40/40 局、1958/1958 与 2131/2131 决策一致。Oracle 构建有 1 条 CS8604 警告 |

本次尚未在游戏内触发 `ENV_UNPORTED` 分支。也没有覆盖 C ABI 未移植返回码的动态样例，动态测试覆盖的是成功与非法动作。完整根导入、CombatSolver 侧完整 Beam 策略复用、同预算性能和跨回合 Env 路线部署仍未完成；当前先验证当前回合的计划动作安全桥接；按[计划](../plans/2026-09-26-m5-combatsolver-env-backend.md)继续。

## 牌实例预检追加

追加字段后，`cmake --build build --target sts2env_c -j 4`、Python 预测检查和 `dotnet build tools/Sts2EnvCompare/Sts2EnvCompare.csproj -c Release --no-restore -v quiet` 均通过，C# 构建为 0 警告、0 错误。已将 Mod 构建产物复制到本机游戏的 `mods/Sts2EnvCompare/`，供隔离实例取快照。

第一次隔离启动因旧实例 `mcc-envcompare-20260926` 占用独占资源，排队 120 秒后以 `host reservations full` 超时；随后确认旧实例属于同一工作树、已脱离测试启动器，使用项目 `--stop-instance` 按 PID/出生身份停止。`MCC-DUAL-CARDSTATE` 和保留日志的 `MCC-DUAL-CARDSTATE-LOG` 均在隔离正版游戏中 `Passed`。后者日志显示新增牌实例字段参与预检后 `structural_root_match=True`、`live_unchanged=True`，Env 与 CombatSolver 都完成搜索：

```text
[Sts2EnvCompare] ENV turn=1 encounter=CULTISTS_NORMAL structural_root_match=True root_mismatch_fields= live_unchanged=True iterations=100 depth=12 elapsed_ms=78.754 action=PlayCard:BASH:target=1
[Sts2EnvCompare] TWO_SOLVERS turn=1 encounter=CULTISTS_NORMAL env_ms=78.754 native_ms=308.450 env_rollouts=100 native_expanded=3 env_action=PlayCard:BASH:target=1 native_action=PlayCard:STRIKE_IRONCLAD:target=1 action_agree=False root_check=structural_only algorithms=different budgets=not_equivalent
```

这只证明该首回合样例的新增结构字段一致和两个现有搜索运行，仍不能证明完整根一致或等算法性能。测试实例结束后已按所有权停止。追加字段后的 `scripts/check.sh` 未重跑；前述完整门禁属于追加前的 C++ 版本，其差分验证范围不包含新增根输出。

## 根覆盖检查追加

`Env::ValidateCombatRoot()` 检查当前战斗根里已存在的遭遇、牌（含附魔/苦难）、遗物、药水、怪物、能力与充能球是否满足生成的 `IsUsable` 表。C ABI、Python 和 Mod 均开放调用；参考搜索入口也在搜索前检查。缺失返回 `Unported`，Mod 按现有 `ENV_UNPORTED` 分支明确不使用 Env。未来生成的内容仍由转移时的 `Unported` 异常保护，根检查不能证明所有未来路径可用。

验证：`cmake --build build --target sts2env_c test_env -j 4` 与 `ctest --test-dir build -R 'env_m3|env_python' --output-on-failure` 通过。`test_env` 在正常根检查成功后，把手牌中一张牌替为已知未移植的 `SPLASH`，确认抛出 `Unported`；Python 正常根检查也通过。C# Release 构建 0 警告、0 错误。隔离正版游戏场景 `MCC-DUAL-COVERAGE-GATE` 为 `Passed`，日志再次记录 `ENV` 和 `TWO_SOLVERS`，表明新根覆盖 C ABI 被 Mod 调用且正常根可继续搜索。该场景未在游戏内构造未移植根，因此 `ENV_UNPORTED` 的界面分支仍待游戏内验证。测试实例已停止。

## Beam 与模式验证追加

新增 `ReferenceBeam`、C ABI `sts2_env_solve_beam` 及 Python `Env.solve_beam()`。Beam 节点持有 Env 分支，使用纯 `Predict` 生成后继，按稳定排序保留固定宽度并遵守节点预算；验证了重复调用确定、首动作合法、源根哈希不变。Env 克隆现在共享只读决策快照，独立状态仍深拷贝；本地微基准的克隆/销毁均值约 2.6 µs（`GameState` 约 140 KiB）。

隔离正版游戏测试均为 `Passed`：

- `MCC-DUAL-BEAM-20260926`：默认 `both`，Env Beam 与 CombatSolver 同场运行，战斗在第 1 回合结束。
- `MCC-DUAL-BEAM-ENV-20260926`：`STS2ENV_COMPARE_MODE=env`、节点预算 32、宽度 4，Env 专用模式完成同一场景。
- `MCC-DUAL-BEAM-NATIVE-20260926`：`STS2ENV_COMPARE_MODE=native`，跳过 Env 调用并由原生 CombatSolver 完成同一场景。
- `MCC-DUAL-ENV-DEPLOY-20260926`：Env 模式安全暂停原生搜索，调用游戏原生 `TryManualPlay`/回合结束动作执行 Env 首动作，再恢复原生搜索；场景完成且启动器结果为 `Passed`。
- `MCC-DUAL-UNPORTED-20260926`：自定义手牌包含 `SPLASH`，`both` 模式场景完成且结果为 `Passed`；Env 根覆盖会在该内容前停止，原生路径继续完成战斗。
- `MCC-DUAL-FUTURE-SESSION-20260926`：Mod 使用持久 Env session 对首动作执行一次分支预测，确认源分支哈希保持不变、后继状态可读取。

六个实例都按启动器的 `--cleanup-instance-on-exit` 清理。测试结果证明 Mod 加载了新的 Beam ABI、模式配置、首动作桥接、可持续预测 session 和未移植保护；由于启动器清理日志目录，未把游戏日志作为持久化证据。仍需在完整 live 根适配后验证后续回合、选择和完整 Env 路线部署；当前后续状态交回原生。

## Beam 计划与逐步部署追加

`ReferenceBeam::ChoosePath` 现在保留完整 Beam 路径；C ABI、Python 和 bench JSON 同时返回首动作与 `plan`。`env` 模式持有同一 Env session，逐步执行当前回合的 `PlayCard`/`EndTurn`，每一步都比较 live root，再用 `Session.Predict` 生成下一分支。根字段不一致、预测失败或游戏动作拒绝会停止剩余计划；外层 finally 释放 session 并恢复 CombatSolver。

验证命令：C++ 全部 8 项 CTest、`python/test_env.py`、`scripts/check.sh --quick` 均通过；C# Release 构建 0 警告、0 错误。隔离正版游戏场景 `MCC-DUAL-ENV-PLAN-20260926B`（`STS2ENV_COMPARE_MODE=env`、节点预算 16、宽度 4、深度 3、`--cleanup-instance-on-exit`）结果为 `Passed`，第 1 回合完成。该场景只证明当前简单遭遇的计划部署路径；跨回合、选择、完整 live 根和完整 CombatSolver 策略复用仍未完成。

另加了 native route 验证：Mod 通过反射读取 CombatSolver 的当前 `BestNode.Actions`，保留卡牌 occurrence、目标 combat ID、药水槽位和结束回合动作，在 `both` 模式把当前回合路线逐步送入 Env session 做纯预测。隔离场景 `MCC-DUAL-NATIVE-PLAN-20260926` 结果为 `Passed`；这条链路验证 native 策略产生的动作能否被 Env 逐步接受，并在 `native_plan_env_check` 记录失败原因。它仍不是完整状态导入或同策略搜索：Env 只作为转移 oracle，当前 Env 搜索仍是参考 Beam。

## native 牌实例寻址与派生 Mod 追加

保留日志的 `MCC-DUAL-NATIVE-PLAN-LOG-20260926` 揭示此前仅有场景 `Passed` 并不能证明路线检查通过：实际日志为 `native_plan_env_check=False`，第二张 Strike 的 ID occurrence 寻址失败。CombatSolver 优先使用 `CardStateKey`/`CardStateOccurrence`，桥接此前未遵守这个优先级。已改为按原生状态键投影到现有结构摘要进行寻址；带未映射选择的动作拒绝，药水必须同时匹配槽位及 ID。修复后 `MCC-DUAL-NATIVE-STATE-20260926` 的持久日志显示 `native_plan_actions=2 native_plan_env_check=True native_plan_reason=ok`。该检查证明动作接受与源不变，尚不证明原生/Env 后继完整相等；状态键投影仍只包含已核对的结构字段。

新增 `tools/CombatSolverEnv/CombatSolverEnv.csproj` 直接编译固定子模块的 `src/**/*.cs`。原生初始化器仅修改 Mod ID，其余搜索、预算、评分和部署源码原样链接；Env 桥接编入同一程序集，反射优先找到本程序集的控制器。Release 构建 0 警告、0 错误。`python3 tools/CombatSolverEnv/run_smoke.py --out /tmp/mcc-derived-smoke-20260926` 创建只含派生版的临时游戏视图，并使用上游启动器的清理约定完成 `MCC-DERIVED-SMOKE`：场景 `Passed`、两个初始化器均来自 `CombatSolverEnv`，`TWO_SOLVERS` 显示 native 270.528 ms、Env 3.002 ms、两步 native 路线重放成功。临时源视图和正式隔离实例都已清理，持久证据留在上述输出目录。最初验证器错误地要求在 Godot 日志中找到单独原生 logger 的 `ENGINE` 行，随后改为检查派生初始化器、场景结果和实际路线检查；仅重新验证保存证据，没有重复启动游戏。

纯预测入口现在对战斗源根与战斗后继都执行覆盖检查；C ABI 调用同一 `Env::Predict`，避免两套转移边界漂移。参考 ISMCTS 也改用纯预测，并把 `Unported` 向上抛出，不能把未移植失败折算成低分候选。单测验证含未移植牌的根即使选择结束回合也会拒绝，源哈希保持不变。

派生版的 native 路径已复用原搜索源码，但 Env 路径仍调用参考 Beam；完整 live 导入、后继对拍和原生搜索的 Env 状态后端仍是未完成项。这组简单场景的原始耗时不代表同算法性能。
