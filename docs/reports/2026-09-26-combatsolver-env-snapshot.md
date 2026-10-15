# 完整 Env 状态导出与战斗 RNG 分支核对

日期：2026-09-26；分支：`feat/combatsolver-env-backend`。
计划：`docs/plans/2026-09-26-m5-combatsolver-env-backend.md`。

## 前置条件与完成范围

既有预测/session 接口、逻辑字段哈希、分支结构核对及独立派生 Mod 已存在。本轮新增的是完整 **Env 逻辑状态导出** 和九条战斗随机流的 native 分支核对。尚未实现完整 live 输入、原算法的独立 Env 执行后端或最终设置 UI，最终安装包验收仍未完成。

对照 v0.111.0 `RunRngSet.cs`/`RunRngType.cs` 确认九条战斗流与 C++ 数组位置；原生 `CombatPredictionRngSet` 的 `*State` getter 读取不可变冻结值或已拥有分支 RNG，不会为了核对物化 lazy 流。`NetFullCombatState` 不能替代完整 live 根：它不包含全部模型内部变量、怪物 FSM 和战斗历史，且去掉了部分非战斗 RNG/奖励概率。

## 实现

- `env/snapshot.cpp`：通过完整生成的命名字段 visitor 导出 `GameState`，覆盖 run、combat、player_combat 及所有对象池；不输出 padding、未使用的 vector 尾部、已释放池对象值。
- 池槽号、分配游标、HashSet count/free 链保留；枚举以数值输出。decimal 输出四个原始 uint32，float/double 输出 IEEE 位模式；RNG 输出 counter 和四个完整 ulong，避免经 double 丢失低位。
- 挂起选择输出当前部分执行状态、原始 replay_base、pending 动作、已有答案与完整 ChoiceRequest。非选择节点的过期 base 不参与后续执行，不输出；已有答案仍保留，与当前 Env Hash 合同一致。
- 协议 `sts2env.logical_state` 版本 1；生成门禁输出相关声明头的 SHA-256，包括手写容器、RNG、decimal 与外层选择状态头。另带 model ID hash 及有序随机流名。schema 摘要用于未来映射审计，当前没有 live mapper 的 schema 允许列表。
- C ABI `sts2_env_export_snapshot` 通过既有 last_result 返回 JSON。Session 在同一 `_gate` 下调用并复制结果；`CaptureSnapshot` 拥有独立 JSON，源释放后可读取。
- 九条战斗流逐一核对 counter/s0/s1/s2/s3；失败报告具体边界、流名与差异字。结构不匹配仍先拒绝。日志范围明确为 `structural_and_combat_rng`，不能声称覆盖全部状态。
- 没有提供任意 JSON 导入入口：对象引用、模型 ID、全局怪物 FSM/回调索引仍需完整适配与验证。当前注册号只属于已加载 native runtime，不是跨进程可携带的存档。

## 验收命令与结果

```bash
python3 tools/gen/gen_state_fields.py --check
scripts/check.sh --quick > /tmp/mcc-env-snapshot-final-quick-20260926.log 2>&1
build/test_snapshot /tmp/mcc-env-snapshot-fixtures-20260926
~/.dotnet/dotnet run --project tools/EnvSessionChecks -c Release -- /tmp/mcc-env-snapshot-fixtures-20260926
~/.dotnet/dotnet build tools/CombatSolverEnv/CombatSolverEnv.csproj -c Release --nologo
python3 tools/CombatSolverEnv/run_smoke.py --mode both --verify-branches --memory-reservation-mib 2048 --out /tmp/mcc-env-rng-branches-20260926
python3 tools/CombatSolverEnv/run_smoke.py --mode both --expect-branch-rejection --memory-reservation-mib 2048 --out /tmp/mcc-env-rng-rejection-20260926
```

实际结果：

```text
100% tests passed, 0 tests failed out of 9
smoke: death=94 unported=6
smoke: death=100
smoke: boss=3 death=97
quick check passed (differential checks skipped)
Complete snapshot checks passed: exact scalars, clone/source isolation, choice replay base.
Env session checks passed: 64 parallel predictions, source isolation, failure recovery, disposal.
Public prediction API checks passed: owned successor and explicit failure without source mutation.
Complete snapshot API checks passed: all run streams, exact scalars, choice base, concurrent export, owned JSON.
Build succeeded. 0 Warning(s) 0 Error(s)
derived smoke passed; evidence: /tmp/mcc-env-rng-branches-20260926
derived smoke passed; evidence: /tmp/mcc-env-rng-rejection-20260926
```

Managed 检查还覆盖：ulong.MaxValue、负零浮点位、decimal 符号/scale、选择重放根、未知协议版本/改变 RNG 顺序的拒绝、五个 RNG 字段分别变化的拒绝，以及释放源后独立读取。数字边界与选择 JSON 来自真实 C++ fixture；没有手写模拟器快照作为成功参照。初版 managed 断言误以为对象池数量等于牌组数量；战斗实例与牌组实例独立，改为核对池 count 与实际槽号、牌堆引用是否能完整解析。

自然根日志：

```text
BRANCH_ENV_CHECK turn=2 action_count=1 transitions=6 ... source_unchanged=true successor_match=true scope=structural_and_combat_rng backend=native_with_env_verification
BRANCH_ENV_SUMMARY native_expanded=100 native_transitions=335 env_checked_transitions_root_cumulative=343 env_disabled=False reason=none scope=structural_and_combat_rng native_timing_includes_verification=true
```

拒绝 fixture 保留上游默认追加的 Strike；root 的 hand/hand_states 不一致，Env 检查停用、native 继续，UI 事件记录原因。summary 为 native_expanded=100、native_transitions=260、env_checked_transitions_root_cumulative=0、env_disabled=True。两次隔离实例均已清理。采样内核峰值分别为 1,591,964 KiB 与 1,453,468 KiB，低于各自 2048 MiB 配额。

加入显式随机流名顺序校验与共享比较器后，最终构建再次通过；最新 DLL/原生库的自然根复验 `/tmp/mcc-env-rng-final-20260926` 同样通过：native_expanded=100、native_transitions=335、env_checked_transitions_root_cumulative=343、env_disabled=False。实例已清理；采样峰值为 1,594,996 KiB，低于 2048 MiB 配额。

## 限制与后续

- 本轮完成全部 Env 状态的读取合同，不能据此声称已捕获/导入对应 native 内部字段。
- 分支核对不覆盖每个怪物的 RNG/FSM、非战斗流、模型内部计数、完整历史或终局/选择边界。
- 全 JSON 导出只适合诊断与映射审计；不能把这条高成本检查路径当作最终 Env 搜索状态读取路径。
- native 搜索耗时包含验证，两个搜索仍使用不同的参考/原生政策，未给出速度倍数。累计 Env 转移也不能直接等同单个原生 solver 的计数。
- 没有修改游戏效果或 RNG 算法，本轮完整 oracle 差分未跑，未合入 main。
- 完整 live 导入、原策略全部预测路径（含 threat projection）的 Env 替代、持久设置 UI、公平性能对比及最终安装包仍为活动目标。
