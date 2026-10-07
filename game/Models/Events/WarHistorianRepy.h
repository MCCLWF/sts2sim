// Models/Events/WarHistorianRepy.cs (v0.111.0).
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Commands/RewardsCmd.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct WarHistorianRepy : EventModel {
    SIM_MODEL(WarHistorianRepy, EventModel)

    bool IsShared() const { return true; }
    bool IsAllowed(RunState) const { return false; }

    EventOptionList GenerateInitialOptions() const {
        return EventOptionList{
            EventOption(SIM_OPTION(WarHistorianRepy, InitialUnlockCage), InitialOptionKey("UNLOCK_CAGE")),
            EventOption(SIM_OPTION(WarHistorianRepy, InitialUnlockChest), InitialOptionKey("UNLOCK_CHEST"))};
    }

    void InitialUnlockCage(const EventOptionState&) const {
        RemoveFirstLanternKey();
        UnlockCage();
        if (HasAnotherLanternKey()) {
            SetEventState(L10NLookup("WAR_HISTORIAN_REPY.pages.UNLOCK_CAGE.description"),
                          EventOptionList{EventOption(SIM_OPTION(WarHistorianRepy, SecondUnlockChest),
                                                      InitialOptionKey("UNLOCK_CHEST"))});
        } else {
            SetEventFinished(L10NLookup("WAR_HISTORIAN_REPY.pages.UNLOCK_CAGE.description"));
        }
    }

    void InitialUnlockChest(const EventOptionState&) const {
        RemoveFirstLanternKey();
        UnlockChest();
        if (HasAnotherLanternKey()) {
            SetEventState(L10NLookup("WAR_HISTORIAN_REPY.pages.UNLOCK_CHEST.description"),
                          EventOptionList{EventOption(SIM_OPTION(WarHistorianRepy, SecondUnlockCage),
                                                      InitialOptionKey("UNLOCK_CAGE"))});
        } else {
            SetEventFinished(L10NLookup("WAR_HISTORIAN_REPY.pages.UNLOCK_CHEST.description"));
        }
    }

    void SecondUnlockCage(const EventOptionState&) const {
        SetEventFinished(L10NLookup("WAR_HISTORIAN_REPY.pages.EXTRA_UNLOCK_CAGE.description"));
        RemoveRemainingLanternKeys();
        UnlockCage();
    }

    void SecondUnlockChest(const EventOptionState&) const {
        SetEventFinished(L10NLookup("WAR_HISTORIAN_REPY.pages.EXTRA_UNLOCK_CHEST.description"));
        RemoveRemainingLanternKeys();
        UnlockChest();
    }

private:
    bool HasAnotherLanternKey() const {
        for (CardModel card : Owner().Deck().Cards())
            if (card.CardId_() == CardId::LANTERN_KEY) return true;
        return false;
    }

    void RemoveFirstLanternKey() const {
        for (CardModel card : Owner().Deck().Cards()) {
            if (card.CardId_() != CardId::LANTERN_KEY) continue;
            // PlayerCmd.CompleteQuest only appends run-history presentation.
            CardPileCmd::RemoveFromDeck(card);
            break;
        }
    }

    void RemoveRemainingLanternKeys() const {
        CardList cards;
        for (CardModel card : Owner().Deck().Cards())
            if (card.CardId_() == CardId::LANTERN_KEY) cards.push_back(card);
        // CompleteQuest records each card before removal in the original.
        CardPileCmd::RemoveFromDeck(cards);
    }

    void UnlockCage() const {
        sim_->g->run.freed_repy = true;
        RelicCmd::Obtain(RelicId::HISTORY_COURSE, Owner());
    }

    void UnlockChest() const {
        StaticVec<Reward, 8> rewards;
        rewards.push_back(Reward::NewPotionReward(Owner()));
        rewards.push_back(Reward::NewPotionReward(Owner()));
        rewards.push_back(Reward::NewRelicReward(Owner()));
        rewards.push_back(Reward::NewRelicReward(Owner()));
        RewardsCmd::OfferCustom(Owner(), rewards);
    }
};

}  // namespace sts2
