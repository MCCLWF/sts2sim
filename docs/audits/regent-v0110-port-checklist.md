# Regent v0.111.0 源码移植清单

基准：STS2 v0.111.0（反编译源：`src/Core`，原版程序集由 `tools/Sts2Oracle` 调用）。此清单从 `RegentCardPool.cs`、`RegentRelicPool.cs`、`RegentPotionPool.cs`、角色/纪元模型及其卡牌/遗物/药水所引用的模型递归提取。`override`、显式控制流/选择条件逐项带复选框；模型、每个覆写/分支和 Oracle 证据都须在完成后勾选。这里的完成范围按原版池和依赖闭包定义，不由路线或首停定义。


标记：`[x]` 表示该源码覆写/分支已在 C++ Env 实现，并纳入对应模型的 Oracle 对拍；`[~]` 表示该成员只影响界面/动画/音频/美术、多人缩放或战役存档中的故事时间线呈现，不参与 Env 的单人局内状态转移。Env 接收已确定的 epoch 位并实现其卡牌、遗物和药水池门控；故事解锁队列和局外存档进度不属于 run-state 合同。
卡牌矩阵逐模型比较三档升级和每个静止点/选择；遗物与药水各模型单独对拍；复杂跨回合机制另用整局逐决策快照恢复和星星历史专项对拍核对。

## 源码基数

- 卡池：91/91 张。
- 职业遗物池：8/8 件。
- Regent4 解锁的药水：3 种。
- 递归的显式生成/变换/提示牌依赖：5 个。
- 卡牌/遗物/药水直接或递归引用 Power：35 个（含通用 Power）。

## 角色、池与解锁纪元

下表 `override` 列逐项对应源码覆写成员；分支列包含过滤门槛及静态模型分支。展示/动画字段不改变战斗规则，但保留在角色清单里便于范围审计。

| 模型 | 原版源文件 | Overrides | 条件分支/循环 | 完成 |
|---|---|---|---|---|
|  角色 `Regent` | `Characters/Regent.cs` | [~] `NameColor` (L26); [~] `Gender` (L28); [~] `UnlocksAfterRunAs` (L30); [x] `StartingHp` (L32); [x] `StartingGold` (L34); [x] `ShouldAlwaysShowStarCounter` (L36); [x] `CardPool` (L38); [x] `RelicPool` (L40); [x] `PotionPool` (L42); [x] `StartingDeck` (L44); [x] `StartingRelics` (L58); [~] `AttackAnimDelay` (L60); [~] `CastAnimDelay` (L62); [~] `EnergyLabelOutlineColor` (L64); [~] `DialogueColor` (L66); [~] `SpeechBubbleColor` (L68); [~] `MapDrawingColor` (L70); [~] `RemoteTargetingLineColor` (L72); [~] `RemoteTargetingLineOutline` (L74); [~] `CharacterTransitionSfx` (L76); [~] `AnimationStates` (L78); [~] `GetArchitectAttackVfx` (L80) | — | [x] 实现 [x] Oracle  |
|  卡池 `RegentCardPool` | `CardPools/RegentCardPool.cs` | [~] `Title` (L12); [~] `EnergyColorName` (L14); [~] `CardFrameMaterialPath` (L16); [~] `DeckEntryCardColor` (L18); [~] `EnergyOutlineColor` (L20); [x] `IsColorless` (L22); [x] `GenerateAllCards` (L24); [x] `FilterThroughEpochs` (L122) | [x] `if (!unlockState.IsEpochRevealed<Regent2Epoch>())` (L125); [x] `Any((CardModel card) => card.Id == c.Id)` (L127); [x] `if (!unlockState.IsEpochRevealed<Regent5Epoch>())` (L129); [x] `Any((CardModel card) => card.Id == c.Id)` (L131); [x] `if (!unlockState.IsEpochRevealed<Regent7Epoch>())` (L133); [x] `Any((CardModel card) => card.Id == c.Id)` (L135) | [x] 实现 [x] Oracle  |
|  遗物池 `RegentRelicPool` | `RelicPools/RegentRelicPool.cs` | [~] `EnergyColorName` (L13); [~] `LabOutlineColor` (L15); [x] `GenerateAllRelics` (L17); [x] `GetUnlockedRelics` (L32) | [x] `if (!unlockState.IsEpochRevealed<Regent3Epoch>())` (L35); [x] `Any((RelicModel relic) => relic.Id == r.Id)` (L37); [x] `if (!unlockState.IsEpochRevealed<Regent6Epoch>())` (L39); [x] `Any((RelicModel relic) => relic.Id == r.Id)` (L41) | [x] 实现 [x] Oracle  |
|  药水池 `RegentPotionPool` | `PotionPools/RegentPotionPool.cs` | [~] `EnergyColorName` (L12); [~] `LabOutlineColor` (L14); [x] `GenerateAllPotions` (L16); [x] `GetUnlockedPotions` (L25) | [x] `if (!unlockState.IsEpochRevealed<Regent4Epoch>())` (L27) | [x] 实现 [x] Oracle  |
|  纪元 `Regent1Epoch` | `Timeline/Epochs/Regent1Epoch.cs` | [x] `Id` (L13); [~] `Era` (L15); [~] `EraPosition` (L17); [~] `StoryId` (L19); [~] `GetTimelineExpansion` (L21); [~] `QueueUnlocks` (L35) | —; 开启角色/初始 timeline；含视觉和 unlock queue。 | [x] 实现 [x] Oracle  |
|  纪元 `Regent2Epoch` | `Timeline/Epochs/Regent2Epoch.cs` | [x] `Id` (L15); [~] `Era` (L17); [~] `EraPosition` (L19); [~] `StoryId` (L21); [~] `UnlockText` (L41); [~] `QueueUnlocks` (L43) | —; 模型清单：Furnace, SpoilsOfBattle, SwordSage | [x] 实现 [x] Oracle  |
|  纪元 `Regent3Epoch` | `Timeline/Epochs/Regent3Epoch.cs` | [x] `Id` (L15); [~] `Era` (L17); [~] `EraPosition` (L19); [~] `StoryId` (L21); [~] `UnlockText` (L41); [~] `QueueUnlocks` (L43) | —; 模型清单：FencingManual, GalacticDust, LunarPastry | [x] 实现 [x] Oracle  |
|  纪元 `Regent4Epoch` | `Timeline/Epochs/Regent4Epoch.cs` | [x] `Id` (L16); [~] `Era` (L18); [~] `EraPosition` (L20); [~] `StoryId` (L22); [~] `UnlockText` (L42); [~] `QueueUnlocks` (L44) | `null-coalesce: locString.GetFormattedText() ?? ""`; 模型清单：CosmicConcoction, KingsCourage, StarPotion | [x] 实现 [x] Oracle  |
|  纪元 `Regent5Epoch` | `Timeline/Epochs/Regent5Epoch.cs` | [x] `Id` (L15); [~] `Era` (L17); [~] `EraPosition` (L19); [~] `StoryId` (L21); [~] `UnlockText` (L41); [~] `QueueUnlocks` (L43) | —; 模型清单：Arsenal, Begone, Supermassive | [x] 实现 [x] Oracle  |
|  纪元 `Regent6Epoch` | `Timeline/Epochs/Regent6Epoch.cs` | [x] `Id` (L15); [~] `Era` (L17); [~] `EraPosition` (L19); [~] `StoryId` (L21); [~] `UnlockText` (L41); [~] `QueueUnlocks` (L43) | —; 模型清单：MiniRegent, OrangeDough, Regalite | [x] 实现 [x] Oracle  |
|  纪元 `Regent7Epoch` | `Timeline/Epochs/Regent7Epoch.cs` | [x] `Id` (L15); [~] `Era` (L17); [~] `EraPosition` (L19); [~] `StoryId` (L21); [~] `UnlockText` (L41); [~] `QueueUnlocks` (L43) | —; 模型清单：HeavenlyDrill, LunarBlast, Patter | [x] 实现 [x] Oracle  |

## RegentCardPool 全部卡牌

