# CombatSolver Env：搜索分支核对、公开预测 API 与哈希修复

日期：2026-09-26；分支：`feat/combatsolver-env-backend`。

## 实际前置状态与范围

基线 `e4ae8c5` 已提供独立派生构建，native 算法直接链接固定子模块 `d231e9e5`；Env 仍是参考 Beam，根由种子重建，原有检查只重放最终 native 路线。完整 live 导入、真正的 Env 搜索执行后端、可切换设置 UI 均未完成。

本轮增加搜索分支的结构后继核对、公开 owned session API，以及状态哈希的健壮性修复。它们不是完整后端替换的验收。

## 实现

- 严格检查锚点后生成根捕获、Snapshot 释放、Replay/ReplayAction、回合准备和 Solve 结束入口；子模块未改动，候选、评分、剪枝及预算政策未改写。
- 主线程冻结 Env 输入；后台读取 native shadow 的四个牌堆、部分卡牌实例字段、人物/敌人标量、能力数量、遗物熔化状态和药水槽。每个已核对快照独立拥有 Env session，随 `ReleaseSimulator` 释放，临时后继在 finally 中释放。
- 同一 Env 源可被并行兄弟分支使用；inspect、clone、predict、dispose 共用锁，避免 C ABI 错误/结果缓冲区竞争。失败不跳过 native 候选；该根停用 Env 核对，UI 显示原因及 Native 执行动作。中英文模板合并进同一内嵌目录。
- `CombatSolverEnv.Api` 提供公开 `OpenSeededIroncladCombat`、Clone、Predict、TryPredict、InspectJson、StateHash。`Ready/Unported/Error` 独立分类，失败没有后继；入口明确限定为种子战斗，尚非 live 导入。
- managed 检查发现相同种子独立创建根的哈希不同。旧实现直接哈希 `GameState` 字节，包含不确定的 padding。现按生成的 39 个 POD 类型、332 个字段编码；忽略 inactive 存储，保留池分配游标与 HashSet 空闲链，挂起 Choice 包含其静止快照与候选元数据。构建强制检查生成 schema 是否过期。

## 验证与原始结果

| 命令 | 实际结果 |
|---|---|
| `dotnet build tools/CombatSolverEnv/CombatSolverEnv.csproj -c Release --no-restore -v quiet` | 最终 Release 通过，0 warnings / 0 errors |
| `dotnet run --project tools/EnvSessionChecks/EnvSessionChecks.csproj -c Release -v quiet` | 64 次并行预测与顺序参考一致；源隔离、错误恢复、Dispose、公开 API 成功/失败合同通过 |
| `scripts/check.sh --quick` | 8/8 CTest；冒烟 `death=94 unported=6`、`death=100`、`boss=3 death=97`；差分门禁按 quick 跳过 |
| `python3 tools/gen/gen_state_fields.py --check` | 通过；padding 污染、有效 RNG 变化、未用存储及 HashSet 空闲链检查由 `test_env` 覆盖 |
| `python3 tools/CombatSolverEnv/run_smoke.py --mode both --verify-branches --memory-reservation-mib 2048 --out /tmp/mcc-env-branches-final-small-20260926` | Passed；100 个展开节点；335 个 native transition；343 个 Env 核对转移，根累计口径包括额外回放；未停用 Env 核对；包含进入第 2 回合的 EndTurn；实例删除 |
| `python3 tools/CombatSolverEnv/run_smoke.py --mode both --expect-branch-rejection --memory-reservation-mib 2048 --out /tmp/mcc-env-branches-rejection-final-20260926` | Passed；native 根 6 张手牌、Env 根 5 张，明确拒绝；UI warning 结构事件出现；native 继续展开 100 节点；实例删除 |

自然根最终日志：

```text
BRANCH_ENV_CHECK turn=2 action_count=1 transitions=6 verification_ms=0.378 source_unchanged=true successor_match=true scope=structural_only backend=native_with_env_verification
BRANCH_ENV_SUMMARY native_expanded=100 native_transitions=335 env_checked_transitions_root_cumulative=343 env_disabled=False reason=none scope=structural_only native_timing_includes_verification=true
```

拒绝日志：

```text
BRANCH_ENV_DISABLED turn=1 action_count=0 reason=unavailable:root_mismatch:hand,hand_states;native_hand=DEFEND_IRONCLAD|BASH|STRIKE_IRONCLAD|STRIKE_IRONCLAD|DEFEND_IRONCLAD|STRIKE_IRONCLAD;env_hand=DEFEND_IRONCLAD|BASH|STRIKE_IRONCLAD|STRIKE_IRONCLAD|DEFEND_IRONCLAD env_used=false scope=structural_only native_search=continues
ENV_BRANCH_WARNING shown=true text=Env 分支核对已停止；当前由 Native 执行动作。
```

首次自然根 fixture 错用上游默认输入，因其在 TurnStarted 后默认追加一张 Strike，核对被拒绝；修正 fixture 为显式 `--cards-json []` 后通过。此前“最终路线可被 Env 接受”的证据不能证明 native 实际搜索根相同，新增检查确实发现了这一缺口。

最终 4096 MiB 配额的复验在启动前排队超时（可用内存不满足配额加 2 GiB 余量），实例已清理。未停止其他任务进程或修改 host 协调器；改用本工具可配置的 2048 MiB 小 fixture 配额，并记录精确实例的内核 RSS 峰值。两组分别为 1,492,724 KiB 与 1,457,876 KiB，均低于配额。原始游戏、启动器及内存日志保存在上述临时目录；大日志不提交。

## 未完成与实际限制

- 分支核对仍基于种子根与结构字段，不核对完整 RNG、内部变量、历史和怪物状态机；不能说明所有后继等价。
- 选择与终局适配未验收；100 节点短测不能支持完整战斗结论。额外 native threat projection 等辅助预测仍使用原模拟器。
- native 搜索的墙钟预算与耗时包含验证开销；参考 Beam 与 native Beam 的政策及预算也不同，本轮没有提速倍数结论。
- 公开 API 的 `Unported` 分类使用既有 C ABI 状态，但本轮 managed fixture 没有动态构造未移植选择；原 C++ 未移植根测试仍在快速门禁中。
- UI 只验收了 headless 的失败提示结构事件；设置切换、持久化、真实可见排版及成品安装验收仍待完成。
- `scripts/check.sh` 完整 oracle 差分本轮未跑；未改动游戏效果/RNG 算法。本轮报告不扩大历史内容移植的正确性覆盖。

下一项仍是完整 live 状态合同和导入、原策略对 Env 状态读取/转移的适配，随后接入游戏内两模式设置、同条件性能对比与最终安装包。
