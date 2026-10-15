# 移植坑点清单

这里收集容易移植错的游戏语义。移植相关代码之前先查一遍；发现新坑就加一条。

每条标明来源：
- **已核实**：我们在源码或 oracle 上确认过。
- **sts2-ai**：来自 sts2-ai 的偏离登记、路线图或提交记录，**还没在我们这边核实**。移植到相关代码时，先对照源码确认，确认后改成"已核实"。

---

## 1. 数值与容器

| 坑 | 正确做法 | 来源 |
|---|---|---|
| decimal 乘积为零时的刻度 | 只有走 32×32 位快速乘法路径时才保留刻度，其他情况返回普通的 0 | 已核实（L1 golden） |
| decimal 加减得到精确零时的符号 | 按 .NET 选中的操作数决定符号，见 `core/dec.cpp` | 已核实（L1 golden） |
| `StableShuffle` 相等元素的顺序 | 先用 .NET introsort（`dotnet_sort`）按 (ModelId, 升级等级) 排序，再洗牌。用 `std::sort` 时，相等元素会落在不同位置 | 已核实（L0 golden） |
| `HashSet` 遍历顺序 | 按槽位顺序遍历；删除后空出的槽位进 LIFO 空闲链表，下次插入先复用它（`DotnetSet`）。`Creature.SetUniqueMonsterHpValue` 依赖这一点 | 已核实（L2 地图） |
| POD 状态哈希 | 不哈希原始对象字节：padding 不受值初始化/赋值的确定性保证。按字段编码，忽略未用存储，保留未来可观测的分配游标及 HashSet 空闲链；挂起选择包含重放快照 | 已核实（独立 managed session 复现、padding 污染用例） |
| `Sum<float>` | 用 double 累加，最后转回 float | 已核实（L0） |
| `NextGaussianInt` | 用 `Math.Round` 取整，即银行家舍入（`nearbyint`） | 已核实（L2 地图） |
| 浮点表达式 | 编译选项必须带 `-ffp-contract=off`，否则 FMA 合并会改变结果 | 已核实 |
| `next_item_index` 遇到空集合 | 返回 -1，并且**不抽随机数** | 已核实（L2） |

## 2. 开局与地图

| 坑 | 正确做法 | 来源 |
|---|---|---|
| 种子派生 | `runSeed + XxHash64(snake_case 名)`，名字必须和 C# 枚举的 snake_case 完全一致 | 已核实（L2） |
| 各幕的问号房数量 | Overgrowth 和 Underdocks 用 Gaussian(7,1,6,7)；Hive 用 Gaussian(6,1,6,7) 再减 1；Glory 用 NextInt(5,7) 再减 1。精英数量在"蜂群"时是 `round(5*1.6f)` | 已核实（L2） |
| Hive 和 Glory 的遭遇生成顺序 | 按 `AllEncounters` 的源码顺序 | sts2-ai（L2 覆盖了 Hive 和 Glory 的开局，应该已经隐含核实） |
| `CardRarityOdds.regularCommonOdds` 是静态字段 | 只在进程内求值一次；oracle 批量跑多局时要确认它不在局与局之间带状态 | 已核实（风险提示） |
| 地图UI的Boss边界优先于自由旅行 | `NMapScreen.RecalculateTravelability` 先处理第二Boss及普通网格最后一行→Boss，再调用`MapTravel`；带WingedBoots直接用`GetPointsInRow`会漏掉网格外Boss节点。无头输入适配须复现实际UI优先级，不改旅行/遗物逻辑 | 已核实，v0.111.0 NMapScreen/ActMap/MapTravel；Q4-ab 正常NQ4ABSUPPUPD000000 |

## 3. 战斗与 Hook

