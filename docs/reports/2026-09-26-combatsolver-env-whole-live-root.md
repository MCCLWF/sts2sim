# CombatSolver Env：完整逻辑根与连续回合有限接受

日期：2026-09-26；分支：`feat/combatsolver-env-backend`；上游：固定 `d231e9e5`。前置提交 `5b3fd7c` 已有 seed-free 模板、组件捕获、房间边界和 UI 选择，但自然安装游戏的部分档案拒绝，原算法 Env 执行适配尚未完成。

## 本轮结果

新增 `LiveRootChecks`，使用正版 `sts2.dll` 的全解锁 Ironclad、Cultists Normal、A0、seed `LIVEROOT001` 对象。`GameBootstrap.Init(testMode:false)`；运行正版新局、怪物生成、`SetUpCombat`、`AfterCreatureAdded`、`BeforeCombatStart` 和 `StartTurn`。为了独立控制动作，夹具设置 manager 的启动标志并手动调用生产回合方法，不启动客户端自动回合循环；不是安装游戏接受测试。

从七个实际组件与 blank template 组装文档，所有状态字段由冻结值或严格普通房间空状态证明提供。`DUAL001` 的 schema session 仅用于 monster registry 绑定与模板元数据，不提供 gameplay 字段。导入后的全部 `state` 与组装输入相同。然后将 C++ 的后继和再次捕获的正版状态递归逐字段比较：

1. `StrikeIronclad.SpendResources` + `OnPlayWrapper`，目标第一个敌人。
2. 完整 `EndPlayerTurnPhaseOneInternal`、`EndPlayerTurnPhaseTwoInternal`、`SwitchFromPlayerToEnemySide`、敌方 `StartTurn` 与下一玩家 `StartTurn`。
3. 再一次完整 EndTurn，覆盖实际敌方伤害、力量生成、弃牌重洗和新手牌。

三个后继全部一致，源 hash 不变。比较未跳过 RNG、费用 lazy flag、能力内部数据、历史聚合或牌堆顺序。

## 发现与修复

- **非法目的字段**：房间组件原先把 `current_room_count/is_game_over` 写入没有这些字段的 RunData。现在保留在房间证据元数据中；mapper 对未知目的字段拒绝。
- **星数费用 lazy flag**：正版 mutable `CardModel.BaseStarCost` 首次 getter 会写 canonical value 和 `_starCostSet=true`。C++ 原先只返回 eager value，Strike 后继出现 `state.cards.slots[12].value.star_cost_set; native=true; env=false`。现按正版源码复现 getter；`LegalActions` 在状态副本上查询，使合法性检查和 `Predict` 不改变源。
- **跨捕获合成编号**：native power 是 CLR 对象，每个 creature 独立追加；Env 是全局持久 pool。第二轮原捕获按 owner 遍历重新编号，出现 `native=STRENGTH_POWER/env=RITUAL_POWER` 的同一槽号差异。`LivePowerAllocationTrace` 只观察 native `PowerApplied` 的同步次序，保存 CLR 身份，重写本次 capture 的 refs/allocator metadata。所有 value 原样来自 native，不按 Env 输出配号，也不改变 C++ 分配顺序。这仅适用当前固定 roster、无删除/未应用对象的夹具，遇到范围外情况拒绝。

## 严格覆盖

`EnvLiveRootAssembler` 与搜索根 context 解耦为 typed components。RunData 26、PlayerData 17、CombatData 33、PlayerCombatData 9 个字段跟踪唯一来源；重复捕获只做相等性检查，不能覆盖前一个来源。任何缺字段或新增 C++ 字段会拒绝。

默认白名单只有 rewards/events/rest_site/treasure/merchant，逐一检查完整嵌套字段、固定数组和空值；GameState 顶层十个 key 也严格检查。ProfileConfig 不在 GameState 文档中，必须由实际 run/player 全解锁证明通过，profile flag 与其证据不一致也拒绝。

`RootCoverageChecks` 从真实捕获组件分别删除 odds/gold/history/stars，注入重复写入、冲突来源、新 combat/reward/state/profile 字段、不一致解锁证明及非空 treasure 默认；全部明确拒绝。之前的 room/synchronizer/unknown subscription 检查仍通过。

