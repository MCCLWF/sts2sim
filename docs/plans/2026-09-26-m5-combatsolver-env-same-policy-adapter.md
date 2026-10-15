# M5：CombatSolver 原政策的 Env 执行后端

> **历史冻结计划**：2026-09-27 的最新用户方向已转为通用 Env / Random Foreseer API。本文件后续任务不再执行，也不因 Env 完成自动恢复；重新启动需要用户另行明确要求。当前计划见 [通用 Env 路线](2026-09-27-generic-env-completion.md)。


> **2026-09-27 状态：移植暂停。** 按用户新方向先完成通用 Env；[完成范围与恢复门槛](2026-09-27-generic-env-completion.md)通过后再恢复。本文的接口、有限测试和最终 Mod 目标保留，后续条目不表示正在继续移植。

日期：2026-09-26  
分支：`feat/combatsolver-env-backend`  
状态：P0 有限整根与 P1 终局 scope 已实现并验证，P2 的游戏 getter、原政策 CanPlayCard 核心探针与部分事件历史已在有限 live 根对拍；完整 trace/其余政策查询尚未完成。本文件是执行计划，不是完整后端完成证据。P1 结果见[报告](../reports/2026-09-27-combatsolver-env-combat-prediction-scope.md)，P2 查询见[游戏 getter](../reports/2026-09-27-combatsolver-env-game-getter-queries.md)、[政策探针](../reports/2026-09-27-combatsolver-env-policy-playability-probe.md)与[部分事件账本](../reports/2026-09-27-combatsolver-env-history-sidecar.md)。
上级计划：[CombatSolver Env 后端与游戏内双模式](2026-09-26-m5-combatsolver-env-backend.md)。

2026-09-27 增量：派生游戏诊断入口开始消费真实组件组装的 owned 根；普通卡牌原指纹已在三组 live 根和 10 个 Beam 后继场景逐卡对拍，仍未开启 P4 正式执行。见[真实根入口与卡牌指纹报告](../reports/2026-09-27-combatsolver-env-live-entry-card-fingerprints.md)。

2026-09-27 后续：Rampage/Thrash 精确增长字段与 Rampage 原特殊指纹已验证，三组 live 整根连续回合包含已观察 Weak 移除闭包；11 个原 Beam 场景逐项核对所有历史条目顺序和已支持风险原因。预测外层为 v5，完整 history/state key/正式执行门禁仍关闭。见[增长状态报告](../reports/2026-09-27-combatsolver-env-card-growth-state.md)。

2026-09-27 查询增量：13 个已支持计算型卡牌公式、游戏/原预测器的终局差别及五种球的只读值/原部分编码已验证；三组 live 根各 52 个计算值、22 张卡牌指纹和 6 个球部分指纹一致，六组真实出牌与两次 EndTurn 全字段通过，原 Beam 扩至 12 场有限投影。见[计算变量](../reports/2026-09-27-combatsolver-env-calculated-card-queries.md)和[球键输入](../reports/2026-09-27-combatsolver-env-orb-key-inputs.md)。完整 P2/P3/P4 门禁仍关闭。

## 1. 交付合同与范围

最终交付仍是可放入 `mods/CombatSolverEnv/` 的文件夹。游戏内可以选择 Native、Env，并打开同根性能比较；设置持久化，切换会使旧搜索、旧续用和旧部署结果失效。在声明支持的内容范围内，Env 从冻结 live 根独立完成所有分支克隆、动作、选择和跨回合推进；原游戏只负责根捕获和最终动作部署。未移植、缺失字段或对拍分歧时明确禁用整个 Env 请求，不略过出错候选，不把 Native 回退算作 Env 成功。

本阶段只替换执行和分支读取边界。以下政策必须继续消费固定 upstream 的原实现：

- 卡牌重复去除、候选产生顺序、目标顺序、动作族配额、选择排序与 occurrence 保留；
- `Snapshot` 的全部评分特征及权重、威胁投影、成长/遗物/药水/偷窃政策；
- 原状态键编码和破平局顺序、转置、Pareto、Beam/BFWS、循环与跨回合保路；
- 原 `SearchPolicySnapshot.Profile`、请求共享预算、选择配额、取消与按序提交。

