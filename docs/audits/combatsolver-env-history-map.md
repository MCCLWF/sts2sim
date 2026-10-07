# CombatSolver Env：原搜索事件历史读取与真实执行点

日期：2026-09-27。固定 upstream：`external/CombatSolver` 的 `d231e9e51a0e58d6bfa1373c6265cce13ffd9a45`。本表是 P2 trace 的实现清单，尚非事件等价证明。

## 不能从状态差重建的读取

| 原搜索读取 | 原生来源 | C++ 真实执行点 | 要保留的顺序/字段 |
|---|---|---|---|
| `CombatBeamSolver.Expansion.Candidates.BuildCandidate` 的 `History.EntriesFrom(before.HistoryEntryCount)` | `CombatPredictionHistory.Record`，只将 CardPlayStarted/Finished、CreatureAttacked、DamageReceived 判为 pure | 各事件发生处，尤其 `CardModel::Play`、`CreatureCmd::Damage`、`AttackCommand::Execute` | 原 `CombatPredictionHistoryEntry.cs` 中 **17 种**条目及原序。若只记录四类，会把原预测器已记录的抽牌/生成/选择等候选错误标为 pure；也不能自作主张给原预测器未记录的弃牌、能力变化增加条目。 |
| `CombatHistoryCounterKey.Append` 六项 | `CombatHistoryCounters.After` | 出牌完成、Orb channel、每次伤害记录、抽牌开始、生成开始 | FinishedPlays 对所有完成出牌加一；EtherealPlays 仅玩家的 ethereal；LightningChannels 仅玩家 Lightning；UnblockedHitsReceived 按玩家每条正 HP 伤害；CardsDrawn/Generated 按 owner/creator。根一旦含开放生成源，原政策保守启用全部六项。 |
| `SimulatedCombatState.CardEventHistory`、`CardLifecycle` | 原历史条目和预测 event sink | 出牌前后、抽牌、攻击、伤害、回复与牌堆变化 | 逐回合及累计量、来源身份、自动/重复出牌；不能从剩余 HP、牌数量反推。 |
| `CardChoiceSupport.FindLatestCardGenerationOptions` 与 choice continuation | `CardGenerationOptions`、`CardsSelected`、deferred `CardGenerated/Resolved` 和 `CardDrawn/Resolved` | 生成/抽牌开始、Hook 与选择之后的完成位置 | 同一原始条目与完成条目关联；暂停选择允许未完成项，恢复后不能双记。 |
| `StateEvaluation`、`Retention`、`LongTermResources`、`Potions` | `PotionUses`、HP loss/recovered、Anger 复制、成长、死亡保护及成本 | 对应药水消费、HP 原语、卡牌生成/牌组变化、遗物/能力触发 | 策略账本独立于 gameplay 聚合和净差，保留分支来源/回合和原战略费用。 |

## Emit 位置及已发现的缺口

