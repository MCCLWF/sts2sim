// Models/Events/ZenWeaver.cs (v0.111.0).
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct ZenWeaver : EventModel {
    SIM_MODEL(ZenWeaver, EventModel)

    EventOptionList GenerateInitialOptions() const {
        EventOptionList options;
        options.push_back(EventOption(SIM_OPTION(ZenWeaver, BreathingTechniques),
                                      "ZEN_WEAVER.pages.INITIAL.options.BREATHING_TECHNIQUES"));
        options.push_back(Owner().Gold() < DynamicVars()["EmotionalAwarenessCost"].IntValue()
                              ? EventOption(0, "ZEN_WEAVER.pages.INITIAL.options.LOCKED")
                              : EventOption(SIM_OPTION(ZenWeaver, EmotionalAwareness),
                                            "ZEN_WEAVER.pages.INITIAL.options.EMOTIONAL_AWARENESS"));
        options.push_back(Owner().Gold() < DynamicVars()["ArachnidAcupunctureCost"].IntValue()
                              ? EventOption(0, "ZEN_WEAVER.pages.INITIAL.options.LOCKED")
                              : EventOption(SIM_OPTION(ZenWeaver, ArachnidAcupuncture),
                                            "ZEN_WEAVER.pages.INITIAL.options.ARACHNID_ACUPUNCTURE"));
        return options;
    }

    void BreathingTechniques(const EventOptionState&) const {
        PlayerCmd::LoseGold(DynamicVars()["BreathingTechniquesCost"].BaseValue(), Owner(), GoldLossType::Spent);
        CardList cards;
        for (int i = 0; i < 2; ++i)
            cards.push_back(Owner().RunState().CreateCard(CardId::ENLIGHTENMENT, Owner()));
        CardPileCmd::Add(cards, PileType::Deck);
        SetEventFinished(L10NLookup("ZEN_WEAVER.pages.BREATHING_TECHNIQUES.description"));
    }

    void EmotionalAwareness(const EventOptionState&) const {
        RemoveCardsAndProceed(DynamicVars()["EmotionalAwarenessCost"].IntValue(), 1);
        SetEventFinished(L10NLookup("ZEN_WEAVER.pages.EMOTIONAL_AWARENESS.description"));
    }

    void ArachnidAcupuncture(const EventOptionState&) const {
        RemoveCardsAndProceed(DynamicVars()["ArachnidAcupunctureCost"].IntValue(), 2);
        SetEventFinished(L10NLookup("ZEN_WEAVER.pages.ARACHNID_ACUPUNCTURE.description"));
    }

    void RemoveCardsAndProceed(int cost, int count) const {
        CardList cards = CardSelectCmd::FromDeckForRemoval(Owner(), CardSelectorPrefs(count));
        CardPileCmd::RemoveFromDeck(cards);
        PlayerCmd::LoseGold(Dec(cost), Owner(), GoldLossType::Spent);
    }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const {
        return all_players(runState, [this](Player p) {
            return Dec(p.Gold()) >= DynamicVars()["EmotionalAwarenessCost"].BaseValue();
        });
    }
};

}  // namespace sts2