| 坑 | 正确做法 | 来源 |
|---|---|---|
| Hook 监听者的顺序 | 按 `CombatState.IterateHookListeners` 的顺序遍历：先我方后敌方；每个生物依次是能力 → 怪物模型或（遗物 → 药水 → 充能球 → 手牌 → 抽牌堆 → 弃牌堆 → 消耗堆 → 打出区，每张牌后面跟它的苦难和附魔）；最后是修饰器 | 已核实（源码） |
| 监听者的存活检查 | 列表在遍历开始时就生成快照，但每一项在轮到它时才检查 `Contains()`：卡牌已被移出战斗、能力的持有者已离开战斗、怪物已死，都会被跳过 | 已核实（源码） |
| 战斗结束后的 Hook | `IsOverOrEnding && !IsStarting` 时，`IterateCombatHookListeners` 不产出任何监听者。例如最后一个敌人死后，`AfterCardPlayed` 不会触发。少数 Hook（如 `AfterBlockBroken`）直接调用 `IterateHookListeners`，不受这条影响 | 已核实（源码） |
| `CardModel.BaseStarCost` lazy getter | mutable 卡首次读取时写入 canonical cost 并置 `_starCostSet=true`；live mapper 不调用 getter，执行预测复现写入；LegalActions 在副本上查询避免改变源分支 | 已核实（正版整根 Strike 后继差分） |
| 本回合免费包括星数 | `SetToFreeThisTurn` 同时执行 `EnergyCost.SetThisTurnOrUntilPlayed(0)` 和 `SetStarCostThisTurn(0)`；星数临时项在出牌或回合结束清除。X-star getter 仍优先返回当前星数 | 原版 CardModel.cs、TemporaryCardCost.cs；2026-09-30 Crossbow/Devastate/CrescentSpear 三条首因 |
| 视觉卡池能影响规则筛选 | HeirloomHammer 使用 `VisualCardPool.IsColorless`；Colorless/Event/Deprecated/Token 均属无色，但 Clash 等事件池卡的视觉池覆写为角色池。不能替换成实际 Pool==Colorless | 原版 HeirloomHammer.cs、CardPoolModel 及十个 VisualCardPool 覆写；2026-09-30 两条储君后续首因 |
| 直接 Damage 仍可能属于攻击组 | Omnislice 与 EchoingSlash 用 AttackContext 将多个 CreatureCmd.Damage 纳入一次 BeforeAttack/AfterAttack，完整保留每段 Results；遗漏上下文会留下 Vigor。NeedChoice 是暂停，C++ 不能在析构时把它当作完成攻击 | 原版 AttackContext.cs 与两个 CreateContextAsync 调用点；2026-09-30 新固定长局及定向回归 |
| Claws 是批量保留属性的变牌 | 先为所有选择构造 Maul，保留升级及兼容附魔的 mutable clone，再一次 Transform；不能逐张创建普通 Maul 后单独 Transform | 原版 Claws.CreateMaulFromOriginal/AfterObtained；2026-09-30 亡灵新固定长局 |
| 移除能力不等于立即释放对象 | Hook 和遗物可能仍保留已移除对象；动作内或挂起选择不能回收。静止边界联合追踪生物/能力，保留 active-card-play capture、UnsettlingLamp 的首个 doubled power、卡牌 target、Queen 的 amalgam 与 Dampen 的 casters；无引用的 retired power 不应累计占满池 | 原版 PowerModel.RemoveInternal、UnsettlingLamp.cs、DampenPower.cs；2026-09-30 274 回合 Decimillipede 场景 |
| 抽牌堆的顶部 | 下标 0 是牌堆顶 | 已核实（源码） |
| 手牌满了 | 上限 10 张；`CardPileCmd.Add` 往满手牌里加牌时，牌会进弃牌堆 | 已核实（源码） |
| 普通弃牌堆重洗 | 重洗前要先做 `StableShuffle` 的预排序（`CardPileCmd.cs:1085`） | sts2-ai（#40） |
| 召唤新敌人 | 加入后调用 `SortEnemiesBySlotName`，按位置名重新排序敌人 | sts2-ai（TS-02） |
| `DoTurnEndCards` | 里面有 `Task.WhenAll` 加延时，交互模式和无头模式的执行顺序可能不同 | 已核实（风险提示，待 L3 确认） |
| 对 async 方法挂 Harmony Postfix | Postfix 在 `Task` 完成前就执行，拿到的是中间状态。oracle 记录状态应该在静止点做（泵空队列之后），不要挂在 async 方法上 | sts2-ai（08b-4 schema 3.7 修订） |
| 泵空队列不等于事件 option 完成 | `ChooseLocalOption` 将 async 任务交给 TaskHelper；等待实际延迟时队列暂时为空。记录下一决策前等待原生 `AwaitPendingOptionTasks` 并持续 pump，使用单调时钟有界等待且显式报告异步异常；不能跳过延迟中的玩法逻辑 | 2026-09-30 Amalgamator 重采；原版 EventSynchronizer / EventRoom.Exit / NEventRoom |

## 4. 怪物状态机

