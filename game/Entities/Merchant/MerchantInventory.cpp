// See MerchantInventory.h.
#include "game/Entities/Merchant/MerchantInventory.h"

#include <cmath>
#include <array>

#include "engine/sim/alloc.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/PotionCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Factories/PotionFactory.h"
#include "game/Factories/RelicFactory.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Hooks/Hook.h"
#include "game/Models/Canonical.h"

namespace sts2::Merchant {

namespace {

// MerchantInventory._coloredCardTypes / _colorlessCardRarities
constexpr CardType kColoredCardTypes[5] = {CardType::Attack, CardType::Attack, CardType::Skill, CardType::Skill, CardType::Power};
constexpr CardRarity kColorlessCardRarities[2] = {CardRarity::Uncommon, CardRarity::Rare};

MerchantState& Shop(Sim* sim) { return sim->g->run.merchant; }

bool InMerchantRoom(Sim* sim) {
    const auto& rooms = sim->g->run.rooms;
    return !rooms.empty() && rooms.back() == RoomType::Shop;
}

MerchantCardEntryState& CardEntry(Sim* sim, EntryRef e) {
    MerchantState& s = Shop(sim);
    return e.kind == EntryRef::CharacterCard ? s.character_cards[e.index] : s.colorless_cards[e.index];
}

// Mathf.RoundToInt(float) / Mathf.Round(float): MathF.Round, to even.
int32_t RoundToInt(float f) { return static_cast<int32_t>(std::nearbyint(f)); }

// ---------------------------------------------------------------- card entry

// MerchantCardEntry.GetCost(card)
int32_t GetCardCost(CardModel card) {
    int32_t num;
    switch (card.Rarity()) {
    case CardRarity::Rare: num = 150; break;
    case CardRarity::Uncommon: num = 75; break;
    default: num = 50; break;
    }
    if (card.Pool() == CardPoolId::COLORLESS_CARD_POOL) num = RoundToInt(static_cast<float>(num) * 1.15f);
    return num;
}

void CalcCardCost(Sim* sim, MerchantCardEntryState& e) {
    if (e.card == 0) throw GameError("There is no item to purchase.");
    Player player(sim, 1);
    float jitter = player.PlayerRng().Shops().NextFloat(0.95f, 1.05f);
    e.cost = RoundToInt(static_cast<float>(GetCardCost(CardModel(sim, e.card))) * jitter);
    if (e.on_sale) e.cost /= 2;
}

// Hook.ModifyMerchantCardCreationResults(runState, player, [CreationResult])
void ModifyCreationResult(Sim* sim, MerchantCardEntryState& e) {
    Player player(sim, 1);
    CardList list;
    list.push_back(CardModel(sim, e.card));
    Hook::ModifyMerchantCardCreationResults(player.RunState(), player, list);
    e.card = list[0].Index();
}

// The canonical cards of the inventory's card entries (CreationResult?.Card.CanonicalInstance).
StaticVec<CardId, 8> StockedCanonicalCards(Sim* sim) {
    StaticVec<CardId, 8> out;
    MerchantState& s = Shop(sim);
    auto add = [&](const MerchantCardEntryState& e) {
        if (e.card == 0) return;
        CardId id = CardModel(sim, e.card).CardId_();
        if (!out.contains(id)) out.push_back(id);
    };
    for (const auto& e : s.character_cards) add(e);
    for (const auto& e : s.colorless_cards) add(e);
    return out;
}

// MerchantCardEntry.Populate
void PopulateCard(Sim* sim, MerchantCardEntryState& e) {
    Player player(sim, 1);
    StaticVec<CardId, 8> second = StockedCanonicalCards(sim);
    CardPoolId pool = e.by_type ? data(player.Character()).card_pool : CardPoolId::COLORLESS_CARD_POOL;
    // _cardPool.Except(second)
    StaticVec<CardId, 640> options;
    for (CardId c : CardFactory::GetUnlockedCards(pool, player.st().profile.unlocked_epoch_bits)) {
        if (!second.contains(c) && !options.contains(c)) options.push_back(c);
    }
    CardModel card = e.by_type ? CardFactory::CreateForMerchant(player, options, e.type)
                               : CardFactory::CreateForMerchant(player, options, e.rarity);
    e.card = card.Index();
    ModifyCreationResult(sim, e);
    CalcCardCost(sim, e);
}

// ------------------------------------------------------- relic / potion entry

void CalcRelicCost(Sim* sim, MerchantRelicEntryState& e) {
    Player player(sim, 1);
    float jitter = player.PlayerRng().Shops().NextFloat(0.85f, 1.15f);
    // (int)Math.Round((float)Model.MerchantCost * jitter)
    e.cost = static_cast<int32_t>(std::nearbyint(static_cast<double>(static_cast<float>(RelicModel(sim, e.relic).MerchantCost()) * jitter)));
}

// MerchantRelicEntry.FillSlot(rarity, blacklist)
void FillRelicSlot(Sim* sim, MerchantRelicEntryState& e, RelicRarity rarity, const StaticVec<RelicId, 8>* blacklist) {
    Player player(sim, 1);
    RelicId relic = RelicFactory::PullNextRelicFromBack(player, rarity, [sim, blacklist](RelicId r) {
        if (blacklist != nullptr && blacklist->contains(r)) return false;
        CanonicalRelic c(sim, r);
        return c.get().IsAllowedInShops();
    });
    e.relic = alloc_relic(*sim->g, relic, 0);  // ToMutable
    CalcRelicCost(sim, e);                     // SetModel
}

// MerchantPotionEntry.GetCost(rarity) / CalcCost (the TestMode branch is the oracle's, restored)
void CalcPotionCost(Sim* sim, MerchantPotionEntryState& e) {
    if (e.potion == 0) throw GameError("There is no item to purchase.");
    int32_t cost;
    switch (PotionModel(sim, e.potion).Rarity()) {
    case PotionRarity::Rare: cost = 100; break;
    case PotionRarity::Uncommon: cost = 75; break;
    default: cost = 50; break;
    }
    Player player(sim, 1);
    float jitter = player.PlayerRng().Shops().NextFloat(0.95f, 1.05f);
    e.cost = static_cast<int32_t>(std::nearbyint(static_cast<float>(cost) * jitter));
}

// MerchantCardRemovalEntry.CalcCost
void CalcRemovalCost(Sim* sim) {
    int32_t baseCost = AscensionHelper::GetValueIfAscension(*sim, AscensionLevel::Inflation, 100, 75);
    int32_t priceIncrease = AscensionHelper::GetValueIfAscension(*sim, AscensionLevel::Inflation, 50, 25);
    Shop(sim).card_removal_cost = baseCost + priceIncrease * sim->g->run.player.card_shop_removals_used;
}

// ------------------------------------------------------------------ purchase

// MerchantInventory.UpdateEntries: every entry's OnMerchantInventoryUpdated
// (UpdateEntry: the card entries re-run the creation results hook).
void UpdateEntries(Sim* sim) {
    for (EntryRef e : AllEntries(sim)) {
        if (e.kind != EntryRef::CharacterCard && e.kind != EntryRef::ColorlessCard) continue;
        MerchantCardEntryState& c = CardEntry(sim, e);
        if (c.card != 0) ModifyCreationResult(sim, c);
    }
}

// OnTryPurchase of each entry kind: (success, goldSpent).
std::pair<bool, int32_t> OnTryPurchase(Sim* sim, EntryRef e, bool ignoreCost = false, bool cancelable = true) {
    Player player(sim, 1);
    int32_t cost = ignoreCost ? 0 : Cost(sim, e);
    switch (e.kind) {
    case EntryRef::CharacterCard:
    case EntryRef::ColorlessCard: {
        CardModel card(sim, CardEntry(sim, e).card);
        if (!CardPileCmd::Add(card, PileType::Deck).success) return {false, 0};  // FailureSpace
        if (!ignoreCost) PlayerCmd::LoseGold(Dec(cost), player, GoldLossType::Spent);
        // RewardSynchronizer.SyncLocal*, map point history: network and history only.
        return {true, cost};
    }
    case EntryRef::Relic: {
        if (!ignoreCost) PlayerCmd::LoseGold(Dec(cost), player, GoldLossType::Spent);
        RelicCmd::Obtain(RelicModel(sim, Shop(sim).relics[e.index].relic), player);
        return {true, cost};
    }
    case EntryRef::Potion: {
        PotionModel potion(sim, Shop(sim).potions[e.index].potion);
        if (!PotionCmd::TryToProcure(potion, player).success) return {false, 0};  // FailureForbidden / FailureSpace
        if (!ignoreCost) PlayerCmd::LoseGold(Dec(cost), player, GoldLossType::Spent);
        return {true, cost};
    }
    case EntryRef::CardRemoval: {
        if (Shop(sim).card_removal_used) return {false, 0};
        // OneOffSynchronizer.DoMerchantCardRemoval(player, goldToSpend, cancelable: true)
        CardSelectorPrefs prefs(1);  // (RemoveSelectionPrompt, 1)
        prefs.Cancelable = cancelable;
        prefs.RequireManualConfirmation = true;
        CardList selected = CardSelectCmd::FromDeckForRemoval(player, prefs);
        if (selected.empty()) return {false, cost};
        if (!ignoreCost) PlayerCmd::LoseGold(Dec(cost), player, GoldLossType::Spent);
        CardPileCmd::RemoveFromDeck(selected[0]);
        ++sim->g->run.player.card_shop_removals_used;
        Shop(sim).card_removal_used = true;  // NMerchantInventory.OnCardRemovalUsed
        return {true, cost};
    }
    }
    return {false, 0};
}

void ClearAfterPurchase(Sim* sim, EntryRef e) {
    switch (e.kind) {
    case EntryRef::CharacterCard:
    case EntryRef::ColorlessCard: CardEntry(sim, e).card = 0; break;
    case EntryRef::Relic: Shop(sim).relics[e.index].relic = 0; break;
    case EntryRef::Potion: Shop(sim).potions[e.index].potion = 0; break;
    case EntryRef::CardRemoval: break;
    }
}

// MerchantEntry.RestockAfterPurchase: invoked only when a run hook requests a
// refill. Card removal is a separate entry type and its callback is empty.
void RestockAfterPurchase(Sim* sim, EntryRef e) {
    Player player(sim, 1);
    MerchantState& s = Shop(sim);
    switch (e.kind) {
    case EntryRef::CharacterCard:
    case EntryRef::ColorlessCard: {
        MerchantCardEntryState& card = CardEntry(sim, e);
        card.on_sale = false;
        PopulateCard(sim, card);
        break;
    }
    case EntryRef::Relic: {
        StaticVec<RelicId, 8> blacklist;
        for (const MerchantRelicEntryState& entry : s.relics) {
            if (!entry.relic) continue;
            RelicId id = RelicModel(sim, entry.relic).RelicId_();
            if (!blacklist.contains(id)) blacklist.push_back(id);
        }
        FillRelicSlot(sim, s.relics[e.index], RelicFactory::RollRarity(player), &blacklist);
        break;
    }
    case EntryRef::Potion: {
        // MerchantPotionEntry computes a canonical blacklist but passes an
        // empty sequence to FillSlot in v0.111.0, so duplicates remain legal.
        PotionId potion = PotionFactory::CreateRandomPotionOutOfCombat(player, player.PlayerRng().Shops());
        s.potions[e.index].potion = alloc_potion(*sim->g, potion, 0);
        CalcPotionCost(sim, s.potions[e.index]);
        break;
    }
    case EntryRef::CardRemoval: break;
    }
}

}  // namespace

void CreateForNormalMerchant(Sim* sim) {
    Player player(sim, 1);
    MerchantState& s = Shop(sim);
    s = MerchantState{};
    s.exists = true;
    // PopulateCharacterCardEntries
    int32_t num = player.PlayerRng().Shops().NextInt(5);
    for (int32_t i = 0; i < 5; ++i) {
        MerchantCardEntryState e;
        e.by_type = true;
        e.type = kColoredCardTypes[i];
        PopulateCard(sim, e);
        Shop(sim).character_cards.push_back(e);
        if (num == i) {
            MerchantCardEntryState& added = Shop(sim).character_cards.back();
            added.on_sale = true;  // SetOnSale
            CalcCardCost(sim, added);
        }
    }
    // PopulateColorlessCardEntries
    for (CardRarity rarity : kColorlessCardRarities) {
        MerchantCardEntryState e;
        e.rarity = rarity;
        PopulateCard(sim, e);
        Shop(sim).colorless_cards.push_back(e);
    }
    // PopulateRelicEntries: the rarities are rolled first.
    RelicRarity rarities[3] = {RelicFactory::RollRarity(player), RelicFactory::RollRarity(player), RelicRarity::Shop};
    for (RelicRarity rarity : rarities) {
        MerchantRelicEntryState e;
        FillRelicSlot(sim, e, rarity, nullptr);
        Shop(sim).relics.push_back(e);
    }
    // PopulatePotionEntries
    for (PotionId p : PotionFactory::CreateRandomPotionsOutOfCombat(player, 3, player.PlayerRng().Shops())) {
        MerchantPotionEntryState e;
        e.potion = alloc_potion(*sim->g, p, 0);  // ToMutable
        CalcPotionCost(sim, e);
        Shop(sim).potions.push_back(e);
    }
    // CardRemovalEntry
    Shop(sim).has_card_removal = true;
    CalcRemovalCost(sim);
    // NMerchantCardRemoval.FillSlot (when the shop's inventory is shown).
    if (!Hook::ShouldAllowMerchantCardRemoval(player.RunState(), player)) Shop(sim).card_removal_used = true;
}

void CreateForFakeMerchant(Sim* sim, Rng& eventRng) {
    static constexpr std::array<RelicId, 9> inventory = {
        RelicId::FAKE_ANCHOR, RelicId::FAKE_BLOOD_VIAL, RelicId::FAKE_HAPPY_FLOWER,
        RelicId::FAKE_LEES_WAFFLE, RelicId::FAKE_MANGO, RelicId::FAKE_ORICHALCUM,
        RelicId::FAKE_SNECKO_EYE, RelicId::FAKE_STRIKE_DUMMY, RelicId::FAKE_VENERABLE_TEA_SET};
    auto shuffled = inventory;
    eventRng.shuffle(std::span<RelicId>(shuffled.data(), shuffled.size()));  // ToList().UnstableShuffle(Rng).Take(6)
    MerchantState& shop = Shop(sim);
    shop = MerchantState{};
    shop.exists = true;
    for (size_t i = 0; i < 6; ++i) {
        MerchantRelicEntryState entry;
        entry.relic = alloc_relic(*sim->g, shuffled[i], 0);
        CalcRelicCost(sim, entry);  // MerchantRelicEntry(model, player)
        Shop(sim).relics.push_back(entry);
    }
}

StaticVec<EntryRef, 16> AllEntries(Sim* sim) {
    StaticVec<EntryRef, 16> out;
    MerchantState& s = Shop(sim);
    for (size_t i = 0; i < s.character_cards.size(); ++i) out.push_back({EntryRef::CharacterCard, static_cast<uint8_t>(i)});
    for (size_t i = 0; i < s.colorless_cards.size(); ++i) out.push_back({EntryRef::ColorlessCard, static_cast<uint8_t>(i)});
    for (size_t i = 0; i < s.relics.size(); ++i) out.push_back({EntryRef::Relic, static_cast<uint8_t>(i)});
    for (size_t i = 0; i < s.potions.size(); ++i) out.push_back({EntryRef::Potion, static_cast<uint8_t>(i)});
    if (s.has_card_removal) out.push_back({EntryRef::CardRemoval, 0});
    return out;
}

bool IsStocked(Sim* sim, EntryRef e) {
    MerchantState& s = Shop(sim);
    switch (e.kind) {
    case EntryRef::CharacterCard:
    case EntryRef::ColorlessCard: return CardEntry(sim, e).card != 0;
    case EntryRef::Relic: return s.relics[e.index].relic != 0;
    case EntryRef::Potion: return s.potions[e.index].potion != 0;
    case EntryRef::CardRemoval: return !s.card_removal_used;
    }
    return false;
}

int32_t Cost(Sim* sim, EntryRef e) {
    MerchantState& s = Shop(sim);
    int32_t cost = 0;
    switch (e.kind) {
    case EntryRef::CharacterCard:
    case EntryRef::ColorlessCard: cost = CardEntry(sim, e).cost; break;
    case EntryRef::Relic: cost = s.relics[e.index].cost; break;
    case EntryRef::Potion: cost = s.potions[e.index].cost; break;
    case EntryRef::CardRemoval: cost = s.card_removal_cost; break;
    }
    if (InMerchantRoom(sim)) {
        Player player(sim, 1);
        cost = Hook::ModifyMerchantPrice(player.RunState(), player, e, Dec(cost)).to_int();
    }
    return cost;
}

bool EnoughGold(Sim* sim, EntryRef e) { return Cost(sim, e) <= Player(sim, 1).Gold(); }

bool TryPurchase(Sim* sim, EntryRef e, bool ignoreCost, bool cancelable) {
    if (e.kind == EntryRef::CardRemoval) {
        // MerchantCardRemovalEntry.OnTryPurchaseWrapper(inventory, ignoreCost, cancelable)
        if (!ignoreCost && !EnoughGold(sim, e)) return false;  // FailureGold
        auto [success, goldSpent] = OnTryPurchase(sim, e, ignoreCost, cancelable);
        if (success) {
            Hook::AfterItemPurchased(Player(sim, 1).RunState(), Player(sim, 1), e, goldSpent);
            UpdateEntries(sim);  // InvokePurchaseCompleted
        }
        return success;
    }
    // MerchantEntry.OnTryPurchaseWrapper(inventory, ignoreCost: false)
    if (!IsStocked(sim, e)) return false;  // FailureOutOfStock
    if (!ignoreCost && !EnoughGold(sim, e)) return false;  // FailureGold
    auto [success, goldSpent] = OnTryPurchase(sim, e, ignoreCost, cancelable);
    if (success) {
        bool refill = false;
        if (InMerchantRoom(sim)) {
            Player player(sim, 1);
            refill = Hook::ShouldRefillMerchantEntry(player.RunState(), e, player);
        }
        if (refill) RestockAfterPurchase(sim, e);
        else ClearAfterPurchase(sim, e);
        Player player(sim, 1);
        Hook::AfterItemPurchased(player.RunState(), player, e, goldSpent);
        UpdateEntries(sim);  // InvokePurchaseCompleted
    }
    (void)goldSpent;
    return success;
}

}  // namespace sts2::Merchant
