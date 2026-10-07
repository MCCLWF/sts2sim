// Models/Events/ByrdonisNest.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/EventModel.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct ByrdonisNest : EventModel {
    SIM_MODEL(ByrdonisNest, EventModel)

    bool IsAllowed(RunState runState) const {
        for (Player p : runState.Players()) if (event_allowed::HasEventPet(p)) return false;
        return true;
    }

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(ByrdonisNest, Eat),
                                   "BYRDONIS_NEST.pages.INITIAL.options.EAT"));
        list.push_back(EventOption(SIM_OPTION(ByrdonisNest, Take),
                                   "BYRDONIS_NEST.pages.INITIAL.options.TAKE"));
        return list;
    }

    void Eat(const EventOptionState&) const {
        CreatureCmd::GainMaxHp(Owner().Creature(), Dec(7));
        SetEventFinished(L10NLookup("BYRDONIS_NEST.pages.EAT.description"));
    }

    void Take(const EventOptionState&) const {
        CardPileCmd::Add(Owner().RunState().CreateCard(CardId::BYRDONIS_EGG, Owner()), PileType::Deck);
        SetEventFinished(L10NLookup("BYRDONIS_NEST.pages.TAKE.description"));
    }
};

}  // namespace sts2
