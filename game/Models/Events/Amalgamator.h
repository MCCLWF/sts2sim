// Models/Events/Amalgamator.cs (v0.111.0).
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct Amalgamator : EventModel {
    SIM_MODEL(Amalgamator, EventModel)

    EventOptionList GenerateInitialOptions() const {
        EventOptionList options;
        options.push_back(EventOption(SIM_OPTION(Amalgamator, CombineStrikes), InitialOptionKey("COMBINE_STRIKES")));
        options.push_back(EventOption(SIM_OPTION(Amalgamator, CombineDefends), InitialOptionKey("COMBINE_DEFENDS")));
        return options;
    }

    void CombineStrikes(const EventOptionState&) const {
        CardList cards = CardSelectCmd::FromDeckForRemoval(Owner(), CardSelectorPrefs(2), IsValidStrike);
        CardPileCmd::RemoveFromDeck(cards);
        CardPileCmd::Add(Owner().RunState().CreateCard(CardId::ULTIMATE_STRIKE, Owner()), PileType::Deck);
        SetEventFinished(L10NLookup("AMALGAMATOR.pages.COMBINE_STRIKES.description"));
    }

    void CombineDefends(const EventOptionState&) const {
        CardList cards = CardSelectCmd::FromDeckForRemoval(Owner(), CardSelectorPrefs(2), IsValidDefend);
        CardPileCmd::RemoveFromDeck(cards);
        CardPileCmd::Add(Owner().RunState().CreateCard(CardId::ULTIMATE_DEFEND, Owner()), PileType::Deck);
        SetEventFinished(L10NLookup("AMALGAMATOR.pages.COMBINE_DEFENDS.description"));
    }

    static bool IsValidStrike(CardModel card) {
        return card.HasTag(CardTag::Strike) && card.Rarity() == CardRarity::Basic && card.IsRemovable();
    }
    static bool IsValidDefend(CardModel card) {
        return card.HasTag(CardTag::Defend) && card.Rarity() == CardRarity::Basic && card.IsRemovable();
    }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const {
        return all_players(runState, [](Player p) {
            int strikes = 0, defends = 0;
            for (CardModel c : p.Deck().Cards()) {
                if (IsValidStrike(c)) ++strikes;
                if (IsValidDefend(c)) ++defends;
            }
            return strikes >= 2 && defends >= 2;
        });
    }
};

}  // namespace sts2