每项列出原版模型**所有 overrides**及源码显式条件分支/循环/谓词筛选。无条件分支以 `—` 表示。升级和动态变量构造也必须随对应 override 验证。

| 卡牌模型 | 原版源文件 | Overrides（逐项） | 条件分支/选择/循环（逐项） | 生成/依赖说明 | 完成 |
|---|---|---|---|---|---|
|  `Alignment` | `Cards/Alignment.cs` | [x] `CanonicalStarCost` (L13); [x] `CanonicalVars` (L15); [~] `ExtraHoverTips` (L17); [x] `OnPlay` (L24); [x] `OnUpgrade` (L30) | — | — | [x] 实现 [x] Oracle  |
|  `Arsenal` | `Cards/Arsenal.cs` | [~] `ExtraHoverTips` (L14); [x] `CanonicalVars` (L16); [x] `OnPlay` (L23); [x] `OnUpgrade` (L29) | — | — | [x] 实现 [x] Oracle  |
|  `AstralPulse` | `Cards/AstralPulse.cs` | [x] `CanonicalStarCost` (L13); [x] `CanonicalVars` (L15); [x] `OnPlay` (L22); [x] `OnUpgrade` (L31) | — | — | [x] 实现 [x] Oracle  |
|  `BeatIntoShape` | `Cards/BeatIntoShape.cs` | [x] `CanonicalVars` (L22); [~] `ExtraHoverTips` (L30); [x] `OnPlay` (L37); [x] `OnUpgrade` (L48) | — | — | [x] 实现 [x] Oracle  |
|  `Begone` | `Cards/Begone.cs` | [~] `ExtraHoverTips` (L14); [x] `OnPlay` (L21) | [x] `if (cardModel != null)` (L24); [x] `if (base.IsUpgraded)` (L27) | CreateCard<MinionStrike>，变换手牌选中牌。 | [x] 实现 [x] Oracle  |
|  `BigBang` | `Cards/BigBang.cs` | [x] `CanonicalKeywords` (L13); [x] `CanonicalVars` (L15); [~] `ExtraHoverTips` (L23); [x] `OnPlay` (L39); [x] `OnUpgrade` (L48) | — | — | [x] 实现 [x] Oracle  |
|  `BlackHole` | `Cards/BlackHole.cs` | [x] `CanonicalVars` (L13); [x] `OnPlay` (L20); [x] `OnUpgrade` (L26) | — | — | [x] 实现 [x] Oracle  |
|  `Bombardment` | `Cards/Bombardment.cs` | [x] `CanonicalKeywords` (L20); [x] `CanonicalVars` (L22); [x] `OnPlay` (L29); [x] `AfterAutoPrePlayPhaseEnteredEarly` (L48); [x] `OnUpgrade` (L57) | [x] `if (nCreature != null)` (L33); [x] `if (pile != null && pile.Type == PileType.Exhaust && player == base.Owner)` (L51) | — | [x] 实现 [x] Oracle  |
|  `Bulwark` | `Cards/Bulwark.cs` | [x] `GainsBlock` (L14); [x] `CanonicalVars` (L16); [~] `ExtraHoverTips` (L22); [x] `OnPlay` (L29); [x] `OnUpgrade` (L36) | — | — | [x] 实现 [x] Oracle  |
|  `BundleOfJoy` | `Cards/BundleOfJoy.cs` | [x] `CanonicalVars` (L14); [x] `CanonicalKeywords` (L16); [x] `OnPlay` (L23); [x] `OnUpgrade` (L32) | [x] `foreach (CardModel item in distinctForCombat)` (L26) | 由来源选择后 AddGeneratedCardToCombat；候选牌按源过滤。 | [x] 实现 [x] Oracle  |
|  `CelestialMight` | `Cards/CelestialMight.cs` | [x] `CanonicalVars` (L14); [x] `OnPlay` (L25); [x] `OnUpgrade` (L34) | — | — | [x] 实现 [x] Oracle  |
|  `Charge` | `Cards/Charge.cs` | [~] `ExtraHoverTips` (L15); [x] `CanonicalVars` (L17); [x] `OnPlay` (L24) | [x] `foreach (CardModel item in selection)` (L28); [x] `if (base.IsUpgraded && cardPileAddResult.HasValue)` (L31) | 选择并 TransformTo<MinionDiveBomb>；升级仅在 transform 成功时发生。 | [x] 实现 [x] Oracle  |
|  `ChildOfTheStars` | `Cards/ChildOfTheStars.cs` | [x] `CanonicalVars` (L16); [~] `ExtraHoverTips` (L18); [x] `OnPlay` (L25); [x] `OnUpgrade` (L31) | — | — | [x] 实现 [x] Oracle  |
|  `CloakOfStars` | `Cards/CloakOfStars.cs` | [x] `CanonicalStarCost` (L13); [x] `GainsBlock` (L15); [x] `CanonicalVars` (L17); [x] `OnPlay` (L24); [x] `OnUpgrade` (L29) | — | — | [x] 实现 [x] Oracle  |
|  `CollisionCourse` | `Cards/CollisionCourse.cs` | [~] `ExtraHoverTips` (L15); [x] `CanonicalVars` (L17); [x] `OnPlay` (L24); [x] `OnUpgrade` (L34) | — | CreateCard<Debris> 后生成加入手牌。 | [x] 实现 [x] Oracle  |
|  `Comet` | `Cards/Comet.cs` | [x] `CanonicalStarCost` (L21); [x] `CanonicalVars` (L23); [~] `ExtraHoverTips` (L30); [x] `OnPlay` (L41); [x] `OnUpgrade` (L59) | [x] `if (nCreature != null)` (L46) | — | [x] 实现 [x] Oracle  |
|  `Conqueror` | `Cards/Conqueror.cs` | [x] `CanonicalVars` (L15); [~] `ExtraHoverTips` (L17); [x] `OnPlay` (L24); [x] `OnUpgrade` (L32) | — | — | [x] 实现 [x] Oracle  |
|  `Constellation` | `Cards/Constellation.cs` | [x] `MultiplayerConstraint` (L16); [x] `CanonicalStarCost` (L18); [x] `CanonicalVars` (L20); [~] `ExtraHoverTips` (L27); [x] `GainsBlock` (L29); [x] `OnPlay` (L36); [x] `OnUpgrade` (L47) | — | — | [x] 实现 [x] Oracle  |
|  `Convergence` | `Cards/Convergence.cs` | [x] `CanonicalVars` (L14); [~] `ExtraHoverTips` (L20); [x] `OnPlay` (L31); [x] `OnUpgrade` (L39) | — | — | [x] 实现 [x] Oracle  |
|  `CosmicIndifference` | `Cards/CosmicIndifference.cs` | [x] `GainsBlock` (L15); [x] `CanonicalVars` (L17); [x] `OnPlay` (L24); [x] `OnUpgrade` (L53) | [x] `if (flag2)` (L32); [x] `switch (cardModel.Pile?.Type)` (L35); [x] `case PileType.Draw:` (L37); [x] `case PileType.Discard:` (L38); [x] `default:` (L41); [x] `if (flag2)` (L47) | 从多个牌堆移动候选牌至抽牌堆顶。 | [x] 实现 [x] Oracle  |
|  `CrashLanding` | `Cards/CrashLanding.cs` | [~] `ExtraHoverTips` (L15); [x] `CanonicalVars` (L17); [x] `OnPlay` (L24); [x] `OnUpgrade` (L39) | [x] `for (int i = 0; i < num; i++)` (L32) | 逐个创建 Debris，批量生成加入手牌。 | [x] 实现 [x] Oracle  |
|  `CrescentSpear` | `Cards/CrescentSpear.cs` | [x] `CanonicalStarCost` (L16); [x] `CanonicalVars` (L18); [x] `OnPlay` (L30); [x] `OnUpgrade` (L38) | — | — | [x] 实现 [x] Oracle  |
|  `CrushUnder` | `Cards/CrushUnder.cs` | [~] `ExtraHoverTips` (L21); [x] `CanonicalVars` (L23); [x] `OnPlay` (L34); [x] `OnUpgrade` (L49) | [x] `foreach (Creature item in enemies)` (L38) | — | [x] 实现 [x] Oracle  |
|  `DecisionsDecisions` | `Cards/DecisionsDecisions.cs` | [x] `CanonicalStarCost` (L14); [x] `CanonicalVars` (L16); [x] `CanonicalKeywords` (L22); [x] `OnPlay` (L29); [x] `OnUpgrade` (L47) | [x] `if (card != null)` (L38); [x] `for (int i = 0; i < base.DynamicVars.Repeat.IntValue; i++)` (L40) | — | [x] 实现 [x] Oracle  |
|  `DefendRegent` | `Cards/DefendRegent.cs` | [x] `GainsBlock` (L13); [x] `CanonicalTags` (L15); [x] `CanonicalVars` (L17); [x] `OnPlay` (L24); [x] `OnUpgrade` (L29) | — | — | [x] 实现 [x] Oracle  |
|  `Devastate` | `Cards/Devastate.cs` | [x] `CanonicalStarCost` (L14); [x] `CanonicalVars` (L16); [x] `OnPlay` (L23); [x] `OnUpgrade` (L31) | — | — | [x] 实现 [x] Oracle  |
|  `DyingStar` | `Cards/DyingStar.cs` | [x] `CanonicalStarCost` (L18); [x] `CanonicalVars` (L20); [x] `CanonicalKeywords` (L26); [~] `ExtraHoverTips` (L28); [x] `OnPlay` (L35); [x] `OnUpgrade` (L50) | [x] `foreach (Creature enemy in enemies)` (L43) | — | [x] 实现 [x] Oracle  |
|  `FallingStar` | `Cards/FallingStar.cs` | [x] `CanonicalStarCost` (L16); [x] `CanonicalVars` (L18); [~] `ExtraHoverTips` (L25); [x] `OnPlay` (L36); [x] `OnUpgrade` (L46) | — | — | [x] 实现 [x] Oracle  |
|  `ForegoneConclusion` | `Cards/ForegoneConclusion.cs` | [x] `CanonicalVars` (L13); [x] `OnPlay` (L20); [x] `OnUpgrade` (L26) | — | — | [x] 实现 [x] Oracle  |
|  `Furnace` | `Cards/Furnace.cs` | [~] `ExtraHoverTips` (L14); [x] `CanonicalVars` (L16); [x] `OnPlay` (L23); [x] `OnUpgrade` (L29) | — | — | [x] 实现 [x] Oracle  |
|  `GammaBlast` | `Cards/GammaBlast.cs` | [x] `CanonicalStarCost` (L16); [~] `ExtraHoverTips` (L18); [x] `CanonicalVars` (L24); [x] `OnPlay` (L36); [x] `OnUpgrade` (L46) | — | — | [x] 实现 [x] Oracle  |
|  `GatherLight` | `Cards/GatherLight.cs` | [x] `GainsBlock` (L13); [x] `CanonicalVars` (L15); [x] `OnPlay` (L26); [x] `OnUpgrade` (L32) | — | — | [x] 实现 [x] Oracle  |
|  `Genesis` | `Cards/Genesis.cs` | [x] `CanonicalVars` (L15); [x] `OnPlay` (L22); [x] `OnUpgrade` (L28) | — | — | [x] 实现 [x] Oracle  |
|  `Glimmer` | `Cards/Glimmer.cs` | [x] `CanonicalVars` (L16); [x] `OnPlay` (L27); [x] `OnUpgrade` (L37) | [x] `if (array.Length != 0)` (L31) | 按原版过滤从抽牌堆取牌并置顶。 | [x] 实现 [x] Oracle  |
|  `Glitterstream` | `Cards/Glitterstream.cs` | [x] `GainsBlock` (L17); [x] `CanonicalVars` (L19); [x] `OnPlay` (L30); [x] `OnUpgrade` (L40) | — | — | [x] 实现 [x] Oracle  |
|  `Glow` | `Cards/Glow.cs` | [x] `CanonicalVars` (L13); [x] `OnPlay` (L24); [x] `OnUpgrade` (L32) | — | — | [x] 实现 [x] Oracle  |
|  `Guards` | `Cards/Guards.cs` | [x] `CanonicalKeywords` (L14); [~] `ExtraHoverTips` (L16); [x] `OnPlay` (L23) | [x] `foreach (CardModel item in list)` (L27); [x] `if (base.IsUpgraded)` (L30) | CreateCard<MinionSacrifice>，变换牌堆选择。 | [x] 实现 [x] Oracle  |
|  `GuidingStar` | `Cards/GuidingStar.cs` | [x] `CanonicalStarCost` (L22); [x] `CanonicalVars` (L24); [x] `OnPlay` (L35); [x] `OnUpgrade` (L53) | [x] `if (nCreature != null)` (L40) | — | [x] 实现 [x] Oracle  |
|  `HammerTime` | `Cards/HammerTime.cs` | [x] `MultiplayerConstraint` (L13); [~] `ExtraHoverTips` (L15); [x] `OnPlay` (L22); [x] `OnUpgrade` (L28) | — | — | [x] 实现 [x] Oracle  |
|  `HeavenlyDrill` | `Cards/HeavenlyDrill.cs` | [x] `HasEnergyCostX` (L14); [~] `ShouldGlowGoldInternal` (L16); [x] `CanonicalVars` (L18); [x] `OnPlay` (L29); [x] `OnUpgrade` (L43) | [x] `if (num >= base.DynamicVars.Energy.IntValue)` (L33) | — | [x] 实现 [x] Oracle  |
|  `Hegemony` | `Cards/Hegemony.cs` | [x] `CanonicalVars` (L19); [~] `ExtraHoverTips` (L25); [x] `OnPlay` (L35); [x] `OnUpgrade` (L44) | — | — | [x] 实现 [x] Oracle  |
|  `HeirloomHammer` | `Cards/HeirloomHammer.cs` | [x] `CanonicalVars` (L16); [x] `OnPlay` (L27); [x] `OnUpgrade` (L44) | [x] `if (selection != null)` (L34); [x] `for (int i = 0; i < base.DynamicVars.Repeat.IntValue; i++)` (L36) | 选择卡牌，按重复变量生成并加入手牌。 | [x] 实现 [x] Oracle  |
|  `HiddenCache` | `Cards/HiddenCache.cs` | [x] `CanonicalVars` (L13); [x] `OnPlay` (L24); [x] `OnUpgrade` (L31) | — | — | [x] 实现 [x] Oracle  |
|  `IAmInvincible` | `Cards/IAmInvincible.cs` | [x] `GainsBlock` (L15); [x] `CanonicalVars` (L17); [x] `OnPlay` (L24); [x] `AfterAutoPostPlayPhaseEntered` (L29); [x] `OnUpgrade` (L41) | [x] `if (player == base.Owner)` (L31); [x] `if (pile.Cards.FirstOrDefault() == this)` (L34) | — | [x] 实现 [x] Oracle  |
|  `KinglyKick` | `Cards/KinglyKick.cs` | [x] `CanonicalVars` (L14); [x] `OnPlay` (L21); [x] `OnUpgrade` (L29); [x] `AfterCardDrawn` (L34) | [x] `if (card != this)` (L36) | — | [x] 实现 [x] Oracle  |
|  `KinglyPunch` | `Cards/KinglyPunch.cs` | [x] `CanonicalVars` (L31); [x] `OnPlay` (L42); [x] `AfterCardDrawn` (L50); [x] `OnUpgrade` (L62); [x] `AfterDowngraded` (L68) | [x] `if (card != this)` (L52) | — | [x] 实现 [x] Oracle  |
|  `KnockoutBlow` | `Cards/KnockoutBlow.cs` | [x] `CanonicalVars` (L19); [x] `OnPlay` (L33); [x] `OnUpgrade` (L44) | [x] `if ((await DamageCmd.Attack(base.DynamicVars.Damage.BaseValue).FromCard(this, cardPlay).Targeting(cardPlay.Target) .WithHitFx("vfx/vfx_attack_blunt", null, "blunt_attack.mp3") .Execute(choiceContext)).Results.SelectMany((List<DamageResult> r) => r).Any((DamageResult r) => r.WasTargetKilled))` (L36); [x] `Any((DamageResult r) => r.WasTargetKilled)` (L38) | — | [x] 实现 [x] Oracle  |
|  `KnowThyPlace` | `Cards/KnowThyPlace.cs` | [x] `CanonicalVars` (L15); [x] `CanonicalKeywords` (L21); [~] `ExtraHoverTips` (L23); [x] `OnPlay` (L34); [x] `OnUpgrade` (L42) | — | — | [x] 实现 [x] Oracle  |
|  `Largesse` | `Cards/Largesse.cs` | [x] `MultiplayerConstraint` (L14); [x] `OnPlay` (L21) | [x] `if (cardModel != null)` (L26); [x] `if (base.IsUpgraded)` (L28) | 复制/生成选中卡牌，按升级状态强化。 | [x] 实现 [x] Oracle  |
|  `LunarBlast` | `Cards/LunarBlast.cs` | [x] `CanonicalVars` (L20); [x] `OnPlay` (L33); [x] `OnUpgrade` (L42) | — | — | [x] 实现 [x] Oracle  |
|  `MakeItSo` | `Cards/MakeItSo.cs` | [x] `CanonicalVars` (L17); [x] `OnPlay` (L28); [x] `AfterCardPlayedLate` (L36); [x] `OnUpgrade` (L48) | [x] `if (cardPlay.Card.Owner == base.Owner && cardPlay.Card.Type == CardType.Skill && base.Pile.Type != PileType.Hand)` (L38); [x] `if (num % base.DynamicVars.Cards.IntValue == 0)` (L41) | — | [x] 实现 [x] Oracle  |
|  `ManifestAuthority` | `Cards/ManifestAuthority.cs` | [~] `ExtraHoverTips` (L17); [x] `GainsBlock` (L19); [x] `CanonicalVars` (L21); [x] `OnPlay` (L28); [x] `OnUpgrade` (L42) | [x] `if (cardModel != null)` (L32); [x] `if (base.IsUpgraded)` (L34) | 复制/生成选中牌，按升级状态强化。 | [x] 实现 [x] Oracle  |
|  `MeteorShower` | `Cards/MeteorShower.cs` | [x] `CanonicalStarCost` (L15); [x] `CanonicalVars` (L17); [~] `ExtraHoverTips` (L24); [x] `OnPlay` (L35); [x] `OnUpgrade` (L44) | — | — | [x] 实现 [x] Oracle  |
|  `MonarchsGaze` | `Cards/MonarchsGaze.cs` | [x] `CanonicalVars` (L16); [~] `ExtraHoverTips` (L18); [x] `OnPlay` (L25); [x] `OnUpgrade` (L31) | — | — | [x] 实现 [x] Oracle  |
|  `Monologue` | `Cards/Monologue.cs` | [x] `CanonicalVars` (L16); [~] `ExtraHoverTips` (L18); [x] `OnPlay` (L25); [x] `OnUpgrade` (L35) | [x] `if (monologuePower != null)` (L29) | — | [x] 实现 [x] Oracle  |
|  `NeutronAegis` | `Cards/NeutronAegis.cs` | [x] `CanonicalStarCost` (L14); [~] `ExtraHoverTips` (L16); [x] `CanonicalVars` (L18); [x] `OnPlay` (L25); [x] `OnUpgrade` (L30) | — | — | [x] 实现 [x] Oracle  |
|  `Orbit` | `Cards/Orbit.cs` | [~] `ExtraHoverTips` (L14); [x] `CanonicalVars` (L16); [x] `OnPlay` (L23); [x] `OnUpgrade` (L29) | — | — | [x] 实现 [x] Oracle  |
|  `PaleBlueDot` | `Cards/PaleBlueDot.cs` | [x] `CanonicalVars` (L13); [x] `OnPlay` (L24); [x] `OnUpgrade` (L30) | — | — | [x] 实现 [x] Oracle  |
|  `Parry` | `Cards/Parry.cs` | [~] `ExtraHoverTips` (L14); [x] `CanonicalVars` (L20); [x] `OnPlay` (L27); [x] `OnUpgrade` (L33) | — | — | [x] 实现 [x] Oracle  |
|  `ParticleWall` | `Cards/ParticleWall.cs` | [x] `CanonicalStarCost` (L13); [x] `GainsBlock` (L15); [x] `CanonicalVars` (L17); [x] `OnPlay` (L24); [x] `GetResultLocationForCardPlay` (L30); [x] `OnUpgrade` (L40) | [x] `if (resultLocationForCardPlay.pileType == PileType.Discard)` (L33) | — | [x] 实现 [x] Oracle  |
|  `Patter` | `Cards/Patter.cs` | [x] `GainsBlock` (L15); [x] `CanonicalVars` (L17); [~] `ExtraHoverTips` (L23); [x] `OnPlay` (L30); [x] `OnUpgrade` (L37) | — | — | [x] 实现 [x] Oracle  |
|  `PhotonCut` | `Cards/PhotonCut.cs` | [x] `CanonicalVars` (L17); [x] `OnPlay` (L29); [x] `OnUpgrade` (L39) | — | — | [x] 实现 [x] Oracle  |
|  `PillarOfCreation` | `Cards/PillarOfCreation.cs` | [~] `ExtraHoverTips` (L15); [x] `CanonicalVars` (L17); [x] `OnPlay` (L24); [x] `OnUpgrade` (L29) | — | — | [x] 实现 [x] Oracle  |
|  `Plot` | `Cards/Plot.cs` | [x] `MultiplayerConstraint` (L15); [x] `CanonicalVars` (L17); [x] `OnPlay` (L24); [x] `OnUpgrade` (L35) | [x] `foreach (Creature item in enumerable)` (L29) | — | [x] 实现 [x] Oracle  |
|  `Prophesize` | `Cards/Prophesize.cs` | [x] `CanonicalVars` (L12); [x] `OnPlay` (L19); [x] `OnUpgrade` (L25) | — | — | [x] 实现 [x] Oracle  |
|  `Quasar` | `Cards/Quasar.cs` | [x] `CanonicalStarCost` (L15); [x] `OnPlay` (L22) | [x] `if (base.IsUpgraded)` (L25); [x] `if (cardModel != null)` (L30) | 从原版随机卡牌池生成牌，升级后加入战斗手牌。 | [x] 实现 [x] Oracle  |
|  `Radiate` | `Cards/Radiate.cs` | [x] `CanonicalVars` (L19); [x] `OnPlay` (L35); [x] `OnUpgrade` (L44) | — | — | [x] 实现 [x] Oracle  |
|  `RefineBlade` | `Cards/RefineBlade.cs` | [x] `CanonicalVars` (L14); [~] `ExtraHoverTips` (L20); [x] `OnPlay` (L27); [x] `OnUpgrade` (L34) | — | — | [x] 实现 [x] Oracle  |
|  `Reflect` | `Cards/Reflect.cs` | [x] `CanonicalStarCost` (L14); [x] `GainsBlock` (L16); [x] `CanonicalVars` (L18); [x] `OnPlay` (L25); [x] `OnUpgrade` (L32) | — | — | [x] 实现 [x] Oracle  |
|  `Resonance` | `Cards/Resonance.cs` | [x] `CanonicalStarCost` (L15); [x] `CanonicalVars` (L17); [~] `ExtraHoverTips` (L19); [x] `OnPlay` (L26); [x] `OnUpgrade` (L37) | [x] `foreach (Creature hittableEnemy in base.CombatState.HittableEnemies)` (L31) | — | [x] 实现 [x] Oracle  |
|  `RoyalGamble` | `Cards/RoyalGamble.cs` | [x] `CanonicalStarCost` (L12); [x] `CanonicalVars` (L14); [x] `CanonicalKeywords` (L16); [x] `OnPlay` (L23); [x] `OnUpgrade` (L29) | — | — | [x] 实现 [x] Oracle  |
|  `Royalties` | `Cards/Royalties.cs` | [x] `CanBeGeneratedInCombat` (L13); [x] `CanonicalVars` (L15); [x] `OnPlay` (L22); [x] `OnUpgrade` (L28) | — | — | [x] 实现 [x] Oracle  |
|  `SeekingEdge` | `Cards/SeekingEdge.cs` | [~] `ExtraHoverTips` (L14); [x] `CanonicalVars` (L16); [x] `OnPlay` (L23); [x] `OnUpgrade` (L30) | — | — | [x] 实现 [x] Oracle  |
|  `SevenStars` | `Cards/SevenStars.cs` | [x] `CanonicalStarCost` (L13); [x] `CanonicalVars` (L15); [x] `OnPlay` (L26); [x] `OnUpgrade` (L35) | — | — | [x] 实现 [x] Oracle  |
|  `ShiningStrike` | `Cards/ShiningStrike.cs` | [x] `CanonicalTags` (L14); [x] `CanonicalVars` (L16); [x] `OnPlay` (L27); [x] `GetResultLocationForCardPlay` (L36); [x] `OnUpgrade` (L47) | [x] `if (resultLocationForCardPlay.pileType == PileType.Discard)` (L39) | — | [x] 实现 [x] Oracle  |
|  `SolarStrike` | `Cards/SolarStrike.cs` | [x] `CanonicalTags` (L14); [x] `CanonicalVars` (L16); [x] `OnPlay` (L27); [x] `OnUpgrade` (L36) | — | — | [x] 实现 [x] Oracle  |
|  `SpectrumShift` | `Cards/SpectrumShift.cs` | [x] `CanonicalVars` (L13); [x] `OnPlay` (L20); [x] `OnUpgrade` (L26) | — | — | [x] 实现 [x] Oracle  |
|  `SpoilsOfBattle` | `Cards/SpoilsOfBattle.cs` | [x] `CanonicalVars` (L13); [~] `ExtraHoverTips` (L19); [x] `OnPlay` (L26); [x] `OnUpgrade` (L32) | — | — | [x] 实现 [x] Oracle  |
|  `Stardust` | `Cards/Stardust.cs` | [x] `HasStarCostX` (L13); [x] `CanonicalVars` (L15); [x] `OnPlay` (L22); [x] `OnUpgrade` (L30) | — | — | [x] 实现 [x] Oracle  |
|  `StrikeRegent` | `Cards/StrikeRegent.cs` | [x] `CanonicalTags` (L17); [x] `CanonicalVars` (L19); [x] `OnPlay` (L29); [x] `OnUpgrade` (L37) | — | — | [x] 实现 [x] Oracle  |
|  `SummonForth` | `Cards/SummonForth.cs` | [x] `CanonicalVars` (L14); [~] `ExtraHoverTips` (L16); [x] `OnPlay` (L32); [x] `OnUpgrade` (L44) | [x] `Where(delegate(SovereignBlade c) { CardPile? pile = c.Pile; return pile == null \|\| pile.Type != PileType.Hand; })` (L35) | — | [x] 实现 [x] Oracle  |
|  `Supermassive` | `Cards/Supermassive.cs` | [x] `CanonicalVars` (L18); [x] `OnPlay` (L30); [x] `OnUpgrade` (L38) | — | — | [x] 实现 [x] Oracle  |
|  `SwordSage` | `Cards/SwordSage.cs` | [~] `ExtraHoverTips` (L14); [x] `CanonicalVars` (L25); [x] `OnPlay` (L32); [x] `OnUpgrade` (L38) | — | — | [x] 实现 [x] Oracle  |
|  `Terraforming` | `Cards/Terraforming.cs` | [x] `CanonicalVars` (L14); [~] `ExtraHoverTips` (L16); [x] `OnPlay` (L23); [x] `OnUpgrade` (L28) | — | — | [x] 实现 [x] Oracle  |
|  `TheSealedThrone` | `Cards/TheSealedThrone.cs` | [x] `CanonicalStarCost` (L11); [x] `OnPlay` (L18); [x] `OnUpgrade` (L24) | — | — | [x] 实现 [x] Oracle  |
|  `TheSmith` | `Cards/TheSmith.cs` | [x] `CanonicalStarCost` (L13); [x] `CanonicalVars` (L15); [~] `ExtraHoverTips` (L17); [x] `OnPlay` (L24); [x] `OnUpgrade` (L30) | — | — | [x] 实现 [x] Oracle  |
|  `Tutor` | `Cards/Tutor.cs` | [x] `MultiplayerConstraint` (L14); [x] `OnPlay` (L21); [x] `OnUpgrade` (L32) | [x] `if (cardModel != null)` (L26) | 从牌堆选择牌移动至手牌。 | [x] 实现 [x] Oracle  |
|  `Tyranny` | `Cards/Tyranny.cs` | [~] `ExtraHoverTips` (L13); [x] `OnPlay` (L20); [x] `OnUpgrade` (L26) | — | — | [x] 实现 [x] Oracle  |
|  `Venerate` | `Cards/Venerate.cs` | [x] `CanonicalVars` (L12); [x] `OnPlay` (L19); [x] `OnUpgrade` (L25) | — | — | [x] 实现 [x] Oracle  |
|  `VoidForm` | `Cards/VoidForm.cs` | [x] `CanonicalKeywords` (L13); [x] `CanonicalVars` (L15); [x] `OnPlay` (L22); [x] `OnUpgrade` (L29) | — | — | [x] 实现 [x] Oracle  |
|  `WroughtInWar` | `Cards/WroughtInWar.cs` | [x] `CanonicalVars` (L15); [~] `ExtraHoverTips` (L21); [x] `OnPlay` (L28); [x] `OnUpgrade` (L37) | — | — | [x] 实现 [x] Oracle  |

