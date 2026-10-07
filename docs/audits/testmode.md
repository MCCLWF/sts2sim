# TestMode / NonInteractiveMode 读取点审计

## 汇总

审计对象是 `$STS2_SRC` 指向的 `src/Core`，排除 `Nodes/` 和 `TestSupport/`。按源码目录执行：

```bash
cd "$STS2_SRC"
rg -n --glob '*.cs' -g '!Nodes/**' -g '!TestSupport/**' 'TestMode\.(IsOn|IsOff)' .
```

该命令得到 **136 行、136 个读取点**。分类为：V 表现/平台 **106**，T 测试接口 **18**，P 档案与引导 **4**，G 游戏逻辑 **8**。

G 类读取点：

- `Entities/Merchant/MerchantPotionEntry.cs:67` `CalcCost`：正式版 Shops 流价格浮动，已恢复。
- `Rewards/RewardsSet.cs:172` `Offer`：测试选择器/自动领取全部奖励，未恢复，登记 O-008。
- `Runs/RunManager.cs:1715` `UpdatePlayerStatsInMapPointHistory`：正式版写入地图历史金币与生命统计，建议恢复。
- `Commands/CreatureCmd.cs:482` `Kill`：正式版死亡结算、结束运行并显示游戏结束界面，未恢复，登记 O-007。
- `Commands/CardSelectCmd.cs:978` `FromChooseABundleScreen`：TestMode 固定首个卡包，未恢复，登记 O-009。
- `Models/Relics/CallingBell.cs:39` `GenerateRewards`：固定遗物改为正式稀有度抽取，已恢复。
- `Models/Relics/Cauldron.cs:40` `GenerateRewards`：固定药水改为正式药水池抽取，已恢复。
- `Models/Powers/RollingBoulderPower.cs:35` `AfterPlayerTurnStart`：测试直接伤害，正式版由 VFX 命中信号驱动，未恢复，登记 O-010。

同一逻辑点的多个读取（若有）仍按每个源码读取逐行列出；本次 G 点均为单行读取。

## 正常规则 NoSL 路径（2026-10-05 Q4-n）

`Sts2Oracle run --oracle-test-mode off` 和 `tools/NoSLTeacherHarness` 保持全局
TestMode=false，使用真实本地化与 AutoSlayerCheck。正常房间、奖励、死亡、遗物和
地图历史规则执行原版代码；输入由既有 selector/同步器提供。旧默认 on 模式及其历史
门禁仍按上面 O 编号限定，不重新声明为正常规则证据。

`Sts2Headless/ProductionVisualReads.cs` 显式列出 94 个方法组、98 个 V 读取，逐个检查
正版 DLL 的 getter 数量，只替换视觉分支判定，保留方法内前后的游戏效果。G/T/P 读取
不替换，ModManager、NonInteractiveMode、SaveManager.ConstructDefault 也不替换。
另跳过原版接受 null 的卡牌附魔、眩晕、Serpent Form 视觉工厂和纯表现的 ThinkCmd；
这些工厂/命令没有伤害或游戏回调。原有 NCard.Create 工厂补丁必须先于调用方编译安装，
否则 JIT 内联可保留旧 NodePool 调用、在 RemoveFromDeck 中间中断真实效果。
Deferred combat UI 通知在玩家已经移出状态时返回 null，保留存在玩家时的
RecalculateCardValues 及 StateChanged 订阅者。后台游戏任务异常明确终止回放，拒绝标签。

原始教师的 display builder 跳过无标题的正版 Mocks powers；真实内容缺标题仍拒绝，
不改 LocString.Exists 或全局模型表。教师采用原始补丁审计的 gameplay-neutral 声明，
只在搜索作用域给已审计的 Headless 类型提供元数据并恢复，其他未知补丁继续拒绝。
`RollingBoulderPower` 的正常规则伤害由视觉 HitCreature 回调产生，属于 G 点；新路径
没有改成测试分支或吞掉伤害。缺少场景树时明确失败，相关轨迹不能作为完整正常规则资格。
具体证据、失败和覆盖范围见 [Q4-n 报告](../reports/2026-10-05-nosl-q4n-production-teacher-root.md)。

## TestMode 逐点表

