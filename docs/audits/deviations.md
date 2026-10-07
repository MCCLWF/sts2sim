# 偏离登记册

模拟器与游戏之间每一处**有意的**偏离都登记在这里。意外的分歧是 bug，要修，不要登记。

每条写清四件事：
- **游戏原文**：源码位置和行为。
- **我们的做法**：模拟器或 oracle 怎么做。
- **理由**：为什么这样做。
- **影响范围**：会影响哪些对拍或哪些玩法，是否可以观测到。

编号只增不减。某条偏离取消后，把状态改成"已撤销"，不要删除。

---

## A. 模拟器的偏离

### D-013 放弃确认弹窗使用 Choice 前缀

- **状态**：生效；本批差分结果见 [Trial 报告](../reports/2026-10-05-env-trial-abandon-confirmation.md)。
- **游戏原文**：`Models/Events/Trial.cs:DoubleDown` 向 `NModalContainer` 添加
  `NAbandonRunConfirmPopup.Create(null)` 后立即返回 CompletedTask；弹窗的 `_Ready`
  按确认、取消顺序绑定按钮，Yes 调用 `RunManager.Abandon()`，No 只 QueueFree。
  单人 AbandonInternal 置 IsAbandoned，再 GuaranteeKillAllPlayers → Kill(force:true)，
  最后关闭弹窗；关闭界面、音乐和玩家间等待是表现层。
- **我们的做法**：同一按钮顺序/本地化 key 表示为 `ChoiceKind::Option`，用既有
  `NeedChoice` 暂停 DoubleDown，Answer 从静止点快照重放根动作；无需 Godot 节点。
  取消不改事件页，确认保留 IsAbandoned、强制 Kill 及死亡 Hook，由既有失败终局结算收尾。
  不执行动画、音频、弹窗销毁及 0.25–0.5s 等待；存档/统计输出仍属于 D-003 边界。
- **理由**：Env 的选择须可克隆/恢复；原版异步 UI 输入通过统一前缀表示。
- **影响范围**：原版 DoubleDown 的任务先完成、再等独立弹窗输入；Env 把两者串成一个
  挂起动作。公开玩法结果、按钮标记、事件 page index、资源与有种子 RNG 保持一致；
  本批不实现实际游戏 UI 操作或存档统计。

### D-001 全解锁存档假设
- **状态**：当前基准生效；2026-09-27 用户调整方向后，列为通用 Env 完成前需补齐并撤销的限制
- **游戏原文**：开局和生成内容时会读取档案进度，例如 `UnlockState`、时代（epoch）解锁、`DiscoveredActs`、角色解锁。这些会改变可选的幕、遗物池、卡池和事件池。
- **我们的做法**：当前按"全解锁的存档"运行（`ProfileConfig::all_unlocked`）。oracle 也使用 `UnlockState.all`。live 的真实档案字段已存入状态，不支持的规则仍明确拒绝。
  2026-10-02 修正 live veteran 门槛：57时代、正跑局数及12个 discovery Boss 已见；未见普通/事件遭遇不等于未解锁，不再要求 `UnlockState.all` 的遭遇全集。实际见闻位图保留，见[实机档案误拒绝](../reports/2026-10-02-live-profile-eligibility.md)。
- **理由**：初始训练/搜索基准限定全解锁。新的通用预测及真实游戏捕获需要忠实保留档案规则，后续按[通用 Env 路线](../plans/2026-09-27-generic-env-completion.md)补齐，不再把该假设作为永久范围限制。
- **影响范围**：
  - 首次游玩、未解锁状态下的行为不在覆盖范围内。
  - 依赖当前对局实时状态的门槛（金币、HP、牌组）不属于这一条，照常移植。

### D-002 只做单人
- **状态**：生效
- **游戏原文**：玩家集合、`MultiplayerScalingModel`、多人专用的遗物袋回退、网络同步。
- **我们的做法**：只有一名玩家，`net_id = 1`，不实现多人专用的分支。
- **理由**：需求决定（2026-09-24）。
- **影响范围**：多人模式不可用。单人模式下这些分支本来就不会执行。

### D-003 存档输出只保留游戏逻辑字段
- **状态**：生效
- **游戏原文**：`SerializableRun` 里包含墙钟时间、平台、`map_drawings`、`discovered_*` 等字段。
- **我们的做法**：`engine/save_json.cpp` 只输出影响游戏逻辑的字段。
- **理由**：这些字段不影响游戏进程。
- **影响范围**：只影响 L2 对拍的字段范围，见 `tests/l2/compare_newrun.py` 的 `IGNORED`。

