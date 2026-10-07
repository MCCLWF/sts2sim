// Models/Events/TheLanternKey.cs (v0.111.0).
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/EventModel.h"
#include "game/Rewards/Reward.h"
#include "game/Runs/RunManager.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct TheLanternKey : EventModel {
    SIM_MODEL(TheLanternKey, EventModel)

    EncounterId CanonicalEncounter() const { return EncounterId::MYSTERIOUS_KNIGHT_EVENT_ENCOUNTER; }
    bool IsShared() const { return true; }

    EventOptionList GenerateInitialOptions() const {
        return EventOptionList{
            EventOption(SIM_OPTION(TheLanternKey, ReturnTheKey), InitialOptionKey("RETURN_THE_KEY")),
            EventOption(SIM_OPTION(TheLanternKey, KeepTheKey), InitialOptionKey("KEEP_THE_KEY"))};
    }

    void ReturnTheKey(const EventOptionState&) const {
        PlayerCmd::GainGold(DynamicVars().Gold().BaseValue(), Owner());
        SetEventFinished(L10NLookup("THE_LANTERN_KEY.pages.DONE.options.RETURN_THE_KEY.description"));
    }

    void KeepTheKey(const EventOptionState&) const {
        SetEventState(L10NLookup("THE_LANTERN_KEY.pages.KEEP_THE_KEY.description"),
                      EventOptionList{EventOption(SIM_OPTION(TheLanternKey, Fight),
                                                  OptionKey("KEEP_THE_KEY", "FIGHT"))});
    }

    void Fight(const EventOptionState&) const {
        CardModel card = Owner().RunState().CreateCard(CardId::LANTERN_KEY, Owner());
        StaticVec<RewardRef, 8> extraRewards;
        extraRewards.push_back(Reward::NewSpecialCardReward(Owner(), card).Index());
        RunManager::EnterEventCombat(sim_, CanonicalEncounter(), extraRewards, false);
    }
};

}  // namespace sts2
