// Models/Events/SpiralingWhirlpool.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct SpiralingWhirlpool : EventModel {
    SIM_MODEL(SpiralingWhirlpool, EventModel)

    void CalculateVars() const {
        // The source uses a HealVar of 33% of the owner's max HP.  HealVar's
        // IntValue is used by Drink, so retaining the decimal here and
        // truncating at the point of healing matches the game for odd HP.
        DynamicVars().Heal().SetBaseValue(Dec(Owner().Creature().MaxHp()) * Dec::lit(33, 2));
    }

    static bool spiralable(CardModel card) { return event_allowed::CanEnchant(card, EnchantmentId::SPIRAL); }

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(SpiralingWhirlpool, ObserveTheSpiral), InitialOptionKey("OBSERVE")));
        list.push_back(EventOption(SIM_OPTION(SpiralingWhirlpool, Drink), InitialOptionKey("DRINK")));
        return list;
    }

    void ObserveTheSpiral(const EventOptionState&) const {
        CardList cards = CardSelectCmd::FromDeckForEnchantment(Owner(), CardSelectorPrefs(1), spiralable);
        if (!cards.empty()) CardCmd::Enchant(EnchantmentId::SPIRAL, cards[0], Dec(1));
        SetEventFinished(L10NLookup("SPIRALING_WHIRLPOOL.pages.OBSERVE.description"));
    }

    void Drink(const EventOptionState&) const {
        CreatureCmd::Heal(Owner().Creature(), Dec(DynamicVars().Heal().IntValue()));
        SetEventFinished(L10NLookup("SPIRALING_WHIRLPOOL.pages.DRINK.description"));
    }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const { return all_players(runState, [](Player p) { for (CardModel c : p.Deck().Cards()) if (event_allowed::CanEnchant(c, EnchantmentId::SPIRAL)) return true; return false; }); }
};

}  // namespace sts2