### D-004 GremlinMerc 的被盗金币返还奖励暂未接入 C++ 奖励房间
- **状态**：已撤销
- **游戏原文**：`Models/Powers/HeistPower.cs:14` 的 `BeforeDeath` 在 FatGremlin 死亡时向当前战斗房间加入 `GoldReward`，并更新地图点历史。
- **我们的做法**：`game/Models/Powers/HeistPower.h` 的 `BeforeDeath` 生成被盗金币奖励并加入 `combat_extra_rewards`；`game/Runs/RunManager.cpp` 将额外奖励带入奖励房间。
- **理由**：M2 补齐整局奖励流程后已能逐字移植该逻辑。
- **影响范围**：无；保留编号以记录该偏离已撤销。

### D-005 原搜索专用的终局预测边界
- **状态**：已撤销并归档；普通 Preview 沿用原版结算，有限合同/差分已验证，完整 Env/RF 尚未完成。
- **游戏原文**：`Combat/CombatManager.cs` 的安全 `CheckWinCondition` 会处理 pending loss，或执行胜利战后 Hook 与清理；原版 `RunFlow` 随后进入奖励或结束局。以下是旧 CombatSolver 移植采用的不同做法。
- **旧做法**：固定 CombatSolver `d231e9e5` 的 `CombatPredictionSimulator.CheckWinCondition(playerTurn)` 只记录胜负与实际玩家回合，清 PendingLoss 并把战斗置为 inactive，保留搜索评分需要的战斗状态；旧 `CombatPredictionSession` 将其包装为专用 prediction ABI 和快照。
- **旧理由**：原搜索会另行估值战后回复等效果，因此当时试图让搜索停在胜负安全点。
- **影响范围**：旧行为仅属于专用 prediction/search 分支，不是普通 Env 规则。该接口已撤销并移入 [`archive/combatsolver/`](../../archive/combatsolver/)；当前原版回退与通用预览的有限验收见[本批报告](../reports/2026-09-27-env-vanilla-preview.md)。未来如需公开安全胜负点停止能力，应作为调用者显式选择的通用预测停止边界实现，不能借用旧旁路。
- **历史记录**：旧 trace、history projection 与未完成的 policy 投影记录见[历史投影报告](../reports/2026-09-27-combatsolver-env-mod-history-projection.md)。其“完整度未完成”等判断描述旧投影，不证明当前 Env 或 RF API 已验收。

### D-006 攻击组 AfterAttack 故障明确失败

- **状态**：生效；异常路径不属于完整游戏等价性认证。
- **游戏原文**：`Commands/Builders/AttackContext.cs:65-79` 的 `DisposeAsync` 只执行一次 AfterAttack，捕获其 `Exception` 后记录日志，不向调用方传播。
- **我们的做法**：`game/Commands/Builders/AttackContext.h` 的 Finish 保留一次性收尾，但传播 AfterAttack 错误。普通 body 异常先收尾；收尾成功则重抛原 body 异常，收尾也失败则传播 hook 错误。NeedChoice 表示原版 await 暂停，原样传播给 Env 前缀重放，不执行暂停后的收尾。
- **理由**：C++ 的 Unported、容量错误及模型错误代表尚不支持或执行失败，不能像原版日志错误一样吞掉后继续产生可被标为 identical 的输出。
- **影响范围**：仅 Omnislice/EchoingSlash 攻击组的收尾故障；正常完成路径按源规则执行。故障不会被认作成功对拍。body 故障及 NeedChoice 的合同有定向回归；AfterAttack 自身失败的源恢复行为尚未认证。

### D-008 局部费用列表采用等价压缩表示（已撤销）

- **状态**：已撤销；当前实现逐项保留原版列表顺序，容量由 `StaticVec` 按需增长。
- **游戏原文**：v0.111.0 `Entities/Cards/CardEnergyCost.cs:175-323` 的 Set/Add 方法向 growable `List<LocalCostModifier>` 追加每一项；`:94` 按顺序求值；`LocalCostModifier.cs:89` 的非 reduceOnly Absolute 无条件返回 Amount。`:331/:341` 依据 expiration 位删除，`:351` 逐项 cap Absolute.Amount，`:411` 深复制每一项。
- **我们的做法**：`CardModel::add_modifier` 逐项追加并按原版顺序求值、过期和深复制；没有 D-008 的删除/合并。
- **理由**：动态列表已消除触发该压缩的旧固定容量错误；保留私有列表形状可直接验证快照和 clone 语义。
- **影响范围**：旧报告中的压缩证据只描述历史版本，不能用于当前 schema；旧 schema 明确拒绝。

