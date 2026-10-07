// Models/Relics/PaelsTooth.cs (v0.111.0).
#pragma once

#include <algorithm>

#include "engine/sim/alloc.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct PaelsTooth : RelicModel {
    SIM_MODEL(PaelsTooth, RelicModel)

    bool HasUponPickupEffect() const { return true; }
    bool ShowCounter() const { return !st().paels_tooth_cards.empty(); }
    int32_t DisplayAmount() const { return static_cast<int32_t>(st().paels_tooth_cards.size()); }

    static bool IsEligible(CardModel card) { return card.IsUpgradable(); }

    // Match CardModel.ToSerializable/FromSerializable rather than retaining a
    // raw live CardState. SerializableCard carries card id, upgrade level,
    // enchantment, floor-added and SavedProperty fields; combat-only state is
    // rebuilt from the canonical card.
    CardRef SerializeCard(CardModel card) const {
        CardRef ref = alloc_card(*sim_->g, card.CardId_(), 0);
        CardModel saved(&sim(), ref);
        const CardState& source = card.st();
        CardState& target = saved.st();
        target.floor_added = source.floor_added;

        // These are the SavedProperty-backed card fields in v0.111.0. Most
        // live state stays at the canonical default when rehydrated.
        switch (card.CardId_()) {
        case CardId::GUILTY:
        case CardId::DOWSING:
        case CardId::MAD_SCIENCE:
        case CardId::SPOILS_MAP:
            std::copy(std::begin(source.extra), std::end(source.extra), std::begin(target.extra));
            break;
        case CardId::GENETIC_ALGORITHM:
        case CardId::THE_SCYTHE:
            target.var_overrides = source.var_overrides;
            break;
        default: break;
        }

        if (card.Enchantment())
            CardCmd::Enchant(card.Enchantment().EnchantmentId_(), saved, Dec(card.Enchantment().Amount()));
        for (int32_t i = 0; i < card.CurrentUpgradeLevel(); ++i) {
            saved.UpgradeInternal();
            saved.FinalizeUpgradeInternal();
        }
        return ref;
    }

    void AfterObtained() const {
        CardList selected = CardSelectCmd::FromDeckForRemoval(
            Owner(), CardSelectorPrefs(DynamicVars().Cards().IntValue()), IsEligible);
        // C# OrderBy(card.Id.Entry, StringComparer.Ordinal) after selection.
        std::stable_sort(selected.begin(), selected.end(), [](CardModel a, CardModel b) {
            return a.data().entry < b.data().entry;
        });
        for (CardModel card : selected) {
            CardRef saved = SerializeCard(card);
            st().paels_tooth_cards.push_back(saved);
            CardPileCmd::RemoveFromDeck(card);
        }
        RefreshStatus();
    }

    void AfterCombatEnd(CombatRoom) const {
        if (Owner().Creature().IsDead() || st().paels_tooth_cards.empty()) return;
        Flash();
        CardRef saved = Owner().PlayerRng().Rewards().NextItem(st().paels_tooth_cards);
        CardModel card(&sim(), saved);
        if (!Owner().RunState().ContainsCard(card)) Owner().RunState().AddCard(card, Owner());
        if (card.IsUpgradable()) CardCmd::Upgrade(card);
        CardPileCmd::Add(card, PileType::Deck);
        st().paels_tooth_cards.remove(saved);
        RefreshStatus();
    }

    void RefreshStatus() const {
        SetStatus(st().paels_tooth_cards.empty() ? RelicStatus::Disabled : RelicStatus::Normal);
    }
};

}  // namespace sts2