| 文件:行号 | 所在方法 | 类别 | 说明 |
|---|---|---|---|
| `Modding/ModManager.cs:89` | `Initialize` | V | TestMode 下跳过普通 mod 初始化（测试初始化器另行处理） |
| `Modding/ModManager.cs:165` | `ResetForTests` | T | 仅测试接口或测试断言，正常游戏流程不会调用 |
| `Modding/ModManager.cs:797` | `LoadMod` | V | TestMode 下不加载 mod DLL |
| `Modding/ModManager.cs:814` | `LoadMod` | V | TestMode 下使用测试初始化器标记 mod 已加载 |
| `Modding/ModManager.cs:819` | `LoadMod` | V | TestMode 下不加载 Godot PCK |
| `Modding/ModManager.cs:840` | `LoadMod` | V | TestMode 下不执行正式 mod 程序集初始化 |
| `Modding/ModManager.cs:870` | `LoadMod` | V | TestMode 下改用测试初始化器 |
| `Entities/Creatures/Creature.cs:397` | `CreateVisuals` | V | TestMode 返回空视觉节点 |
| `Entities/Creatures/Creature.cs:760` | `GetCreatureNode` | V | TestMode 不查找战斗中的生物节点 |
| `Entities/Creatures/Creature.cs:792` | `GetVfxContainer` | V | TestMode 返回空前景 VFX 容器 |
| `Entities/Creatures/Creature.cs:809` | `GetBackVfxContainer` | V | TestMode 返回空背景 VFX 容器 |
| `Assets/PreloadManager.cs:50` | `LoadMainMenuEssentials` | V | TestMode 跳过主菜单必需资源预加载 |
| `Assets/PreloadManager.cs:72` | `LoadMainMenuAssets` | V | TestMode 跳过主菜单资源预加载 |
| `Assets/PreloadManager.cs:80` | `LoadRunAssets` | V | TestMode 跳过局内资源预加载 |
| `Assets/PreloadManager.cs:92` | `LoadActAssets` | V | TestMode 跳过幕资源预加载 |
| `Assets/PreloadManager.cs:137` | `LoadRoomAssets` | V | TestMode 跳过房间资源加载 |
| `Assets/PreloadManager.cs:214` | `GetCombatAssetPaths` | V | TestMode 不生成战斗资源路径 |
| `Logging/Logger.cs:83` | `GetIsRunningFromGodotEditor` | V | TestMode 视为非 Godot 编辑器运行 |
| `Combat/CombatState.cs:605` | `GodotTimerTask` | V | TestMode 用 Task.Delay 代替 Godot 计时器 |
| `Combat/CombatStateTracker.cs:205` | `NotifyCombatStateChanged` | T | TestMode 对后端订阅 CombatStateChanged 直接抛出测试断言 |
| `Localization/Fonts/FontControlUtils.cs:13` | `ApplyLocaleFontSubstitution` | V | TestMode 跳过表现层、平台接口或日志调用 |
| `Rewards/RewardsSet.cs:172` | `Offer` | G | TestMode 调用测试选择器或自动选取全部奖励；正式版显示奖励界面 |
| `Helpers/OneTimeInitialization.cs:51` | `ExecuteVeryEarly` | T | TestMode 读取测试设置存储 |
| `Rewards/Reward.cs:100` | `CreateIcon` | V | TestMode 不创建奖励图标 |
| `Rewards/PotionReward.cs:65` | `CreateIcon` | V | TestMode 不创建药水奖励图标 |
| `Entities/Merchant/MerchantPotionEntry.cs:67` | `CalcCost` | G | 正式版抽 Shops 流价格浮动；TestMode 跳过该抽取（已由 ProductionBranches 恢复） |
| `Helpers/NonInteractiveMode.cs:14` | `IsActive getter` | V | TestMode 令非交互模式恒为 true |
| `Platform/Null/NullLeaderboardStrategy.cs:178` | `Read` | V | TestMode 不读取排行榜存档 |
| `Platform/Null/NullLeaderboardStrategy.cs:197` | `Write` | V | TestMode 不写排行榜存档 |
| `Saves/Managers/ProgressSaveManager.cs:856` | `SeenPopup` | P | TestMode 将新手提示视为已看过 |
| `Localization/LocManager.cs:269` | `SmartFormat` | T | TestMode 遇本地化格式错误直接抛出，正式版记录并返回原文 |
| `Localization/LocManager.cs:354` | `SetLanguageInternal` | V | TestMode 延迟触发语言变更回调 |
| `Models/AbstractModel.cs:2469` | `NeverEverCallThisOutsideOfTests_SetIsMutable` | T | 仅测试模式允许清除模型可变性保护 |
| `Entities/RestSite/MendRestSiteOption.cs:136` | `OnSelect` | V | TestMode 跳过休息点治疗特效 |
| `Entities/RestSite/RestSiteOption.cs:55` | `Generate` | T | TestMode 使用注入的测试选项生成器 |
| `Saves/SaveManager.cs:183` | `ConstructDefault` | V | TestMode 使用内存 MockGodotFileIo |
| `Saves/SaveManager.cs:301` | `IncrementNumReloads` | V | TestMode 不写正式重载存档 |
| `Runs/RunManager.cs:473` | `InitializeShared` | V | TestMode 禁用联机校验和 |
| `Runs/RunManager.cs:488` | `InitializeShared` | V | TestMode 禁用战斗回放写入 |
| `Runs/RunManager.cs:779` | `ShouldApplyTutorialModifications` | P | TestMode 不应用首局教学修改 |
| `Runs/RunManager.cs:1003` | `TryGetRoomTypeForTutorial` | P | TestMode 跳过首局问号房强制事件 |
| `Runs/RunManager.cs:1033` | `FadeIn` | V | TestMode 调用测试 FadeIn 回调 |
| `Runs/RunManager.cs:1052` | `FadeOut` | V | TestMode 调用测试 FadeOut 回调 |
| `Runs/RunManager.cs:1071` | `ClearScreens` | V | TestMode 不清理屏幕节点 |
| `Runs/RunManager.cs:1400` | `UpdateRichPresence` | V | TestMode 不更新平台 Rich Presence |
| `Runs/RunManager.cs:1715` | `UpdatePlayerStatsInMapPointHistory` | G | TestMode 不更新地图历史中的金币与生命统计；正式版写入运行状态（可无节点恢复） |
| `Runs/RunManager.cs:1780` | `ReturnToMainMenuWithError` | V | TestMode 跳过返回主菜单和错误弹窗 |
| `GameActions/ActionExecutor.cs:235` | `WaitForUnpause` | V | TestMode 用 Task.Delay 轮询暂停状态 |
| `Runs/RunRngSet.cs:97` | `GetMockInstance` | T | 仅 TestMode 允许取得 mock RunRng |
| `GameActions/DiscardPotionGameAction.cs:77` | `CancelAction` | V | TestMode 跳过取消药水时的界面回调 |
| `GameActions/MoveToMapCoordAction.cs:37` | `ExecuteAction` | V | TestMode 走无地图节点的地图移动路径 |
| `Commands/CardCmd.cs:466` | `Transform` | V | TestMode 跳过卡牌/特效表现层调用 |
| `Commands/CardCmd.cs:488` | `Transform` | V | TestMode 跳过卡牌/特效表现层调用 |
| `Commands/CardCmd.cs:737` | `Preview` | V | TestMode 跳过卡牌/特效表现层调用 |
| `Commands/CardCmd.cs:749` | `PreviewCardPileAdd` | V | TestMode 跳过卡牌/特效表现层调用 |
| `Commands/CardCmd.cs:757` | `PreviewCardPileAdd` | V | TestMode 跳过卡牌/特效表现层调用 |
| `Commands/CardCmd.cs:780` | `PreviewInternal` | V | TestMode 跳过卡牌/特效表现层调用 |
| `GameActions/UsePotionAction.cs:139` | `CancelAction` | V | TestMode 跳过取消药水时的界面回调 |
| `GameActions/PlayCardAction.cs:112` | `CancelAction` | T | 无运行状态的测试清理时不取消出牌动作 |
| `Commands/MapCmd.cs:14` | `SetBossEncounter` | V | TestMode 跳过 Boss 图标和地图界面刷新 |
| `Commands/ForgeCmd.cs:90` | `PreviewSovereignBlade` | V | TestMode 跳过锻造刀的卡牌预览 |
| `Commands/CreatureCmd.cs:482` | `Kill` | G | TestMode 跳过死亡后的正式结算、存档和游戏结束界面；正式版调用 OnEnded |
| `Commands/CreatureCmd.cs:788` | `Heal` | V | TestMode 跳过休息点治疗 VFX |
| `Commands/CreatureCmd.cs:972` | `TriggerAnim` | V | TestMode 缺少节点时不记录动画错误 |
| `Models/CardModel.cs:1260` | `NeverEverCallThisOutsideOfTests_ClearOwner` | T | 仅 TestMode 允许清除卡牌 owner |
| `Commands/CardSelectCmd.cs:164` | `Reset` | T | 正式运行清理泄漏的测试选择器并告警，TestMode 保留选择器 |
| `Commands/CardSelectCmd.cs:978` | `FromChooseABundleScreen` | G | TestMode 固定选择第一个卡包；正式版等待本地或远程玩家选择 |
| `GameActions/Multiplayer/NetCombatCardDb.cs:40` | `StartCombat` | T | TestMode 跳过缺少战斗状态的测试玩家 |
| `Commands/CardPileCmd.cs:74` | `RemoveFromDeck` | V | TestMode 跳过移除卡牌的移除特效延时 |
| `Commands/CardPileCmd.cs:561` | `GetTweenForCardsChangingPiles` | V | TestMode 不创建换堆动画及其表现层回调 |
| `Commands/CardPileCmd.cs:841` | `AddDuringManualCardPlay` | V | TestMode 不创建出牌节点和移动动画 |
| `Models/PotionModel.cs:304` | `OnUseWrapper` | V | TestMode 跳过药水目标和施法 VFX |
| `Commands/VfxCmd.cs:116` | `PlayFullScreenInCombat` | V | TestMode 跳过卡牌/特效表现层调用 |
| `Commands/VfxCmd.cs:131` | `GetSideCenter` | V | TestMode 跳过卡牌/特效表现层调用 |
| `Commands/VfxCmd.cs:160` | `GetSideCenterFloor` | V | TestMode 跳过卡牌/特效表现层调用 |
| `Commands/VfxCmd.cs:194` | `PlayOnSide` | V | TestMode 跳过卡牌/特效表现层调用 |
| `Commands/VfxCmd.cs:214` | `PlayOnCreatureCenter` | V | TestMode 跳过卡牌/特效表现层调用 |
| `Commands/VfxCmd.cs:234` | `PlayOnCreature` | V | TestMode 跳过卡牌/特效表现层调用 |
| `Commands/VfxCmd.cs:246` | `PlayVfx` | V | TestMode 跳过卡牌/特效表现层调用 |
| `Commands/VfxCmd.cs:257` | `PlayNonCombatVfx` | V | TestMode 跳过卡牌/特效表现层调用 |
| `Models/Potions/FoulPotion.cs:128` | `ShowPotionVfx` | V | TestMode 跳过表现层、平台接口或日志调用 |
| `Rooms/MerchantRoom.cs:74` | `Exit` | V | TestMode 跳过房间 UI 节点创建或离开时的界面统计 |
| `Models/MonsterModel.cs:444` | `PerformMove` | V | TestMode 跳过表现层、平台接口或日志调用 |
| `Rooms/MapRoom.cs:24` | `EnterInternal` | V | TestMode 跳过房间 UI 节点创建或离开时的界面统计 |
| `Models/EventModel.cs:417` | `GetAssetPaths` | V | TestMode 跳过资源路径或资源预加载 |
| `Models/Events/DollRoom.cs:96` | `BeforeEventStarted` | V | TestMode 跳过表现层、平台接口或日志调用 |
| `Models/Enchantments/Slither.cs:22` | `Value setter` | T | 仅测试接口或测试断言，正常游戏流程不会调用 |
| `Models/ActModel.cs:550` | `GetRandomList` | P | TestMode 跳过新手引导/发现进度分支，等价于已完成引导的存档 |
| `Multiplayer/Game/ChecksumTracker.cs:205` | `CompareChecksums` | V | TestMode 跳过联机分歧日志与上报 |
| `Multiplayer/Game/ChecksumTracker.cs:224` | `LogStateDivergence` | V | TestMode 跳过联机分歧日志与上报 |
| `Multiplayer/Game/PeerInput/NetCursorHelper.cs:20` | `GetNormalizedPosition` | T | 仅测试接口或测试断言，正常游戏流程不会调用 |
| `Multiplayer/Game/PeerInput/NetCursorHelper.cs:35` | `GetControlSpacePosition` | T | 仅测试接口或测试断言，正常游戏流程不会调用 |
| `Multiplayer/Game/PeerInput/PeerInputSynchronizer.cs:419` | `GetControlSpaceFocusPosition` | T | 仅测试接口或测试断言，正常游戏流程不会调用 |
| `Models/Relics/CallingBell.cs:39` | `GenerateRewards` | G | TestMode 给固定三个遗物；正式版按普通/非凡/稀有稀有度抽取（已由 ProductionBranches 恢复） |
| `Models/Monsters/Flyconid.cs:54` | `VulnerableSporesMove` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/Flyconid.cs:65` | `FrailSporesMove` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Relics/LargeCapsule.cs:40` | `GetStrikeForCharacter` | T | TestMode 为测试角色 Deprived 回退到铁甲战士打击牌 |
| `Models/Relics/LargeCapsule.cs:49` | `GetDefendForCharacter` | T | TestMode 为测试角色 Deprived 回退到铁甲战士防御牌 |
| `Models/Powers/SandpitPower.cs:60` | `AfterApplied` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Powers/SandpitPower.cs:80` | `AfterPowerAmountChanged` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Powers/SandpitPower.cs:97` | `AfterRemoved` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Powers/SandpitPower.cs:114` | `AfterRemoved` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Powers/SandpitPower.cs:149` | `UpdateCreaturePositions` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/FuzzyWurmCrawler.cs:64` | `AcidGoop` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/LeafSlimeM.cs:53` | `StickyShotMove` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Cards/BouncingFlask.cs:48` | `OnPlay` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/BowlbugSilk.cs:64` | `SpitMove` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/Noisebot.cs:34` | `AfterAddedToRoom` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/Stabbot.cs:32` | `AfterAddedToRoom` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/Myte.cs:66` | `ToxicMove` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/KinFollower.cs:137` | `BoomerangMove` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Powers/InfestedPower.cs:41` | `RevealWrigglersAfterDeathAnim` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/WaterfallGiant.cs:318` | `IncrementBuildUpAnimationTrack` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/TwigSlimeM.cs:57` | `StickyShotMove` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/ToughEgg.cs:126` | `AfterAddedToRoom` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/BygoneEffigy.cs:72` | `WakeMove` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/BygoneEffigy.cs:84` | `SlashMove` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/SnappingJaxfruit.cs:55` | `EnergyOrb` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/Zapbot.cs:32` | `AfterAddedToRoom` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/Guardbot.cs:30` | `AfterAddedToRoom` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/DecimillipedeSegment.cs:224` | `AnimSegmentsAttack` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/DecimillipedeSegment.cs:247` | `AfterDeath` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Relics/ScrollBoxes.cs:108` | `GetCardPool` | T | TestMode 为测试角色 Deprived 使用铁甲战士卡池 |
| `Models/Monsters/Tunneler.cs:108` | `BelowMove` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/TorchHeadAmalgam.cs:71` | `OnDieToDoom` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Relics/Cauldron.cs:40` | `GenerateRewards` | G | TestMode 给固定五瓶药水；正式版逐瓶从药水池抽取（已由 ProductionBranches 恢复） |
| `Models/Powers/RollingBoulderPower.cs:35` | `AfterPlayerTurnStart` | G | TestMode 直接对所有可命中敌人造成伤害；正式版由滚石 VFX 的 HitCreature 信号逐个造成伤害 |
| `Models/Monsters/Vantom.cs:138` | `InkBlotMove` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/Vantom.cs:158` | `InkyLanceMove` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/Vantom.cs:172` | `DismemberMove` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/Vantom.cs:196` | `PrepareMove` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/SlumberingBeetle.cs:131` | `RolloutMove` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Monsters/MagiKnight.cs:116` | `MagicBombMove` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Relics/LordsParasol.cs:40` | `PurchaseEverything` | V | TestMode 跳过商店 UI 控件禁用与恢复 |
| `Models/Cards/Nightmare.cs:35` | `OnPlay` | V | TestMode 跳过动画、音频或战斗表现层调用 |
| `Models/Cards/HandOfGreed.cs:51` | `OnPlay` | V | TestMode 跳过动画、音频或战斗表现层调用 |