---

### D-007 相邻相同临时星数费用使用单个状态记录（已撤销）

- **状态**：已撤销；当前实现逐项保留原版记录，容量由 `StaticVec` 按需增长。
- **游戏原文**：`Models/CardModel.cs` 的 `AddTemporaryStarCost` 将每次调用追加到 List；`CurrentStarCost` 取最后一项，`GetStarCostThisCombat` 取首个不随回合/出牌清除的项目。升级按 Cost 条件过滤，出牌和回合结束按两类过期标记过滤。`StarCostChanged` 的订阅方是手牌显示 Flash 与 CombatStateTracker 的表现层通知。
- **我们的做法**：`CardModel::AddTemporaryStarCost` 逐次追加，保留 cost、两个清除标志和顺序；clone/过期按原版列表执行。
- **理由**：动态容量已覆盖四次及更长的合法序列，不需改变源表示。
- **影响范围**：旧报告中的相邻合并只适用于历史 schema；当前内部条数、快照和 hash 与源列表一致。

### D-011 动态列表与对象池容量

- **状态**：生效；已通过55个 CTest、真实保存的容量失败回放、C ABI/Python/.NET 大状态合同和资源预算测试。
- **原版事实**：游戏使用可增长 `List<T>`/对象集合；观察到的长局活牌数远超旧 C++ 固定1023槽位。
- **我们的做法**：`StaticVec` 小容量内联、超出后拥有型扩容；对象池使用地址稳定的分块存储，引用/计数为32位并在 INT32_MAX 处拒绝。快照、Hash、GC 和 clone 保留 live/free/cursor 语义。
- **影响范围**：仍受 `STS2_ENV_MAX_*` 宿主预算、JSON 结构深度64和32位引用/计数范围约束；这不是任意内存或跨构建迁移保证。

2026-10-02池存储改为最多8槽的拥有型稳定块，未用块尾不进入逻辑capacity、cursor、
hash或快照；没有新增规则偏离。70000引用、旧新池10000步随机摘要、6500张牌API/SDK、
真实旧容量失败1067决策及57CTest/L2/L3/L4复验通过，见[性能报告](../reports/2026-10-02-combatsolver-performance.md)。

### D-012 C# checked/unchecked 数值边界

- **状态**：生效；1332 个正版 DLL 数值探针逐行与 C++ 输出一致。
- **我们的做法**：显式实现 C# checked 聚合、unchecked int32 加减乘和列表计数边界，避免宿主编译器溢出策略改变规则；极值超出源 checked 范围时返回相同错误类别。
- **影响范围**：探针覆盖已审计费用、伤害、Poison、History、Dampen 和药水路径，不等于所有内容模型的穷尽数学证明。

### D-009 Unmovable 格挡历史采用同阶段计数

- **状态**：生效；限定当前单人按源 turn/side/round 前进的执行与选择前缀重放。
- **游戏原文**：`Combat/History/Entries/BlockGainedEntry.cs` 保留所有条目及 CardPlay 对象身份；`UnmovablePower.cs` 按 HappenedThisTurn、CardPlay.Player.Creature、Move props 排除当前 CardPlay 后计数。源码全部消费者审计只有 Unmovable 读取此类。
- **我们的做法**：POD 存最后写入阶段的总数、round/side/player-turn stamp 与 Clear epoch；临时 CardPlay 记自身同 stamp/epoch 的entry数，查询时减去。每次 CardPlay 对象重建，不以同卡身份冒充同次出牌。不能无条件在额外回合递增 player-turn 前丢弃源仍可见记录。
- **理由**：当前唯一消费者不读旧阶段条目；源正常阶段时间单向前进，旧阶段不会再匹配。保持无指针/trivial-copy及stationary base前缀重建，避免无上界堆历史。
- **影响范围**：认证 16 项原版无TestMode primitive、快照后继和原始整局失败；不认证原版私有历史列表相等、任意人工倒退阶段、任意中途 native capture。完整原件与限制见[8000批次报告](../reports/2026-09-30-natural-v2-rule-gaps.md)。

### D-010 怪物 StateLog 使用完整规则摘要

