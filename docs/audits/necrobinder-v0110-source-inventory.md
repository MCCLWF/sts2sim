# Necrobinder v0.111.0 source inventory

Baseline: `59b51ec54a52b5069a56d05c768b35f81e950776`; game Core source at `../Slay the Spire 2/src/Core` (release v0.111.0, 41cef1ea). This inventory is captured before Necrobinder content implementation. C# `overrides` come from the committed v0.111.0 `data/content.json`; control-flow and call-site line items are scanned from the corresponding original C# files in source order.

## Scope and source model inventory

| Group | Count | Source-of-truth enumeration |
|---|---:|---|
| Necrobinder card pool | 91 | `NecrobinderCardPool.AllCardIds` |
| Necrobinder relic pool | 8 | `NecrobinderRelicPool.AllRelicIds` |
| Character and pools | 4 | Character, card pool, relic pool, potion pool |
| Directly referenced power models | 34 | Generic `PowerCmd` targets in the 91 card bodies |
| Additional generated/support models | 5 | Osty, Soul/Sweeping Gaze cards, Die For You power, SoulsPower enchantment |

### Character, pools, and direct content models

- **CHARACTER.NECROBINDER** — `src/Core/Models/Characters/Necrobinder.cs`; overrides: `CharacterModel.GetArchitectAttackVfx`, `CharacterModel.get_AnimationStates`, `CharacterModel.get_AttackAnimDelay`, `CharacterModel.get_CardPool`, `CharacterModel.get_CastAnimDelay`, `CharacterModel.get_CharacterTransitionSfx`, `CharacterModel.get_DialogueColor`, `CharacterModel.get_EnergyLabelOutlineColor`, `CharacterModel.get_ExtraAssetPaths`, `CharacterModel.get_Gender`, `CharacterModel.get_MapDrawingColor`, `CharacterModel.get_NameColor`, `CharacterModel.get_PotionPool`, `CharacterModel.get_RelicPool`, `CharacterModel.get_RemoteTargetingLineColor`, `CharacterModel.get_RemoteTargetingLineOutline`, `CharacterModel.get_StartingDeck`, `CharacterModel.get_StartingGold`, `CharacterModel.get_StartingHp`, `CharacterModel.get_StartingRelics`, `CharacterModel.get_UnlocksAfterRunAs`.
  Starting contract: HP `66`, gold `99`, energy `3`, deck `CARD.STRIKE_NECROBINDER, CARD.STRIKE_NECROBINDER, CARD.STRIKE_NECROBINDER, CARD.STRIKE_NECROBINDER, CARD.DEFEND_NECROBINDER, CARD.DEFEND_NECROBINDER, CARD.DEFEND_NECROBINDER, CARD.DEFEND_NECROBINDER, CARD.BODYGUARD, CARD.UNLEASH`, relics `RELIC.BOUND_PHYLACTERY`, no starting potions; original `UnlocksAfterRunAs` returns `Regent`.
  Epoch gate: `FilterThroughEpochs` materializes the input in order and removes locked epoch cards while preserving the relative order of remaining cards. This method makes no direct RNG calls. Exact card/relic membership and epoch order are below.
- **CARD_POOL.NECROBINDER_CARD_POOL** — `src/Core/Models/CardPools/NecrobinderCardPool.cs`; overrides: `CardPoolModel.FilterThroughEpochs`, `CardPoolModel.GenerateAllCards`, `CardPoolModel.get_CardFrameMaterialPath`, `CardPoolModel.get_DeckEntryCardColor`, `CardPoolModel.get_EnergyColorName`, `CardPoolModel.get_EnergyOutlineColor`, `CardPoolModel.get_IsColorless`, `CardPoolModel.get_Title`.
  Candidates in original pool order: CARD.AFTERLIFE, CARD.BANSHEES_CRY, CARD.BLIGHT_STRIKE, CARD.BODYGUARD, CARD.BONE_SHARDS, CARD.BORROWED_TIME, CARD.BURY, CARD.CACOPHONY, CARD.CALCIFY, CARD.CALL_OF_THE_VOID, CARD.CAPTURE_SPIRIT, CARD.CLEANSE, CARD.COUNTDOWN, CARD.DANSE_MACABRE, CARD.DEATH_MARCH, CARD.DEATHBRINGER, CARD.DEATHS_DOOR, CARD.DEBILITATE, CARD.DEFEND_NECROBINDER, CARD.DEFILE, CARD.DEFY, CARD.DELAY, CARD.DEMESNE, CARD.DEVOUR_LIFE, CARD.DIRGE, CARD.DRAIN_POWER, CARD.DREDGE, CARD.EIDOLON, CARD.END_OF_DAYS, CARD.ENFEEBLING_TOUCH, CARD.ERADICATE, CARD.FEAR, CARD.FETCH, CARD.FLATTEN, CARD.FORBIDDEN_GRIMOIRE, CARD.FRIENDSHIP, CARD.GLIMPSE_BEYOND, CARD.GRAVE_WARDEN, CARD.GRAVEBLAST, CARD.HANG, CARD.HAUNT, CARD.HIGH_FIVE, CARD.INVOKE, CARD.LEGION_OF_BONE, CARD.LETHALITY, CARD.MELANCHOLY, CARD.MISERY, CARD.NECRO_MASTERY, CARD.NEGATIVE_PULSE, CARD.NEUROSURGE, CARD.NO_ESCAPE, CARD.OBLIVION, CARD.PAGESTORM, CARD.PARSE, CARD.POKE, CARD.PROTECTOR, CARD.PULL_AGGRO, CARD.PULL_FROM_BELOW, CARD.PUTREFY, CARD.RATTLE, CARD.REANIMATE, CARD.REAP, CARD.REAPER_FORM, CARD.REAVE, CARD.RIGHT_HAND_HAND, CARD.SACRIFICE, CARD.SCOURGE, CARD.SCULPTING_STRIKE, CARD.SEANCE, CARD.SENTRY_MODE, CARD.SEVERANCE, CARD.SHARED_FATE, CARD.SHROUD, CARD.SIC_EM, CARD.SLEIGHT_OF_FLESH, CARD.SNAP, CARD.SOUL_STORM, CARD.SOULBOUND, CARD.SOW, CARD.SPIRIT_OF_ASH, CARD.SPUR, CARD.SQUEEZE, CARD.STRIKE_NECROBINDER, CARD.THE_SCYTHE, CARD.TIMES_UP, CARD.TRANSFIGURE, CARD.UNDEATH, CARD.UNDERWORLD, CARD.UNLEASH, CARD.VEILPIERCER, CARD.WISP.
- **RELIC_POOL.NECROBINDER_RELIC_POOL** — `src/Core/Models/RelicPools/NecrobinderRelicPool.cs`; overrides: `RelicPoolModel.GenerateAllRelics`, `RelicPoolModel.GetUnlockedRelics`, `RelicPoolModel.get_EnergyColorName`, `RelicPoolModel.get_LabOutlineColor`.
  Candidates in original pool order: RELIC.BIG_HAT, RELIC.BONE_FLUTE, RELIC.BOOK_REPAIR_KNIFE, RELIC.BOOKMARK, RELIC.BOUND_PHYLACTERY, RELIC.FUNERARY_MASK, RELIC.IVORY_TILE, RELIC.UNDYING_SIGIL.
- **POTION_POOL.NECROBINDER_POTION_POOL** — `src/Core/Models/PotionPools/NecrobinderPotionPool.cs`; overrides: `PotionPoolModel.GenerateAllPotions`, `PotionPoolModel.GetUnlockedPotions`, `PotionPoolModel.get_EnergyColorName`, `PotionPoolModel.get_LabOutlineColor`.
  Integration contract only: `POTION.POTION_OF_DOOM`, `POTION.POT_OF_GHOULS`, and `POTION.BONE_BREW` stay owned by the parallel all-potions task. This task will only verify character pool exposure and use their effects after merge; no potion model edits here.

### All 91 card models: every declared override

