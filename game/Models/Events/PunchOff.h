// Models/Events/PunchOff.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/RewardsCmd.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunManager.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct PunchOff : EventModel {
    SIM_MODEL(PunchOff, EventModel)

    EncounterId CanonicalEncounter() const { return EncounterId::PUNCH_OFF_EVENT_ENCOUNTER; }
    bool IsShared() const { return true; }
    bool IsAllowed(RunState runState) const { return runState.TotalFloor() >= 6; }
    void CalculateVars() const { DynamicVars().Gold().SetBaseValue(Dec(Rng().NextInt(91, 99))); }
    void AfterEventStarted() const {}  // PunchEachOther animates the event layout only.

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(PunchOff, Nab), InitialOptionKey("NAB")));
        list.push_back(EventOption(SIM_OPTION(PunchOff, TakeThem), InitialOptionKey("I_CAN_TAKE_THEM")));
        return list;
    }

    void Nab(const EventOptionState&) const {
        CardPileCmd::AddCurseToDeck(CardId::INJURY, Owner());
        StaticVec<Reward, 8> rewards;
        rewards.push_back(Reward::NewRelicReward(Owner()));
        RewardsCmd::OfferCustom(Owner(), rewards);
        SetEventFinished(L10NLookup("PUNCH_OFF.pages.NAB.description"));
    }

    void TakeThem(const EventOptionState&) const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(PunchOff, Fight), OptionKey("I_CAN_TAKE_THEM", "FIGHT")));
        SetEventState(L10NLookup("PUNCH_OFF.pages.I_CAN_TAKE_THEM.description"), list);
    }

    void Fight(const EventOptionState&) const {
        StaticVec<RewardRef, 8> rewards;
        rewards.push_back(Reward::NewRelicReward(Owner()).Index());
        rewards.push_back(Reward::NewPotionReward(Owner()).Index());
        RunManager::EnterEventCombat(sim_, EncounterId::PUNCH_OFF_EVENT_ENCOUNTER, rewards);
    }
};

}  // namespace sts2
