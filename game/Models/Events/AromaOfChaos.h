#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/EventModel.h"

namespace sts2 {

struct AromaOfChaos : EventModel {
    SIM_MODEL(AromaOfChaos, EventModel)
    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(AromaOfChaos, LetGo), InitialOptionKey("LET_GO")));
        list.push_back(EventOption(SIM_OPTION(AromaOfChaos, MaintainControl), InitialOptionKey("MAINTAIN_CONTROL")));
        return list;
    }
    void LetGo(const EventOptionState&) const {
        CardList cards = CardSelectCmd::FromDeckForTransformation(Owner(), CardSelectorPrefs(1));
        if (!cards.empty()) CardCmd::TransformToRandom(cards[0], Rng());
        SetEventFinished(L10NLookup("AROMA_OF_CHAOS.pages.LET_GO.description"));
    }
    void MaintainControl(const EventOptionState&) const {
        CardList cards = CardSelectCmd::FromDeckForUpgrade(Owner(), CardSelectorPrefs(1));
        if (!cards.empty()) CardCmd::Upgrade(cards[0]);
        SetEventFinished(L10NLookup("AROMA_OF_CHAOS.pages.MAINTAIN_CONTROL.description"));
    }
};

}  // namespace sts2
