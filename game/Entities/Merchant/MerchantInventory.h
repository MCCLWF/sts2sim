// Entities/Merchant/{MerchantInventory,MerchantEntry,MerchantCardEntry,
// MerchantRelicEntry,MerchantPotionEntry,MerchantCardRemovalEntry}.cs, with
// the parts of the shop nodes that change the state (NMerchantCardRemoval:
// the card removal is marked used when bought, or when a hook forbids it).
// The inventory lives in RunData::merchant.
#pragma once

#include "game/All.h"

namespace sts2::Merchant {

// MerchantInventory.CreateForNormalMerchant(player), then the removal
// node's FillSlot check.
void CreateForNormalMerchant(Sim* sim);
// FakeMerchant.BeforeEventStarted: six shuffled event relics, priced by Shops RNG.
void CreateForFakeMerchant(Sim* sim, Rng& eventRng);

// AllEntries: CardEntries (character, then colorless), RelicEntries,
// PotionEntries, CardRemovalEntry.
StaticVec<EntryRef, 16> AllEntries(Sim* sim);
bool IsStocked(Sim* sim, EntryRef e);
int32_t Cost(Sim* sim, EntryRef e);  // with Hook.ModifyMerchantPrice in a merchant room
bool EnoughGold(Sim* sim, EntryRef e);

// The entry's OnTryPurchaseWrapper(inventory): whether it was bought. Relics
// such as Lord's Parasol may ignore costs and make a non-cancelable removal.
bool TryPurchase(Sim* sim, EntryRef e, bool ignoreCost = false, bool cancelable = true);

}  // namespace sts2::Merchant