开始支持范围须写成可执行根门禁，例如单人、Ironclad、全解锁档案、已移植原版模型、已审计的房间/监听者；未来产生未知内容也拒绝整个请求。不能把模型名称白名单、关闭药水或关闭选择当作最终完整交付。当前仓库的全解锁假设继续有效；原始部分档案未验证支持时仍拒绝，最终报告必须写明此限制。

## 2. 固定源码证据

本次只读审计使用 `external/CombatSolver` 的固定提交 `d231e9e51a0e58d6bfa1373c6265cce13ffd9a45`。下表 upstream 行号均针对这个提交；C++/派生桥接行号针对审计时工作树。战斗语义真值仍为正版 v0.111.0 源码/oracle，upstream 仅决定要保留的搜索政策。

| 证据 | 当前行为及接入影响 |
|---|---|
| `src/Search/CombatPlan.cs:1159`、`:1232`、`:1341`、`:1347` | `SimulationSnapshot` 构造参数和属性硬绑定 `CombatPredictionSimulator`，释放只清 CLR 引用；Env 必须有显式 owned handle 释放。 |
| `src/Search/CombatBeamSolver.Expansion.cs:45`、`:74`、`:108`；`ParallelExpansion.cs:454`、`:478`、`:521` | 串行和并行候选都直接读 `SimulatedCombatState`，调用 `CanPlayCard`；首动作还调用 `card.Original.CanPlayTargeting`。只替换 Replay 不足以消除 native 查询/live 读取。 |
| `src/Search/SimulatedCombatState.cs:1517`；`Engine/InCombat/Simulation/CombatPredictionSimulator.Card.cs:159` | 合法性查询读取费用、星能、`HookMirrors.ShouldPlay`、`CardIsPlayableMirrors`。Env 投影必须路由这些查询。 |
| `src/Search/CombatBeamSolver.Expansion.Replay.cs:244`、`:299`、`:326`、`:635`、`:700`、`:718`、`:955` | 根准备、普通/增量 Replay、动作尾部、Fork、续执行和整个回合分别具有 native 执行入口。 |
| `src/Search/CombatBeamSolver.Expansion.Replay.cs:531`、`:548`、`:573` | `ManualPlay` 后仍调用死亡补偿/规范化/胜负检查；仅将 `ManualPlay` 改成 Env 会双结算。 |
| `src/Search/CombatBeamSolver.RoundTransition.cs:11`、`:324`；`TurnExecutionContinuation.cs:56`、`:68`、`:113`、`:127`、`:151` | 玩家下一回合推进与恢复仍执行 native Hook、抽牌与自动打牌。Env 必须接管这些入口。 |
| `src/Search/CombatBeamSolver.StateEvaluation.cs:43`、`:681`、`:728`、`:932`、`:965`、`:1131`、`:1144`、`:1291`、`:1386` | 评分既读状态也调用费用/抽牌/最大能量/关键词/伤害/HP-loss/目标重定向查询；只把字段投影回 C# 仍会执行 native gameplay 查询。 |
| `src/Search/CombatBeamSolver.StateEvaluation.cs:1553`、`:1634`；`SimulatedCombatState.cs:2222` | 原状态键包含卡牌、九 RNG、历史依赖及领域内部状态；键数值还用于排序/破平局。不能换成 `Env.StateHash`。 |
| `src/Search/CombatBeamSolver.Expansion.Candidates.cs:22`、`:38`、`:403` | 候选的 `IsPure` 依赖从父历史游标开始的事件类型；只允许 CardPlayStarted/Finished、CreatureAttacked、DamageReceived 为 pure。前后 HP/格挡差不能推导其他事件是否发生。 |
| `src/Engine/InCombat/Simulation/CombatHistoryCounters.cs:6`；`src/Search/CombatHistoryCounterKey.cs:57`、`:66` | 六类 history counter 影响状态键；随机生成来源在根冻结时保守开启全部历史依赖。 |
| `src/Search/SimulatedCombatState.CardEventHistory.cs:180`、`:203`；`LongTermResources.cs:40`、`:57`；`Potions.cs:21` | 累计 HP 损失与回复、Anger 复制、成长、死亡保护、药水使用及战略成本均为搜索分支统计，不是净状态差。 |
| `src/Engine/InCombat/Simulation/CombatPredictionSimulator.cs:217` | 原胜负检查只锁 `TerminalStamp` 并结束预测，不执行战后清理、回复、奖励或跑局流转。 |
| `game/Combat/CombatManager.cpp:125`、`:467`；`game/Runs/RunFlow.cpp:38`、`:137`；`env/env.cpp:122` | 现有 Env 普通动作胜利后执行 `EndCombatInternal` 的战后 Hook/玩家清理，再生成奖励。直接用于 Beam 会改变 terminal 评分与 RNG。 |
| `src/Search/CombatBeamSolver.CardChoiceContinuation.cs:81`；`PotionChoiceContinuation.cs:109`；`ExecutionChoiceContinuation.cs:57`；`RoundTransition.cs:181` | native continuation 保存 C# 执行帧与模型图；不能恢复到 Env 后再继续 native 尾部。 |
| `engine/sim/sim.h:23`；`env/env.cpp:101`、`:114` | Env choice 已有前缀重放、候选引用、选数与索引协议，但缺少 Beam 需要的 Effect/SourcePile/ContextId/Timing/主选择与嵌套关系合同。 |
| `src/Search/CombatBeamSolver.Terminal.cs:309`、`:341`；`Runtime/ContinuationStamp.cs:117` | 最佳路线重新 Replay 来生成连续回合续用戳；最终路线/标注也必须走 Env。 |
| `Runtime/CombatRootSnapshot.cs:162`、`:174`、`:177`、`:337` | 主线程根构造了 native forecast/影子状态，worker 又通过 `ForkSimulator` 克隆；Env 根不能在后台再读 live 以补缺失值。 |
| `src/Prediction/IntentForecaster.cs:40`、`:70`；`src/Search/CombatBeamSolver.Expansion.Opening.cs:211` | 根 forecast 会前推 future AI，且首层开局政策读取它；Env 根须产生同合同的 forecast，不能继续调用 native AI 预测而仅把它称为元数据。 |
| `tools/CombatSolverEnv/prepare_sources.py:1`、`:43`、`:55`；`Runtime/EnvBackendSettings.cs:30` | 当前生成代码只观察 native Replay；Env 分支挂在 native snapshot 上用于核对，执行门禁仍返回 incomplete。没有正式执行后端。 |