| Card id | C# source | Overrides in source metadata |
|---|---|---|
| `CARD.AFTERLIFE` | `src/Core/Models/Cards/Afterlife.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.BANSHEES_CRY` | `src/Core/Models/Cards/BansheesCry.cs` | `AbstractModel.AfterCardEnteredCombat`, `AbstractModel.AfterCardPlayed`, `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.BLIGHT_STRIKE` | `src/Core/Models/Cards/BlightStrike.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.BODYGUARD` | `src/Core/Models/Cards/Bodyguard.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.BONE_SHARDS` | `src/Core/Models/Cards/BoneShards.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_GainsBlock`, `CardModel.get_ShouldGlowRedInternal` |
| `CARD.BORROWED_TIME` | `src/Core/Models/Cards/BorrowedTime.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.BURY` | `src/Core/Models/Cards/Bury.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars` |
| `CARD.CACOPHONY` | `src/Core/Models/Cards/Cacophony.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_MultiplayerConstraint` |
| `CARD.CALCIFY` | `src/Core/Models/Cards/Calcify.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars` |
| `CARD.CALL_OF_THE_VOID` | `src/Core/Models/Cards/CallOfTheVoid.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.CAPTURE_SPIRIT` | `src/Core/Models/Cards/CaptureSpirit.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.CLEANSE` | `src/Core/Models/Cards/Cleanse.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.COUNTDOWN` | `src/Core/Models/Cards/Countdown.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.DANSE_MACABRE` | `src/Core/Models/Cards/DanseMacabre.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.DEATH_MARCH` | `src/Core/Models/Cards/DeathMarch.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars` |
| `CARD.DEATHBRINGER` | `src/Core/Models/Cards/Deathbringer.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.DEATHS_DOOR` | `src/Core/Models/Cards/DeathsDoor.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_GainsBlock`, `CardModel.get_ShouldGlowGoldInternal` |
| `CARD.DEBILITATE` | `src/Core/Models/Cards/Debilitate.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.DEFEND_NECROBINDER` | `src/Core/Models/Cards/DefendNecrobinder.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_GainsBlock` |
| `CARD.DEFILE` | `src/Core/Models/Cards/Defile.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars` |
| `CARD.DEFY` | `src/Core/Models/Cards/Defy.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_GainsBlock` |
| `CARD.DELAY` | `src/Core/Models/Cards/Delay.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_GainsBlock` |
| `CARD.DEMESNE` | `src/Core/Models/Cards/Demesne.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.DEVOUR_LIFE` | `src/Core/Models/Cards/DevourLife.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.DIRGE` | `src/Core/Models/Cards/Dirge.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_HasEnergyCostX` |
| `CARD.DRAIN_POWER` | `src/Core/Models/Cards/DrainPower.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars` |
| `CARD.DREDGE` | `src/Core/Models/Cards/Dredge.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars` |
| `CARD.EIDOLON` | `src/Core/Models/Cards/Eidolon.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanBeGeneratedInCombat`, `CardModel.get_CanonicalKeywords`, `CardModel.get_ExtraHoverTips` |
| `CARD.END_OF_DAYS` | `src/Core/Models/Cards/EndOfDays.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.ENFEEBLING_TOUCH` | `src/Core/Models/Cards/EnfeeblingTouch.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.ERADICATE` | `src/Core/Models/Cards/Eradicate.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_HasEnergyCostX` |
| `CARD.FEAR` | `src/Core/Models/Cards/Fear.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.FETCH` | `src/Core/Models/Cards/Fetch.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ShouldGlowGoldInternal`, `CardModel.get_ShouldGlowRedInternal` |
| `CARD.FLATTEN` | `src/Core/Models/Cards/Flatten.cs` | `AbstractModel.AfterAttack`, `AbstractModel.AfterCardEnteredCombat`, `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ShouldGlowGoldInternal`, `CardModel.get_ShouldGlowRedInternal` |
| `CARD.FORBIDDEN_GRIMOIRE` | `src/Core/Models/Cards/ForbiddenGrimoire.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords` |
| `CARD.FRIENDSHIP` | `src/Core/Models/Cards/Friendship.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.GLIMPSE_BEYOND` | `src/Core/Models/Cards/GlimpseBeyond.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_MultiplayerConstraint` |
| `CARD.GRAVE_WARDEN` | `src/Core/Models/Cards/GraveWarden.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_GainsBlock` |
| `CARD.GRAVEBLAST` | `src/Core/Models/Cards/Graveblast.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars` |
| `CARD.HANG` | `src/Core/Models/Cards/Hang.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars` |
| `CARD.HAUNT` | `src/Core/Models/Cards/Haunt.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.HIGH_FIVE` | `src/Core/Models/Cards/HighFive.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_IsPlayable`, `CardModel.get_ShouldGlowRedInternal` |
| `CARD.INVOKE` | `src/Core/Models/Cards/Invoke.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.LEGION_OF_BONE` | `src/Core/Models/Cards/LegionOfBone.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_MultiplayerConstraint` |
| `CARD.LETHALITY` | `src/Core/Models/Cards/Lethality.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars` |
| `CARD.MELANCHOLY` | `src/Core/Models/Cards/Melancholy.cs` | `AbstractModel.AfterDeath`, `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_GainsBlock` |
| `CARD.MISERY` | `src/Core/Models/Cards/Misery.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars` |
| `CARD.NECRO_MASTERY` | `src/Core/Models/Cards/NecroMastery.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.NEGATIVE_PULSE` | `src/Core/Models/Cards/NegativePulse.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_GainsBlock` |
| `CARD.NEUROSURGE` | `src/Core/Models/Cards/Neurosurge.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.NO_ESCAPE` | `src/Core/Models/Cards/NoEscape.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.OBLIVION` | `src/Core/Models/Cards/Oblivion.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.PAGESTORM` | `src/Core/Models/Cards/Pagestorm.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.PARSE` | `src/Core/Models/Cards/Parse.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars` |
| `CARD.POKE` | `src/Core/Models/Cards/Poke.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ShouldGlowRedInternal` |
| `CARD.PROTECTOR` | `src/Core/Models/Cards/Protector.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ShouldGlowRedInternal` |
| `CARD.PULL_AGGRO` | `src/Core/Models/Cards/PullAggro.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_GainsBlock` |
| `CARD.PULL_FROM_BELOW` | `src/Core/Models/Cards/PullFromBelow.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.PUTREFY` | `src/Core/Models/Cards/Putrefy.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.RATTLE` | `src/Core/Models/Cards/Rattle.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ShouldGlowRedInternal` |
| `CARD.REANIMATE` | `src/Core/Models/Cards/Reanimate.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.REAP` | `src/Core/Models/Cards/Reap.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars` |
| `CARD.REAPER_FORM` | `src/Core/Models/Cards/ReaperForm.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_ExtraHoverTips` |
| `CARD.REAVE` | `src/Core/Models/Cards/Reave.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.RIGHT_HAND_HAND` | `src/Core/Models/Cards/RightHandHand.cs` | `AbstractModel.AfterCardPlayedLate`, `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_ShouldGlowRedInternal` |
| `CARD.SACRIFICE` | `src/Core/Models/Cards/Sacrifice.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_GainsBlock`, `CardModel.get_ShouldGlowRedInternal` |
| `CARD.SCOURGE` | `src/Core/Models/Cards/Scourge.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.SCULPTING_STRIKE` | `src/Core/Models/Cards/SculptingStrike.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.SEANCE` | `src/Core/Models/Cards/Seance.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.SENTRY_MODE` | `src/Core/Models/Cards/SentryMode.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.SEVERANCE` | `src/Core/Models/Cards/Severance.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.SHARED_FATE` | `src/Core/Models/Cards/SharedFate.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.SHROUD` | `src/Core/Models/Cards/Shroud.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.SIC_EM` | `src/Core/Models/Cards/SicEm.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_ShouldGlowRedInternal` |
| `CARD.SLEIGHT_OF_FLESH` | `src/Core/Models/Cards/SleightOfFlesh.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars` |
| `CARD.SNAP` | `src/Core/Models/Cards/Snap.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_ShouldGlowRedInternal` |
| `CARD.SOUL_STORM` | `src/Core/Models/Cards/SoulStorm.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.SOULBOUND` | `src/Core/Models/Cards/Soulbound.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_ExtraHoverTips`, `CardModel.get_MultiplayerConstraint` |
| `CARD.SOW` | `src/Core/Models/Cards/Sow.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars` |
| `CARD.SPIRIT_OF_ASH` | `src/Core/Models/Cards/SpiritOfAsh.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.SPUR` | `src/Core/Models/Cards/Spur.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.SQUEEZE` | `src/Core/Models/Cards/Squeeze.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ShouldGlowRedInternal` |
| `CARD.STRIKE_NECROBINDER` | `src/Core/Models/Cards/StrikeNecrobinder.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars` |
| `CARD.THE_SCYTHE` | `src/Core/Models/Cards/TheScythe.cs` | `CardModel.AfterDowngraded`, `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars` |
| `CARD.TIMES_UP` | `src/Core/Models/Cards/TimesUp.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.TRANSFIGURE` | `src/Core/Models/Cards/Transfigure.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanBeGeneratedInCombat`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.UNDEATH` | `src/Core/Models/Cards/Undeath.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_GainsBlock` |
| `CARD.UNDERWORLD` | `src/Core/Models/Cards/Underworld.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_ExtraHoverTips`, `CardModel.get_MultiplayerConstraint` |
| `CARD.UNLEASH` | `src/Core/Models/Cards/Unleash.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ShouldGlowRedInternal` |
| `CARD.VEILPIERCER` | `src/Core/Models/Cards/Veilpiercer.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |
| `CARD.WISP` | `src/Core/Models/Cards/Wisp.cs` | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips` |

### All 8 relic models: every declared override

| Relic id | C# source | Overrides in source metadata |
|---|---|---|
| `RELIC.BIG_HAT` | `src/Core/Models/Relics/BigHat.cs` | `AbstractModel.AfterSideTurnStart`, `RelicModel.get_CanonicalVars`, `RelicModel.get_ExtraHoverTips`, `RelicModel.get_Rarity` |
| `RELIC.BONE_FLUTE` | `src/Core/Models/Relics/BoneFlute.cs` | `AbstractModel.AfterAttack`, `RelicModel.get_CanonicalVars`, `RelicModel.get_Rarity` |
| `RELIC.BOOK_REPAIR_KNIFE` | `src/Core/Models/Relics/BookRepairKnife.cs` | `AbstractModel.AfterDiedToDoom`, `RelicModel.get_CanonicalVars`, `RelicModel.get_ExtraHoverTips`, `RelicModel.get_Rarity` |
| `RELIC.BOOKMARK` | `src/Core/Models/Relics/Bookmark.cs` | `AbstractModel.AfterFlush`, `RelicModel.get_ExtraHoverTips`, `RelicModel.get_Rarity` |
| `RELIC.BOUND_PHYLACTERY` | `src/Core/Models/Relics/BoundPhylactery.cs` | `AbstractModel.AfterEnergyResetLate`, `AbstractModel.BeforeCombatStart`, `RelicModel.get_CanonicalVars`, `RelicModel.get_ExtraHoverTips`, `RelicModel.get_Rarity`, `RelicModel.get_SpawnsPets` |
| `RELIC.FUNERARY_MASK` | `src/Core/Models/Relics/FuneraryMask.cs` | `AbstractModel.BeforeHandDraw`, `RelicModel.get_CanonicalVars`, `RelicModel.get_ExtraHoverTips`, `RelicModel.get_Rarity` |
| `RELIC.IVORY_TILE` | `src/Core/Models/Relics/IvoryTile.cs` | `AbstractModel.AfterCardPlayed`, `RelicModel.get_CanonicalVars`, `RelicModel.get_ExtraHoverTips`, `RelicModel.get_Rarity` |
| `RELIC.UNDYING_SIGIL` | `src/Core/Models/Relics/UndyingSigil.cs` | `AbstractModel.ModifyDamageMultiplicative`, `RelicModel.get_CanonicalVars`, `RelicModel.get_ExtraHoverTips`, `RelicModel.get_Rarity` |

### Power and supporting model dependencies

| Model | C# source | Referenced by | Override metadata |
|---|---|---|---|
| `BorrowedTimePower` (`POWER.BORROWED_TIME_POWER`) | `src/Core/Models/Powers/BorrowedTimePower.cs` | `CARD.BORROWED_TIME` | `AbstractModel.AfterSideTurnEnd`, `AbstractModel.TryModifyEnergyCostInCombat`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `CacophonyPower` (`POWER.CACOPHONY_POWER`) | `src/Core/Models/Powers/CacophonyPower.cs` | `CARD.CACOPHONY` | `AbstractModel.AfterCardDrawn`, `PowerModel.get_CanonicalVars`, `PowerModel.get_DisplayAmount`, `PowerModel.get_InstanceType`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `CalcifyPower` (`POWER.CALCIFY_POWER`) | `src/Core/Models/Powers/CalcifyPower.cs` | `CARD.CALCIFY` | `AbstractModel.ModifyDamageAdditive`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `CallOfTheVoidPower` (`POWER.CALL_OF_THE_VOID_POWER`) | `src/Core/Models/Powers/CallOfTheVoidPower.cs` | `CARD.CALL_OF_THE_VOID` | `AbstractModel.BeforeHandDraw`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `CountdownPower` (`POWER.COUNTDOWN_POWER`) | `src/Core/Models/Powers/CountdownPower.cs` | `CARD.COUNTDOWN` | `AbstractModel.AfterSideTurnStart`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `DanseMacabrePower` (`POWER.DANSE_MACABRE_POWER`) | `src/Core/Models/Powers/DanseMacabrePower.cs` | `CARD.DANSE_MACABRE` | `AbstractModel.BeforeCardPlayed`, `PowerModel.get_CanonicalVars`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `DebilitatePower` (`POWER.DEBILITATE_POWER`) | `src/Core/Models/Powers/DebilitatePower.cs` | `CARD.DEBILITATE` | `AbstractModel.AfterSideTurnEnd`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `DemesnePower` (`POWER.DEMESNE_POWER`) | `src/Core/Models/Powers/DemesnePower.cs` | `CARD.DEMESNE` | `AbstractModel.ModifyHandDraw`, `AbstractModel.ModifyMaxEnergy`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `DevourLifePower` (`POWER.DEVOUR_LIFE_POWER`) | `src/Core/Models/Powers/DevourLifePower.cs` | `CARD.DEVOUR_LIFE` | `AbstractModel.AfterCardPlayed`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `DoomPower` (`POWER.DOOM_POWER`) | `src/Core/Models/Powers/DoomPower.cs` | `CARD.BLIGHT_STRIKE`, `CARD.DEATHBRINGER`, `CARD.END_OF_DAYS`, `CARD.NEGATIVE_PULSE`, `CARD.NO_ESCAPE`, `CARD.SCOURGE` | `AbstractModel.AfterSideTurnEnd`, `AbstractModel.BeforeSideTurnEnd`, `PowerModel.get_AmountLabelColor`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `EnergyNextTurnPower` (`POWER.ENERGY_NEXT_TURN_POWER`) | `src/Core/Models/Powers/EnergyNextTurnPower.cs` | `CARD.DELAY`, `CARD.INVOKE` | `AbstractModel.AfterEnergyReset`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `EnfeeblingTouchPower` (`POWER.ENFEEBLING_TOUCH_POWER`) | `src/Core/Models/Powers/EnfeeblingTouchPower.cs` | `CARD.ENFEEBLING_TOUCH` | `AbstractModel.AfterPowerAmountChanged (via TemporaryStrengthPower)`, `AbstractModel.AfterSideTurnEnd (via TemporaryStrengthPower)`, `PowerModel.BeforeApplied (via TemporaryStrengthPower)`, `PowerModel.get_Description (via TemporaryStrengthPower)`, `PowerModel.get_ExtraHoverTips (via TemporaryStrengthPower)`, `PowerModel.get_SmartDescriptionLocKey (via TemporaryStrengthPower)`, `PowerModel.get_StackType (via TemporaryStrengthPower)`, `PowerModel.get_Title (via TemporaryStrengthPower)`, `PowerModel.get_Type (via TemporaryStrengthPower)`, `TemporaryStrengthPower.get_IsPositive`, `TemporaryStrengthPower.get_OriginModel` |
| `ForbiddenGrimoirePower` (`POWER.FORBIDDEN_GRIMOIRE_POWER`) | `src/Core/Models/Powers/ForbiddenGrimoirePower.cs` | `CARD.FORBIDDEN_GRIMOIRE` | `AbstractModel.AfterCombatEnd`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `FriendshipPower` (`POWER.FRIENDSHIP_POWER`) | `src/Core/Models/Powers/FriendshipPower.cs` | `CARD.FRIENDSHIP` | `AbstractModel.ModifyMaxEnergy`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `HangPower` (`POWER.HANG_POWER`) | `src/Core/Models/Powers/HangPower.cs` | `CARD.HANG` | `AbstractModel.ModifyDamageMultiplicative`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `HauntPower` (`POWER.HAUNT_POWER`) | `src/Core/Models/Powers/HauntPower.cs` | `CARD.HAUNT` | `AbstractModel.AfterCardPlayed`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `LethalityPower` (`POWER.LETHALITY_POWER`) | `src/Core/Models/Powers/LethalityPower.cs` | `CARD.LETHALITY` | `AbstractModel.ModifyDamageMultiplicative`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `NecroMasteryPower` (`POWER.NECRO_MASTERY_POWER`) | `src/Core/Models/Powers/NecroMasteryPower.cs` | `CARD.NECRO_MASTERY` | `AbstractModel.AfterCurrentHpChanged`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `NeurosurgePower` (`POWER.NEUROSURGE_POWER`) | `src/Core/Models/Powers/NeurosurgePower.cs` | `CARD.NEUROSURGE` | `AbstractModel.AfterSideTurnStart`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `OblivionPower` (`POWER.OBLIVION_POWER`) | `src/Core/Models/Powers/OblivionPower.cs` | `CARD.OBLIVION` | `AbstractModel.AfterCardPlayed`, `AbstractModel.AfterSideTurnEnd`, `AbstractModel.BeforeCardPlayed`, `PowerModel.InitInternalData`, `PowerModel.get_InstanceType`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `PagestormPower` (`POWER.PAGESTORM_POWER`) | `src/Core/Models/Powers/PagestormPower.cs` | `CARD.PAGESTORM` | `AbstractModel.AfterCardDrawn`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `ReaperFormPower` (`POWER.REAPER_FORM_POWER`) | `src/Core/Models/Powers/ReaperFormPower.cs` | `CARD.REAPER_FORM` | `AbstractModel.AfterDamageGiven`, `PowerModel.AfterApplied`, `PowerModel.AfterRemoved`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `SentryModePower` (`POWER.SENTRY_MODE_POWER`) | `src/Core/Models/Powers/SentryModePower.cs` | `CARD.SENTRY_MODE` | `AbstractModel.BeforeHandDraw`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `ShroudPower` (`POWER.SHROUD_POWER`) | `src/Core/Models/Powers/ShroudPower.cs` | `CARD.SHROUD` | `AbstractModel.AfterPowerAmountChanged`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `SicEmPower` (`POWER.SIC_EM_POWER`) | `src/Core/Models/Powers/SicEmPower.cs` | `CARD.SIC_EM` | `AbstractModel.AfterDamageGiven`, `AbstractModel.AfterSideTurnEnd`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `SleightOfFleshPower` (`POWER.SLEIGHT_OF_FLESH_POWER`) | `src/Core/Models/Powers/SleightOfFleshPower.cs` | `CARD.SLEIGHT_OF_FLESH` | `AbstractModel.AfterPowerAmountChanged`, `PowerModel.get_CanonicalVars`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `SoulboundPower` (`POWER.SOULBOUND_POWER`) | `src/Core/Models/Powers/SoulboundPower.cs` | `CARD.SOULBOUND` | `AbstractModel.AfterCardGeneratedForCombat`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `SpiritOfAshPower` (`POWER.SPIRIT_OF_ASH_POWER`) | `src/Core/Models/Powers/SpiritOfAshPower.cs` | `CARD.SPIRIT_OF_ASH` | `AbstractModel.BeforeCardPlayed`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `StrengthPower` (`POWER.STRENGTH_POWER`) | `src/Core/Models/Powers/StrengthPower.cs` | `CARD.FRIENDSHIP`, `CARD.SHARED_FATE`, `CARD.SHARED_FATE` | `AbstractModel.ModifyDamageAdditive`, `PowerModel.get_AllowNegative`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `SummonNextTurnPower` (`POWER.SUMMON_NEXT_TURN_POWER`) | `src/Core/Models/Powers/SummonNextTurnPower.cs` | `CARD.INVOKE` | `AbstractModel.AfterPlayerTurnStart`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `UnderworldPower` (`POWER.UNDERWORLD_POWER`) | `src/Core/Models/Powers/UnderworldPower.cs` | `CARD.UNDERWORLD` | `AbstractModel.AfterDamageGiven`, `AbstractModel.AfterSideTurnEnd`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `VeilpiercerPower` (`POWER.VEILPIERCER_POWER`) | `src/Core/Models/Powers/VeilpiercerPower.cs` | `CARD.VEILPIERCER` | `AbstractModel.BeforeCardPlayed`, `AbstractModel.TryModifyEnergyCostInCombatLate`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `VulnerablePower` (`POWER.VULNERABLE_POWER`) | `src/Core/Models/Powers/VulnerablePower.cs` | `CARD.FEAR`, `CARD.HIGH_FIVE`, `CARD.PUTREFY` | `AbstractModel.AfterSideTurnEnd`, `AbstractModel.ModifyDamageMultiplicative`, `PowerModel.get_CanonicalVars`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `WeakPower` (`POWER.WEAK_POWER`) | `src/Core/Models/Powers/WeakPower.cs` | `CARD.DEATHBRINGER`, `CARD.DEFY`, `CARD.PUTREFY` | `AbstractModel.AfterSideTurnEnd`, `AbstractModel.ModifyDamageMultiplicative`, `PowerModel.get_CanonicalVars`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `MONSTER.OSTY` | `src/Core/Models/Monsters/Osty.cs` | supporting entity | `MonsterModel.GenerateAnimator`, `MonsterModel.GenerateMoveStateMachine`, `MonsterModel.get_DeathSfx`, `MonsterModel.get_HasDeathSfx`, `MonsterModel.get_IsHealthBarVisible`, `MonsterModel.get_MaxInitialHp`, `MonsterModel.get_MinInitialHp` |
| `CARD.SOUL` | `src/Core/Models/Cards/Soul.cs` | generated draw/token card | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars` |
| `CARD.SWEEPING_GAZE` | `src/Core/Models/Cards/SweepingGaze.cs` | generated by Sentry Mode | `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ShouldGlowRedInternal` |
| `POWER.DIE_FOR_YOU_POWER` | `src/Core/Models/Powers/DieForYouPower.cs` | automatically applied to newly summoned Osty | `AbstractModel.ModifyUnblockedDamageTarget`, `AbstractModel.ShouldAllowHitting`, `AbstractModel.ShouldCreatureBeRemovedFromCombatAfterDeath`, `PowerModel.ShouldPowerBeRemovedAfterOwnerDeath`, `PowerModel.get_ShouldPlayVfx`, `PowerModel.get_StackType`, `PowerModel.get_Type` |
| `ENCHANTMENT.SOULS_POWER` | `src/Core/Models/Enchantments/SoulsPower.cs` | supporting entity | `EnchantmentModel.CanEnchant`, `EnchantmentModel.OnEnchant`, `EnchantmentModel.get_ExtraHoverTips` |

