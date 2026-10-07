// Models/Events/GraveOfTheForgotten.cs (v0.111.0).
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Models/Enchantments/SoulsPower.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct GraveOfTheForgotten : EventModel {
    SIM_MODEL(GraveOfTheForgotten, EventModel)

    static bool HasEnchantableCards(Player player) {
        for (CardModel card : player.Deck().Cards()) if (SoulsPower::CanEnchant(card)) return true;
        return false;
    }

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(HasEnchantableCards(Owner())
            ? EventOption(SIM_OPTION(GraveOfTheForgotten, Confront), InitialOptionKey("CONFRONT"))
            : EventOption(0, InitialOptionKey("CONFRONT_LOCKED")));
        list.push_back(EventOption(SIM_OPTION(GraveOfTheForgotten, Accept), InitialOptionKey("ACCEPT")));
        return list;
    }

    void Confront(const EventOptionState&) const {
        CardPileCmd::AddCurseToDeck(CardId::DECAY, Owner());
        CardList selected = CardSelectCmd::FromDeckForEnchantment(Owner(), CardSelectorPrefs(1), SoulsPower::CanEnchant);
        if (!selected.empty()) CardCmd::Enchant(EnchantmentId::SOULS_POWER, selected[0], Dec(1));
        SetEventFinished(L10NLookup("GRAVE_OF_THE_FORGOTTEN.pages.CONFRONT.description"));
    }

    void Accept(const EventOptionState&) const {
        RelicCmd::Obtain(RelicId::FORGOTTEN_SOUL, Owner());
        SetEventFinished(L10NLookup("GRAVE_OF_THE_FORGOTTEN.pages.ACCEPT.description"));
    }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const { return all_players(runState, HasEnchantableCards); }
};

}  // namespace sts2