| 坑 | 正确做法 | 来源 |
|---|---|---|
| `RandomBranchState.AddBranch` 的重载 | 共 10 个重载，整数参数的含义取决于重载：`AddBranch(state, int maxRepeats)`、`AddBranch(state, int cooldown, MoveRepeatType)`、`AddBranch(state, int cooldown, int maxRepeats, Func<float>)` 等。**都不是权重。** 必须照源码逐字写调用 | 已核实重载列表；sts2-ai 在 TwigSlimeM、Flyconid、FakeMerchantMonster 上读错过 |
| 固定血量的怪物 | 也要消耗一次 Niche 随机数（生成唯一血量的流程照走） | sts2-ai（MSA-04） |
| Tunneler 的眩晕回调 | 注意回调的注册与触发时机 | sts2-ai（MSA-08） |
| TwigSlimeM 的招式重复语义 | 看 `MoveRepeatType` 的具体含义 | sts2-ai（8F 局的分歧） |

## 5. 房间、奖励与商店

| 坑 | 正确做法 | 来源 |
|---|---|---|
| EventOption 的两个 bool 与 IsProceed | 原版重载按 `(disableOnChosen, isProceed)`，C++ 包装按 `(isProceed, disableOnChosen)`，翻译时明确参数名。Trial.DoubleDown 原版 `(false,true)` 可反复打开弹窗；IsProceed 只让 NEventRoom.OptionButtonClicked 直接 await Chosen，不清按钮、不经过 shared vote/page increment，也不表示回地图。确认才 RunManager.Abandon → IsAbandoned → Kill(force:true)，取消仍在 Reject 页 | v0.111.0 EventOption、NEventRoom、Trial、RunManager；2026-10-05 TestMode=false 原生按钮/连续取消/强制死亡差分 |
| 问号房的商店黑名单 | 根据上一次进入的商店和"子节点全是商店"的情况构建黑名单 | sts2-ai |
| 卡牌选择药水 | 从完整的药水池洗牌之后再选 | sts2-ai（issue 22，`87a3f03d`） |
| 补药循环必须检查获取失败 | EntropicBrew 先生成药水（消耗 RNG），再调用 TryToProcure；失败时 break。持有 Sozu 的玩家可以使用此前携带的熵之酿，空槽并不保证允许补药；遗漏退出条件会无限生成 | 原版 EntropicBrew.cs、Sozu.cs；2026-10-05 NT100652020008441 与 TestMode=false DLL 探针 |
| ScrollBoxes 的卡包生成 | 照源码的生成方式 | sts2-ai（issue 21，`58ea593c`） |
| 创建选项的 record 可原地修改 | `WithCardPools/WithFlags` 返回同一对象；DingyRug/PrismaticGem 改池后，整批生成与 LastingCandy 必须看到修改，普通 CardReward 的 Options/RerollOptions 共享修改。LastingCandy 另建的 options 重新设置 flags，不继承父 NoUpgradeRoll | 原版 CardCreationOptions/CardFactory/CardReward/LastingCandy；2026-09-30 三条奖励首因及 TestMode=false primitive |
| 最终幕的 Boss | 不发奖励 | sts2-ai（#311） |
| 宝箱遗物的获得 | `TreasureRoomRelicSynchronizer` 只发 `RelicsAwarded` 事件；真正 `RelicCmd.Obtain` 的是 UI 节点 `NTreasureRoomRelicCollection.AnimateRelicAwards`。无头 oracle 要自己订阅这个事件，否则遗物永远拿不到 | 已核实（M1-i） |
| 宝箱的顺序 | 客户端 `NTreasureRoom.OpenChest`：先 `DoNormalRewards`（金币，玩家 Rewards 流 42..52）和 `DoExtraRewardsIfNeeded`，再显示遗物；进房间时 `BeginRelicPicking` 已经抽完遗物稀有度（`TreasureRoomRelics` 流）和遗物（共享遗物袋） | 已核实（M1-i） |
| 商店删牌的"已用" | `MerchantCardRemovalEntry.SetUsed` 只由 UI 节点调用（买完删牌、或者 `ShouldAllowMerchantCardRemoval` 为假时）；无头 oracle 要自己调 | 已核实（M1-i） |
| 商店卡牌打折 | `SetOnSale` 会**再调一次** `CalcCost`，也就是再抽一次 Shops 流 | 已核实（M1-i） |
| 事件里的奖励界面 | `RewardsCmd.OfferCustom` 会等奖励界面关掉，之后的代码才继续执行（TestMode 下 `testSelector` 在 `Offer` 里内联执行）。模拟器把它做成嵌套选择，不是顶层阶段 | 已核实（M1-i） |
| 卡的 Owner 和生成者不同 | PersonalHive 的 Dazed 和 TheInsatiable 的 FranticEscape 属于目标玩家，但 `AddGeneratedCardToCombat` 的 creator 为 null；误传 Owner 会额外触发 RocketPunch 降费与 Supermassive 生成伤害 Hook | v0.111.0 PersonalHivePower.cs、TheInsatiable.cs；2026-09-30 多角色首因 |
| Deck 到 Deck 的变换也触发获得类 Hook | BingBong 只看新 pile、owner 和 clone 抑制，不检查 oldPile；HATCH 的 Egg→Swoop 会复制一张，LuckyFysh 对两张各给 15 金 | 原版 BingBong.cs、LuckyFysh.cs、CardCmd.Transform；2026-09-30 两条 HATCH 首因 |
| 入场就结束的战斗也有房间奖励 | oracle 不能只在观察到 IsInProgress 时重置“奖励已提供”标记；按 CombatRoom 身份重置，才能处理 FurCoat + FestivePopper 在首回合消灭全部敌人的情况 | v0.111.0 NCombatUi.OnCombatWon；2026-09-30 AU9954CEBF37 |
| 验证后继之前保留当前决策 | CLI 的快照后继回放可能因 oracle 动作与当前阶段不同而抛错。先输出当前真实决策，再验证后继，才能保留最早的阶段分歧 | 2026-09-30 replay_cmd；故障协议测试 |
| 事件选两张牌的 Add 重载决定 Hook 顺序 | RoomFullOfCheese 调用的原生 helper 逐张 await Add，BingBong 因而产生 A,A,B,B；不要把 selected 集合一次传给批量 Add。ZenWeaver 原版明确批量 Add，仍保留其集合语义 | EventModel.SelectCardsToAddToDeckFromGrid、RoomFullOfCheese.Gorge；2026-10-01 两条原始故障完整回归 |
| 单次休息采集策略可能在嵌套奖励之后完成 | CLI 的 OracleRun 策略须在 reward/proceed 返回 RestSite 后继续一次 Proceed，使用 last_chosen/proceed_enabled 确认选项已完成；公共 Env 的 MiniatureTent 仍允许额外操作，不能为对拍关闭剩余选项 | OracleRun restHandled、TinyMailbox、MiniatureTent、NRestSiteRoom.ShowProceedButton；2026-10-01 原始故障整局回归 |
| 脚本可连续选择休息选项 | MiniatureTent 保留其余选项，已选项删除后索引重排；脚本再次 rest 须重新检查原版 Options/IsEnabled。实际后续 decision=rest 用离线 rest_hold 标记保留界面，不能让旧一次策略的自动 Proceed 提前关闭；嵌套选牌/奖励仍按原顺序返回，不修改 Env | 原版 RestSiteSynchronizer.ChooseOption、MiniatureTent、NRestSiteRoom.AfterSelectingOptionAsync；Q4-w NQ4VEVAL000330 正常完整538决策与快照 |
| 上一玩家回合的最后攻击牌不能只存一条全局 latest | Hellraiser 可在新回合初始抽牌阶段自动打出攻击牌，早于 HistoryCourse 的 AutoPrePlay 查询；须保留精确 `TurnNumber - 1` 的最后非 Dupe Attack。敌方阶段的记录也按玩家 TurnNumber 匹配，不能擅自增加 CurrentSide 条件；无攻击回合不得回放更早记录 | HistoryCourse.AfterAutoPrePlayPhaseEntered、CombatHistoryEntry.HappenedLastPlayerTurn；2026-10-01 新 seed 第934决策故障 |
| 战斗外死亡也结束整局 | `CreatureCmd.Kill` 在所有玩家死亡时无论是否在战斗中都调用 `RunManager.OnEnded(isVictory: false)` 并显示结算界面。移植版原先只在 `AfterCombatAction` 处理战斗死亡；事件扣血致死（RoomFullOfCheese 的 Search）后回到地图，带着死亡玩家进入下一场战斗会在自动结束回合处死循环。顶层动作结束时由 `RunFlow::SettleGameOver` 统一置为 Ended。采集策略会避开 `WillKillPlayer` 选项，所以 L4 对拍一直没有覆盖这条路径 | 原版 CreatureCmd.Kill、RunState.IsGameOver；2026-10-03 NOSLDEV0051 随机策略挂起首因 |
| 只管显示的状态不在 oracle 对拍范围里 | `RelicStatus`、选牌请求来自哪个 `CardSelectCmd` 方法，这些不影响规则，L2–L4 不比较，移植错了门禁也是绿的；但它们是玩家看得到的，会进 NoSL 观察。已发现并修正：Silver Crucible 的状态要在 `TimesUsed` 改变时就检查（setter 里的 `CheckIfUsedUp`）；Kusarigama 的 `AfterCombatEnd` 把 `IsActivating` 置假时 setter 会按攻击计数重算状态；Spiraling Whirlpool、Sapphire Seed、Self Help Book、Wood Carvings 的 Snake、Electric Shrymp 选牌走 `FromDeckForEnchantment` 而不是 `FromDeckGeneric`。移植带 setter 的属性时，setter 里的副作用（`UpdateDisplay`、`CheckIfUsedUp`）也要照搬 | 实机逐决策观察对照（`tools/NoslPilot`，[Q5-b](../plans/2026-10-04-nosl-q5b-direct-observation.md)） |