## Control-flow, generated-candidate, and RNG-order scan

For every listed C# source, conditional/loop sites (`if`/`else if`/`switch`/`case`/`foreach`/`for`/`while`/ternary) are recorded in file order. Candidate creation, selection, pile-generation, power application, and Osty calls are listed in file order. RNG-related statements are separately kept in lexical call order; no result below establishes correctness without the full method body. These line references make the source review repeatable and identify the exact branches/loops that must be represented in the C++ models.

### `CHARACTER.NECROBINDER` — `Models/Characters/Necrobinder.cs`
Overrides: `CharacterModel.GetArchitectAttackVfx`, `CharacterModel.get_AnimationStates`, `CharacterModel.get_AttackAnimDelay`, `CharacterModel.get_CardPool`, `CharacterModel.get_CastAnimDelay`, `CharacterModel.get_CharacterTransitionSfx`, `CharacterModel.get_DialogueColor`, `CharacterModel.get_EnergyLabelOutlineColor`, `CharacterModel.get_ExtraAssetPaths`, `CharacterModel.get_Gender`, `CharacterModel.get_MapDrawingColor`, `CharacterModel.get_NameColor`, `CharacterModel.get_PotionPool`, `CharacterModel.get_RelicPool`, `CharacterModel.get_RemoteTargetingLineColor`, `CharacterModel.get_RemoteTargetingLineOutline`, `CharacterModel.get_StartingDeck`, `CharacterModel.get_StartingGold`, `CharacterModel.get_StartingHp`, `CharacterModel.get_StartingRelics`, `CharacterModel.get_UnlocksAfterRunAs`
Conditional/loop sites:
  - L108 `CharacterTransitionSfx`: `if (!(character is Necrobinder))`
  - L117 `CharacterTransitionSfx`: `if (!(character is Necrobinder))`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD_POOL.NECROBINDER_CARD_POOL` — `Models/CardPools/NecrobinderCardPool.cs`
Overrides: `CardPoolModel.FilterThroughEpochs`, `CardPoolModel.GenerateAllCards`, `CardPoolModel.get_CardFrameMaterialPath`, `CardPoolModel.get_DeckEntryCardColor`, `CardPoolModel.get_EnergyColorName`, `CardPoolModel.get_EnergyOutlineColor`, `CardPoolModel.get_IsColorless`, `CardPoolModel.get_Title`
Conditional/loop sites:
  - L125 `IsColorless`: `if (!unlockState.IsEpochRevealed<Necrobinder2Epoch>())`
  - L129 `IsColorless`: `if (!unlockState.IsEpochRevealed<Necrobinder5Epoch>())`
  - L133 `IsColorless`: `if (!unlockState.IsEpochRevealed<Necrobinder7Epoch>())`
Generation, choice, pile, and cross-model call sites:
  - L24: `protected override CardModel[] GenerateAllCards()`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `RELIC_POOL.NECROBINDER_RELIC_POOL` — `Models/RelicPools/NecrobinderRelicPool.cs`
Overrides: `RelicPoolModel.GenerateAllRelics`, `RelicPoolModel.GetUnlockedRelics`, `RelicPoolModel.get_EnergyColorName`, `RelicPoolModel.get_LabOutlineColor`
Conditional/loop sites:
  - L35 `LabOutlineColor`: `if (!unlockState.IsEpochRevealed<Necrobinder3Epoch>())`
  - L39 `LabOutlineColor`: `if (!unlockState.IsEpochRevealed<Necrobinder6Epoch>())`
Generation, choice, pile, and cross-model call sites:
  - L17: `protected override IEnumerable<RelicModel> GenerateAllRelics()`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POTION_POOL.NECROBINDER_POTION_POOL` — `Models/PotionPools/NecrobinderPotionPool.cs`
Overrides: `PotionPoolModel.GenerateAllPotions`, `PotionPoolModel.GetUnlockedPotions`, `PotionPoolModel.get_EnergyColorName`, `PotionPoolModel.get_LabOutlineColor`
Conditional/loop sites:
  - L26 `LabOutlineColor`: `if (!unlockState.IsEpochRevealed<Necrobinder4Epoch>())`
Generation, choice, pile, and cross-model call sites:
  - L16: `protected override IEnumerable<PotionModel> GenerateAllPotions()`
  - L30: `return GenerateAllPotions();`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.AFTERLIFE` — `Models/Cards/Afterlife.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L28: `await OstyCmd.Summon(choiceContext, base.Owner, base.DynamicVars.Summon.BaseValue, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.BANSHEES_CRY` — `Models/Cards/BansheesCry.cs`
