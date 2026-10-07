// Models/Events/Reflections.cs (v0.111.0).
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct Reflections : EventModel {
    SIM_MODEL(Reflections, EventModel)

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(Reflections, TouchAMirror),
                                   InitialOptionKey("TOUCH_A_MIRROR")));
        list.push_back(EventOption(SIM_OPTION(Reflections, Shatter), InitialOptionKey("SHATTER")));
        return list;
    }

    void OnRoomEnter() const {}  // MirrorVfx is presentation only.

    void TouchAMirror(const EventOptionState&) const {
        CardList upgradedCards;
        for (CardModel card : Owner().Deck().Cards()) {
            if (card.CurrentUpgradeLevel() > 0) upgradedCards.push_back(card);
        }
        for (int i = 0; i < 2; ++i) {
            if (upgradedCards.empty()) break;
            CardModel card = Rng().NextItem(upgradedCards);
            upgradedCards.remove(card);
            CardCmd::Downgrade(card);
        }

        CardList upgradableCards;
        for (CardModel card : Owner().Deck().Cards()) {
            if (card.IsUpgradable()) upgradableCards.push_back(card);
        }
        for (int i = 0; i < 4; ++i) {
            if (upgradableCards.empty()) break;
            CardModel card = Rng().NextItem(upgradableCards);
            upgradableCards.remove(card);
            CardCmd::Upgrade(card);
        }

        SetEventFinished(L10NLookup("REFLECTIONS.pages.TOUCH_A_MIRROR.description"));
    }

    void Shatter(const EventOptionState&) const {
        const size_t originalDeckSize = Owner().Deck().Cards().size();
        for (size_t i = 0; i < originalDeckSize; ++i) {
            CardModel clone = Owner().RunState().CloneCard(Owner().Deck().Cards()[i]);
            CardPileCmd::Add(clone, PileType::Deck);
        }
        CardPileCmd::AddCurseToDeck(CardId::BAD_LUCK, Owner());
        SetEventFinished(L10NLookup("REFLECTIONS.pages.SHATTER.description"));
    }
};

}  // namespace sts2