- **状态**：表示修复已通过正版DLL私有权重primitive、集成完整2000身份复验、最终52CTest/L2/L3/L4完整门禁和两条原始长局。2000原件中1993完整一致，2活卡容量和5旧采集非法第二次REROLL仍保留失败。
- **游戏原文**：v0.111.0 `MonsterMoveStateMachine.cs` 的 `List<MonsterState> StateLog` 每次记录追加；全部玩法消费者是 `RandomBranchState.GetStateWeight` 的 Contains、连续重复尾部、过滤IsMove后的末尾cooldown，以及 `Creature.StunInternal`、`IllusionPower`、`FlutterPower` 的Last。不存在1024回合规则上限。
- **我们的做法**：保存曾记录状态mask、曾记录Move mask、总记录数、Move数、最后状态、同状态后缀长度、各Move距最后Move的记录距离。注册状态ID唯一，Contains的对象身份与注册索引对应；冷却只在记录Move时推进。Count和后缀用int32可表示范围，重复阈值依旧转float比较。C++不存全部私有列表；Append顺序、权重callback调用及RNG抽取不变。
- **理由**：这些充分统计保存所有已审计玩法消费者的结果，并消除1024条日志容量错误，同时保持POD平凡复制和无堆状态。不能以丢弃旧条目冒充Contains，也不能将non-Move记录计入cooldown。
- **影响范围**：原版私有列表逐项查询/导出不是此表示的承诺；snapshot/hash结构改变，旧schema明确拒绝，不能跨版本当作同一字节状态。当前primitive覆盖123552权重查询、624 Last/Count边界、16388追加；两条HP10000注入长局4907/4907严格完整一致。没有认证所有人工历史、原版中途live映射或完整API；超过原版int32 Count明确失败。完整证据与门禁结果见[本项报告](../reports/2026-09-30-monster-history-summary.md)。

## B. oracle 与真实客户端的差异

oracle 在普通 .NET 进程里运行 `sts2.dll`，靠 Harmony 补丁跳过依赖 Godot 引擎的代码。**每个补丁都是"它不影响游戏逻辑"的一个假设。** 这些假设最终需要用真机对局来验证。

