// Models/Events/InfestedAutomaton.cs (v0.111.0).
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/EventModel.h"
#include "game/Runs/CardCreationOptions.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct InfestedAutomaton : EventModel {
    SIM_MODEL(InfestedAutomaton, EventModel)

    EventOptionList GenerateInitialOptions() const {
        EventOptionList result;
        result.push_back(EventOption(SIM_OPTION(InfestedAutomaton, Study), InitialOptionKey("STUDY")));
        result.push_back(EventOption(SIM_OPTION(InfestedAutomaton, TouchCore), InitialOptionKey("TOUCH_CORE")));
        return result;
    }

    void Study(const EventOptionState&) const {
        CardCreationOptionsState options = CardCreationOptions::ForNonCombatWithDefaultOdds(
            {sts2::data(Owner().Character()).card_pool});
        options.required_type = CardType::Power;
        CardList cards = CardFactory::CreateForReward(Owner(), 1, options);
        if (!cards.empty()) CardPileCmd::Add(cards[0], PileType::Deck);
        SetEventFinished(L10NLookup("INFESTED_AUTOMATON.pages.STUDY.description"));
    }

    void TouchCore(const EventOptionState&) const {
        CardCreationOptionsState options = CardCreationOptions::ForNonCombatWithDefaultOdds(
            {sts2::data(Owner().Character()).card_pool});
        options.filter = CardCreationOptions::ZeroCanonicalCostNonX;
        options.flags = options.flags | CardCreationFlags::NoCardPoolModifications;
        CardList cards = CardFactory::CreateForReward(Owner(), 1, options);
        if (!cards.empty()) CardPileCmd::Add(cards[0], PileType::Deck);
        SetEventFinished(L10NLookup("INFESTED_AUTOMATON.pages.TOUCH_CORE.description"));
    }
};

}  // namespace sts2
