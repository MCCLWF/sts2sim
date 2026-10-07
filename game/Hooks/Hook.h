// Hooks/Hook.cs. GENERATED declarations (tools/gen/gen_hook_header.py); the bodies
// are ported in Hook.cpp.
#pragma once

#include "game/All.h"

namespace sts2 {

// IEnumerable<AbstractModel> / List<AbstractModel> of hook modifiers.
using ModelList = StaticVec<AbstractModel, 32>;

namespace Hook {

void AfterActEntered(sts2::RunState runState);
void BeforeAttack(sts2::CombatState combatState, sts2::AttackCommand& command);
void AfterAttack(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::AttackCommand& command);
void AfterBlockBroken(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::Creature target, sts2::Creature breaker);
void AfterBlockCleared(sts2::CombatState combatState, sts2::Creature creature);
void BeforeBlockGained(sts2::CombatState combatState, sts2::Creature creature, sts2::Dec amount, sts2::ValueProp props, sts2::CardModel cardSource);
void AfterBlockGained(sts2::CombatState combatState, sts2::Creature creature, sts2::Dec amount, sts2::ValueProp props, sts2::CardModel cardSource);
void BeforeCardAutoPlayed(sts2::CombatState combatState, sts2::CardModel card, sts2::Creature target, sts2::AutoPlayType type);
void AfterCardChangedPiles(sts2::RunState runState, sts2::CombatState combatState, sts2::CardModel card, sts2::PileType oldPile, sts2::AbstractModel clonedBy);
void AfterCardDiscarded(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::CardModel card);
void AfterCardDrawn(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::CardModel card, bool fromHandDraw);
void AfterCardEnteredCombat(sts2::CombatState combatState, sts2::CardModel card);
void AfterCardExhausted(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::CardModel card, bool causedByEthereal);
void AfterCardGeneratedForCombat(sts2::CombatState combatState, sts2::CardModel card, sts2::Player creator);
void BeforeCardPlayed(sts2::CombatState combatState, const sts2::CardPlay* cardPlay);
void AfterCardPlayed(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, const sts2::CardPlay* cardPlay);
void BeforeCardRemoved(sts2::RunState runState, sts2::CardModel card);
void BeforeCombatStart(sts2::RunState runState, sts2::CombatState combatState);
void AfterCombatEnd(sts2::RunState runState, sts2::CombatState combatState, sts2::CombatRoom room);
void BeforeCombatRewardOffered(sts2::RewardsSet rewards, sts2::RunState runState, sts2::CombatRoom room);
void AfterCombatVictory(sts2::RunState runState, sts2::CombatState combatState, sts2::CombatRoom room);
void AfterCreatureAddedToCombat(sts2::CombatState combatState, sts2::Creature creature);
void AfterCurrentHpChanged(sts2::RunState runState, sts2::CombatState combatState, sts2::Creature creature, sts2::Dec delta);
void AfterDamageGiven(sts2::PlayerChoiceContext& choiceContext, sts2::CombatState combatState, sts2::Creature dealer, const sts2::DamageResult& results, sts2::ValueProp props, sts2::Creature target, sts2::CardModel cardSource);
void BeforeDamageReceived(sts2::PlayerChoiceContext& choiceContext, sts2::RunState runState, sts2::CombatState combatState, sts2::Creature target, sts2::Dec amount, sts2::ValueProp props, sts2::Creature dealer, sts2::CardModel cardSource);
void AfterDamageReceived(sts2::PlayerChoiceContext& choiceContext, sts2::RunState runState, sts2::CombatState combatState, sts2::Creature target, const sts2::DamageResult& result, sts2::ValueProp props, sts2::Creature dealer, sts2::CardModel cardSource);
void BeforeDeath(sts2::RunState runState, sts2::CombatState combatState, sts2::Creature creature);
void AfterDeath(sts2::RunState runState, sts2::CombatState combatState, sts2::Creature creature, bool wasRemovalPrevented, float deathAnimLength);
void AfterGoldGained(sts2::RunState runState, sts2::Player player);
void AfterDiedToDoom(sts2::CombatState combatState, const sts2::CreatureList& creatures);
void AfterEnergyReset(sts2::CombatState combatState, sts2::Player player);
void AfterEnergySpent(sts2::CombatState combatState, sts2::CardModel card, int32_t amount);
void BeforeFlush(sts2::CombatState combatState, sts2::Player player);
void AfterFlush(sts2::CombatState combatState, sts2::Player player, sts2::PlayerChoiceContext& playerChoiceContext, const sts2::CardList& flushedCards, const sts2::CardList& retainedCards);
void AfterForge(sts2::CombatState combatState, sts2::Dec amount, sts2::Player forger, sts2::AbstractModel source);
void BeforeHandDraw(sts2::CombatState combatState, sts2::Player player, sts2::PlayerChoiceContext& playerChoiceContext);
void AfterHandEmptied(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::Player player);
void AfterItemPurchased(sts2::RunState runState, sts2::Player player, sts2::Merchant::EntryRef itemPurchased, int32_t goldSpent);
void AfterMapGenerated(sts2::RunState runState, sts2::ActMap& map, int32_t actIndex);
void AfterModifyingBlockAmount(sts2::CombatState combatState, sts2::Dec modifiedBlock, sts2::CardModel cardSource, const sts2::CardPlay* cardPlay, const sts2::ModelList& modifiers);
void AfterModifyingCardPlayCount(sts2::CombatState combatState, sts2::CardModel card, const sts2::ModelList& modifiers);
void AfterModifyingCardRewardOptions(sts2::RunState runState, const sts2::ModelList& modifiers);
void AfterModifyingDamageAmount(sts2::RunState runState, sts2::CombatState combatState, sts2::CardModel cardSource, const sts2::ModelList& modifiers);
void AfterModifyingEnergyGain(sts2::CombatState combatState, const sts2::ModelList& modifiers);
void AfterModifyingGoldGained(sts2::RunState runState, sts2::CombatState combatState, const sts2::ModelList& modifiers, sts2::Player player, sts2::Dec amount);
void AfterModifyingHandDraw(sts2::CombatState combatState, const sts2::ModelList& modifiers);
void AfterModifyingHpLostBeforeOsty(sts2::RunState runState, sts2::CombatState combatState, const sts2::ModelList& modifiers);
void AfterModifyingHpLostAfterOsty(sts2::RunState runState, sts2::CombatState combatState, const sts2::ModelList& modifiers);
void AfterModifyingOrbPassiveTriggerCount(sts2::CombatState combatState, sts2::OrbModel orb, const sts2::ModelList& modifiers);
void AfterModifyingPowerAmountGiven(sts2::CombatState combatState, const sts2::ModelList& modifiers, sts2::PowerModel modifiedPower);
void AfterModifyingPowerAmountReceived(sts2::CombatState combatState, const sts2::ModelList& modifiers, sts2::PowerModel modifiedPower);
void AfterModifyingRewards(sts2::RunState runState, const sts2::ModelList& modifiers);
void AfterOrbChanneled(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::Player player, sts2::OrbModel orb);
void AfterOrbEvoked(sts2::PlayerChoiceContext& choiceContext, sts2::CombatState combatState, sts2::OrbModel orb, const sts2::CreatureList& targets);
void AfterOstyRevived(sts2::CombatState combatState, sts2::Creature osty);
void AfterPlayerTurnStart(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::Player player);
void AfterAutoPostPlayPhaseEntered(sts2::PlayerChoiceContext& playerChoiceContext, sts2::CombatState combatState, sts2::Player player);
void AfterAutoPrePlayPhaseEntered(sts2::PlayerChoiceContext& playerChoiceContext, sts2::CombatState combatState, sts2::Player player);
void AfterPotionDiscarded(sts2::RunState runState, sts2::CombatState combatState, sts2::PotionModel potion);
void AfterPotionProcured(sts2::RunState runState, sts2::CombatState combatState, sts2::PotionModel potion);
void BeforePotionUsed(sts2::RunState runState, sts2::CombatState combatState, sts2::PotionModel potion, sts2::Creature target);
void AfterPotionUsed(sts2::RunState runState, sts2::CombatState combatState, sts2::PotionModel potion, sts2::Creature target);
void BeforePowerAmountChanged(sts2::CombatState combatState, sts2::PowerModel power, sts2::Dec amount, sts2::Creature target, sts2::Creature applier, sts2::CardModel cardSource);
void AfterPowerAmountChanged(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::PowerModel power, sts2::Dec amount, sts2::Creature applier, sts2::CardModel cardSource);
void AfterPreventingBlockClear(sts2::CombatState combatState, sts2::AbstractModel preventer, sts2::Creature creature);
void AfterPreventingDeath(sts2::RunState runState, sts2::CombatState combatState, sts2::AbstractModel preventer, sts2::Creature creature);
void AfterPreventingDraw(sts2::CombatState combatState, sts2::AbstractModel modifier);
void AfterRestSiteHeal(sts2::RunState runState, sts2::Player player, bool isMimicked);
void AfterRestSiteSmith(sts2::RunState runState, sts2::Player player);
void AfterRewardTaken(sts2::RunState runState, sts2::Player player, sts2::Reward reward);
void BeforeRoomEntered(sts2::RunState runState, sts2::AbstractRoom room);
void AfterRoomEntered(sts2::RunState runState, sts2::AbstractRoom room);
void AfterShuffle(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::Player shuffler);
void BeforeSideTurnStart(sts2::CombatState combatState, sts2::CombatSide side, const sts2::CreatureList& participants);
void AfterSideTurnStart(sts2::CombatState combatState, sts2::CombatSide side, const sts2::CreatureList& participants);
void AfterStarsGained(sts2::CombatState combatState, int32_t amount, sts2::Player gainer);
void AfterStarsSpent(sts2::CombatState combatState, int32_t amount, sts2::Player spender);
void AfterSummon(sts2::CombatState combatState, sts2::PlayerChoiceContext& choiceContext, sts2::Player summoner, sts2::Dec amount);
void AfterTakingExtraTurn(sts2::CombatState combatState, sts2::Player player);
void BeforeSideTurnEnd(sts2::CombatState combatState, sts2::CombatSide side, const sts2::CreatureList& participants);
void AfterSideTurnEnd(sts2::CombatState combatState, sts2::CombatSide side, const sts2::CreatureList& participants);
sts2::Dec ModifyAttackHitCount(sts2::CombatState combatState, sts2::AttackCommand& attackCommand, int32_t originalHitCount);
sts2::Dec ModifyBlock(sts2::CombatState combatState, sts2::Creature target, sts2::Dec block, sts2::ValueProp props, sts2::CardModel cardSource, const sts2::CardPlay* cardPlay, sts2::ModelList& modifiers);
sts2::CardModel ModifyCardBeingAddedToDeck(sts2::RunState runState, sts2::CardModel card, sts2::ModelList& modifyingModels);
int32_t ModifyCardPlayCount(sts2::CombatState combatState, sts2::CardModel card, int32_t playCount, sts2::Creature target, sts2::ModelList& modifyingModels);
sts2::CardLocation ModifyCardPlayResultLocation(sts2::CombatState combatState, sts2::CardModel card, bool isAutoPlay, const sts2::ResourceInfo& resources, sts2::CardLocation cardLocation, sts2::ModelList& modifiers);
sts2::CardCreationOptionsState ModifyCardRewardCreationOptions(sts2::RunState runState, sts2::Player player, const sts2::CardCreationOptionsState& options);
bool TryModifyCardRewardOptions(sts2::RunState runState, sts2::Player player, sts2::CardList& cardRewardOptions, const sts2::CardCreationOptionsState& creationOptions, sts2::ModelList& modifiers);
sts2::Dec ModifyCardRewardUpgradeOdds(sts2::RunState runState, sts2::Player player, sts2::CardModel card, sts2::Dec originalOdds);
sts2::Dec ModifyDamage(sts2::RunState runState, sts2::CombatState combatState, sts2::Creature target, sts2::Creature dealer, sts2::Dec damage, sts2::ValueProp props, sts2::CardModel cardSource, const sts2::CardPlay* cardPlay, sts2::ModifyDamageHookType modifyDamageHookType, sts2::CardPreviewMode previewMode, sts2::ModelList& modifiers);
sts2::Dec ModifyEnergyCostInCombat(sts2::CombatState combatState, sts2::CardModel card, sts2::Dec originalCost);
void ModifyKeywordsInCombat(sts2::CombatState combatState, sts2::CardModel card, sts2::CardKeywordSet& keywords);
sts2::Dec ModifyEnergyGain(sts2::CombatState combatState, sts2::Player player, sts2::Dec originalAmount, sts2::ModelList& modifiers);
sts2::Dec ModifyGoldGained(sts2::RunState runState, sts2::CombatState combatState, sts2::Dec amount, sts2::Player player, sts2::ModelList& modifiers);
sts2::ActMap ModifyGeneratedMap(sts2::RunState runState, sts2::ActMap map, int32_t actIndex);
sts2::ActMap ModifyGeneratedMapLate(sts2::RunState runState, sts2::ActMap map, int32_t actIndex);
sts2::Dec ModifyHandDraw(sts2::CombatState combatState, sts2::Player player, sts2::Dec originalCardCount, sts2::ModelList& modifiers);
sts2::Dec ModifyHpLost(sts2::RunState runState, sts2::CombatState combatState, sts2::Creature target, sts2::Dec amount, sts2::ValueProp props, sts2::Creature dealer, sts2::CardModel cardSource, sts2::HpLossHookPhase phases, sts2::ModelList& modifiers);
sts2::Dec ModifyMaxEnergy(sts2::CombatState combatState, sts2::Player player, sts2::Dec amount);
void ModifyMerchantCardCreationResults(sts2::RunState runState, sts2::Player player, sts2::CardList& cards);
sts2::CardRarity ModifyMerchantCardRarity(sts2::RunState runState, sts2::Player player, sts2::CardRarity rarity);
sts2::Dec ModifyMerchantPrice(sts2::RunState runState, sts2::Player player, sts2::Merchant::EntryRef entry, sts2::Dec result);
sts2::EventModel ModifyNextEvent(sts2::RunState runState, sts2::EventModel currentEvent);
float ModifyOddsIncreaseForUnrolledRoomType(sts2::RunState runState, sts2::RoomType roomType, float oddsIncrease);
int32_t ModifyOrbPassiveTriggerCount(sts2::CombatState combatState, sts2::OrbModel orb, int32_t triggerCount, sts2::ModelList& modifyingModels);
sts2::Dec ModifyOrbValue(sts2::CombatState combatState, sts2::OrbModel orb, sts2::Dec amount);
sts2::Dec ModifyPowerAmountGiven(sts2::CombatState combatState, sts2::PowerModel power, sts2::Creature giver, sts2::Dec amount, sts2::Creature target, sts2::CardModel cardSource, sts2::ModelList& modifiers);
sts2::Dec ModifyPowerAmountReceived(sts2::CombatState combatState, sts2::PowerModel canonicalPower, sts2::Creature target, sts2::Dec amount, sts2::Creature giver, sts2::ModelList& modifiers);
sts2::Dec ModifyRestSiteHealAmount(sts2::RunState runState, sts2::Creature creature, sts2::Dec amount);
sts2::ModelList ModifyRestSiteHealRewards(sts2::RunState runState, sts2::Player player, sts2::RewardList rewards, bool isMimicked);
sts2::ModelList ModifyRewards(sts2::RunState runState, sts2::Player player, sts2::RewardList rewards, sts2::AbstractRoom room);
void ModifyShuffleOrder(sts2::CombatState combatState, sts2::Player player, sts2::CardList& cards, bool isInitialShuffle);
sts2::Dec ModifyStarCost(sts2::CombatState combatState, sts2::CardModel card, sts2::Dec originalCost);
sts2::Dec ModifySummonAmount(sts2::CombatState combatState, sts2::Player summoner, sts2::Dec amount, sts2::AbstractModel source);
sts2::Creature ModifyUnblockedDamageTarget(sts2::CombatState combatState, sts2::Creature originalTarget, sts2::Dec amount, sts2::ValueProp props, sts2::Creature dealer);
sts2::RoomTypeSet ModifyUnknownMapPointRoomTypes(sts2::RunState runState, sts2::RoomTypeSet roomTypes);
int32_t ModifyXValue(sts2::CombatState combatState, sts2::CardModel card, int32_t originalValue);
bool ShouldAddToDeck(sts2::RunState runState, sts2::CardModel card, sts2::AbstractModel& preventer);
bool ShouldAfflict(sts2::CombatState combatState, sts2::CardModel card, sts2::AfflictionModel affliction);
bool ShouldAllowHitting(sts2::CombatState combatState, sts2::Creature creature);
bool ShouldAllowMerchantCardRemoval(sts2::RunState runState, sts2::Player player);
bool ShouldAllowSelectingMoreCardRewards(sts2::RunState runState, sts2::Player player, sts2::Reward reward);
bool ShouldAllowTargeting(sts2::CombatState combatState, sts2::Creature target, sts2::AbstractModel& preventer);
bool ShouldClearBlock(sts2::CombatState combatState, sts2::Creature creature, sts2::AbstractModel& preventer);
bool ShouldCreatureBeRemovedFromCombatAfterDeath(sts2::CombatState combatState, sts2::Creature creature);
bool ShouldDie(sts2::RunState runState, sts2::CombatState combatState, sts2::Creature creature, sts2::AbstractModel& preventer);
bool ShouldDisableRemainingRestSiteOptions(sts2::RunState runState, sts2::Player player);
bool ShouldDraw(sts2::CombatState combatState, sts2::Player player, bool fromHandDraw, sts2::AbstractModel& modifier);
bool ShouldEtherealTrigger(sts2::CombatState combatState, sts2::CardModel card);
bool ShouldFlush(sts2::CombatState combatState, sts2::Player player);
bool ShouldGenerateTreasure(sts2::RunState runState, sts2::Player player);
bool ShouldGainStars(sts2::CombatState combatState, sts2::Dec amount, sts2::Player player);
bool ShouldPayExcessEnergyCostWithStars(sts2::CombatState combatState, sts2::Player player);
bool ShouldPlay(sts2::CombatState combatState, sts2::CardModel card, sts2::AbstractModel& preventer, sts2::AutoPlayType autoPlayType);
bool ShouldPlayerResetEnergy(sts2::CombatState combatState, sts2::Player player);
bool ShouldProceedToNextMapPoint(sts2::RunState runState);
bool ShouldProcurePotion(sts2::RunState runState, sts2::CombatState combatState, sts2::PotionModel potion, sts2::Player player);
bool ShouldRefillMerchantEntry(sts2::RunState runState, sts2::Merchant::EntryRef entry, sts2::Player player);
bool ShouldStopCombatFromEnding(sts2::CombatState combatState);
bool ShouldTakeExtraTurn(sts2::CombatState combatState, sts2::Player player);
bool ShouldForcePotionReward(sts2::RunState runState, sts2::Player player, sts2::RoomType roomType);
bool ShouldAllowFreeTravel(sts2::RunState runState);
bool ShouldPowerBeRemovedOnDeath(sts2::PowerModel power);

}  // namespace Hook
}  // namespace sts2

// Not declared yet (a C# type in the signature has no C++ port):
//   IEnumerable<AbstractModel> ModifyCardRewardAlternatives(IRunState runState, Player player, CardReward cardReward, List<CardRewardAlternative> alternatives)
//   IReadOnlyList<LocString> ModifyExtraRestSiteHealText(IRunState runState, Player player, IReadOnlyList<LocString> extraText)
//   IEnumerable<CardModel> ModifyMerchantCardPool(IRunState runState, Player player, IEnumerable<CardModel> options)
//   IEnumerable<AbstractModel> ModifyRestSiteOptions(IRunState runState, Player player, ICollection<RestSiteOption> options)
//   bool ShouldAllowAncient(IRunState runState, Player player, AncientEventModel ancient)
