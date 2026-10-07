// Models/Events/Symbiote.cs (v0.111.0).
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/Enchantments/Corrupted.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct Symbiote : EventModel {
    SIM_MODEL(Symbiote, EventModel)

    // The source declares a CardsVar(1) in CanonicalVars. The exporter does
    // not include this event-local canonical var in content.json, so retain
    // the exact source value here until the exporter gains that case.
    static constexpr int32_t kCardsToTransform = 1;

    static bool can_enchant(CardModel card) { return Corrupted::CanEnchant(card); }

    EventOptionList GenerateInitialOptions() const {
        bool hasEnchantableCard = false;
        for (CardModel card : Owner().Deck().Cards()) {
            if (can_enchant(card)) {
                hasEnchantableCard = true;
                break;
            }
        }
        EventOptionList list;
        list.push_back(hasEnchantableCard
                           ? EventOption(SIM_OPTION(Symbiote, Approach), InitialOptionKey("APPROACH"))
                           : EventOption(0, InitialOptionKey("APPROACH_LOCKED")));
        list.push_back(EventOption(SIM_OPTION(Symbiote, KillWithFire), InitialOptionKey("KILL_WITH_FIRE")));
        return list;
    }

    void Approach(const EventOptionState&) const {
        CardList selected = CardSelectCmd::FromDeckForEnchantment(
            Owner(), CardSelectorPrefs(1), can_enchant);
        if (!selected.empty()) CardCmd::Enchant(EnchantmentId::CORRUPTED, selected[0], Dec(1));
        SetEventFinished(L10NLookup("SYMBIOTE.pages.APPROACH.description"));
    }

    void KillWithFire(const EventOptionState&) const {
        CardList selected = CardSelectCmd::FromDeckForTransformation(
            Owner(), CardSelectorPrefs(kCardsToTransform));
        for (CardModel card : selected) CardCmd::TransformToRandom(card, Rng());
        SetEventFinished(L10NLookup("SYMBIOTE.pages.KILL_WITH_FIRE.description"));
    }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const { return runState.CurrentActIndex() > 0; }
};

}  // namespace sts2
