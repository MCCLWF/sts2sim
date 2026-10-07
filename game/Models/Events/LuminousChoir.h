// Models/Events/LuminousChoir.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Factories/RelicFactory.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct LuminousChoir : EventModel {
    SIM_MODEL(LuminousChoir, EventModel)

    bool IsAllowed(RunState runState) const {
        for (Player p : runState.Players()) {
            if (Dec(p.Gold()) < DynamicVars().Gold().BaseValue()) return false;
            if (!event_allowed::HasAvailableRelics(runState, p)) return false;
        }
        return true;
    }

    void CalculateVars() const {
        DynamicVars().Gold().SetBaseValue(DynamicVars().Gold().BaseValue() - Dec(Rng().NextInt(0, 50)));
    }

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(LuminousChoir, ReachIntoTheFlesh),
                                   "LUMINOUS_CHOIR.pages.INITIAL.options.REACH_INTO_THE_FLESH"));
        if (Owner().Gold() >= DynamicVars().Gold().IntValue()) {
            list.push_back(EventOption(SIM_OPTION(LuminousChoir, OfferTribute),
                                       "LUMINOUS_CHOIR.pages.INITIAL.options.OFFER_TRIBUTE"));
        } else {
            list.push_back(EventOption(0, "LUMINOUS_CHOIR.pages.INITIAL.options.OFFER_TRIBUTE_LOCKED"));
        }
        return list;
    }

    void ReachIntoTheFlesh(const EventOptionState&) const {
        CardList cards = CardSelectCmd::FromDeckForRemoval(Owner(), CardSelectorPrefs(2));
        CardPileCmd::RemoveFromDeck(cards);
        CardPileCmd::Add(Owner().RunState().CreateCard(CardId::SPORE_MIND, Owner()), PileType::Deck);
        SetEventFinished(L10NLookup("LUMINOUS_CHOIR.pages.REACH_INTO_THE_FLESH.description"));
    }

    void OfferTribute(const EventOptionState&) const {
        PlayerCmd::LoseGold(DynamicVars().Gold().IntValue(), Owner(), GoldLossType::Spent);
        RelicCmd::Obtain(RelicFactory::PullNextRelicFromFront(Owner()), Owner());
        SetEventFinished(L10NLookup("LUMINOUS_CHOIR.pages.OFFER_TRIBUTE.description"));
    }
};

}  // namespace sts2