## 显式生成/转换及派生卡依赖

依赖闭包包括从 Regent 池卡、职业遗物、解锁药水和其触发 Power 中找到的 `CreateCard<T>`、`TransformTo<T>` 与卡牌模型引用。池外通用/其他职业的模型也必须已接入通用 Env，不能把它们当作缺少条件而忽略。

| 依赖卡模型 | 原版源文件 | Overrides（逐项） | 条件分支/选择/循环（逐项） | 生成/依赖说明 | 完成 |
|---|---|---|---|---|---|
|  `Debris` | `Cards/Debris.cs` | [x] `MaxUpgradeLevel` (L10); [x] `CanonicalKeywords` (L12); [x] `OnPlay` (L19) | — | 由 Regent 内容显式引用/生成/变换。 | [x] 实现 [x] Oracle  |
|  `MinionDiveBomb` | `Cards/MinionDiveBomb.cs` | [x] `CanonicalTags` (L15); [x] `CanonicalVars` (L17); [x] `CanonicalKeywords` (L19); [x] `OnPlay` (L26); [x] `OnUpgrade` (L35) | — | 由 Regent 内容显式引用/生成/变换。 | [x] 实现 [x] Oracle  |
|  `MinionSacrifice` | `Cards/MinionSacrifice.cs` | [x] `GainsBlock` (L13); [x] `CanonicalTags` (L15); [x] `CanonicalKeywords` (L17); [x] `CanonicalVars` (L19); [x] `OnPlay` (L26); [x] `OnUpgrade` (L31) | — | 由 Regent 内容显式引用/生成/变换。 | [x] 实现 [x] Oracle  |
|  `MinionStrike` | `Cards/MinionStrike.cs` | [x] `CanonicalTags` (L14); [x] `CanonicalKeywords` (L20); [x] `CanonicalVars` (L22); [x] `OnPlay` (L33); [x] `OnUpgrade` (L42) | — | 由 Regent 内容显式引用/生成/变换。 | [x] 实现 [x] Oracle  |
|  `SovereignBlade` | `Cards/SovereignBlade.cs` | [~] `ExtraRunAssetPaths` (L33); [x] `TargetType` (L38); [x] `GainsBlock` (L53); [x] `CanonicalKeywords` (L94); [x] `CanonicalVars` (L96); [x] `OnPlay` (L122); [x] `OnUpgrade` (L169); [~] `AfterCloned` (L174); [x] `AfterDowngraded` (L180); [~] `AfterTransformedFrom` (L187); [~] `AfterCardChangedPiles` (L192) | [x] `if (!HasSeekingEdge)` (L42); [x] `if (base.IsMutable && base.Owner != null)` (L109); [~] `ternary@L124: …animName = ((base.Owner.Character is Regent) ? "sovereignBladeTrigger" : "Cast"); float delay = ((base.Owner.Char…` (L124); [~] `ternary@L125: …at delay = ((base.Owner.Character is Regent) ? 0.25f : base.Owner.Character.CastAnimDelay); AttackCommand attackC…` (L125); [x] `if (HasSeekingEdge)` (L129); [~] `if (hittableEnemies.Count <= 0)` (L134); [~] `if (vfxNode != null && nCreature != null)` (L140); [x] `else` (L147); [~] `if (vfxNode != null && nCreature != null)` (L154); [x] `if (GetOwnerParryAmount(this) > 0m)` (L163); [~] `if (card != this)` (L194); [~] `if ((!CreatedThroughForge && oldPileType == PileType.None) \|\| oldPileType == PileType.Exhaust)` (L198); [~] `if (card.Pile.Type == PileType.Exhaust)` (L202); [~] `FirstOrDefault((NSovereignBladeVfx b) => b.Card == originalCard)` (L224); [x] `if (!card.IsMutable)` (L234); [x] `if (card.Owner == null)` (L238); [x] `if (card.Pile == null)` (L242); [x] `if (!card.Pile.IsCombatPile)` (L246) | 由 Regent 内容显式引用/生成/变换。 | [x] 实现 [x] Oracle  |

