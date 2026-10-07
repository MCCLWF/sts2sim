// Models/Events/PotionCourier.cs
#pragma once

#include "engine/sim/alloc.h"
#include "game/Commands/RewardsCmd.h"
#include "game/Factories/PotionFactory.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct PotionCourier : EventModel {
    SIM_MODEL(PotionCourier, EventModel)

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(PotionCourier, GrabPotions),
                                   "POTION_COURIER.pages.INITIAL.options.GRAB_POTIONS"));
        list.push_back(EventOption(SIM_OPTION(PotionCourier, Ransack),
                                   "POTION_COURIER.pages.INITIAL.options.RANSACK"));
        return list;
    }

    void GrabPotions(const EventOptionState&) const {
        StaticVec<Reward, 8> rewards;
        for (int i = 0; i < 3; ++i) {
            PotionModel potion(&sim(), alloc_potion(*sim_->g, PotionId::FOUL_POTION, 0));
            rewards.push_back(Reward::NewPotionReward(Owner(), potion));
        }
        RewardsCmd::OfferCustom(Owner(), rewards);
        SetEventFinished(L10NLookup("POTION_COURIER.pages.GRAB_POTIONS.description"));
    }

    void Ransack(const EventOptionState&) const {
        StaticVec<PotionId, 96> options;
        for (PotionId p : PotionFactory::GetPotionOptions(Owner())) {
            if (sts2::data(p).rarity == PotionRarity::Uncommon) options.push_back(p);
        }
        PotionId potion_id = Owner().PlayerRng().Rewards().NextItem(options);
        if (potion_id != PotionId::NONE) {
            StaticVec<Reward, 8> rewards;
            PotionModel potion(&sim(), alloc_potion(*sim_->g, potion_id, 0));
            rewards.push_back(Reward::NewPotionReward(Owner(), potion));
            RewardsCmd::OfferCustom(Owner(), rewards);
        }
        SetEventFinished(L10NLookup("POTION_COURIER.pages.RANSACK.description"));
    }

    bool IsAllowed(RunState runState) const { return runState.CurrentActIndex() > 0; }
};

}  // namespace sts2