## 3. 架构选择：原搜索 + Env 所有权 + 只读兼容投影

推荐继续直接编译固定 upstream 搜索源码，不复制 Beam 算法。派生生成器只增加经过唯一锚点检查的接口分支，新增代码放在 `tools/CombatSolverEnv`；不编辑固定子模块。实现先沿用 concrete/partial，暂不引入大规模接口改造。

每个 Env 节点由一个 `EnvSearchBranch` 持有：

1. owned C++ combat-prediction session；这是唯一可执行战斗状态；
2. 来自该 session 的不可变读投影；C# 对象只保留身份、类型、已物化静态元数据和本分支字段；
3. 从 C++ 转移事件构建的政策 history/统计；只有一个搜索账本；
4. pending choice 语义信息、terminal stamp、逻辑转移/洗牌边界信息；
5. snapshot/checkpoint 的独立引用所有权，取消或丢弃时确定释放。

`CombatPredictionSimulator` 可以保留为搜索当前 concrete 类型的兼容载体，增加 Env 分支标记和 projection 工厂。它的 Env `Fork` 只克隆 C++ session、共享/复制不可变读投影和政策历史；不运行 native `CombatPredictionState.Fork`、`StateStore.Fork`、模型克隆或 `SimulatedCombatState.MaterializeRoot`。新增 projection 工厂直接填入 `SimCreatureState`、`SimPlayerCombatState`、`SimCardPile`、`PredictedCard` 和必要领域字段，禁止其惰性回读 `_liveState`。卡牌预览必须来自冻结元数据与本分支状态，不能回写 `Original`。生成模型按 Env UID/池槽绑定，不用牌 ID 代替实例身份；移除后的已知敌人与复活/召唤顺序继续保留。

