// Models/Relics/LostCoffer.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/RewardsCmd.h"
#include "game/Runs/CardCreationOptions.h"
namespace sts2 {
struct LostCoffer : RelicModel {
    SIM_MODEL(LostCoffer, RelicModel)
    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const {
        StaticVec<Reward, 8> rewards;
        rewards.push_back(Reward::NewCardReward(Owner(), CardCreationOptions::Make({sts2::data(Owner().Character()).card_pool}, CardCreationSource::Other, CardRarityOddsType::RegularEncounter), 3));
        rewards.push_back(Reward::NewPotionReward(Owner()));
        RewardsCmd::OfferCustom(Owner(), rewards);
    }
};
}
