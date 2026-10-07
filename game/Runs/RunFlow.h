// What the game's UI does between the model-level calls: the map screen
// (NMapScreen: which points can be travelled to), the combat UI (offering the
// room's rewards after a victory, NCombatUi.OnCombatWon) and the rewards
// screen (NRewardsScreen: take a reward, proceed). Each player decision is one
// call; RunData.phase says which decision the run waits for.
#pragma once

#include "engine/map.h"
#include "game/All.h"
#include "game/Entities/Merchant/MerchantInventory.h"

namespace sts2::RunFlow {

// RunManager.EnterAct(0) for a new run (Neow when StartedWithNeow).
void StartRun(Sim* sim);
// The headless equivalent of the room UI exposing its next player decision.
// RunManager invokes this after ordinary act/room entry, before act hooks.
void AfterRoomEntered(Sim* sim);
// After a top-level action: the run is over when the player died outside
// combat (CreatureCmd.Kill -> RunManager.OnEnded(isVictory: false)).
void SettleGameOver(Sim* sim);

// --- RunPhase::Map
StaticVec<MapCoord, 8> MapOptions(Sim* sim);  // travelable points, by (col, row)
bool IsBossPoint(Sim* sim, MapCoord coord);
void ChooseMapPoint(Sim* sim, MapCoord coord);

// --- RunPhase::Combat
bool PlayCard(Sim* sim, int32_t handIndex, int32_t targetCombatId);  // -1: no target
void EndTurn(Sim* sim);
void UsePotion(Sim* sim, int32_t slotIndex, int32_t targetCombatId); // -1: no creature
void DiscardPotion(Sim* sim, int32_t slotIndex);

// --- RunPhase::Event
const EventOptionList& EventOptions(Sim* sim);
void ChooseEventOption(Sim* sim, int32_t index);  // EventSynchronizer.ChooseLocalOption

// --- RunPhase::RestSite (NRestSiteRoom: an enabled option; the proceed
// button once an option succeeded or none is left)
struct RestSiteOptionView {
    RestSiteOptionId id;
    bool enabled;
};
StaticVec<RestSiteOptionView, 8> RestSiteOptions(Sim* sim);
bool CanProceedFromRestSite(Sim* sim);
bool ChooseRestSiteOption(Sim* sim, int32_t index);  // RestSiteSynchronizer.ChooseLocalOption
void ProceedFromRestSite(Sim* sim);

// --- RunPhase::Treasure (NTreasureRoom). The chest is opened on entering the
// room (the gold and extra rewards); the decision is the relic.
const StaticVec<RelicId, 4>& TreasureRelics(Sim* sim);
void PickTreasureRelic(Sim* sim, int32_t index);  // -1: skip (the proceed button)

// --- RunPhase::Shop (NMerchantRoom / NMerchantInventory): the stocked
// entries, in AllEntries order; buy an affordable one, or leave.
StaticVec<Merchant::EntryRef, 16> ShopOptions(Sim* sim);
bool ShopOptionBuyable(Sim* sim, int32_t index);
bool BuyShopOption(Sim* sim, int32_t index);  // whether the purchase succeeded
void LeaveShop(Sim* sim);

// --- RunPhase::Rewards
// The screen lists the whole set (taken rewards included), in RewardsSet order.
StaticVec<Reward, 16> RewardOptions(Sim* sim);
bool RewardTakable(Sim* sim, int32_t index);  // not yet successfully selected
void TakeReward(Sim* sim, int32_t index);       // RewardsSetSynchronizer.SelectLocalReward
void ProceedFromRewards(Sim* sim);              // SkipLocalRewardsSet if rewards are left

}  // namespace sts2::RunFlow
