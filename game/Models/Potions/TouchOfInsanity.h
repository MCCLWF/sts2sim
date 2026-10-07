#pragma once

#include "game/Commands/CardSelectCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct TouchOfInsanity : PotionModel {
    SIM_MODEL(TouchOfInsanity, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        if (!target) return;
        CardList cards = CardSelectCmd::FromHand(ctx, target.Player(), CardSelectorPrefs(1),
            [](CardModel card) { return card.CostsEnergyOrStars(false) || card.CostsEnergyOrStars(true); }, *this);
        if (!cards.empty()) cards.front().SetToFreeThisCombat();
    }
};

}  // namespace sts2