| 事件族 | C++ 已有执行点 | 与原搜索的差异或待做 |
|---|---|---|
| Play started/finished | `game/Models/CardModel.cpp` 调用 `CombatHistory::CardPlayStarted/Finished`，顺序分别在 `OnPlay` 前、`AfterCardPlayed` 前 | 当前 `CombatHistory` 只更新 POD 聚合；sidecar 需卡实例、当时快照、CardPlay 的 player/target/resources/auto/index/count 与 ethereal。 |
| Damage received | `game/Commands/CreatureCmd.cpp` 在 Hook 后逐 `DamageResult` 调用 `CombatHistory::DamageReceived` | 当前只累计玩家正伤害次数和本回合标志；需每条 blocked/unblocked/overkill/kill、receiver/dealer/card/model 来源。来源种类不能一律猜为 Card。 |
| Creature attacked | `game/Commands/Builders/AttackCommand.cpp` 在所有 hit 完成后、`AfterAttack` 前拥有扁平化结果 | 游戏 `CombatHistory` 当前无此方法，原预测器在 `EndAttackContext` 记录。需显式记录；普通 `CreatureCmd::Damage` 不一定是攻击。 |
| Draw started/resolved | `game/Commands/CardPileCmd.cpp` 加入手牌后记录 Drawn，然后调用 `AfterCardDrawn` 与 `InvokeDrawn` | 原预测器有分开的 DrawResolved 条目。需在 Hook/选择完成后记录同一抽牌的完成；选择前缀重放必须丢弃本次未提交的局部事件。 |
| Generated started/resolved/options | `game/Commands/CardPileCmd.cpp` 放入牌堆之前记录 Generated，Hook 后可取得最终卡；`game/Commands/CardCmd.cpp` 替换牌另有记录 | 原历史还保存生成结果类型、选项、同一条目的完成关系。仅在 `CombatHistory` 加计数会丢这些政策语义。 |
| Exhaust/discard/affliction | `game/Commands/CardCmd.cpp` 已在相应 Hook 前调用游戏 `CombatHistory` | **只有 affliction** 对应原预测 `CombatPredictionHistoryEntry`；discard/exhaust 是原搜索的其他状态/事件 sink 读取，不能额外加入原 `History.Entries` 改变 pure 判断。 |
| Power/resource/block | `game/Commands/PowerCmd.cpp`、`CardModel.cpp` 能量消费、`CreatureCmd.cpp` 格挡、星能修改入口 | 原 `CombatPredictionHistoryEntry` 没有这三类。仍须核对状态/事件 sink 与评分消费者，但不能伪造 History 条目。 |
| Potion/orb/HP restore | 药水流程、orb channel、`CreatureCmd::Heal` | 原搜索还有政策账本与 orb/history 条目，必须逐项对应实际游戏命令和原预测 event sink。 |

`game/Combat/History/CombatHistory.h` 当前注释明确只保留已移植游戏模型读取的 POD 聚合；它不足以满足原搜索。新增事件只能在专用 prediction session 的侧账本里，普通 Env 的 POD/哈希/逻辑不能被改变。

`RiskEntry` 是**原预测器的政策判断**，不能仅从游戏 C++ 执行事件产生。例如上述 VoidForm 场景的游戏后继虽已对齐，原派生预测器仍记录 `MethodNotMirrored`。同算法适配须在对应的政策入口给 Env 分支补同风险，或明确拒绝该根；不能因为 Env 移植了对应效果就悄悄消掉原风险评分。

后续已在预测 scope 的 OnPlay 分派点加入固定默认 inference 政策的有限映射：Rampage 为 `MethodMirrorIncomplete`，VoidForm 为 `MethodNotMirrored`，位于 Started 之后、实际效果之前。v5 事件第 21 项保存可选风险原因，hash/严格恢复覆盖它；11 场原 Beam 对拍不再排除任何 Risk 条目，并逐项比较原因。其他模型分派、Hook、选择/抽牌/球数量限制和非默认 inference 设置仍未覆盖，因此完整历史标志与正式执行门禁不变。结果见[增长状态报告](../reports/2026-09-27-combatsolver-env-card-growth-state.md)。下方 v4/十场记录是此前的历史状态。

生成与选择历史还有两处不能按游戏命令统一打点。`CombatPredictionSimulator.CardPile.AddGeneratedCardsToCombat` 的 `CardGenerationResultKind` 是调用者传入的搜索政策信息，默认 Random，若干 Card/Potion/Power/Hook 路径显式传 Fixed 或 Contextual；游戏 `CardPileCmd.AddGeneratedCardsToCombat` 没有这个参数。C++ 只能在实际生成点记录卡与 creator，不能由卡 ID 或后继状态推断原 `ResultKind`。Env 同政策适配需在原调用点/等价政策层带入这一标注，无法对应时拒绝请求。`History.CardsSelected` 也只在原 `CardChoiceSupport`、若干 `CardSelectionCardMirrors`、`AfterCardPlayedMirrors` 和 `ReactiveRelics` 政策路径发出；游戏 `CardSelectCmd.Select` 的所有调用并不等于这些条目。直接在每次游戏选择追加 `CardsSelected` 会改变事件种类序列、候选 pure 判定与历史索引。

