// Models/Events/WaterloggedScriptorium.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct WaterloggedScriptorium : EventModel {
    SIM_MODEL(WaterloggedScriptorium, EventModel)
    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(WaterloggedScriptorium, BloodyInk), InitialOptionKey("BLOODY_INK")));
        list.push_back(Owner().Gold() >= 55
                           ? EventOption(SIM_OPTION(WaterloggedScriptorium, TentacleQuill), InitialOptionKey("TENTACLE_QUILL"))
                           : EventOption(0, InitialOptionKey("TENTACLE_QUILL_LOCKED")));
        list.push_back(Owner().Gold() >= 99
                           ? EventOption(SIM_OPTION(WaterloggedScriptorium, PricklySponge), InitialOptionKey("PRICKLY_SPONGE"))
                           : EventOption(0, InitialOptionKey("PRICKLY_SPONGE_LOCKED")));
        return list;
    }

    void BloodyInk(const EventOptionState&) const {
        CreatureCmd::GainMaxHp(Owner().Creature(), Dec(6));
        SetEventFinished(L10NLookup("WATERLOGGED_SCRIPTORIUM.pages.BLOODY_INK.description"));
    }

    void TentacleQuill(const EventOptionState&) const {
        PlayerCmd::LoseGold(Dec(55), Owner(), GoldLossType::Spent);
        CardList selected = CardSelectCmd::FromDeckForEnchantment(Owner(), CardSelectorPrefs(1), event_allowed::BaseCanEnchant);
        for (CardModel card : selected) CardCmd::Enchant(EnchantmentId::STEADY, card, Dec(1));
        SetEventFinished(L10NLookup("WATERLOGGED_SCRIPTORIUM.pages.TENTACLE_QUILL.description"));
    }

    void PricklySponge(const EventOptionState&) const {
        PlayerCmd::LoseGold(Dec(99), Owner(), GoldLossType::Spent);
        CardList selected = CardSelectCmd::FromDeckForEnchantment(Owner(), CardSelectorPrefs(2), event_allowed::BaseCanEnchant);
        for (CardModel card : selected) CardCmd::Enchant(EnchantmentId::STEADY, card, Dec(1));
        SetEventFinished(L10NLookup("WATERLOGGED_SCRIPTORIUM.pages.PRICKLY_SPONGE.description"));
    }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const { return all_players(runState, [](Player p) { return p.Gold() >= 55; }); }
};

}  // namespace sts2