## 6. oracle（无头运行）

- **教师自动选牌没有 selector 回调**：原版 `CardSelectCmd.FromHand` 在非手动确认且候选数不超过 MinSelect 时直接返回全部候选。离线消费者必须观察真实结果并逐项核验完整卡牌状态、来源/选项 occurrence、数量和顺序；不能清空未消费 token。可选观察器放在原版异步体，保留公开入口的调用栈；包住入口会将公开选择来源 `FromHand` 误记为 `FromHandForDiscard`。Q4-t 原成功八条逐字控制专门覆盖此元数据，L4 本身不比较它。
- **替代路线预算与奖励屏边界**：教师替代动作继续累计原版决策数，不能用旧学生 script.Count 提前截断。独立有界预算只控制宿主，终点仍要求全部计划、准确 HP、普通清理/奖励 RNG。原版 `RewardsSet.Offer` 仅对 CombatRoom 设置 terminal，`NRewardsScreen.OnProceedButtonPressed` 仅在 terminal Boss 屏投下一章票；自定义休息/事件奖励的 Skip 不得留下终局 Proceed 标记。TinyMailbox 第16层自定义奖励会使旧标记跨过稍后 Boss 奖励处的 ScriptEnded，产生无玩家动作的错误章节跳转（Q4-t NQ4S000008，329决策一致后终点act漂移）。