## 关联 Power 依赖闭包

含 Regent 独有 Power 和共享通用 Power。`OnApply`/`Before*`/`After*`、静态值和内部数据只按原版覆写范围处理；触发顺序以 Hook 源码为准。

| Power 模型 | 原版源文件 | Overrides（逐项） | 条件分支/选择/循环（逐项） | 生成/依赖说明 | 完成 |
|---|---|---|---|---|---|
|  `ArsenalPower` | `Powers/ArsenalPower.cs` | [x] `Type` (L13); [x] `StackType` (L15); [~] `ExtraHoverTips` (L17); [x] `AfterCardGeneratedForCombat` (L19) | [x] `if (creator != null && creator.Creature == base.Owner)` (L21) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `BlackHolePower` | `Powers/BlackHolePower.cs` | [x] `Type` (L13); [x] `StackType` (L15); [x] `AfterCardPlayed` (L21); [x] `AfterStarsGained` (L29) | [x] `if (cardPlay.Resources.StarsSpent > 0 && cardPlay.Card.Owner == base.Owner.Player && cardPlay.IsLastInSeries)` (L23); [x] `if (amount > 0 && gainer == base.Owner.Player)` (L31) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `BlockNextTurnPower` | `Powers/BlockNextTurnPower.cs` | [x] `Type` (L13); [x] `StackType` (L15); [~] `ExtraHoverTips` (L17); [x] `AfterBlockCleared` (L19) | [x] `if (creature == base.Owner)` (L21) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `ChildOfTheStarsPower` | `Powers/ChildOfTheStarsPower.cs` | [x] `Type` (L11); [x] `StackType` (L13); [x] `AfterStarsSpent` (L15) | [x] `if (amount > 0 && spender == base.Owner.Player)` (L17) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `ConquerorPower` | `Powers/ConquerorPower.cs` | [x] `Type` (L18); [x] `StackType` (L20); [~] `ExtraHoverTips` (L22); [x] `ModifyDamageMultiplicative` (L24); [x] `AfterSideTurnEnd` (L41) | [x] `if (!(cardSource is SovereignBlade))` (L26); [x] `if (!props.IsPoweredAttack())` (L30); [x] `if (target != base.Owner)` (L34); [x] `if (participants.Contains(base.Owner))` (L43) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `CrushUnderPower` | `Powers/CrushUnderPower.cs` | [x] `OriginModel` (L7); [x] `IsPositive` (L9) | — | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `DrawCardsNextTurnPower` | `Powers/DrawCardsNextTurnPower.cs` | [x] `Type` (L18); [x] `StackType` (L20); [x] `ModifyHandDraw` (L22); [x] `AfterSideTurnStart` (L35) | [x] `if (player != base.Owner.Player)` (L24); [x] `if (base.AmountOnTurnStart == 0)` (L28); [x] `if (participants.Contains(base.Owner) && base.AmountOnTurnStart != 0)` (L37) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `DyingStarPower` | `Powers/DyingStarPower.cs` | [x] `OriginModel` (L7); [x] `IsPositive` (L9) | — | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `EnergyNextTurnPower` | `Powers/EnergyNextTurnPower.cs` | [x] `Type` (L12); [x] `StackType` (L14); [~] `ExtraHoverTips` (L16); [x] `AfterEnergyReset` (L18) | [x] `if (player == base.Owner.Player)` (L20) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `ForegoneConclusionPower` | `Powers/ForegoneConclusionPower.cs` | [x] `Type` (L14); [x] `StackType` (L16); [x] `BeforeHandDraw` (L18) | [x] `if (player == base.Owner.Player)` (L20) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `FurnacePower` | `Powers/FurnacePower.cs` | [x] `Type` (L14); [x] `StackType` (L16); [~] `ExtraHoverTips` (L18); [x] `AfterSideTurnStart` (L20) | [x] `if (participants.Contains(base.Owner))` (L22) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `GenesisPower` | `Powers/GenesisPower.cs` | [x] `Type` (L10); [x] `StackType` (L12); [x] `AfterEnergyReset` (L14) | [x] `if (player == base.Owner.Player)` (L16) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `HammerTimePower` | `Powers/HammerTimePower.cs` | [x] `Type` (L13); [x] `StackType` (L15); [~] `ExtraHoverTips` (L17); [x] `AfterForge` (L19) | [x] `if (source is HammerTimePower \|\| forger != base.Owner.Player)` (L21); [x] `Where((Player p) => p.Creature.IsAlive && p != forger)` (L25); [x] `foreach (Player item in enumerable)` (L26) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `MonarchsGazePower` | `Powers/MonarchsGazePower.cs` | [x] `Type` (L12); [x] `StackType` (L14); [x] `AfterDamageGiven` (L16) | [x] `if (dealer == base.Owner && props.IsPoweredAttack())` (L18) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `MonarchsGazeStrengthDownPower` | `Powers/MonarchsGazeStrengthDownPower.cs` | [x] `OriginModel` (L7); [x] `IsPositive` (L9) | — | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `MonologuePower` | `Powers/MonologuePower.cs` | [x] `Type` (L29); [x] `StackType` (L31); [~] `DisplayAmount` (L43); [x] `InstanceType` (L45); [x] `CanonicalVars` (L47); [~] `ExtraHoverTips` (L53); [x] `InitInternalData` (L55); [x] `BeforeCardPlayed` (L60); [x] `AfterCardPlayed` (L70); [x] `AfterSideTurnEnd` (L81) | [x] `if (base.DynamicVars["StrengthApplied"].IntValue != 0)` (L35); [x] `if (cardPlay.Card.Owner.Creature != base.Owner)` (L62); [x] `if (cardPlay.Card.Owner == base.Owner.Player && GetInternalData<Data>().amountsForPlayedCards.Remove(cardPlay.Card, out var value))` (L72); [x] `if (participants.Contains(base.Owner))` (L83) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `OrbitPower` | `Powers/OrbitPower.cs` | [x] `Type` (L21); [x] `StackType` (L23); [~] `DisplayAmount` (L25); [~] `ExtraHoverTips` (L27); [x] `InstanceType` (L29); [x] `CanonicalVars` (L31); [x] `InitInternalData` (L33); [x] `AfterEnergySpent` (L38) | [x] `if (card.Owner.Creature == base.Owner && amount > 0)` (L40); [x] `if (triggers > 0)` (L45) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `PaleBlueDotPower` | `Powers/PaleBlueDotPower.cs` | [~] `DisplayAmount` (L27); [x] `Type` (L29); [x] `StackType` (L31); [x] `CanonicalVars` (L33); [x] `InitInternalData` (L37); [x] `AfterCardPlayed` (L42); [x] `AfterSideTurnEnd` (L61) | [x] `if (cardPlay.Card.Owner != base.Owner.Player)` (L44); [x] `if (!internalData.alreadyActivatedThisTurn)` (L49); [x] `if (AttacksPlayedThisTurn >= 5)` (L52); [x] `if (!participants.Contains(base.Owner))` (L63) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `ParryPower` | `Powers/ParryPower.cs` | [x] `Type` (L11); [x] `StackType` (L13) | — | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `PillarOfCreationPower` | `Powers/PillarOfCreationPower.cs` | [x] `Type` (L13); [x] `StackType` (L15); [~] `ExtraHoverTips` (L17); [x] `AfterCardGeneratedForCombat` (L19) | [x] `if (creator != null && creator.Creature == base.Owner)` (L21) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `PlatingPower` | `Powers/PlatingPower.cs` | [x] `Type` (L19); [x] `StackType` (L21); [~] `ShouldScaleInMultiplayer` (L23); [~] `ExtraHoverTips` (L25); [x] `CanonicalVars` (L27); [x] `AfterApplied` (L29); [x] `BeforeSideTurnStart` (L41); [x] `BeforeSideTurnEndEarly` (L61); [x] `AfterSideTurnStart` (L70); [~] `GetScaledAmountForMultiplayer` (L85) | [x] `if (base.Owner.Side == CombatSide.Enemy)` (L31); [x] `if (side != CombatSide.Player)` (L43); [x] `if (base.Owner.IsPlayer)` (L47); [x] `if (combatState.RoundNumber > 1)` (L51); [x] `if (participants.Contains(base.Owner))` (L63); [x] `if (participants.Contains(base.Owner) && (base.Owner.Player == null \|\| base.Owner.Player.PlayerCombatState.TurnNumber != 1) && (base.Owner.Side != CombatSide.Enemy \|\| combatState.RoundNumber != 1))` (L72); [x] `if (base.Owner.Side == CombatSide.Enemy)` (L74); [x] `else` (L78) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `ReflectPower` | `Powers/ReflectPower.cs` | [x] `Type` (L15); [x] `StackType` (L17); [x] `AfterDamageReceived` (L19); [x] `AfterSideTurnStart` (L27) | [x] `if (target == base.Owner && result.BlockedDamage > 0 && props.IsPoweredAttack() && dealer != null)` (L21); [x] `if (participants.Contains(base.Owner))` (L29) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `RetainHandPower` | `Powers/RetainHandPower.cs` | [x] `Type` (L15); [x] `StackType` (L17); [x] `ShouldFlush` (L19); [x] `AfterSideTurnEnd` (L28) | [x] `if (player != base.Owner.Player)` (L21); [x] `if (participants.Contains(base.Owner))` (L30) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `RoyaltiesPower` | `Powers/RoyaltiesPower.cs` | [x] `Type` (L10); [x] `StackType` (L12); [x] `AfterCombatEnd` (L14) | — | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `SeekingEdgePower` | `Powers/SeekingEdgePower.cs` | [x] `Type` (L13); [x] `StackType` (L15); [~] `ExtraHoverTips` (L17) | — | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `SpectrumShiftPower` | `Powers/SpectrumShiftPower.cs` | [x] `Type` (L17); [x] `StackType` (L19); [x] `BeforeHandDraw` (L21) | [x] `if (player == base.Owner.Player)` (L23) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `StarNextTurnPower` | `Powers/StarNextTurnPower.cs` | [x] `Type` (L10); [x] `StackType` (L12); [x] `AfterEnergyReset` (L14) | [x] `if (player == base.Owner.Player)` (L16) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `StrengthPower` | `Powers/StrengthPower.cs` | [x] `Type` (L10); [x] `StackType` (L12); [x] `AllowNegative` (L14); [x] `ModifyDamageAdditive` (L16) | [x] `if (base.Owner != dealer)` (L18); [x] `if (!props.IsPoweredAttack())` (L22) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `SwordSagePower` | `Powers/SwordSagePower.cs` | [x] `Type` (L14); [x] `StackType` (L16); [~] `ExtraHoverTips` (L18); [x] `AfterPowerAmountChanged` (L29); [x] `AfterCardEnteredCombat` (L47); [x] `AfterRemoved` (L57) | [x] `if (!(power is SwordSagePower))` (L31); [x] `if (power.Owner != base.Owner)` (L35); [x] `foreach (CardModel item in enumerable)` (L40); [x] `if (card.IsClone)` (L49); [x] `foreach (CardModel item in enumerable)` (L60); [x] `if (item is SovereignBlade sovereignBlade)` (L62); [x] `if (card.Owner == base.Owner.Player && card is SovereignBlade sovereignBlade)` (L72) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `TheSealedThronePower` | `Powers/TheSealedThronePower.cs` | [x] `Type` (L10); [x] `StackType` (L12); [x] `BeforeCardPlayed` (L14) | [x] `if (cardPlay.Card.Owner == base.Owner.Player)` (L16) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `TyrannyPower` | `Powers/TyrannyPower.cs` | [x] `Type` (L15); [x] `StackType` (L17); [~] `ExtraHoverTips` (L19); [x] `ModifyHandDraw` (L21); [x] `AfterPlayerTurnStart` (L30) | [x] `if (player != base.Owner.Player)` (L23); [x] `if (player != base.Owner.Player)` (L32); [x] `foreach (CardModel item in await CardSelectCmd.FromHand(choiceContext, player, new CardSelectorPrefs(CardSelectorPrefs.ExhaustSelectionPrompt, base.Amount), null, this))` (L36) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `VigorPower` | `Powers/VigorPower.cs` | [x] `Type` (L21); [x] `StackType` (L23); [x] `InitInternalData` (L25); [x] `BeforeAttack` (L30); [x] `ModifyDamageAdditive` (L58); [x] `AfterAttack` (L80) | [x] `if (command.Attacker != base.Owner)` (L32); [x] `if (!command.DamageProps.IsPoweredAttack())` (L36); [x] `if (internalData.commandToModify != null)` (L41); [x] `if (command.ModelSource != null && !(command.ModelSource is CardModel))` (L45); [x] `if (!command.DamageProps.IsPoweredAttack())` (L49); [x] `if (base.Owner != dealer)` (L60); [x] `if (!props.IsPoweredAttack())` (L64); [x] `if (internalData.commandToModify != null && cardSource != null && cardSource != internalData.commandToModify.ModelSource)` (L69); [x] `if (internalData.commandToModify != null && internalData.commandToModify.Attacker != dealer)` (L73); [x] `if (command == internalData.commandToModify)` (L83) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `VoidFormPower` | `Powers/VoidFormPower.cs` | [x] `Type` (L23); [x] `StackType` (L25); [x] `AfterApplied` (L48); [x] `AfterRemoved` (L54); [x] `InitInternalData` (L60); [x] `BeforePowerAmountChanged` (L65); [x] `BeforeApplied` (L75); [x] `TryModifyEnergyCostInCombatLate` (L81); [x] `TryModifyStarCost` (L92); [x] `AfterCardPlayed` (L103); [x] `BeforeSideTurnStart` (L116) | [~] `if (_vfx == null)` (L31); [~] `if (!_vfx.IsValid())` (L35); [x] `if (power != this)` (L67); [x] `if (ShouldSkip(card))` (L84); [x] `if (ShouldSkip(card))` (L95); [x] `if (cardPlay.Card.Owner.Creature == base.Owner && cardPlay != null && !cardPlay.IsAutoPlay && cardPlay.IsLastInSeries)` (L105); [x] `if (GetInternalData<Data>().cardsPlayedThisTurn >= base.Amount)` (L109); [x] `if (!participants.Contains(base.Owner))` (L118); [x] `if (!flag2)` (L131); [x] `switch (card.Pile?.Type)` (L134); [x] `case PileType.Hand:` (L136); [x] `case PileType.Play:` (L137); [x] `default:` (L140); [x] `if (!flag2)` (L146) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `VulnerablePower` | `Powers/VulnerablePower.cs` | [x] `Type` (L19); [x] `StackType` (L21); [x] `CanonicalVars` (L23); [x] `ModifyDamageMultiplicative` (L28); [x] `AfterSideTurnEnd` (L60) | [x] `if (target != base.Owner)` (L30); [x] `if (!props.IsPoweredAttack())` (L34); [x] `if (dealer != null)` (L39); [x] `if (paperPhrog != null)` (L42); [x] `if (crueltyPower != null)` (L47); [x] `if (power != null)` (L53); [x] `if (side == CombatSide.Enemy)` (L62) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |
|  `WeakPower` | `Powers/WeakPower.cs` | [x] `Type` (L19); [x] `StackType` (L21); [x] `CanonicalVars` (L23); [x] `ModifyDamageMultiplicative` (L25); [x] `AfterSideTurnEnd` (L49) | [x] `if (dealer != base.Owner)` (L27); [x] `if (!props.IsPoweredAttack())` (L31); [x] `if (paperKrane != null)` (L37); [x] `if (power != null)` (L42); [x] `if (side == CombatSide.Enemy)` (L51) | 由 Regent 内容直接或递归触发。 | [x] 实现 [x] Oracle  |