这条路线可以复用原 typed reads、评分公式和集合比较器，但**不能只换 `CombatPredictionSimulator` 的执行方法**。`SimulatedCombatState` 的合法性、AI/威胁、内部计数以及原评分的 gameplay 查询也必须适配。每个 query 无法映射时拒绝整次 Env 请求，不返回默认值。projection 上的 native mutation 入口必须抛稳定错误，以暴露漏接路径。

## 4. C++ 必须先提供的三个边界

### 4.1 Combat prediction scope：在原搜索的终局点截停

新增独立 combat-prediction session/transition 入口，不改变普通跑局 `Env::Predict` 的语义。上下文由 owned session/`Sim` 显式携带，不能使用全局开关。Clone 和 choice 重放均保留 scope。

当正常游戏逻辑在已有安全点调用 `CombatManager::CheckWinCondition` 时，prediction scope 记录 `{player_turn, victory|defeat}`，按原预测器只结束战斗预测；禁止进入 `EndCombatInternal` 的战后回复、清牌、Hook、奖励、保存及地图流程。pending loss 必须按原边界锁定，即使随后效果回了 HP 也不能改变 outcome。动作、敌方回合、额外回合中的检查顺序继续取自游戏源码；不能只在 `RunFlow::AfterCombatAction` 截停，因为战后副作用在此之前已发生。

scope 的终局是显式结果，不能假装普通 `RunPhase::Ended`、奖励页或仍有合法动作的 Combat。查询返回 terminal stamp；terminal 无 legal actions，继续 Predict 拒绝。导出/恢复协议要携带 scope/terminal/policy history，保留原 game-state snapshot 协议的明确版本兼容性。

最小 fixture：致命 Strike + BurningBlood（截停前 HP 不接受战后回复）、敌方 lethal loss、phase-one/poison lethal、forced-end lethal。对照原预测器的终局状态，同时使用 oracle 的原战斗时序确认截停点；普通 Env 同种动作仍完成原战后流程。测试不是放宽现有普通 Env 比对。

### 4.2 政策 history sidecar：事件来源真实，重放不重复

17 种原预测条目、六项键计数、其他策略账本与 C++ 执行点的逐项清单见[事件历史审计](../audits/combatsolver-env-history-map.md)。原 `CombatPredictionHistory` **没有**弃牌、能力、资源、格挡条目；这些其他消费者不能被额外伪造为 `History.Entries`，否则会改变原候选 pure 判断。

`GameState` 继续 POD；不能为搜索 trace 塞入 heap 指针。事件与搜索专用统计由 prediction session 的 sidecar 持有。引擎仅在已经发生的原语边界发出结构化事件，不靠重算 native mirror，不修改 gameplay RNG。

先逐项建 `policy consumer -> native entry/counter -> C++ emit site` 清单，再实现全部消费者所需事件：

- CardPlayStarted/Finished（UID、类型、升级、是否自动/重复/ethereal、回合）、draw/discard/exhaust/generation/options/affliction；
- CreatureAttacked 与每次 DamageReceived（接收者/施加者、来源种类与模型 ID、blocked/unblocked/overkill、kill），HP 回复与最大 HP 变化；
- Power/资源/卡牌状态变化、药水消费/自动死亡保护、遗物触发与成长/偷窃统计；
- shuffle 次数及发生阶段、怪物移除/复活/逃跑/召唤、terminal 和 choice suspend/complete。

事件顺序、计数与 native policy entry 一一映射；纯度不是「只发生了伤害」。`History.Entries.Count` 与父游标、六项 `CombatHistoryCounters`、累计 HP loss/recovery、`PotionUses`、死亡保护恢复及成本、`LongTermResourceValue/GrowthRewards/AngerCopiesGenerated` 都要有明确来源。原根中已经发生的 live history 继续按 upstream 根捕获合同初始化；新的 prediction history 从原预测器对应的空预测事件流开始，不能直接把完整 live 事件数写为新分支 history count。