## NonInteractiveMode.IsActive（排除 Nodes/）

命令：

```bash
cd "$STS2_SRC"
rg -n --glob '*.cs' -g '!Nodes/**' 'NonInteractiveMode\.IsActive' .
```

共 **18 处**：V 表现/等待/音频 **17**，T 测试断言 **1**。TestMode 下 `NonInteractiveMode.IsActive` 恒为 true。

| 文件:行号 | 所在方法 | 类别 | 说明 |
|---|---|---|---|
| `Commands/Cmd.cs:38` | `Wait/CustomScaledWait` | V | 非交互模式跳过 Godot 计时器等待 |
| `Commands/Cmd.cs:56` | `Wait/CustomScaledWait` | V | 非交互模式跳过 Godot 计时器等待 |
| `Combat/CombatManager.cs:1897` | `Pause/Unpause` | V | 非交互模式跳过暂停/取消暂停和等待帧 |
| `Combat/CombatManager.cs:1908` | `Pause/Unpause` | V | 非交互模式跳过暂停/取消暂停和等待帧 |
| `Combat/CombatManager.cs:1952` | `WaitForUnpause` | V | 非交互模式跳过暂停/取消暂停和等待帧 |
| `Commands/SfxCmd.cs:16` | `Play/PlayLoop/StopLoop/SetParam/PlayDamage/PlayDeath` | V | 非交互模式跳过音效播放、循环和参数更新 |
| `Commands/SfxCmd.cs:24` | `Play/PlayLoop/StopLoop/SetParam/PlayDamage/PlayDeath` | V | 非交互模式跳过音效播放、循环和参数更新 |
| `Commands/SfxCmd.cs:32` | `Play/PlayLoop/StopLoop/SetParam/PlayDamage/PlayDeath` | V | 非交互模式跳过音效播放、循环和参数更新 |
| `Commands/SfxCmd.cs:40` | `Play/PlayLoop/StopLoop/SetParam/PlayDamage/PlayDeath` | V | 非交互模式跳过音效播放、循环和参数更新 |
| `Commands/SfxCmd.cs:48` | `Play/PlayLoop/StopLoop/SetParam/PlayDamage/PlayDeath` | V | 非交互模式跳过音效播放、循环和参数更新 |
| `Commands/SfxCmd.cs:56` | `Play/PlayLoop/StopLoop/SetParam/PlayDamage/PlayDeath` | V | 非交互模式跳过音效播放、循环和参数更新 |
| `Commands/SfxCmd.cs:64` | `Play/PlayLoop/StopLoop/SetParam/PlayDamage/PlayDeath` | V | 非交互模式跳过音效播放、循环和参数更新 |
| `Commands/SfxCmd.cs:72` | `Play/PlayLoop/StopLoop/SetParam/PlayDamage/PlayDeath` | V | 非交互模式跳过音效播放、循环和参数更新 |
| `Commands/SfxCmd.cs:80` | `Play/PlayLoop/StopLoop/SetParam/PlayDamage/PlayDeath` | V | 非交互模式跳过音效播放、循环和参数更新 |
| `Commands/SfxCmd.cs:88` | `Play/PlayLoop/StopLoop/SetParam/PlayDamage/PlayDeath` | V | 非交互模式跳过音效播放、循环和参数更新 |
| `Commands/SfxCmd.cs:96` | `Play/PlayLoop/StopLoop/SetParam/PlayDamage/PlayDeath` | V | 非交互模式跳过音效播放、循环和参数更新 |
| `GameActions/ActionExecutor.cs:140` | `ExecuteActions` | V | 非交互模式直接等待动作完成并推进队列，不依赖逐帧轮询 |
| `GameActions/GameAction.cs:153` | `Execute` | T | 非交互模式下把动作状态错误直接抛出；测试 fail-fast 断言 |