| 坑 | 正确做法 | 来源 |
|---|---|---|
| TestMode | 会改变游戏逻辑（CallingBell、Cauldron、RollingBoulder 等）；sts2-ai 统计出 140 个 TestMode 读取点。**不要用**，用 `NonInteractiveMode.AutoSlayerCheck` | 已核实，sts2-ai 也独立发现了 |
| Pandora变牌后的检查屏幕 | `PandorasBox.AfterObtained`先完成全部原始Transform/获得Hook/Niche RNG，后调用唯一的`NSimpleCardsViewScreen.ShowScreen`；后者只建卡牌查看节点、音效与关闭按钮。无头仅跳过这个纯展示工厂，不跳过遗物效果或吞异步异常 | 已核实，v0.111.0 PandorasBox/NSimpleCardsViewScreen；Q4-ab 正常DARV轨迹NQ4ABPILOT000007 |
| VoidFormPower 的视觉命名 helper | `HideTemporaryZeroCostVisual()` 会把 `Data.cardsPlayedThisTurn` 写为 `999999999`，这个数被能量与星数费用 Hook 读取，必须移植。只有 `_vfx` 节点与激活调用是纯表现 | 正版 `VoidFormPower.cs` 源码逐项核实；差分结果见终局 scope 报告 |
| `FieldInfo.GetValue` | 会触发类型的静态构造函数，而后者可能分配 Godot `StringName` 导致段错误。改用 DynamicMethod 直接 `ldfld` | 已核实 |
| `GodotFileIo` 的 out 参数 | 原生桩函数会留下不确定的垃圾值，所以每个方法都要打补丁并返回确定的值 | 已核实 |
| Harmony 补丁遇到重载 | `AccessTools.Method(type, name)` 遇到同名重载会抛 `AmbiguousMatchException`，要按参数类型指定。`CardPileCmd.GetTweenForCardsChangingPiles` 就有两个重载 | 已核实 |
