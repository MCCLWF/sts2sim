#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct Bugslayer : EventModel {
    SIM_MODEL(Bugslayer, EventModel)

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(Bugslayer, Extermination), InitialOptionKey("EXTERMINATION")));
        list.push_back(EventOption(SIM_OPTION(Bugslayer, Squash), InitialOptionKey("SQUASH")));
        return list;
    }

    void Extermination(const EventOptionState&) const {
        CardPileCmd::Add(Owner().RunState().CreateCard(CardId::EXTERMINATE, Owner()), PileType::Deck);
        SetEventFinished(L10NLookup("BUGSLAYER.pages.EXTERMINATION.description"));
    }

    void Squash(const EventOptionState&) const {
        CardPileCmd::Add(Owner().RunState().CreateCard(CardId::SQUASH, Owner()), PileType::Deck);
        SetEventFinished(L10NLookup("BUGSLAYER.pages.SQUASH.description"));
    }
};

}  // namespace sts2