## 所有权和重放合同

事件侧账本随 owned prediction session Clone。普通动作从父账本复制、追加；选择 `Answer` 时 `Env::Run` 会把 gameplay state 回滚到动作起点并从答案前缀完整重放，所以事件账本也必须从**同一个动作起点**恢复后重新追加。暂停期间可以有原生预测允许的 deferred start；完成时按原条目身份配对。普通源、兄弟和失败分支的事件不能变。导出/恢复需携带完整事件、未完成关系和选择动作起点，独立哈希域须覆盖它们；不能把只有 `GameState` 的 v1 envelope 称为完整 trace。事件完成前，正式 Env 执行门禁继续关闭。

下一验收顺序：先对 `strike`、`enemy`、`fire_potion`、`poison` 和抽牌/选择夹具逐事件同序对拍，再核对六计数、纯度、HP 及战略账本；每新增事件族验证 `Answer` 重放、Clone、导出恢复以及普通 Env 无行为变化。`tools/EnvCombatPredictionChecks` 已有 TestMode=false 的同根原预测器夹具，可扩展为此差分。

已增加 `--native-only --out` 时的 `.native-history.json` 基线导出（条目序号/种类、已知卡 ID、伤害接收者/数值、风险原因和六计数；只是原派生预测器观察，不是 Env 对拍）。2026-09-27 执行 `~/.dotnet/dotnet run --project tools/EnvCombatPredictionChecks -c Release -- --native-only --out /tmp/mcc-p2-native-history`，十个场景均产出：`strike` 两次 PlayStarted→DamageReceived→CreatureAttacked→PlayFinished，六计数中的 FinishedPlays=2；`enemy` 为 DamageReceived→CreatureAttacked 且 UnblockedHitsReceived=1；`phase_one` 仅一次 DamageReceived；`fire_potion` 五条（首张攻击四条及药水一次 DamageReceived）；三个下一回合玩家中毒场景各五对 Drawn/DrawResolved 后接一次 DamageReceived，CardsDrawn=5；`forced_end` 的 VoidForm 后还有 `MethodNotMirrored` RiskEntry，说明不能把语义状态对齐等同于政策风险对齐。原始 JSON 位于 `/tmp/mcc-p2-native-history/`，不作为已实现的 Env trace。

2026-09-27 的 C++ prediction-only sidecar 已记录 Started/Finished、DamageReceived、CreatureAttacked、Drawn/Resolved、Generated/Resolved 与 OrbChanneled 的部分 scalar 字段；`sts2env.combat_prediction` v4 导出事件、六计数、卡牌/出牌快照、伤害来源 kind/ID、攻击命中结果及选择动作基线，独立 hash 覆盖它们。选择 Answer 从动作基线重放，普通游戏快照中的待选择状态因无政策基线被拒绝。TestMode=false 的十场原预测器对拍：忽略**已知未实现**的 VoidForm `MethodNotMirrored` RiskEntry 后，事件种类顺序、六计数及已列出的卡牌/出牌/伤害/攻击字段一致。最初终击缺失来自游戏 `CreatureCmd` 的 history 结束保护；预测作用域已补终击，普通 Env 保持原行为。原预测器的 Self 卡目标身份与游戏执行参数不同，预测专用账本按原政策保留自身目标。FirePotion 的原搜索来源为 Unknown，Env 保持相同值而不从模型类型猜测。未测试的其他来源可能仍缺失；生成结果分类/选项、选择条目、能力/资源/药水策略账本尚未覆盖，因此 v4 仍明确给出 `history_projection_complete=false`，不能解除正式 Env 搜索门禁。