Overrides: `AbstractModel.AfterCardEnteredCombat`, `AbstractModel.AfterCardPlayed`, `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - L44 `CanonicalVars`: `if (card != this)`
  - L48 `CanonicalVars`: `if (base.IsClone)`
  - L59 `CanonicalVars`: `if (cardPlay.Card.Owner != base.Owner)`
  - L63 `CanonicalVars`: `if (!cardPlay.Card.Keywords.Contains(CardKeyword.Ethereal))`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.BLIGHT_STRIKE` — `Models/Cards/BlightStrike.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L36: `await PowerCmd.Apply<DoomPower>(choiceContext, cardPlay.Target, attackCommand.Results.SelectMany((List<DamageResult> r) => r).Sum((DamageResult r) => r.TotalDamage), base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.BODYGUARD` — `Models/Cards/Bodyguard.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L26: `await OstyCmd.Summon(choiceContext, base.Owner, base.DynamicVars.Summon.BaseValue, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.BONE_SHARDS` — `Models/Cards/BoneShards.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_GainsBlock`, `CardModel.get_ShouldGlowRedInternal`
Conditional/loop sites:
  - L33 `CanonicalVars`: `if (!Osty.CheckMissingWithAnim(base.Owner))`
  - L39 `CanonicalVars`: `if (base.Owner.IsOstyAlive)`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.BORROWED_TIME` — `Models/Cards/BorrowedTime.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L33: `await PowerCmd.Apply<BorrowedTimePower>(choiceContext, base.Owner.Creature, base.DynamicVars["ExtraCost"].BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.BURY` — `Models/Cards/Bury.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.CACOPHONY` — `Models/Cards/Cacophony.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_MultiplayerConstraint`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L31: `await PowerCmd.Apply<CacophonyPower>(choiceContext, base.Owner.Creature, base.DynamicVars.Damage.IntValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.CALCIFY` — `Models/Cards/Calcify.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L23: `await PowerCmd.Apply<CalcifyPower>(choiceContext, base.Owner.Creature, base.DynamicVars["CalcifyPower"].BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.CALL_OF_THE_VOID` — `Models/Cards/CallOfTheVoid.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L26: `await PowerCmd.Apply<CallOfTheVoidPower>(choiceContext, base.Owner.Creature, base.DynamicVars.Cards.BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.CAPTURE_SPIRIT` — `Models/Cards/CaptureSpirit.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L35: `CardCmd.PreviewCardPileAdd(await CardPileCmd.AddGeneratedCardsToCombat(cards, PileType.Draw, base.Owner, CardPilePosition.Random));`
RNG-related call sites in source order:
  - L35: `CardCmd.PreviewCardPileAdd(await CardPileCmd.AddGeneratedCardsToCombat(cards, PileType.Draw, base.Owner, CardPilePosition.Random));`

### `CARD.CLEANSE` — `Models/Cards/Cleanse.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - L34 `ExtraHoverTips`: `if (cardModel != null)`
Generation, choice, pile, and cross-model call sites:
  - L32: `await OstyCmd.Summon(choiceContext, base.Owner, base.DynamicVars.Summon.BaseValue, this);`
  - L33: `CardModel cardModel = (await CardSelectCmd.FromCombatPile(choiceContext, PileType.Draw.GetPile(base.Owner), base.Owner, new CardSelectorPrefs(CardSelectorPrefs.ExhaustSelectionPrompt, 1))).FirstOrDefault();`
  - L36: `await CardCmd.Exhaust(choiceContext, cardModel);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.COUNTDOWN` — `Models/Cards/Countdown.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L26: `await PowerCmd.Apply<CountdownPower>(choiceContext, base.Owner.Creature, base.DynamicVars["CountdownPower"].BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.DANSE_MACABRE` — `Models/Cards/DanseMacabre.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L30: `await PowerCmd.Apply<DanseMacabrePower>(choiceContext, base.Owner.Creature, base.DynamicVars["DanseMacabrePower"].BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.DEATH_MARCH` — `Models/Cards/DeathMarch.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.DEATHBRINGER` — `Models/Cards/Deathbringer.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L34: `await PowerCmd.Apply<DoomPower>(choiceContext, base.CombatState?.HittableEnemies, base.DynamicVars.Doom.BaseValue, base.Owner.Creature, this);`
  - L35: `await PowerCmd.Apply<WeakPower>(choiceContext, base.CombatState?.HittableEnemies, base.DynamicVars.Weak.BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.DEATHS_DOOR` — `Models/Cards/DeathsDoor.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_GainsBlock`, `CardModel.get_ShouldGlowGoldInternal`
Conditional/loop sites:
  - L41 `WasDoomAppliedThisTurn`: `if (WasDoomAppliedThisTurn)`
  - L45 `WasDoomAppliedThisTurn`: `for (int i = 0; i < blockGains; i++)`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.DEBILITATE` — `Models/Cards/Debilitate.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L39: `await PowerCmd.Apply<DebilitatePower>(choiceContext, cardPlay.Target, base.DynamicVars["DebilitatePower"].BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.DEFEND_NECROBINDER` — `Models/Cards/DefendNecrobinder.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_GainsBlock`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.DEFILE` — `Models/Cards/Defile.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.DEFY` — `Models/Cards/Defy.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_GainsBlock`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L38: `await PowerCmd.Apply<WeakPower>(choiceContext, cardPlay.Target, base.DynamicVars.Weak.BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.DELAY` — `Models/Cards/Delay.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_GainsBlock`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L34: `await PowerCmd.Apply<EnergyNextTurnPower>(choiceContext, base.Owner.Creature, base.DynamicVars.Energy.IntValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.DEMESNE` — `Models/Cards/Demesne.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L32: `await PowerCmd.Apply<DemesnePower>(choiceContext, base.Owner.Creature, base.DynamicVars.Cards.BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.DEVOUR_LIFE` — `Models/Cards/DevourLife.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L30: `await PowerCmd.Apply<DevourLifePower>(choiceContext, base.Owner.Creature, base.DynamicVars["DevourLifePower"].BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.DIRGE` — `Models/Cards/Dirge.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_HasEnergyCostX`
Conditional/loop sites:
  - L35 `ExtraHoverTips`: `for (int i = 0; i < xValue; i++)`
  - L40 `ExtraHoverTips`: `if (base.IsUpgraded)`
  - L42 `ExtraHoverTips`: `foreach (Soul item in list)`
Generation, choice, pile, and cross-model call sites:
  - L37: `await OstyCmd.Summon(choiceContext, base.Owner, base.DynamicVars.Summon.BaseValue, this);`
  - L44: `CardCmd.Upgrade(item);`
  - L47: `CardCmd.PreviewCardPileAdd(await CardPileCmd.AddGeneratedCardsToCombat(list, PileType.Draw, base.Owner, CardPilePosition.Random));`
RNG-related call sites in source order:
  - L47: `CardCmd.PreviewCardPileAdd(await CardPileCmd.AddGeneratedCardsToCombat(list, PileType.Draw, base.Owner, CardPilePosition.Random));`

### `CARD.DRAIN_POWER` — `Models/Cards/DrainPower.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`
Conditional/loop sites:
  - L34 `CanonicalVars`: `foreach (CardModel item in enumerable)`
Generation, choice, pile, and cross-model call sites:
  - L36: `CardCmd.Upgrade(item);`
  - L37: `CardCmd.Preview(item);`
RNG-related call sites in source order:
  - L33: `IEnumerable<CardModel> enumerable = PileType.Discard.GetPile(base.Owner).Cards.Where((CardModel c) => c.IsUpgradable).TakeRandom(base.DynamicVars.Cards.IntValue, base.Owner.RunState.Rng.CombatCardSelection);`

### `CARD.DREDGE` — `Models/Cards/Dredge.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`
Conditional/loop sites:
  - L27 `CanonicalKeywords`: `if (num > 0)`
Generation, choice, pile, and cross-model call sites:
  - L29: `await CardPileCmd.Add(await CardSelectCmd.FromCombatPile(choiceContext, PileType.Discard.GetPile(base.Owner), base.Owner, new CardSelectorPrefs(base.SelectionScreenPrompt, num)), PileType.Hand);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.EIDOLON` — `Models/Cards/Eidolon.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanBeGeneratedInCombat`, `CardModel.get_CanonicalKeywords`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - L28 `ExtraHoverTips`: `foreach (CardModel item in list)`
Generation, choice, pile, and cross-model call sites:
  - L13: `public override bool CanBeGeneratedInCombat => false;`
  - L30: `await CardCmd.AutoPlay(choiceContext, item, null);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.END_OF_DAYS` — `Models/Cards/EndOfDays.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - L35 `ExtraHoverTips`: `if (sideCenterFloor.HasValue)`
  - L38 `ExtraHoverTips`: `if (nLargeMagicMissileVfx != null)`
  - L44 `ExtraHoverTips`: `foreach (Creature hittableEnemy in base.CombatState.HittableEnemies)`
Generation, choice, pile, and cross-model call sites:
  - L46: `await PowerCmd.Apply<DoomPower>(choiceContext, hittableEnemy, base.DynamicVars.Doom.BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.ENFEEBLING_TOUCH` — `Models/Cards/EnfeeblingTouch.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L32: `await PowerCmd.Apply<EnfeeblingTouchPower>(choiceContext, cardPlay.Target, base.DynamicVars["StrengthLoss"].BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.ERADICATE` — `Models/Cards/Eradicate.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_HasEnergyCostX`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.FEAR` — `Models/Cards/Fear.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L38: `await PowerCmd.Apply<VulnerablePower>(choiceContext, cardPlay.Target, base.DynamicVars.Vulnerable.BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.FETCH` — `Models/Cards/Fetch.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ShouldGlowGoldInternal`, `CardModel.get_ShouldGlowRedInternal`
Conditional/loop sites:
  - L40 `HasBeenPlayedThisTurn`: `if (!Osty.CheckMissingWithAnim(base.Owner))`
  - L45 `HasBeenPlayedThisTurn`: `if (!HasBeenPlayedThisTurn)`
Generation, choice, pile, and cross-model call sites:
  - L47: `await CardPileCmd.Draw(choiceContext, base.DynamicVars.Cards.BaseValue, base.Owner);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.FLATTEN` — `Models/Cards/Flatten.cs`
Overrides: `AbstractModel.AfterAttack`, `AbstractModel.AfterCardEnteredCombat`, `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ShouldGlowGoldInternal`, `CardModel.get_ShouldGlowRedInternal`
Conditional/loop sites:
  - L37 `HasOstyAttackedThisTurn`: `if (!Osty.CheckMissingWithAnim(base.Owner))`
  - L52 `HasOstyAttackedThisTurn`: `if (card != this)`
  - L56 `HasOstyAttackedThisTurn`: `if (!HasOstyAttackedThisTurn)`
  - L66 `HasOstyAttackedThisTurn`: `if (command.Attacker == null)`
  - L70 `HasOstyAttackedThisTurn`: `if (command.Attacker != base.Owner.Osty)`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.FORBIDDEN_GRIMOIRE` — `Models/Cards/ForbiddenGrimoire.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L22: `await PowerCmd.Apply<ForbiddenGrimoirePower>(choiceContext, base.Owner.Creature, 1m, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.FRIENDSHIP` — `Models/Cards/Friendship.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L30: `await PowerCmd.Apply<StrengthPower>(choiceContext, base.Owner.Creature, -base.DynamicVars["StrengthPower"].BaseValue, base.Owner.Creature, this);`
  - L31: `await PowerCmd.Apply<FriendshipPower>(choiceContext, base.Owner.Creature, base.DynamicVars.Energy.BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.GLIMPSE_BEYOND` — `Models/Cards/GlimpseBeyond.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_MultiplayerConstraint`
Conditional/loop sites:
  - L35 `CanonicalKeywords`: `foreach (Creature creature in enumerable)`
  - L39 `CanonicalKeywords`: `if (LocalContext.IsMe(creature))`
Generation, choice, pile, and cross-model call sites:
  - L38: `IReadOnlyList<CardPileAddResult> results = await CardPileCmd.AddGeneratedCardsToCombat(cards, PileType.Draw, base.Owner, CardPilePosition.Random);`
  - L41: `CardCmd.PreviewCardPileAdd(results);`
RNG-related call sites in source order:
  - L38: `IReadOnlyList<CardPileAddResult> results = await CardPileCmd.AddGeneratedCardsToCombat(cards, PileType.Draw, base.Owner, CardPilePosition.Random);`

### `CARD.GRAVE_WARDEN` — `Models/Cards/GraveWarden.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_GainsBlock`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L33: `CardCmd.PreviewCardPileAdd(await CardPileCmd.AddGeneratedCardsToCombat(Soul.Create(base.Owner, base.DynamicVars.Cards.IntValue, base.CombatState), PileType.Draw, base.Owner, CardPilePosition.Random));`
RNG-related call sites in source order:
  - L33: `CardCmd.PreviewCardPileAdd(await CardPileCmd.AddGeneratedCardsToCombat(Soul.Create(base.Owner, base.DynamicVars.Cards.IntValue, base.CombatState), PileType.Draw, base.Owner, CardPilePosition.Random));`

### `CARD.GRAVEBLAST` — `Models/Cards/Graveblast.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`
Conditional/loop sites:
  - L32 `CanonicalVars`: `if (cardModel != null)`
Generation, choice, pile, and cross-model call sites:
  - L31: `CardModel cardModel = (await CardSelectCmd.FromCombatPile(prefs: new CardSelectorPrefs(base.SelectionScreenPrompt, 1), context: choiceContext, pile: PileType.Discard.GetPile(base.Owner), player: base.Owner)).FirstOrDefault();`
  - L34: `await CardPileCmd.Add(cardModel, PileType.Hand);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.HANG` — `Models/Cards/Hang.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`
Conditional/loop sites:
  - L30 `CanonicalVars`: `if (powerAmount + num > 999999999)`
Generation, choice, pile, and cross-model call sites:
  - L34: `await PowerCmd.Apply<HangPower>(choiceContext, cardPlay.Target, num, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.HAUNT` — `Models/Cards/Haunt.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L26: `await PowerCmd.Apply<HauntPower>(choiceContext, base.Owner.Creature, base.DynamicVars.HpLoss.BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.HIGH_FIVE` — `Models/Cards/HighFive.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_IsPlayable`, `CardModel.get_ShouldGlowRedInternal`
Conditional/loop sites:
  - L37 `ExtraHoverTips`: `if (!Osty.CheckMissingWithAnim(base.Owner))`
Generation, choice, pile, and cross-model call sites:
  - L42: `await PowerCmd.Apply<VulnerablePower>(choiceContext, base.CombatState?.HittableEnemies, base.DynamicVars.Vulnerable.BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.INVOKE` — `Models/Cards/Invoke.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L35: `await PowerCmd.Apply<SummonNextTurnPower>(choiceContext, base.Owner.Creature, base.DynamicVars.Summon.IntValue, base.Owner.Creature, this);`
  - L36: `await PowerCmd.Apply<EnergyNextTurnPower>(choiceContext, base.Owner.Creature, base.DynamicVars.Energy.IntValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.LEGION_OF_BONE` — `Models/Cards/LegionOfBone.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_MultiplayerConstraint`
Conditional/loop sites:
  - L31 `MultiplayerConstraint`: `foreach (Creature item in enumerable)`
Generation, choice, pile, and cross-model call sites:
  - L33: `await OstyCmd.Summon(choiceContext, item.Player, base.DynamicVars.Summon.BaseValue, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.LETHALITY` — `Models/Cards/Lethality.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L25: `await PowerCmd.Apply<LethalityPower>(choiceContext, base.Owner.Creature, base.DynamicVars["LethalityPower"].BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.MELANCHOLY` — `Models/Cards/Melancholy.cs`
Overrides: `AbstractModel.AfterDeath`, `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_GainsBlock`
Conditional/loop sites:
  - L37 `ExtraHoverTips`: `if (wasRemovalPrevented)`
  - L42 `ExtraHoverTips`: `if (pile == null || !pile.IsCombatPile)`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.MISERY` — `Models/Cards/Misery.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`
Conditional/loop sites:
  - L30 `CanonicalVars`: `foreach (KeyValuePair<PowerModel, int> item in debuffAmounts)`
  - L34 `CanonicalVars`: `if (temporaryPower != null)`
  - L37 `CanonicalVars`: `if (keyValuePair.Key != null)`
  - L46 `CanonicalVars`: `foreach (Creature enemy in base.CombatState.HittableEnemies)`
  - L48 `CanonicalVars`: `if (enemy == cardPlay.Target)`
  - L52 `CanonicalVars`: `foreach (KeyValuePair<PowerModel, int> item2 in debuffAmounts)`
  - L54 `CanonicalVars`: `if (item2.Value != 0)`
  - L57 `CanonicalVars`: `if (powerModel != null)`
Generation, choice, pile, and cross-model call sites:
  - L56: `PowerModel powerModel = PowerCmd.FindExistingInstanceForStacking(item2.Key, enemy, item2.Key.Applier);`
  - L59: `await PowerCmd.ModifyAmount(choiceContext, powerModel, item2.Value, item2.Key.Applier, this);`
  - L63: `await PowerCmd.Apply(choiceContext, power, enemy, item2.Value, item2.Key.Applier, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.NECRO_MASTERY` — `Models/Cards/NecroMastery.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L26: `await OstyCmd.Summon(choiceContext, base.Owner, base.DynamicVars.Summon.BaseValue, this);`
  - L27: `await PowerCmd.Apply<NecroMasteryPower>(choiceContext, base.Owner.Creature, 1m, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.NEGATIVE_PULSE` — `Models/Cards/NegativePulse.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_GainsBlock`
Conditional/loop sites:
  - L35 `ExtraHoverTips`: `foreach (Creature hittableEnemy in base.CombatState.HittableEnemies)`
Generation, choice, pile, and cross-model call sites:
  - L37: `await PowerCmd.Apply<DoomPower>(choiceContext, hittableEnemy, base.DynamicVars.Doom.BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.NEUROSURGE` — `Models/Cards/Neurosurge.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L36: `await CardPileCmd.Draw(choiceContext, base.DynamicVars.Cards.BaseValue, base.Owner);`
  - L37: `await PowerCmd.Apply<NeurosurgePower>(choiceContext, base.Owner.Creature, base.DynamicVars["NeurosurgePower"].IntValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.NO_ESCAPE` — `Models/Cards/NoEscape.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L44: `await PowerCmd.Apply<DoomPower>(choiceContext, cardPlay.Target, ((CalculatedVar)base.DynamicVars["CalculatedDoom"]).Calculate(cardPlay.Target), base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.OBLIVION` — `Models/Cards/Oblivion.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L28: `await PowerCmd.Apply<OblivionPower>(choiceContext, cardPlay.Target, base.DynamicVars.Doom.BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.PAGESTORM` — `Models/Cards/Pagestorm.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L26: `await PowerCmd.Apply<PagestormPower>(choiceContext, base.Owner.Creature, base.DynamicVars.Cards.BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.PARSE` — `Models/Cards/Parse.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L23: `await CardPileCmd.Draw(choiceContext, base.DynamicVars.Cards.IntValue, base.Owner);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.POKE` — `Models/Cards/Poke.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ShouldGlowRedInternal`
Conditional/loop sites:
  - L29 `CanonicalVars`: `if (!Osty.CheckMissingWithAnim(base.Owner))`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.PROTECTOR` — `Models/Cards/Protector.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ShouldGlowRedInternal`
Conditional/loop sites:
  - L27 `CanonicalVars`: `return (osty != null && osty.IsAlive) ? osty.MaxHp : 0;`
  - L39 `CanonicalVars`: `if (!Osty.CheckMissingWithAnim(base.Owner))`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.PULL_AGGRO` — `Models/Cards/PullAggro.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_GainsBlock`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L33: `await OstyCmd.Summon(choiceContext, base.Owner, base.DynamicVars.Summon.BaseValue, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.PULL_FROM_BELOW` — `Models/Cards/PullFromBelow.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.PUTREFY` — `Models/Cards/Putrefy.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L37: `await PowerCmd.Apply<WeakPower>(choiceContext, cardPlay.Target, amount, base.Owner.Creature, this);`
  - L38: `await PowerCmd.Apply<VulnerablePower>(choiceContext, cardPlay.Target, amount, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.RATTLE` — `Models/Cards/Rattle.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ShouldGlowRedInternal`
Conditional/loop sites:
  - L41 `CanonicalVars`: `if (!Osty.CheckMissingWithAnim(base.Owner))`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.REANIMATE` — `Models/Cards/Reanimate.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L28: `await OstyCmd.Summon(choiceContext, base.Owner, base.DynamicVars.Summon.BaseValue, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.REAP` — `Models/Cards/Reap.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.REAPER_FORM` — `Models/Cards/ReaperForm.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L23: `await PowerCmd.Apply<ReaperFormPower>(choiceContext, base.Owner.Creature, 1m, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.REAVE` — `Models/Cards/Reave.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - L35 `ExtraHoverTips`: `if (base.IsUpgraded)`
  - L37 `ExtraHoverTips`: `foreach (Soul item in enumerable)`
Generation, choice, pile, and cross-model call sites:
  - L39: `CardCmd.Upgrade(item);`
  - L42: `CardCmd.PreviewCardPileAdd(await CardPileCmd.AddGeneratedCardsToCombat(enumerable, PileType.Draw, base.Owner, CardPilePosition.Random));`
RNG-related call sites in source order:
  - L42: `CardCmd.PreviewCardPileAdd(await CardPileCmd.AddGeneratedCardsToCombat(enumerable, PileType.Draw, base.Owner, CardPilePosition.Random));`

### `CARD.RIGHT_HAND_HAND` — `Models/Cards/RightHandHand.cs`
Overrides: `AbstractModel.AfterCardPlayedLate`, `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_ShouldGlowRedInternal`
Conditional/loop sites:
  - L36 `CanonicalVars`: `if (!Osty.CheckMissingWithAnim(base.Owner))`
  - L46 `CanonicalVars`: `if (cardPlay.Card.Owner == base.Owner && cardPlay.Resources.EnergyValue >= base.DynamicVars.Energy.IntValue)`
  - L49 `CanonicalVars`: `if (pile != null && pile.Type == PileType.Discard)`
Generation, choice, pile, and cross-model call sites:
  - L51: `await CardPileCmd.Add(this, PileType.Hand);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.SACRIFICE` — `Models/Cards/Sacrifice.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_GainsBlock`, `CardModel.get_ShouldGlowRedInternal`
Conditional/loop sites:
  - L25 `CanonicalVars`: `new CalculatedBlockVar(ValueProp.Move).WithMultiplier((CardModel card, Creature? _) => card.Owner.IsOstyAlive ? (card.Owner.Osty.MaxHp * 3) : 0)`
  - L35 `CanonicalVars`: `if (!Osty.CheckMissingWithAnim(base.Owner))`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.SCOURGE` — `Models/Cards/Scourge.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L32: `await PowerCmd.Apply<DoomPower>(choiceContext, cardPlay.Target, base.DynamicVars.Doom.BaseValue, base.Owner.Creature, this);`
  - L33: `await CardPileCmd.Draw(choiceContext, base.DynamicVars.Cards.BaseValue, base.Owner);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.SCULPTING_STRIKE` — `Models/Cards/SculptingStrike.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - L35 `CanonicalVars`: `if (cardModel != null)`
Generation, choice, pile, and cross-model call sites:
  - L34: `CardModel cardModel = (await CardSelectCmd.FromHand(choiceContext, base.Owner, new CardSelectorPrefs(base.SelectionScreenPrompt, 1), (CardModel c) => !c.GetKeywordsWithSources(KeywordSources.Local).Contains(CardKeyword.Ethereal), this)).FirstOrDefault();`
  - L37: `CardCmd.ApplyKeyword(cardModel, CardKeyword.Ethereal);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.SEANCE` — `Models/Cards/Seance.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - L30 `ExtraHoverTips`: `foreach (CardModel item in selection)`
Generation, choice, pile, and cross-model call sites:
  - L28: `List<CardModel> selection = (await CardSelectCmd.FromCombatPile(choiceContext, PileType.Draw.GetPile(base.Owner), base.Owner, new CardSelectorPrefs(CardSelectorPrefs.TransformSelectionPrompt, base.DynamicVars.Cards.IntValue))).ToList();`
  - L32: `await CardCmd.TransformTo<Soul>(item);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.SENTRY_MODE` — `Models/Cards/SentryMode.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L26: `await PowerCmd.Apply<SentryModePower>(choiceContext, base.Owner.Creature, base.DynamicVars["SentryModePower"].BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.SEVERANCE` — `Models/Cards/Severance.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L32: `CardPileAddResult drawResult = await CardPileCmd.AddGeneratedCardToCombat(souls[0], PileType.Draw, base.Owner, CardPilePosition.Random);`
  - L33: `CardPileAddResult discardResult = await CardPileCmd.AddGeneratedCardToCombat(souls[1], PileType.Discard, base.Owner);`
  - L34: `await CardPileCmd.AddGeneratedCardToCombat(souls[2], PileType.Hand, base.Owner);`
  - L35: `CardCmd.PreviewCardPileAdd(new global::_003C_003Ez__ReadOnlyArray<CardPileAddResult>(new CardPileAddResult[2] { drawResult, discardResult }));`
RNG-related call sites in source order:
  - L32: `CardPileAddResult drawResult = await CardPileCmd.AddGeneratedCardToCombat(souls[0], PileType.Draw, base.Owner, CardPilePosition.Random);`

### `CARD.SHARED_FATE` — `Models/Cards/SharedFate.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L38: `await PowerCmd.Apply<StrengthPower>(choiceContext, base.Owner.Creature, -base.DynamicVars["PlayerStrengthLoss"].BaseValue, base.Owner.Creature, this);`
  - L39: `await PowerCmd.Apply<StrengthPower>(choiceContext, cardPlay.Target, -base.DynamicVars["EnemyStrengthLoss"].BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.SHROUD` — `Models/Cards/Shroud.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L31: `await PowerCmd.Apply<ShroudPower>(choiceContext, base.Owner.Creature, base.DynamicVars.Block.BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.SIC_EM` — `Models/Cards/SicEm.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_ShouldGlowRedInternal`
Conditional/loop sites:
  - L37 `CanonicalVars`: `if (!Osty.CheckMissingWithAnim(base.Owner))`
Generation, choice, pile, and cross-model call sites:
  - L43: `await PowerCmd.Apply<SicEmPower>(choiceContext, cardPlay.Target, base.DynamicVars["SicEmPower"].BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.SLEIGHT_OF_FLESH` — `Models/Cards/SleightOfFlesh.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L23: `await PowerCmd.Apply<SleightOfFleshPower>(choiceContext, base.Owner.Creature, base.DynamicVars["SleightOfFleshPower"].BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.SNAP` — `Models/Cards/Snap.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`, `CardModel.get_ShouldGlowRedInternal`
Conditional/loop sites:
  - L34 `CanonicalVars`: `if (!Osty.CheckMissingWithAnim(base.Owner))`
  - L41 `CanonicalVars`: `if (cardModel != null)`
Generation, choice, pile, and cross-model call sites:
  - L40: `CardModel cardModel = (await CardSelectCmd.FromHand(prefs: new CardSelectorPrefs(base.SelectionScreenPrompt, 1), context: choiceContext, player: base.Owner, filter: (CardModel c) => !c.Keywords.Contains(CardKeyword.Retain), source: this)).FirstOrDefault();`
  - L43: `CardCmd.ApplyKeyword(cardModel, CardKeyword.Retain);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.SOUL_STORM` — `Models/Cards/SoulStorm.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.SOULBOUND` — `Models/Cards/Soulbound.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_ExtraHoverTips`, `CardModel.get_MultiplayerConstraint`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L28: `await PowerCmd.Apply<SoulboundPower>(choiceContext, cardPlay.Target, 1m, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.SOW` — `Models/Cards/Sow.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.SPIRIT_OF_ASH` — `Models/Cards/SpiritOfAsh.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L32: `await PowerCmd.Apply<SpiritOfAshPower>(choiceContext, base.Owner.Creature, base.DynamicVars["BlockOnExhaust"].BaseValue, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.SPUR` — `Models/Cards/Spur.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L32: `await OstyCmd.Summon(choiceContext, base.Owner, base.DynamicVars.Summon.BaseValue, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.SQUEEZE` — `Models/Cards/Squeeze.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ShouldGlowRedInternal`
Conditional/loop sites:
  - L36 `CanonicalVars`: `if (!Osty.CheckMissingWithAnim(base.Owner))`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.STRIKE_NECROBINDER` — `Models/Cards/StrikeNecrobinder.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.THE_SCYTHE` — `Models/Cards/TheScythe.cs`
Overrides: `CardModel.AfterDowngraded`, `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.TIMES_UP` — `Models/Cards/TimesUp.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.TRANSFIGURE` — `Models/Cards/Transfigure.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanBeGeneratedInCombat`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - L35 `ExtraHoverTips`: `foreach (CardModel item in cardToTransfigure)`
  - L37 `ExtraHoverTips`: `if (!item.EnergyCost.CostsX && item.EnergyCost.GetWithModifiers(CostModifiers.None) >= 0)`
Generation, choice, pile, and cross-model call sites:
  - L14: `public override bool CanBeGeneratedInCombat => false;`
  - L33: `IEnumerable<CardModel> cardToTransfigure = await CardSelectCmd.FromHand(choiceContext, base.Owner, new CardSelectorPrefs(base.SelectionScreenPrompt, 1), null, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.UNDEATH` — `Models/Cards/Undeath.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_GainsBlock`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L26: `CardCmd.PreviewCardPileAdd(await CardPileCmd.AddGeneratedCardToCombat(card, PileType.Discard, base.Owner), 2.2f);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.UNDERWORLD` — `Models/Cards/Underworld.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_ExtraHoverTips`, `CardModel.get_MultiplayerConstraint`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L27: `await PowerCmd.Apply<UnderworldPower>(choiceContext, base.Owner.Creature, 1m, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.UNLEASH` — `Models/Cards/Unleash.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalTags`, `CardModel.get_CanonicalVars`, `CardModel.get_ShouldGlowRedInternal`
Conditional/loop sites:
  - L27 `CanonicalVars`: `return (osty != null && osty.IsAlive) ? osty.CurrentHp : 0;`
  - L39 `CanonicalVars`: `if (!Osty.CheckMissingWithAnim(base.Owner))`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.VEILPIERCER` — `Models/Cards/Veilpiercer.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L31: `await PowerCmd.Apply<VeilpiercerPower>(choiceContext, base.Owner.Creature, 1m, base.Owner.Creature, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.WISP` — `Models/Cards/Wisp.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`, `CardModel.get_ExtraHoverTips`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `RELIC.BIG_HAT` — `Models/Relics/BigHat.cs`
Overrides: `AbstractModel.AfterSideTurnStart`, `RelicModel.get_CanonicalVars`, `RelicModel.get_ExtraHoverTips`, `RelicModel.get_Rarity`
Conditional/loop sites:
  - L25 `ExtraHoverTips`: `if (participants.Contains(base.Owner.Creature) && base.Owner.PlayerCombatState.TurnNumber <= 1)`
  - L30 `ExtraHoverTips`: `if (readOnlyList.Count > 0)`
Generation, choice, pile, and cross-model call sites:
  - L33: `List<CardModel> cards = CardFactory.GetDistinctForCombat(base.Owner, readOnlyList, base.DynamicVars.Cards.IntValue, base.Owner.RunState.Rng.CombatCardGeneration).ToList();`
  - L35: `await CardPileCmd.AddGeneratedCardsToCombat(cards, PileType.Hand, base.Owner);`
RNG-related call sites in source order:
  - L33: `List<CardModel> cards = CardFactory.GetDistinctForCombat(base.Owner, readOnlyList, base.DynamicVars.Cards.IntValue, base.Owner.RunState.Rng.CombatCardGeneration).ToList();`

### `RELIC.BONE_FLUTE` — `Models/Relics/BoneFlute.cs`
Overrides: `AbstractModel.AfterAttack`, `RelicModel.get_CanonicalVars`, `RelicModel.get_Rarity`
Conditional/loop sites:
  - L21 `CanonicalVars`: `if (!(command.Attacker?.Monster is Osty))`
  - L25 `CanonicalVars`: `if (command.Attacker.PetOwner?.Creature != base.Owner.Creature)`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `RELIC.BOOK_REPAIR_KNIFE` — `Models/Relics/BookRepairKnife.cs`
Overrides: `AbstractModel.AfterDiedToDoom`, `RelicModel.get_CanonicalVars`, `RelicModel.get_ExtraHoverTips`, `RelicModel.get_Rarity`
Conditional/loop sites:
  - L25 `CanonicalVars`: `if (num == 0)`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `RELIC.BOOKMARK` — `Models/Relics/Bookmark.cs`
Overrides: `AbstractModel.AfterFlush`, `RelicModel.get_ExtraHoverTips`, `RelicModel.get_Rarity`
Conditional/loop sites:
  - L20 `ExtraHoverTips`: `if (player != base.Owner)`
  - L25 `ExtraHoverTips`: `if (list.Count == 0)`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - L30: `base.Owner.RunState.Rng.CombatCardSelection.NextItem(list)?.EnergyCost.AddUntilPlayed(-1);`

### `RELIC.BOUND_PHYLACTERY` — `Models/Relics/BoundPhylactery.cs`
Overrides: `AbstractModel.AfterEnergyResetLate`, `AbstractModel.BeforeCombatStart`, `RelicModel.get_CanonicalVars`, `RelicModel.get_ExtraHoverTips`, `RelicModel.get_Rarity`, `RelicModel.get_SpawnsPets`
Conditional/loop sites:
  - L34 `ExtraHoverTips`: `if (player == base.Owner && base.Owner.PlayerCombatState.TurnNumber != 1)`
Generation, choice, pile, and cross-model call sites:
  - L42: `await OstyCmd.Summon(new ThrowingPlayerChoiceContext(), base.Owner, base.DynamicVars.Summon.BaseValue, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `RELIC.FUNERARY_MASK` — `Models/Relics/FuneraryMask.cs`
Overrides: `AbstractModel.BeforeHandDraw`, `RelicModel.get_CanonicalVars`, `RelicModel.get_ExtraHoverTips`, `RelicModel.get_Rarity`
Conditional/loop sites:
  - L25 `ExtraHoverTips`: `if (player == base.Owner && base.Owner.PlayerCombatState.TurnNumber == 1)`
  - L28 `ExtraHoverTips`: `for (int i = 0; (decimal)i < base.DynamicVars.Cards.BaseValue; i++)`
Generation, choice, pile, and cross-model call sites:
  - L31: `CardCmd.PreviewCardPileAdd(await CardPileCmd.AddGeneratedCardToCombat(card, PileType.Draw, base.Owner, CardPilePosition.Random));`
RNG-related call sites in source order:
  - L31: `CardCmd.PreviewCardPileAdd(await CardPileCmd.AddGeneratedCardToCombat(card, PileType.Draw, base.Owner, CardPilePosition.Random));`

### `RELIC.IVORY_TILE` — `Models/Relics/IvoryTile.cs`
Overrides: `AbstractModel.AfterCardPlayed`, `RelicModel.get_CanonicalVars`, `RelicModel.get_ExtraHoverTips`, `RelicModel.get_Rarity`
Conditional/loop sites:
  - L28 `ExtraHoverTips`: `if (cardPlay.Card.Owner == base.Owner && cardPlay.Resources.EnergyValue >= base.DynamicVars["EnergyThreshold"].IntValue)`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `RELIC.UNDYING_SIGIL` — `Models/Relics/UndyingSigil.cs`
Overrides: `AbstractModel.ModifyDamageMultiplicative`, `RelicModel.get_CanonicalVars`, `RelicModel.get_ExtraHoverTips`, `RelicModel.get_Rarity`
Conditional/loop sites:
  - L28 `ExtraHoverTips`: `if (dealer == null)`
  - L32 `ExtraHoverTips`: `if (!props.IsPoweredAttack())`
  - L36 `ExtraHoverTips`: `if (target != base.Owner.Creature)`
  - L40 `ExtraHoverTips`: `if (dealer == base.Owner.Creature)`
  - L44 `ExtraHoverTips`: `if (dealer.CurrentHp > dealer.GetPowerAmount<DoomPower>())`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.DOOM_POWER` — `Models/Powers/DoomPower.cs`
Overrides: `AbstractModel.AfterSideTurnEnd`, `AbstractModel.BeforeSideTurnEnd`, `PowerModel.get_AmountLabelColor`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L42 `DoomKill`: `if (creatures.Count == 0)`
  - L47 `DoomKill`: `foreach (Creature creature in creatures)`
  - L62 `BeforeSideTurnEnd`: `if (side != CombatSide.Player && ShouldDoomTrigger(participants))`
  - L70 `AfterSideTurnEnd`: `if (side != CombatSide.Enemy && ShouldDoomTrigger(participants))`
  - L78/L82/L86/L90/L95 `ShouldDoomTrigger`: rejects combat ending, owners not in this turn's participants, dead owners, owners above their Doom threshold, and every doomed owner except the first in source-side order.
  - L105/L110/L116/L123/L125 `PlayVfx`: returns when no rendered creature node exists, limits monster-only death VFX checks, attaches the shared Doom overlay once, and only waits when the creature is about to die (with a separate delay for the last living teammate).
  - L152/L159/L163/L174 `StartDoomAnim`: branches for lethal animation, valid Spine animation, the `hurt` animation case, and collecting the death/VFX tasks.
Generation, choice, pile, and cross-model call sites:
  - L42-L52: `DoomKill` processes the complete supplied batch in list order with `CreatureCmd.Kill`, then invokes `Hook.AfterDiedToDoom` once for the batch (so fatal/revival listeners see the shared set).
  - L55-L58: `GetDoomedCreatures` filters the input sequence in its original order by `CurrentHp <= DoomPower.Amount`.
  - L62-L72: side-turn hooks call `DoomKill` only from the active side's end phase; L94-L95 makes the first doomed owner the sole batch initiator.
  - L104-L179: `PlayVfx`, `StartDoomAnim` and `WaitForTween` are presentation-only; they do not change simulation choices, RNG, or outcomes.
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.BORROWED_TIME_POWER` — `Models/Powers/BorrowedTimePower.cs`
Overrides: `AbstractModel.AfterSideTurnEnd`, `AbstractModel.TryModifyEnergyCostInCombat`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L24 `ExtraHoverTips`: `if (card.Owner.Creature != base.Owner)`
  - L34 `ExtraHoverTips`: `if (participants.Contains(base.Owner))`
Generation, choice, pile, and cross-model call sites:
  - L36: `await PowerCmd.Remove(this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.CACOPHONY_POWER` — `Models/Powers/CacophonyPower.cs`
Overrides: `AbstractModel.AfterCardDrawn`, `PowerModel.get_CanonicalVars`, `PowerModel.get_DisplayAmount`, `PowerModel.get_InstanceType`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L28 `CanonicalVars`: `if (base.DynamicVars.Cards.IntValue <= 0)`
  - L34 `CanonicalVars`: `if (enemy != null)`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - L30: `Creature enemy = base.Owner.Player.RunState.Rng.CombatTargets.NextItem(base.CombatState.HittableEnemies);`

### `POWER.CALCIFY_POWER` — `Models/Powers/CalcifyPower.cs`
Overrides: `AbstractModel.ModifyDamageAdditive`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L17 `StackType`: `if (!(dealer?.Monster is Osty))`
  - L21 `StackType`: `if (base.Owner != dealer.PetOwner?.Creature)`
  - L25 `StackType`: `if (!props.IsPoweredAttack())`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.CALL_OF_THE_VOID_POWER` — `Models/Powers/CallOfTheVoidPower.cs`
Overrides: `AbstractModel.BeforeHandDraw`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L26 `ExtraHoverTips`: `if (player != base.Owner.Player)`
  - L33 `ExtraHoverTips`: `bool flag = ((rarity == CardRarity.Basic || rarity == CardRarity.Ancient) ? true : false);`
  - L36 `ExtraHoverTips`: `if (readOnlyList.Count > 0)`
  - L40 `ExtraHoverTips`: `for (int num = 0; num < base.Amount; num++)`
Generation, choice, pile, and cross-model call sites:
  - L12: `using MegaCrit.Sts2.Core.Random;`
  - L42: `CardCmd.ApplyKeyword(array[num] = CardFactory.GetDistinctForCombat(player, readOnlyList, 1, combatCardGeneration).First(), CardKeyword.Ethereal);`
  - L45: `await CardPileCmd.AddGeneratedCardsToCombat(array, PileType.Hand, base.Owner.Player);`
RNG-related call sites in source order:
  - L12: `using MegaCrit.Sts2.Core.Random;`
  - L39: `Rng combatCardGeneration = base.Owner.Player.RunState.Rng.CombatCardGeneration;`

### `POWER.COUNTDOWN_POWER` — `Models/Powers/CountdownPower.cs`
Overrides: `AbstractModel.AfterSideTurnStart`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L23 `ExtraHoverTips`: `if (participants.Contains(base.Owner))`
  - L27 `ExtraHoverTips`: `if (creature != null)`
Generation, choice, pile, and cross-model call sites:
  - L29: `await PowerCmd.Apply<DoomPower>(new ThrowingPlayerChoiceContext(), creature, base.Amount, base.Owner, null);`
RNG-related call sites in source order:
  - L26: `Creature creature = base.Owner.Player.RunState.Rng.CombatTargets.NextItem(base.CombatState.HittableEnemies);`

### `POWER.DANSE_MACABRE_POWER` — `Models/Powers/DanseMacabrePower.cs`
Overrides: `AbstractModel.BeforeCardPlayed`, `PowerModel.get_CanonicalVars`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L24 `ExtraHoverTips`: `if (cardPlay.Card.Owner.Creature == base.Owner && cardPlay.Card.EnergyCost.GetResolved() >= base.DynamicVars.Energy.IntValue)`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.WEAK_POWER` — `Models/Powers/WeakPower.cs`
Overrides: `AbstractModel.AfterSideTurnEnd`, `AbstractModel.ModifyDamageMultiplicative`, `PowerModel.get_CanonicalVars`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L27 `CanonicalVars`: `if (dealer != base.Owner)`
  - L31 `CanonicalVars`: `if (!props.IsPoweredAttack())`
  - L37 `CanonicalVars`: `if (paperKrane != null)`
  - L42 `CanonicalVars`: `if (power != null)`
  - L51 `CanonicalVars`: `if (side == CombatSide.Enemy)`
Generation, choice, pile, and cross-model call sites:
  - L53: `await PowerCmd.TickDownDuration(this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.DEBILITATE_POWER` — `Models/Powers/DebilitatePower.cs`
Overrides: `AbstractModel.AfterSideTurnEnd`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L28 `ExtraHoverTips`: `if (target != base.Owner)`
  - L32 `ExtraHoverTips`: `if (!props.IsPoweredAttack())`
  - L41 `ExtraHoverTips`: `if (dealer != base.Owner)`
  - L45 `ExtraHoverTips`: `if (!props.IsPoweredAttack())`
  - L54 `ExtraHoverTips`: `if (participants.Contains(base.Owner))`
Generation, choice, pile, and cross-model call sites:
  - L56: `await PowerCmd.Decrement(this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.ENERGY_NEXT_TURN_POWER` — `Models/Powers/EnergyNextTurnPower.cs`
Overrides: `AbstractModel.AfterEnergyReset`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L20 `ExtraHoverTips`: `if (player == base.Owner.Player)`
Generation, choice, pile, and cross-model call sites:
  - L23: `await PowerCmd.Remove(this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.DEMESNE_POWER` — `Models/Powers/DemesnePower.cs`
Overrides: `AbstractModel.ModifyHandDraw`, `AbstractModel.ModifyMaxEnergy`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L18 `ExtraHoverTips`: `if (player != base.Owner.Player)`
  - L27 `ExtraHoverTips`: `if (player != base.Owner.Player)`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.DEVOUR_LIFE_POWER` — `Models/Powers/DevourLifePower.cs`
Overrides: `AbstractModel.AfterCardPlayed`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L26 `ExtraHoverTips`: `if (cardPlay.Card is Soul && cardPlay.Card.Owner.Creature == base.Owner)`
Generation, choice, pile, and cross-model call sites:
  - L28: `await OstyCmd.Summon(choiceContext, cardPlay.Card.Owner, base.Amount, this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.ENFEEBLING_TOUCH_POWER` — `Models/Powers/EnfeeblingTouchPower.cs`
Overrides: `AbstractModel.AfterPowerAmountChanged (via TemporaryStrengthPower)`, `AbstractModel.AfterSideTurnEnd (via TemporaryStrengthPower)`, `PowerModel.BeforeApplied (via TemporaryStrengthPower)`, `PowerModel.get_Description (via TemporaryStrengthPower)`, `PowerModel.get_ExtraHoverTips (via TemporaryStrengthPower)`, `PowerModel.get_SmartDescriptionLocKey (via TemporaryStrengthPower)`, `PowerModel.get_StackType (via TemporaryStrengthPower)`, `PowerModel.get_Title (via TemporaryStrengthPower)`, `PowerModel.get_Type (via TemporaryStrengthPower)`, `TemporaryStrengthPower.get_IsPositive`, `TemporaryStrengthPower.get_OriginModel`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.VULNERABLE_POWER` — `Models/Powers/VulnerablePower.cs`
Overrides: `AbstractModel.AfterSideTurnEnd`, `AbstractModel.ModifyDamageMultiplicative`, `PowerModel.get_CanonicalVars`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L30 `CanonicalVars`: `if (target != base.Owner)`
  - L34 `CanonicalVars`: `if (!props.IsPoweredAttack())`
  - L39 `CanonicalVars`: `if (dealer != null)`
  - L42 `CanonicalVars`: `if (paperPhrog != null)`
  - L47 `CanonicalVars`: `if (crueltyPower != null)`
  - L53 `CanonicalVars`: `if (power != null)`
  - L62 `CanonicalVars`: `if (side == CombatSide.Enemy)`
Generation, choice, pile, and cross-model call sites:
  - L64: `await PowerCmd.TickDownDuration(this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.FORBIDDEN_GRIMOIRE_POWER` — `Models/Powers/ForbiddenGrimoirePower.cs`
Overrides: `AbstractModel.AfterCombatEnd`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L16 `StackType`: `for (int i = 0; i < base.Amount; i++)`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.STRENGTH_POWER` — `Models/Powers/StrengthPower.cs`
Overrides: `AbstractModel.ModifyDamageAdditive`, `PowerModel.get_AllowNegative`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L18 `AllowNegative`: `if (base.Owner != dealer)`
  - L22 `AllowNegative`: `if (!props.IsPoweredAttack())`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.FRIENDSHIP_POWER` — `Models/Powers/FriendshipPower.cs`
Overrides: `AbstractModel.ModifyMaxEnergy`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L18 `ExtraHoverTips`: `if (player != base.Owner.Player)`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.HANG_POWER` — `Models/Powers/HangPower.cs`
Overrides: `AbstractModel.ModifyDamageMultiplicative`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L21 `ExtraHoverTips`: `if (target != base.Owner)`
  - L25 `ExtraHoverTips`: `if (!(cardSource is Hang))`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.HAUNT_POWER` — `Models/Powers/HauntPower.cs`
Overrides: `AbstractModel.AfterCardPlayed`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L21 `StackType`: `if (cardPlay.Card is Soul && cardPlay.Card.Owner.Creature == base.Owner)`
  - L24 `StackType`: `if (hittableEnemies.Count != 0)`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - L26: `Creature item = base.Owner.Player.RunState.Rng.CombatTargets.NextItem(hittableEnemies);`

### `POWER.SUMMON_NEXT_TURN_POWER` — `Models/Powers/SummonNextTurnPower.cs`
Overrides: `AbstractModel.AfterPlayerTurnStart`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L21 `ExtraHoverTips`: `if (player == base.Owner.Player && base.AmountOnTurnStart != 0)`
Generation, choice, pile, and cross-model call sites:
  - L23: `await OstyCmd.Summon(choiceContext, base.Owner.Player, base.Amount, this);`
  - L24: `await PowerCmd.Remove(this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.LETHALITY_POWER` — `Models/Powers/LethalityPower.cs`
Overrides: `AbstractModel.ModifyDamageMultiplicative`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L19 `StackType`: `if (!props.IsPoweredAttack())`
  - L23 `StackType`: `if (cardSource == null)`
  - L27 `StackType`: `if (cardSource.Owner.Creature != base.Owner)`
  - L32 `StackType`: `if (cardSource != null)`
  - L35 `StackType`: `if (pile != null && pile.Type == PileType.Play && cardSource.CurrentPlayIndex > 0)`
  - L42 `StackType`: `int num2 = ((pile != null && pile.Type == PileType.Play) ? 1 : 0);`
  - L43 `StackType`: `if (num > num2)`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.NECRO_MASTERY_POWER` — `Models/Powers/NecroMasteryPower.cs`
Overrides: `AbstractModel.AfterCurrentHpChanged`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L19 `StackType`: `if (!(delta >= 0m) && creature.Monster is Osty && creature.PetOwner == base.Owner.Player)`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.NEUROSURGE_POWER` — `Models/Powers/NeurosurgePower.cs`
Overrides: `AbstractModel.AfterSideTurnStart`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L23 `ExtraHoverTips`: `if (participants.Contains(base.Owner))`
Generation, choice, pile, and cross-model call sites:
  - L25: `await PowerCmd.Apply<DoomPower>(new ThrowingPlayerChoiceContext(), base.Owner, base.Amount, base.Owner, null);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.OBLIVION_POWER` — `Models/Powers/OblivionPower.cs`
Overrides: `AbstractModel.AfterCardPlayed`, `AbstractModel.AfterSideTurnEnd`, `AbstractModel.BeforeCardPlayed`, `PowerModel.InitInternalData`, `PowerModel.get_InstanceType`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L36 `InstanceType`: `if (base.Applier?.Player == null)`
  - L40 `InstanceType`: `if (cardPlay.Card.Owner != base.Applier.Player)`
  - L50 `InstanceType`: `if (GetInternalData<Data>().amountsForPlayedCards.Remove(cardPlay.Card, out var value))`
  - L59 `InstanceType`: `if (side == CombatSide.Player)`
Generation, choice, pile, and cross-model call sites:
  - L53: `await PowerCmd.Apply<DoomPower>(choiceContext, base.Owner, value, base.Applier, null);`
  - L61: `await PowerCmd.Remove(this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.PAGESTORM_POWER` — `Models/Powers/PagestormPower.cs`
Overrides: `AbstractModel.AfterCardDrawn`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L21 `ExtraHoverTips`: `if (card.Owner.Creature == base.Owner && card.Keywords.Contains(CardKeyword.Ethereal))`
Generation, choice, pile, and cross-model call sites:
  - L24: `await CardPileCmd.Draw(choiceContext, base.Amount, base.Owner.Player);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.REAPER_FORM_POWER` — `Models/Powers/ReaperFormPower.cs`
Overrides: `AbstractModel.AfterDamageGiven`, `PowerModel.AfterApplied`, `PowerModel.AfterRemoved`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L28 `ExtraHoverTips`: `if (_vfx == null)`
  - L32 `ExtraHoverTips`: `if (!_vfx.IsValid())`
  - L59 `ExtraHoverTips`: `if (dealer != null && (dealer == base.Owner || dealer.PetOwner?.Creature == base.Owner) && props.IsPoweredAttack() && result.TotalDamage > 0)`
Generation, choice, pile, and cross-model call sites:
  - L62: `await PowerCmd.Apply<DoomPower>(choiceContext, target, result.TotalDamage * base.Amount, base.Owner, null);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.SENTRY_MODE_POWER` — `Models/Powers/SentryModePower.cs`
Overrides: `AbstractModel.BeforeHandDraw`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L24 `ExtraHoverTips`: `if (player == base.Owner.Player)`
  - L26 `ExtraHoverTips`: `for (int i = 0; i < base.Amount; i++)`
Generation, choice, pile, and cross-model call sites:
  - L29: `await CardPileCmd.AddGeneratedCardToCombat(card, PileType.Hand, base.Owner.Player);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.SHROUD_POWER` — `Models/Powers/ShroudPower.cs`
Overrides: `AbstractModel.AfterPowerAmountChanged`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L22 `ExtraHoverTips`: `if (applier == base.Owner && power is DoomPower)`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.SIC_EM_POWER` — `Models/Powers/SicEmPower.cs`
Overrides: `AbstractModel.AfterDamageGiven`, `AbstractModel.AfterSideTurnEnd`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L25 `ExtraHoverTips`: `if (dealer?.Monster is Osty osty && osty.Creature.PetOwner != null && base.Applier != null && osty.Creature.PetOwner.Creature == base.Applier && target == base.Owner)`
  - L33 `ExtraHoverTips`: `if (participants.Contains(base.Owner))`
Generation, choice, pile, and cross-model call sites:
  - L27: `await OstyCmd.Summon(choiceContext, dealer.PetOwner, base.Amount, this);`
  - L35: `await PowerCmd.Remove(this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.SLEIGHT_OF_FLESH_POWER` — `Models/Powers/SleightOfFleshPower.cs`
Overrides: `AbstractModel.AfterPowerAmountChanged`, `PowerModel.get_CanonicalVars`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L23 `CanonicalVars`: `if (!(amount == 0m) && power.GetTypeForAmount(amount) == PowerType.Debuff && power.Owner.IsEnemy && applier == base.Owner && !(power is ITemporaryPower))`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.SOULBOUND_POWER` — `Models/Powers/SoulboundPower.cs`
Overrides: `AbstractModel.AfterCardGeneratedForCombat`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L37 `ExtraHoverTips`: `if (creator != null && creator.Creature == base.Applier && card is Soul && !IsAddingSoul)`
Generation, choice, pile, and cross-model call sites:
  - L35: `public override async Task AfterCardGeneratedForCombat(CardModel card, Player? creator)`
  - L41: `CardCmd.PreviewCardPileAdd(await CardPileCmd.AddGeneratedCardsToCombat(Soul.Create(base.Owner.Player, base.Amount, base.CombatState), PileType.Draw, base.Owner.Player, CardPilePosition.Random));`
RNG-related call sites in source order:
  - L41: `CardCmd.PreviewCardPileAdd(await CardPileCmd.AddGeneratedCardsToCombat(Soul.Create(base.Owner.Player, base.Amount, base.CombatState), PileType.Draw, base.Owner.Player, CardPilePosition.Random));`

### `POWER.SPIRIT_OF_ASH_POWER` — `Models/Powers/SpiritOfAshPower.cs`
Overrides: `AbstractModel.BeforeCardPlayed`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L25 `ExtraHoverTips`: `if (cardPlay.Card.Owner == base.Owner.Player && cardPlay.Card.Keywords.Contains(CardKeyword.Ethereal))`
Generation, choice, pile, and cross-model call sites:
  - none detected
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.UNDERWORLD_POWER` — `Models/Powers/UnderworldPower.cs`
Overrides: `AbstractModel.AfterDamageGiven`, `AbstractModel.AfterSideTurnEnd`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L23 `ExtraHoverTips`: `if (dealer != null && dealer.Side == base.Owner.Side && dealer != base.Owner && dealer.PetOwner != base.Owner.Player && props.IsPoweredAttack() && result.TotalDamage > 0)`
  - L31 `ExtraHoverTips`: `if (side == CombatSide.Enemy)`
Generation, choice, pile, and cross-model call sites:
  - L25: `await PowerCmd.Apply<DoomPower>(choiceContext, target, result.TotalDamage * base.Amount, base.Owner, null);`
  - L33: `await PowerCmd.Remove(this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `POWER.VEILPIERCER_POWER` — `Models/Powers/VeilpiercerPower.cs`
Overrides: `AbstractModel.BeforeCardPlayed`, `AbstractModel.TryModifyEnergyCostInCombatLate`, `PowerModel.get_ExtraHoverTips`, `PowerModel.get_StackType`, `PowerModel.get_Type`
Conditional/loop sites:
  - L21 `ExtraHoverTips`: `if (card.Owner.Creature != base.Owner)`
  - L25 `ExtraHoverTips`: `if (!card.Keywords.Contains(CardKeyword.Ethereal))`
  - L30 `ExtraHoverTips`: `switch (card.Pile?.Type)`
  - L32 `ExtraHoverTips`: `case PileType.Hand:`
  - L33 `ExtraHoverTips`: `case PileType.Play:`
  - L40 `ExtraHoverTips`: `if (!flag)`
  - L50 `ExtraHoverTips`: `if (cardPlay.Card.Owner.Creature == base.Owner && cardPlay.Card.Keywords.Contains(CardKeyword.Ethereal))`
  - L53 `ExtraHoverTips`: `switch (cardPlay.Card.Pile?.Type)`
  - L55 `ExtraHoverTips`: `case PileType.Hand:`
  - L56 `ExtraHoverTips`: `case PileType.Play:`
  - L63 `ExtraHoverTips`: `if (flag)`
Generation, choice, pile, and cross-model call sites:
  - L65: `await PowerCmd.Decrement(this);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `MONSTER.OSTY` — `Models/Monsters/Osty.cs`
Overrides: `MonsterModel.GenerateAnimator`, `MonsterModel.GenerateMoveStateMachine`, `MonsterModel.get_DeathSfx`, `MonsterModel.get_HasDeathSfx`, `MonsterModel.get_IsHealthBarVisible`, `MonsterModel.get_MaxInitialHp`, `MonsterModel.get_MinInitialHp`
Conditional/loop sites:
  - none detected
Generation, choice, pile, and cross-model call sites:
  - L37: `protected override MonsterMoveStateMachine GenerateMoveStateMachine()`
  - L44: `public override CreatureAnimator GenerateAnimator(MegaSprite controller)`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `CARD.BYRDONIS_EGG` — `Models/Cards/ByrdonisEgg.cs` (generated quest card)
Overrides: `AbstractModel.TryModifyRestSiteOptions`, `CardModel.get_CanonicalKeywords`, `CardModel.get_MaxUpgradeLevel`
Conditional/loop sites:
  - L20 `TryModifyRestSiteOptions`: `if (player != base.Owner) return false`; otherwise append `HatchRestSiteOption`.
Generation, choice, pile, and cross-model call sites:
  - `ByrdonisNest.Take` creates the card and adds it to the deck; `RestSiteOption::Generate` maps the guarded hook to the single-player `Hatch` option, and `RestSiteOption::OnSelect` obtains Byrdpip.
RNG-related call sites in source order:
  - none in the card model or Hatch path.

### `CARD.SOUL` — `Models/Cards/Soul.cs`
Overrides: `CardModel.OnPlay`, `CardModel.OnUpgrade`, `CardModel.get_CanonicalKeywords`, `CardModel.get_CanonicalVars`
Conditional/loop sites:
  - L48 `CanonicalVars`: `for (int i = 0; i < amount; i++)`
Generation, choice, pile, and cross-model call sites:
  - L34: `await CardPileCmd.AddGeneratedCardsToCombat(souls, PileType.Hand, creator ?? owner);`
  - L57: `await CardPileCmd.Draw(choiceContext, base.DynamicVars.Cards.BaseValue, base.Owner);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

### `ENCHANTMENT.SOULS_POWER` — `Models/Enchantments/SoulsPower.cs`
Overrides: `EnchantmentModel.CanEnchant`, `EnchantmentModel.OnEnchant`, `EnchantmentModel.get_ExtraHoverTips`
Conditional/loop sites:
  - L14 `ExtraHoverTips`: `if (!base.CanEnchant(card))`
  - L18 `ExtraHoverTips`: `if (!card.GetKeywordsWithSources(KeywordSources.Local).Contains(CardKeyword.Exhaust))`
Generation, choice, pile, and cross-model call sites:
  - L6: `namespace MegaCrit.Sts2.Core.Models.Enchantments;`
  - L8: `public sealed class SoulsPower : EnchantmentModel`
  - L12: `public override bool CanEnchant(CardModel card)`
  - L14: `if (!base.CanEnchant(card))`
  - L25: `protected override void OnEnchant()`
  - L27: `CardCmd.RemoveKeyword(base.Card, CardKeyword.Exhaust);`
RNG-related call sites in source order:
  - none detected in model body; random placement/selection may be delegated to the called engine command

## Cross-model and Engine contract ledger

- **Start/run setup:** character → Necrobinder card/relic/potion pools → Bound Phylactery; starting deck includes 4 Strike, 4 Defend, Bodyguard, Unleash. Carry HP, deck, relic inventory, epoch/profile unlocks across act transition using generic run state.
- **Osty:** targetable allied pet with its own HP, summon cap/amount changes, revive/respawn rules, attacks as card source, owner-targeted attack/block/power effects, end of turn and enemy turn behavior. Sources to reconcile: `Monsters/Osty.cs`, `Commands/OstyCmd.cs`, Necrobinder cards, Bound Phylactery, existing shared pet relics/powers and HP-loss Hook phases.
- **Power graph:** the preceding table lists direct generic power targets from card models. For each power also inspect its own overrides and the timing hooks it relies on; duration/turn transition must follow source model, not card behavior assumptions.
- **Generated candidates:** `Soul` is generated by Grave Warden (not in the 91-card pool); `ByrdonisEgg` is generated by Byrdonis Nest and is an unplayable Quest card whose Hatch rest-site hook is handled by `RestSiteOption::Generate`. Exact creation/count/keywords must come from `Cards/Soul.cs` and `Cards/ByrdonisEgg.cs`. Capture Spirit/Dirge/Glimpse Beyond/Grave Warden/Reave/Severance/Undeath and all other scanned generator calls must preserve candidate order and insertion position.
- **Selection:** Cleanse, Dredge, Graveblast, Sculpting Strike, Seance, Snap, Transfigure (and any control sites in the full listing) require `CardSelectCmd` contracts: prefix order, selector preferences/filter, optional-empty behavior, and stable snapshot replay.
- **Event eligibility:** `ByrdonisNest.IsAllowed` calls `Player.HasEventPet`. The source checks `RelicModel.AddsPet` (currently Byrdpip/Pael's Legion) or a Byrdonis Egg, which is distinct from `SpawnsPets`; Bound Phylactery's Osty summon must therefore leave Byrdonis Nest eligible. The generic predicate is covered by a Necrobinder regression.
- **Enchantment:** SoulsPower is an event-accessible run effect and an in-scope enchantment dependency; audit its `CanEnchant`, `OnEnchant`, effect/hooks, persistence, and all card interaction points.
- **Epoch gate:** port the exact filter from the card pool and relevant epoch unlock state; compare both filtered candidates and downstream choice/RNG state against Sts2Oracle.
- **Potions (external dependency):** pool contract owns three Necrobinder potion IDs. Implementations remain exclusively in `feat/shared-cards-all-potions-v0110`; record whether that branch has been merged before final run validation and do not duplicate its model changes.
- **RNG:** preserve source call order; random discard upgrade selection uses CombatCardSelection stream at Drain Power; card draw, random pile insertion, and generation can consume RNG in generic command/factory paths, so validate each command end-to-end rather than injecting branch-specific draws.

## Epoch unlock source facts

Source files: `src/Core/Timeline/Epochs/Necrobinder1Epoch.cs` through `Necrobinder7Epoch.cs`, plus the character, card-pool, relic-pool and potion-pool sources. Indices follow the original `EpochModel.AllEpochIds` order. E1 unlocks Necrobinder and expands the timeline; E2/5/7 filter cards, E3/6 filter relics, and E4 unlocks the three Necrobinder potions. Pool filters preserve original candidate order. Exact memberships and epoch order below are from original source:

| Epoch | Bit | Era / position | Unlock candidates in original source order |
|---|---:|---|---|
| Necrobinder1 | 26 | Invitation5 / 0 | Character: Necrobinder; timeline expansion: Necrobinder2–7 then Defect1 |
| Necrobinder2 | 27 | Invitation0 / 2 | Cards: Scourge, Oblivion, Countdown |
| Necrobinder3 | 28 | Invitation1 / 3 | Relics: Bone Flute, Funerary Mask, Book Repair Knife |
| Necrobinder4 | 29 | Invitation2 / 2 | Potions: Potion of Doom, Pot of Ghouls, Bone Brew |
| Necrobinder5 | 30 | Invitation3 / 1 | Cards: Afterlife, Sacrifice, Calcify |
| Necrobinder6 | 31 | Invitation4 / 0 | Relics: Ivory Tile, Big Hat, Bookmark |
| Necrobinder7 | 32 | Invitation7 / 1 | Cards: Sculpting Strike, Veilpiercer, Banshee's Cry |

The unlock text and queue methods are presentation-facing. E1 character eligibility/expansion, card/relic filtering and E4 potion visibility are simulation-relevant. The three potion models remain owned by the parallel task; this task owns only the E4 profile-gating contract and Oracle verification, not potion model code.

### Additional supporting-source scan

- `src/Core/Models/Cards/SweepingGaze.cs`: `OnPlay` condition at L33 is `!Osty.CheckMissingWithAnim(owner)`; then it runs Osty-powered damage against random opponents (model call at L35; random selection is delegated to the generic attack/target command). `OnUpgrade` and the token card's keywords/tags/dynamic vars/glow are also declared overrides in `data/content.json`.
- `src/Core/Models/Powers/DieForYouPower.cs`: per source order, `ModifyUnblockedDamageTarget` branches for `target != owner.PetOwner?.Creature` (L17), dead pet owner (L21), and non-powered attacks (L25); `ShouldAllowHitting` branches on the pet's death (L33 comment); `ShouldCreatureBeRemovedFromCombatAfterDeath` branches on `creature != owner` (L42). This shared power is already in the baseline and is applied by `OstyCmd.Summon`.
- `src/Core/Timeline/Epochs/Necrobinder1Epoch.cs`: source constructs a fixed 7-entry expansion in this order (Necrobinder2, 3, 4, 5, 6, 7, Defect1), unlocks Necrobinder, stores its pending character id and queues the expansion. No conditional, loop or RNG call in the model.
- `src/Core/Timeline/Epochs/Necrobinder4Epoch.cs`: source constructs a fixed 3-potion unlock list in this order (PotionOfDoom, PotOfGhouls, BoneBrew) and queues the potion plus a misc unlock. No conditional, loop or RNG call in the model. Potion use/model effects remain exclusively owned by the parallel all-potions task.

### Necrobinder potion contract (owned by `feat/shared-cards-all-potions-v0110`)

This branch must consume these exact pool/use contracts and must not add potion model or `PotionFactory` implementation:

| Potion ID | Rarity / usage / target | Source effect contract |
| --- | --- | --- |
| `POTION_OF_DOOM` | Common / CombatOnly / AnyEnemy | Apply 33 `DoomPower` to the selected enemy (`PowerCmd.Apply<DoomPower>`; source also plays purple splash VFX). |
| `POT_OF_GHOULS` | Rare / CombatOnly / AnyPlayer | Create 2 `Soul` cards in the selected player's hand (`Soul.CreateInHand`). |
| `BONE_BREW` | Uncommon / CombatOnly / AnyPlayer | Summon Osty for the selected player with 15 summon amount (`OstyCmd.Summon`; source also plays blue splash VFX). |

All three belong to `POTION_POOL.NECROBINDER_POTION_POOL`, whose source ordering is Potion of Doom, Pot of Ghouls, Bone Brew and whose pool unlock is Necrobinder Epoch 4. The shared-content task owns model code, pool factory behavior, and its Oracle probes. The Necrobinder acceptance contract is that each potion remains obtainable through its Epoch 4 gate and its effect is compared in a Necrobinder combat/run context after both branches are integrated. Before that merge, any results from the shared task are dependency evidence rather than a complete integrated-run validation.
- `src/Core/Timeline/Epochs/Necrobinder2Epoch.cs`, `Necrobinder3Epoch.cs`, `Necrobinder5Epoch.cs`, `Necrobinder6Epoch.cs`, and `Necrobinder7Epoch.cs`: each materializes the fixed unlock list from the table above and queues it; no conditional, loop or RNG call in these epoch models.
