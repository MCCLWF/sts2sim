// Models/Events/DrowningBeacon.cs
#pragma once

#include "engine/sim/alloc.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Commands/RewardsCmd.h"
#include "game/Models/EventModel.h"

namespace sts2 {

struct DrowningBeacon : EventModel {
    SIM_MODEL(DrowningBeacon, EventModel)

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(DrowningBeacon, BottleOption), InitialOptionKey("BOTTLE")));
        list.push_back(EventOption(SIM_OPTION(DrowningBeacon, ClimbOption), InitialOptionKey("CLIMB"))
                           .ThatDecreasesMaxHp(Dec(13)));
        return list;
    }

    void BottleOption(const EventOptionState&) const {
        StaticVec<Reward, 8> rewards;
        PotionModel potion(&sim(), alloc_potion(*sim_->g, PotionId::GLOWWATER_POTION, 0));
        rewards.push_back(Reward::NewPotionReward(Owner(), potion));
        RewardsCmd::OfferCustom(Owner(), rewards);
        SetEventFinished(L10NLookup("DROWNING_BEACON.pages.BOTTLE.description"));
    }

    void ClimbOption(const EventOptionState&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::LoseMaxHp(ctx, Owner().Creature(), Dec(13), false);
        RelicCmd::Obtain(RelicId::FRESNEL_LENS, Owner());
        SetEventFinished(L10NLookup("DROWNING_BEACON.pages.CLIMB.description"));
    }

    bool IsAllowed(RunState runState) const {
        return true;
    }
};

}  // namespace sts2