## RegentRelicPool 全部遗物

| 遗物模型 | 原版源文件 | Overrides（逐项） | 条件分支/选择/循环（逐项） | 生成/依赖说明 | 完成 |
|---|---|---|---|---|---|
|  `DivineRight` | `Relics/DivineRight.cs` | [x] `Rarity` (L12); [x] `CanonicalVars` (L14); [x] `AfterRoomEntered` (L16) | [x] `if (room is CombatRoom)` (L18) | — | [x] 实现 [x] Oracle  |
|  `FencingManual` | `Relics/FencingManual.cs` | [x] `Rarity` (L15); [x] `CanonicalVars` (L17); [~] `ExtraHoverTips` (L19); [x] `AfterSideTurnStart` (L21) | [x] `if (participants.Contains(base.Owner.Creature) && base.Owner.PlayerCombatState.TurnNumber <= 1)` (L23) | — | [x] 实现 [x] Oracle  |
|  `GalacticDust` | `Relics/GalacticDust.cs` | [~] `ShowCounter` (L21); [~] `DisplayAmount` (L23); [~] `ExtraHoverTips` (L64); [x] `Rarity` (L66); [x] `CanonicalVars` (L68); [x] `AfterStarsSpent` (L88) | [x] `if (!IsActivating)` (L27); [x] `if (IsActivating)` (L76); [x] `else` (L80); [x] `ternary@L83: …base.Status = ((StarsSpent == intValue - 1) ? RelicStatus.Active : RelicStatus.Normal); } InvokeDisplayAmountC…` (L83); [x] `if (spender == base.Owner)` (L90); [x] `if (StarsSpent >= base.DynamicVars.Stars.IntValue)` (L93) | — | [x] 实现 [x] Oracle  |
|  `LunarPastry` | `Relics/LunarPastry.cs` | [x] `Rarity` (L15); [x] `CanonicalVars` (L17); [x] `AfterSideTurnEnd` (L19) | [x] `if (participants.Contains(base.Owner.Creature))` (L21) | — | [x] 实现 [x] Oracle  |
|  `MiniRegent` | `Relics/MiniRegent.cs` | [x] `Rarity` (L21); [x] `CanonicalVars` (L23); [~] `ExtraHoverTips` (L25); [x] `AfterStarsSpent` (L40); [x] `BeforeSideTurnStart` (L50); [x] `AfterCombatEnd` (L60) | [x] `if (spender == base.Owner && !UsedThisTurn)` (L42); [x] `if (!participants.Contains(base.Owner.Creature))` (L52) | — | [x] 实现 [x] Oracle  |
|  `OrangeDough` | `Relics/OrangeDough.cs` | [x] `Rarity` (L17); [x] `CanonicalVars` (L19); [x] `AfterSideTurnStart` (L21) | [x] `if (participants.Contains(base.Owner.Creature) && base.Owner.PlayerCombatState.TurnNumber <= 1)` (L23) | — | [x] 实现 [x] Oracle  |
|  `Regalite` | `Relics/Regalite.cs` | [x] `Rarity` (L20); [x] `CanonicalVars` (L22); [x] `AfterCardGeneratedForCombat` (L37); [x] `BeforeSideTurnStart` (L47); [x] `AfterCombatEnd` (L57) | [x] `if (creator != null && creator == base.Owner && !UsedThisTurn)` (L39); [x] `if (!participants.Contains(base.Owner.Creature))` (L49) | — | [x] 实现 [x] Oracle  |
|  `VitruvianMinion` | `Relics/VitruvianMinion.cs` | [x] `Rarity` (L13); [~] `ExtraHoverTips` (L15); [x] `ModifyDamageMultiplicative` (L17); [x] `ModifyBlockMultiplicative` (L34) | [x] `if (cardSource == null)` (L19); [x] `if (cardSource.Owner != base.Owner)` (L23); [x] `if (!cardSource.Tags.Contains(CardTag.Minion))` (L27); [x] `if (cardSource == null)` (L36); [x] `if (cardSource.Owner != base.Owner)` (L40); [x] `if (!cardSource.Tags.Contains(CardTag.Minion))` (L44) | — | [x] 实现 [x] Oracle  |