choice 前缀重放须同时回滚 sidecar 到动作前基线，再重建事件；不能把每次试答产生的事件累计。未完成/嵌套 deferred entries 的顺序与完成配对有独立对拍，不能只检查最终总计。C# 可以把事件转换成原 history 的只读 entry 类型以复用消费者；这不是再次执行游戏效果。观察者抛错/未移植时 successor 为空，source 和 sidecar 均不变。

### 4.3 Pure queries：保留原评分公式，gameplay 查询由 Env 回答

先以 versioned JSON/bounded buffer 实现可审计的批量查询；功能正确后再替换热路径运输格式。每个 query 在私有 scratch state 上执行惰性 getter，保证 source state/hash/history/RNG 不变。至少需要：

| 查询面 | 被保留的原消费者 |
|---|---|
| UID/hand index 对应的 playable、费用/星费、target type、逐目标合法性/可命中性；本地/有效关键词 | `Expand`、`PrepareCardActions`、`TargetsFor/TargetsForPotion`、`CalculateReachableHandPotential`、free-card/first-attack 估值。 |
| 最大能量、手牌上限、手牌抽数、有效费用、是否清格挡、是否 ethereal 触发 | `CaptureEnergyRefundWindow`、`CaptureAct3BossInteractionPotential`、`LeavesHandAtTurnEnd`、防御 reserve。 |
| 当前 FSM move ID、攻击 hit 列表/原始伤害、forced/stun 状态与领域静态参数 | `CurrentMonsterMoves`、`BuildThreatFocus`、`AdjustMonsterMoveDamage`；不得调用 native AI advance。 |
| 原 threat 投影每 hit 的 attack 修正、BeforeOsty/AfterOsty HP loss、重定向、Buffer 消费、Gambit/死亡保护的**局部投影** | `ProjectHpAfterThreat/ProjectThreatHit`；返回原公式所需纯值及局部 projector 状态。不得把完整 EndTurn 后 HP 填入 `ProjectedPlayerHp`，该值含抽牌/Hook/毒/回复，会改变原评分。 |
| branch powers/relic/potion/orb 的完整只读内部数据与 references，VoidForm/PenNib 等状态 | `StateStore.Peek/TryGetReadOnly`、`EvaluateRelicCounters`、`StrategicEffectModel`、`AppendFingerprint`、续用戳。 |

原纯政策运算可保留 C#，例如 `CardChoiceSupport.CardValue` 的已捕获元数据权重、`ReachableHandValue.Calculate`、`RelicCounterPolicy` 和 `BuildProjectedShuffleOrder` 使用 RNG **副本**的稳定排序/洗牌；不可读 live，不可消耗 Env 的 RNG。原战略 evaluator 若来自未适配第三方注册表，则根门禁拒绝，不能在 Env worker 执行 opaque delegate。

## 5. 原搜索中的必须接入位置

### 5.1 根、请求和所有权

- `CombatRootSnapshot.Capture` 在原稳定性区间冻结 backend mode、live components、模型身份/显示表、原政策 root features、history dependencies、原始档案；组装并导入一次完整根。记录捕获前后 continuation。不能让每个 solver 另开 seed combat。Env 的 forecast 从 C++ scratch branch/AI 查询按原 forecast 合同产生；保留原 horizon、近似/支持标记和首轮 hit 序列，不在根捕获时继续执行 `IntentForecaster.Build` 的 native future roll。
- `CombatRootSnapshot.ForkSimulator` 根据不可变 backend 路由；`CombatSearchCoordinator` 的主搜索、portfolio、无药/强制用药反事实审计、prefix probe、恢复全部继承同一 backend。不得某条子路径恢复 Native。
- `SimulationSnapshot`、`ReplayForkSeed`、deferred probe/frontier/choice/round checkpoints 都管理 Env handle。snapshot 释放、seed 未消费、取消、异常排空和终局临时回放均释放一次。现有 `ReplayForkSeed.Dispose` 只清引用必须改。
- `SolverSearchSession`/`SolverDeploymentSession` 冻结 backend 与设置 generation；切换取消并排空旧 worker，在安全边界清除 `LatestResult`、续用/磁盘路线缓存/choice 接管，部署仍由唯一会话拥有。