| 编号 | 补丁或设置 | 假设 | 验证状态 |
|---|---|---|---|
| O-001 | oracle 在 `Headless.EnableTestMode` 中打开 `TestMode.IsOn`，并在 `GameBootstrap` 中设置 `NonInteractiveMode.AutoSlayerCheck = () => true`；TestMode 的正式分支由 `ProductionBranches` 逐方法恢复 | TestMode 负责跳过节点、特效、音频和资源路径；非交互模式负责跳过等待与输入暂停。136 个 TestMode 读取点审计为 V/T/P/G，G 类逐项登记如下 | 已完成读取点审计；仍需 L3 对拍验证时序与结算影响 |
| O-002 | 原生函数表填满桩函数（返回 0 或空），引擎单例预先播种 | 游戏逻辑不依赖 Godot 原生调用的返回值 | 部分验证：L2 零分歧 |
| O-003 | `feat/oracle-combat` 的 headless 运行跳过视觉和音效：`RunManager.ClearScreens/FadeIn/FadeOut`、`VfxCmd`、`Creature.CreateVisuals`、奖励图标、`CombatStateTracker.NotifyCombatStateChanged` 等 | 纯表现层代码；TestMode 审计中归为 V，测试断言点归为 T | 已实现；战斗部分待 L3 验证 |
| O-004 | `GodotFileIo` 全部改成空操作，改用内存里的 `SaveManager`（`MockGodotFileIo`）；`SaveManager.SeenFtue` 通过 headless 补丁视为已看过 | 存档 IO 和新手引导不影响游戏逻辑；TestMode 读取审计中的 `ProgressSaveManager.SeenPopup` 同属引导状态 | L2 零分歧 |
| O-005 | `feat/oracle-combat` 的 `PreloadManager.Load*` 直接返回已完成的任务或空资源集合 | 资源预加载不影响游戏逻辑；审计中的资源读取归为 V | 待 L3 验证 |
| O-006 | `feat/oracle-combat` 的 oracle 使用单线程、先进先出的手动 `SynchronizationContext` 泵 | 与 Godot 主线程的续体执行顺序一致 | 待验证：`Task.WhenAll` 加延时的路径（如 `DoTurnEndCards`）是主要风险 |
| O-007 | `CreatureCmd.Kill` 在 TestMode 下跳过正式死亡结算（`RunManager.OnEnded`）、存档和游戏结束界面 | 正式分支混合 `OnEnded` 与音频/UI 节点，不能把单个 TestMode 读取直接 transpile 成 headless 等价物；会影响死亡结束局的结算记录 | 已登记；应拆分无 UI 的结算与表现层后再恢复 |
| O-008 | `RewardsSet.Offer` 在 TestMode 下调用 `testSelector`，没有选择器时自动领取全部奖励；正式版显示奖励界面等待玩家选择 | 奖励领取集合和完成状态改变游戏状态；正式分支依赖本地奖励 UI/玩家选择，不能直接 transpile | 已登记；需提供 headless 奖励选择适配 |
| O-009 | `CardSelectCmd.FromChooseABundleScreen` 在 TestMode 下固定返回 `bundles[0]`；正式版走本地界面或远程选择同步 | 整局记录（`OracleRun`）用 Harmony 前缀替换整个方法：保留 `IsEnding` 与空列表两个提前返回，其余由 oracle 的策略 RNG 选一个卡包，输出 `bundle_select` 决策与动作，相当于客户端的选包界面。模拟器侧是一次 `ChoiceKind::Bundle` 选择 | 已解决（整局记录）。`combat` 命令尚未安装该前缀 |
| O-010 | `RollingBoulderPower.AfterPlayerTurnStart` 在 TestMode 下立即对所有目标伤害；正式版由 `NRollingBoulderVfx.HitCreature` 信号按命中顺序驱动伤害 | 正式伤害顺序由特效信号决定，不能用同一 transpiler 在无节点环境中恢复 | 已登记；应提供保留命中顺序的无 UI 事件源 |
| O-011 | `Rng.Chaotic` 用墙钟时间初始化，服务于存档唯一 ID、动画、音效等非游戏逻辑 | RNG 账本不输出 `Rng.Chaotic` 的抽取；有种子、影响战斗的 `Rng` 仍按 `other` 记录 | 若要审计表现层随机数，需要单独的非确定性记录格式 |
| O-012 | 静默猎手起始牌 `NEUTRALIZE` 访问战斗视觉节点，`SURVIVOR` 会进入玩家选牌暂停；oracle 的 `combat` 目前没有选择器 | 随机策略跳过这两张牌，继续执行无需选择的动作 | 只影响静默猎手的 oracle 策略轨迹；选择机制（M1-g）完成后给 oracle 加选择器并移除这条 |
| O-013 | `Commands/TalkCmd.cs:15` 的 `TalkCmd.Play(LocString, Creature, VfxColor, VfxDuration)` 格式化本地化文本、读取快进偏好、创建并挂接 `NSpeechBubbleVfx` | `tools/Sts2Headless/Headless.cs` 用 Harmony 前缀直接返回 `null`，跳过整个表现层调用 | 无头进程没有 Godot 语音气泡节点；该方法不读写战斗状态、不抽 RNG。只影响语音气泡和其等待表现，不影响战斗逻辑；用于 `CULTISTS_NORMAL`、`SEAPUNK_NORMAL`、`GREMLIN_MERC_NORMAL`、`BYGONE_EFFIGY_ELITE` 的 oracle 对拍 |

| O-014 | `Headless.InstallLiveCombatPresentation` 为 TestMode 关闭的 live 根夹具安装 scoped CardPile 节点/重洗 VFX/伤害数字 VFX/低 HP 边框 VFX 前缀及 pile animation wait 后缀；夹具停止 replay diagnostic IO | 只跳过表现层 factory；原始 pile/events/Hook/伤害/RNG 照常运行，fromSilentAdd 的原 helper 仍执行；非空 tween 会拒绝，不跳过未知动画路径。低 HP factory 只创建场景，调用方接受 null；死亡/OnEnded 逻辑不跳过。观察 native PowerApplied/Poison 的夹具只读身份/时序，无玩法写入 | Ironclad/Cultists A0 的完整根、Strike 和两次 EndTurn（伤害/能力增长/重洗）差分通过；真实非致死玩家 Poison 的 T+1 抽牌后触发已观测。有限场景，尚无真机接受证明 |
| O-015 | `Crusher.BeforeDeath`、`Rocket.BeforeDeath` 在无头进程中访问不存在的 `NAudioManager.Instance`，导致死亡动作抛异常并留下中途状态 | `OracleCombat.InstallKaiserHeadlessPatch` 在 `combat` 和 `run` 两种 oracle 中跳过这两个只调用音频、背景动画及音乐参数的怪物方法；`CreatureCmd` 和其他战斗/死亡 Hook 照常执行 | 修正前原版 full-run 轨迹在 Crusher 死亡后的 597 决策出现中途状态；修正后新轨迹 603/603 决策与 Env 和逐决策快照恢复一致。只验收该有限路径，仍需真机接受验证 |

