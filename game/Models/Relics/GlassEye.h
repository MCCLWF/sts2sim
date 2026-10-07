// Models/Relics/GlassEye.cs (v0.111.0).
#pragma once

#include "game/Commands/RewardsCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/CardCreationOptions.h"

namespace sts2 {

struct GlassEye : RelicModel {
    SIM_MODEL(GlassEye, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        StaticVec<Reward, 8> rewards;
        constexpr CardRarity rarities[] = {CardRarity::Common, CardRarity::Common,
                                           CardRarity::Uncommon, CardRarity::Uncommon,
                                           CardRarity::Rare};
        for (CardRarity rarity : rarities) {
            CardCreationOptionsState options = CardCreationOptions::Make(
                {sts2::data(Owner().Character()).card_pool}, CardCreationSource::Other,
                CardRarityOddsType::Uniform);
            options.required_rarity = rarity;
            options.flags = options.flags | CardCreationFlags::NoRarityModification |
                            CardCreationFlags::NoUpgradeRoll;
            rewards.push_back(Reward::NewCardReward(Owner(), options, 3));
        }
        RewardsCmd::OfferCustom(Owner(), rewards);
    }
};

}  // namespace sts2