## G 类处理结论

| 逻辑点 | ProductionBranches | 结论 |
|---|---|---|
| `MerchantPotionEntry.CalcCost` | 已有 | 已恢复 Shops 流抽价。 |
| `CallingBell.GenerateRewards` | 已有 | 已恢复正式稀有度奖励。 |
| `Cauldron.GenerateRewards` | 已有 | 已恢复正式药水池抽取。 |
| `RunManager.UpdatePlayerStatsInMapPointHistory` | 建议新增 | 正式分支无节点/UI 依赖，可用同一 transpiler 恢复。 |
| `CreatureCmd.Kill` | 不建议直接新增 | 正式死亡分支混合 `OnEnded`、音频和游戏结束 UI；登记 O-007，拆分结算与 UI 后再处理。 |
| `RewardsSet.Offer` | 不建议直接新增 | 正式分支依赖奖励界面/玩家选择；登记 O-008，需为 headless 提供等价选择适配。 |
| `CardSelectCmd.FromChooseABundleScreen` | 不建议直接新增 | 正式分支依赖本地/远程选择；登记 O-009，需注入 headless 选择而非固定首包。 |
| `RollingBoulderPower.AfterPlayerTurnStart` | 不建议直接新增 | 正式伤害由 `NRollingBoulderVfx.HitCreature` 信号决定顺序；登记 O-010，需保留命中顺序的无 UI 事件源。 |