## RegentPotionPool 全部药水

| 药水模型 | 原版源文件 | Overrides（逐项） | 条件分支/选择/循环（逐项） | 生成/依赖说明 | 完成 |
|---|---|---|---|---|---|
|  `CosmicConcoction` | `Potions/CosmicConcoction.cs` | [x] `Rarity` (L17); [x] `Usage` (L19); [x] `TargetType` (L21); [x] `CanonicalVars` (L23); [x] `OnUse` (L25) | [x] `foreach (CardModel item in distinctForCombat)` (L30) | — | [x] 实现 [x] Oracle  |
|  `KingsCourage` | `Potions/KingsCourage.cs` | [x] `Rarity` (L15); [x] `Usage` (L17); [x] `TargetType` (L19); [x] `CanonicalVars` (L21); [~] `ExtraHoverTips` (L23); [x] `OnUse` (L25) | — | — | [x] 实现 [x] Oracle  |
|  `StarPotion` | `Potions/StarPotion.cs` | [x] `Rarity` (L14); [x] `Usage` (L16); [x] `TargetType` (L18); [x] `CanonicalVars` (L20); [x] `OnUse` (L22) | — | — | [x] 实现 [x] Oracle  |

## 星星资源与跨回合机制核对点

- [x] 玩家战斗态中的 `Stars`/`SetStars`/`GainStars`/`LoseStars`；初始为 0、边界截断和 decimal 转整数规则与原版一致。
- [x] 卡牌 `CanonicalStarCost`、X 星、临时星费用、费用修改 hook、可支付判定、支付发生顺序和 `LastStarsSpent`。
- [x] `ShouldGainStars`、`AfterStarsGained`、`AfterStarsSpent` listener 顺序；禁止获取、重复消费和星星产生后反应。
- [x] 以星代能量：缺少能量时的星星倍数、组合支付顺序、可玩判定和历史记录。
- [x] 星星快照/哈希/克隆/选择前缀重放完整保留；恢复后无重抽、无重复 hook。
- [x] `AfterEnergyReset`、`Before/AfterSideTurnStart`、`AfterSideTurnEnd` 等跨回合 Power 对参与者、玩家归属、触发先后与消耗/移除的限制。
- [x] Forge、下一回合能量/星星/抽牌/格挡、保留手牌、额外伤害、临时力量、回合/打牌/生成计数的状态在 clone/restore 后一致。
- [x] Regent start character: HP/gold/deck/relic/pools/counter metadata; unlock gates: Regent2/3/4/5/6/7 epochs and allUnlocked behavior.

## 验证记录索引

- [x] 逐模型/分支 Oracle 差分用例/原始 JSON：报告中记录相对文件和 SHA-256。
- [x] `scripts/check.sh` 原始 stdout/stderr、退出码：报告中记录相对文件和 SHA-256。
- [x] 最终完成核对：91/91 卡、全部显式派生卡、所有 Power、8/8 遗物、3/3 药水、角色与池/纪元；清单无未勾选项。