## 无头表现层范围

`Headless.InstallLiveCombatPresentation` 使用可恢复的 scoped Harmony 补丁，仅跳过卡牌节点、洗牌特效和伤害数字 factory。原始 `GetTweenForCardsChangingPiles` 仍执行 fromSilentAdd 的 pile events，其返回的无节点动画等待标志由后缀清除；若返回非空 tween，明确失败。所有 pile 变化、damage/history、Hook、StableShuffle 与玩法 RNG 均由正版执行。Replay writer 只关闭 diagnostic IO。方法依据已写入 Headless 注释与偏离登记 O-014。

日志中的无 creature node 动画提示来自游戏的安全返回；此前 `Failed to open file` 来自旧组件夹具的无头存档 IO，不是差分忽略规则。

## 验证记录

```text
cmake --build build -j 4
ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 9

dotnet run --project tools/EnvLiveCardChecks -c Release
live root coverage: four required-field omissions, duplicate writer, conflicting component,
  new combat/reward/state/profile fields, inconsistent profile proof and nonblank room default rejected
whole live root fixture: seed-free assemble/restore, production Strike and two full
  next-turn successors including shuffle matched; TestMode=false; installed_game_acceptance=false

dotnet build tools/CombatSolverEnv/CombatSolverEnv.csproj -c Release -p:CopyModOnBuild=true
Build succeeded. 0 Warning(s), 0 Error(s)

dotnet run --project tools/EnvSessionChecks -c Release
Env session checks passed: 64 parallel predictions, source isolation, failure recovery, disposal.
Public prediction API checks passed: owned successor and explicit failure without source mutation.
Complete snapshot API checks passed: all run streams, exact scalars, choice base, concurrent export, owned JSON.
Snapshot import checks passed: owned restore, supplied state, strict fields/schema/runtime/refs/hash and failure isolation.
```

临时原始输出：`/tmp/mcc-live-root-checks-final.log`。Quick 门禁与当前派生程序集的隔离游戏拒绝检查也通过：

```text
scripts/check.sh --quick
9/9 tests passed; smoke: death=94 unported=6; death=100; boss=3 death=97
quick check passed (differential checks skipped)

python3 tools/CombatSolverEnv/run_smoke.py --mode both --expect-profile-root-rejection \
  --memory-reservation-mib 2048 --out /tmp/mcc-whole-live-root-profile-game
derived smoke passed
native_expanded=100 native_transitions=335 env_checked_transitions_root_cumulative=0
reason=live_run_requires_fully_unlocked_veteran_profile (run/player runs=0, missing_seen_encounters=85)
ENV_BRANCH_WARNING shown=true; Native continued; scenario Passed; isolated instance removed
reservation_mib=2048 sampled_peak_rss_kib=1468728
```

当前构建与实际游戏 mods 中的两个二进制 SHA256 一致：DLL `1efbae34214e1949bf09191cb94ac9d8970f975e9271b09319e3679781a5f605`，native library `32f44c0998b1d138675f7b1da09bc242faf124644e953c4c4bfe1a43b77ac84e`。该实机检查仅证明拒绝、UI 提示与 Native 继续，不能作为全解锁 live 接受证据。

## 仍需完成

- 当前安装游戏的部分解锁档案仍明确拒绝；全解锁普通有限场景通过不能扩为任意 live 根接受。动态怪物 registry、已移除/未应用对象闭包、附件与未知内部状态仍有限制。
- 原算法 Env backend 尚未接管。审计发现原搜索需要独立的安全终局截停、预测历史事件、只读威胁查询和原 BuildStateKey/破平局编码；普通整局 Env.Predict 会运行战后 Hook/奖励，不能直接用于原搜索终局评分。详见[同算法适配计划](../plans/2026-09-26-m5-combatsolver-env-same-policy-adapter.md)。
- UI 持久化选择已存在，Env 执行仍明确被 incomplete gate 拒绝，实际 Native 继续。连续游戏回合、选择/药水、部署、安全切换、可视 UI 和相同根/算法/预算性能验收未完成。
- 本轮是分支阶段验证，未宣称完整 `scripts/check.sh` gate 或最终 mods 成品完成。