| O-016 | `Amalgamator.CombineStrikes/CombineDefends` 的异步 MoveNext 在无节点环境中访问 `NGame.ScreenShakeTrauma` | 只将每个方法恰好两处震动调用改为同参数空表现函数，保留 IL labels/blocks；原版删牌、两次 300ms 等待、合并卡和事件完成均继续执行。匹配数变化会拒绝安装，不假造 NGame 节点 | `AU44CBB604A0` 重采走到 Act 2 Boss 死亡，1505 个决策；采集端未再产生 NRE 或重复事件。逐点 C++ 结果见 2026-09-30 严格批次报告；仍不是实机接受证明 |

O-012 已撤销：`OracleCombat` 现在通过 `ICardSelector` 处理 Survivor 的选牌，也允许策略使用 Neutralize。Silent 起始牌的 16 场 L3 记录已逐点对齐。

### O-018 Trial 确认输入与放弃表现层

- **游戏原文**：DoubleDown 创建实际 Godot 弹窗；按钮调用原生 OnYes/OnNo。
  AbandonInternal 关闭顶层/地图，CreatureCmd.Kill 的正式失败分支访问音乐和结算界面，
  并执行 OnEnded。Accept 有本地画像/VFX 调用。
- **oracle 做法**：`trial-confirmation` 默认关闭 TestMode，使用
  NonInteractiveMode.AutoSlayerCheck 与内存存档。仅将 DoubleDown 的三个节点边界调用
  替换为输入适配，保留原生 DoubleDown 方法；按钮 key 从正版 `_Ready` IL 读取，
  对无节点对象调用原生按钮方法。IsProceed 路径直接 Chosen，其余经过原生同步器。
  Headless 中严格计数的 IL 适配只移除 Close、QueueFree、音乐、ShowGameOverScreen；
  原生 IsAbandoned 写入、Abandon/Kill、死亡 Hook、OnEnded 都执行并观测。
  Accept 的两个纯画像分支沿用已有 LocalContext 表现层适配。
- **理由/影响范围**：无 Godot 节点的输入与表现层桥接，不重建玩法输出、不补抽 RNG。
  本批直接注入第三幕事件根，覆盖确认、重复取消、取消后 Accept、Lizard Tail 强制死亡；
  不能当作实际游戏 UI 认证，也不证明四条 NoSL 完整历史已经在正版重放。

2026-09-30 采集驱动修正（不改 DLL 玩法规则）：FakeMerchant 复用事件自身 `Inventory` 与原生采购方法，采购后若房间/战斗/StartedFight 改变则返回外层；普通事件选择等待 `EventSynchronizer.AwaitPendingOptionTasks()`，持续泵同步队列并保留原生实际等待，60s 超时或异步故障明确失败。客户端 Exit 也等待该原生 barrier。旧事件驱动提前抓取中间状态的输出保留，不作为 C++ 正确性的真值。

同批 oracle 按 `CombatRoom` 身份重置“奖励已提供”标记，处理 FurCoat + FestivePopper 在首次观测 IsInProgress 前已赢得战斗的路径；客户端仍由原生胜利通知提供奖励，采集驱动不改变奖励生成。PenNib 的静止点 counter 按 `AttacksPlayed % 10` 比较：NonInteractiveMode 中 DoActivateVisuals 的等待内联完成，`AttackToDouble` 与视觉 `_isActivating` 是不同状态。真实客户端短暂显示 10 的动画时序尚未认证。

### O-017 reward reroll alternatives 的 headless 缓存资格

`CardReward.OnSelect` 为 testSelector 保留首次 alternatives；真实 `NCardRewardSelectionScreen.RefreshOptions(CardRewardAlternative.Generate(...))` 会在已重抽后移除 REROLL。coverage-v2保留历史缓存采样，第二次REROLL没有真实UI合法性；其旧证据不重写、不升级。API已按源拒绝禁用项。新默认coverage-v3绑定正在领取的原生CardReward，读取CanReroll，只采样可用的原索引并记录原生合法性事实；5个旧故障身份新采集4301/4301完整一致，实际7次重抽后的7次选择均排除REROLL，52CTest/L2/L3/L4候选门禁通过，见[采集与覆盖报告](../reports/2026-09-30-coverage-v3-legal-rewards.md)。自然8000历史批没有REROLL/SACRIFICE执行。该项仍限制旧driver/UI证据，未放宽游戏规则、API或比较。