### 5.2 Replay、round、choice

为原入口加入 Env dispatch，保留入口外部计数、取消、日志、候选提交与预算：

| 原入口 | Env 行为 |
|---|---|
| `ReplayTurnSetup` / `PreparePlayerPlayPhase` | 支持真实 pre-setup/已有选择边界的冻结根；由 C++ 执行剩余准备阶段，不能用 native energy reset/draw。尚未支持此根时明确拒绝该请求。 |
| `Replay` / `ReplayAction` | 按原 `PlanAction` 身份规则映射到 UID/当前手牌 index、目标 combat ID、药水槽，调用 prediction transition，读取后继/trace，生成原 Snapshot。逻辑 action/transition 数与原调用位置一致。 |
| `PrepareReplayForkSeedCore` | C++ Clone；原 parent Fork gate 仍串行保护 source handle/诊断缓冲，siblings 独立。 |
| `SettleReplayActionBoundary` / `ResolveRequestedPlayerTurnEnd` | C++ 已完成动作尾部、死亡与规范化；只读取结果。强制结束回合由 C++ 推进，禁止再执行 CorePowerSupport/normalize。 |
| `AdvanceRound` / `AdvanceRoundPlayerStart` / `ResumeRoundPlayerStart` / `ContinuePlayerStart` | 一个原逻辑转移拥有 C++ 玩家回合末、敌方回合、extra turn、下一回合准备。返回原 Timing 的 pending choice 或完整 play boundary，保留原 shufflesCrossed 规则。 |
| `BuildPrimaryCardChoiceSpec` / `BuildCurrentPendingChoiceBudgetSeed` / pending-layer builders | 将 Env request 转为**相同语义**的 `CardChoiceSpec`，继续调用原 `CardChoiceSupport.BuildChoices`、原选择预算与 occurrence collectors。不能把 Env 全部 LegalActions 当作新的选择算法。 |
| `ResumeExecutionChoice` 及 card/potion/round continuation | checkpoint 持有 Env pending session/前缀与政策 sidecar；按原 planned choices 编码 Answer（选数、索引、跳过、顺序），全程 C++ 重放，不恢复 native frame。 |
| 终局 annotation Replay / `BuildContinuations` | 同一 Env backend 从根重放所选路线，生成原字段的 continuation 与标注；不得为最终结果偷偷执行 native 原路线。 |

Env 已提供完整 choice 前缀重放，因此第一版可以禁用**物理 continuation 优化**并走同父全动作重放，但须维持所有原逻辑 transition/choice/replay attempt 预算与 candidate 顺序；在这项等价证明前不作等工作量性能结论。后续 Env checkpoint 只替换物理重放，不改选择政策/预算。未知 choice semantic（Effect/SourcePile/ContextId/Timing/主嵌套关系）拒绝整个请求，不能从 prompt 文本猜测。给 C++ request 增加结构化语义或维护逐来源审计表；实际效果仍只由 C++ 原效果代码完成。

### 5.3 Snapshot、状态键和 continuation

继续复用原 `Snapshot` 特征计算与 `BuildStateKey`、`AppendPile`、`BuildCardStateFingerprint`、`BuildCycle*`、`ApplyStateKeySalt`。九 RNG 各自四段内部状态/计数、有效 Power 次序、已知敌人次序、卡牌实例变化/费用/附件、`AppendFingerprint` 所有非空 maps/sets/生命周期字段必须从 Env 投影提供；源没有某项时不能用空集合伪装。

`Env.StateHash` 用于 source purity/运输完整性，不成为原搜索状态键。先证明每个同根/同动作的原 key、所有评分特征与 score 精确相同，再用相同原 comparator 验证准入/保路序列。`ContinuationStamp.CapturePredicted` 也通过只读 projection 或直接 Env 同字段 adapter；live/predicted 继续字段差分，不只比 HP/牌 ID。

## 6. 分阶段文件分工与依赖顺序

