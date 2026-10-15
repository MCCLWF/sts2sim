# 完整 combat/choice 快照的校验导入

日期：2026-09-26；分支：`feat/combatsolver-env-backend`。
关联计划：`docs/plans/2026-09-26-m5-combatsolver-env-backend.md`。

## 前置状态与本轮完成范围

上一轮已提供完整 Env 逻辑状态导出、owned session 和九条战斗 RNG 检查。本轮完成这个传输合同的导入端，供后续 live 字段映射调用；不是从游戏捕获完整根的实现，也没有替换原生搜索模拟器。

## 实现与失败合同

- 新协议版本 2 携带当前加载 runtime 的身份。schema SHA、model ID hash、有序随机流名都必须匹配；外来进程的 FSM/回调注册号明确拒绝，不能把此协议当作跨进程存档。
- 生成器增加 mutable 命名 visitor 及 flags 验证；全部字段必填，未知字段、未知枚举、越界整数、无效 decimal 位和非有限浮点拒绝。整数 JSON 不经 double，保留完整 ulong 精度。
- parser 限定 4 MiB、64 层、250,000 节点与单字符串长度；重复键、尾随内容、非法整数/转义失败。没有新增外部 JSON 依赖。
- 池校验槽号、重复槽、count、游标；HashSet 校验全部槽、重复元素、空闲链节点/终止/循环，保留未来分配与插入顺序。
- 战斗校验包括阶段、seed/act 身份、牌堆/牌组引用与归属、对象池范围、roster/combat IDs、能力归属、动态变量索引及怪物 FSM 索引。原 FSM 注册表记录产生定义的 MonsterId，导入不能把另一个怪物的定义绑定过来；注册号分配顺序及游戏状态未改变。新增 metadata 与定义在分配异常时保持对齐。
- 导入在全新的 Env 上进行；C ABI 任意失败都没有 successor，诊断 handle 的模拟状态保持原样。先拷贝输入再清除诊断 result，支持输入直接引用该 handle 的 last_result。
- 已有 state hash 必须吻合；内部 fresh combat 文档可以显式提供 null hash，由 native 计算。非选择根必须处于玩家 Play 决策边界。choice 必须保留 hash/pending/base，且只能是战斗转移；从 base 与已有答案重放必须重现当前逻辑 hash/request。活动未移植模型仍返回单独的 Unported。
- 公共 C# `RestoreCombatSnapshot` 和 Python `restore_combat_snapshot` 恢复独立 owned 分支。原生 Replay 的诊断根也走完整导出/恢复路径，日志标明 `root_source=seed_snapshot_import`，不宣称它是 live 根。

## 验收

```bash
python3 tools/gen/gen_state_fields.py --check
cmake --build build -j 4 --target sts2env_c test_snapshot
build/test_snapshot /tmp/mcc-env-import-fixtures-20260926
~/.dotnet/dotnet run --project tools/EnvSessionChecks -c Release -- /tmp/mcc-env-import-fixtures-20260926
~/.dotnet/dotnet build tools/CombatSolverEnv/CombatSolverEnv.csproj -c Release --nologo
python3 python/test_env.py
scripts/check.sh > /tmp/mcc-env-import-full-check-20260926.log 2>&1
python3 tools/CombatSolverEnv/run_smoke.py --mode both --verify-branches --memory-reservation-mib 2048 --out /tmp/mcc-env-import-game-final-20260926
python3 tools/CombatSolverEnv/run_smoke.py --mode both --expect-branch-rejection --memory-reservation-mib 2048 --out /tmp/mcc-env-import-game-rejection-20260926
```

已取得结果：

```text
Complete snapshot checks passed: exact scalars, owned restore, later turns, choice replay, storage order, unported rejection.
Env session checks passed: 64 parallel predictions, source isolation, failure recovery, disposal.
Public prediction API checks passed: owned successor and explicit failure without source mutation.
Complete snapshot API checks passed: all run streams, exact scalars, choice base, concurrent export, owned JSON.
Snapshot import checks passed: owned restore, supplied state, strict fields/schema/runtime/refs/hash and failure isolation.
Build succeeded. 0 Warning(s) 0 Error(s)  [派生 Mod]
100% tests passed, 0 tests failed out of 9
L2 newrun: 200 runs, 0 mismatched
L3 combat: 40/40 combats identical, 0 unported, 0 diverged, 784/784 states matched
L3 combat: 40/40 combats identical, 0 unported, 0 diverged, 827/827 states matched
L4 run: 40 runs: 40 identical, 0 stopped at unported content, 0 diverged; 1958/1958 decisions matched
L4 run: 40 runs: 40 identical, 0 stopped at unported content, 0 diverged; 2131/2131 decisions matched
all checks passed
derived smoke passed; evidence: /tmp/mcc-env-import-game-final-20260926
```

全门禁的 oracle 构建仍有既有 CS8604 warning（`OracleRun.cs:234`），无 error；没有把它记为派生 Mod 的构建 warning。L2/L3/L4 证明当前游戏逻辑未因 registry metadata 改动出现矩阵分歧，不是对 native live mapper 的验收。

恢复测试比较完整文档/hash，随后执行同样 EndTurn/Answer；也明确恢复了第二回合根。碎片池和 HashSet 的下一次分配/插入保持一致；循环空闲链拒绝。C#/Python 验证改变输入 energy 的完整文档被实际加载，而不是重新开种子局；同时覆盖错误 schema/runtime/引用/字段/整数/hash。Python 直接 C ABI 验证 last_result 原地输入以及重复键、前导零、孤立 surrogate 和过深文档，失败无后继且 source hash 不变。

最新自然根隔离游戏：native_expanded=100、native_transitions=335、env_checked_transitions_root_cumulative=343、env_disabled=False。根经过完整快照恢复，包含进入第二回合的分支/RNG 核对。实例已清理，采样峰值 1,586,244 KiB，低于 2048 MiB 配额。前一版同输入也通过；由于补了 seed 身份等实际行为校验，最终 source 版本单独复验。

拒绝 fixture 同样通过：额外注入 Strike 导致 root hand/hand_states 不一致，Env 检查停用，native 继续 100 个展开节点/260 次转移，Env 累计检查 0 次；UI warning 记录了原因。`root_source=seed_snapshot_import` 仍明确标出根来源。实例已清理，采样峰值 1,449,760 KiB，低于 2048 MiB 配额。

## 未完成项与限制

- 完整 native/live 字段映射仍未实现，种子根恢复不能证明任意真实战斗根等价。
- 完整导入的结构校验不证明所有字段值构成真实游戏历史；正确映射仍须逐字段及逐动作对拍。声明的根类型限于 combat/战斗内 choice，其他房间与终局快照不在恢复入口范围内。
- 部分 inactive 历史引用只检查池范围；活动 roster/piles 等检查 live。模型内部 extra 槽的引用需保持对应内容语义，当前检查了已识别的引用型槽；不是任意第三方模型的反射导入器。
- FSM/stun 回调仍按本 runtime 的已注册定义引用；不自动重建外部定义或推断未知回调。完整游戏 mapper 必须明确绑定这些定义与状态名。
- native 搜索仍执行所有真正的预测及辅助 threat projection；Env 仅做诊断。验证开销包含在 native 耗时中，未报告速度倍数。
- 设置 UI、公平同算法性能对比、完整 Env 搜索替换和最终安装包仍属于活动目标。本轮没有合并 main 或发布成品。