以下是实施边界，新增名字可按实际 API 收敛；阶段不得交叉改变搜索政策。

| 阶段 | 主要文件 | 完成 gate |
|---|---|---|
| P0 根接受 | 现有 `Runtime/EnvLive*.cs`、`EnvLiveRootAssembler.cs`、`tools/EnvLiveCardChecks` | 任意支持 live 根完整字段覆盖；普通动作和至少两次完整回合逐字段/RNG 对拍；拒绝部分档案/未知字段的证据。不能把 seed restore 算作根接受。 |
| P1 终局 scope | `engine/sim/sim.h`；`game/Combat/CombatManager.cpp`、`game/Runs/RunFlow.cpp`；新增 `env/combat_prediction.*`；`env/c_api.*`/snapshot scope 协议 | 第 4.1 节四个最小终局对拍；普通跑局旧语义通过；terminal 无合法动作、source purity、恢复后 terminal 不丢失。 |
| P2 trace/queries | 新增 `env/combat_prediction_trace.*`、`env/combat_prediction_queries.*`，精准 emit sites；`env/c_api.*`；C# bridge 与 `Api/EnvPredictionApi.cs` | 逐事件同序/六 counters/统计对账，choice 重放不重复；费用/威胁 query 与原只读评分对拍，source/RNG 不变。 |
| P3 read projection | 新增 `Runtime/EnvSearchRoot.cs`、`Search/EnvSearchBranch.cs`、`Search/EnvSearchProjection*.cs`；生成对应 simulator/state/pile/card 工厂 | 原 `Snapshot` 所有标量、特征、StateKey、cycle/pile keys、continuation 相同；投影不引用可变 live，所有 native mutation guard 命中即失败。 |
| P4 原执行 seam | `prepare_sources.py` + csproj excludes；新增 `Search/CombatBeamSolver.EnvReplay.cs` / `EnvChoices.cs` / `EnvQueries.cs`；生成 Replay/ParallelExpansion/choice/round/terminal 接入 | 同根固定预算完整原候选与 admission/score/key/retention 顺序相同；所有 branches/子 solver 执行计数 native=0；有选择、药水、第二回合和 terminal。 |
| P5 runtime/UI | 生成 coordinator/controller/session 接入；`Runtime/EnvBackendSettings.cs`、现有 UI；撤销派生程序集的 reference-beam 自动执行入口 | Native/Env/Compare 同一 root/policy，单部署所有权；持久化、切换/取消、旧结果失效、连续回合/原生选牌页/失败后新战斗恢复。 |
| P6 性能/包 | 同根基准工具、隔离游戏测试、打包入口、README/报告 | 同平台独占可见游戏性能，根 import/query/projection/search/clone/transition 分开计量；安装目录可直接加载，依赖与实际支持范围明确。 |

P1/P2 可按文件并行，P3 依赖它们的稳定协议；P4 必须等 P3 字段等价。UI 外观可以并行，但正式 Env enable gate 只在 P4/P5 通过后解除。每阶段按任务分支提交计划/报告，不合并 upstream submodule 的工作历史约束作为本次用户指令。

## 7. 具体验证与禁止路径

### 7.1 同根原政策等价

新增独立小型同政策检查工具（链接实际派生生产源码，不抄测试版算法），从 oracle 原对象冻结一次 root，构造 Native/Env 两种 backend 的同一个 `SearchPolicySnapshot`。先固定 `FixedBudget`、短节点预算、DOP=1、同 beam/profile/strategy，不让墙钟或 GC/内存压力提前截断：

1. 每动作边界对比全状态/九 RNG/pending/terminal 和完整政策 history；
2. Snapshot 逐字段对比，包括 key/score/所有战略、威胁、药水与成长特征；
3. 记录 parent/action identity/choice token 的原序，逐项比较 candidate、duplicate/dominance、admission family、selected、transposition、retention/final ordering；
4. 对比 `Expanded`、逻辑 `TransitionCount`、choice replay attempts/quota、请求总账、boundary、动作/回合数、结果 scope 和最终完整路线；
5. 同输入 DOP>1 仅覆盖兄弟隔离、原序提交、取消/异常排空，不用并行运行的耗时算独占收益。

代表集合按风险最早边界划分：普通 Strike/Defend/Bash；费用变化/抽牌洗牌；Armaments/TrueGrit/BurningPact 的主选择与嵌套选择；手动/自动死亡保护药水；至少一个额外回合；死亡/召唤/复活；第 4.1 节终局。根支持面扩大时增加相应独立最小差分，不为同根重复跑整场。oracle 保持 TestMode=false，纯表现/IO 补丁必须说明原因。

预算计数 gate 不能拿 C ABI call 次数代替：原一个 `PlanAction` 可能因 choice prefix 物理重放多次，仍只有原位置的一次逻辑 transition；查询、hash/export、Clone/Answer 内部物理工作另记。比较时 Native 的 Env verification 关闭，Env 的 Native verification 独立计时，不掺入 native 基准。

### 7.2 Forbidden native calls：结构与运行时双门禁

Env 标记分支在以下调用前必须拒绝并给出 call-site，整次请求失败、排空 owned handles：

- native `CombatPredictionSimulator.ManualPlay/ExecuteAttack/Draw/ManualUse/Kill`、`PotionExecutionSupport.Prepare/Complete`、普通 native Fork/continuation resume；
- `CorePowerSupport` 的 apply/flush/death/poison、`Persistent*Support` 的 Trigger、`PlayerTurnEndLifecycle`、`MonsterMoveSemantics.ApplyForecastMove`、`BranchMonsterAi.Advance/RollInitial`；
- mirror/原生 gameplay Hook 查询尚未改为 Env query 的 `ShouldPlay/CardIsPlayable/Modify*` 路径；
- `MutablePreview`/卡牌升级/附魔/native 状态写入、StateStore 的 effect mutation；
- worker 的 `card.Original.CanPlayTargeting` 或 `_liveState` 惰性读取；
- 最终 annotation/continuation/potion audit 内偷偷使用 Native simulator。

计数必须按 root/backend/request 域记录：`env_transitions`、`env_clones`、`env_queries`、`native_prediction_calls`、`native_gameplay_queries`、`live_worker_reads`。正常 Env 请求断言后三项为零；故意调用一次 forbidden path 的 fixture 必须失败且无可部署结果。静态生成检查确保 upstream 锚点一旦漂移停止构建。root 捕获的可审计元数据读取、纯 CLR 政策计算、最终真实部署不记作搜索 native prediction，仍记录发生阶段。

### 7.3 完整产品 gate

隔离正版实例验证安装/加载、UI 保存后重启、Native→Env→Native 在安全边界切换、搜索中切换取消、旧结果迟到不能发布/部署、连续回合与 choice 接管、药水、terminal、unknown/Unported 拒绝与后续新战斗恢复。比较显示实际 backend、同根/同政策标识、import 与 search 成本、节点/transition 吞吐、克隆/query/projection 成本、差分状态；CPU 竞争下只显示原时长与资源条件，不输出可比倍数。

正式比较运行两次相同冻结 root 的原 coordinator 请求；比较请求彼此拥有独立预算与 sidecar，不能从一次主请求再复制额外免费预算。可部署的只有 UI 选定 backend 的 validated result。原 `Sts2EnvCompare` 参考 Beam/seed 重建/独立部署循环在派生 Mod 中停止自动运行，保留为独立研究工具也不纳入本后端报告。

只有 P0–P6 均有实际报告，才能解除 incomplete 门禁并声称成品符合用户最终目标。本计划的存在、生成代码构建成功、native 路线 Env 重放成功，都不能作为完成依据。

## 2026-09-27 能力实例键增量

P3 增加独立 owned `CapturePowerKeyInputs()` 与原个体 `GetPowerFingerprint` 编码核对：三组真实根各 11 个能力、原 Beam 12 场有限后继通过。只完成个体键输入；原 EffectivePowers 聚合次序/退休 root 槽位和其余 AppendFingerprint 账本未完成，P3/P4/P5/P6 gate 继续关闭。验收及限制见[报告](../reports/2026-09-27-combatsolver-env-power-key-inputs.md)。
