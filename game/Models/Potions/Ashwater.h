#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct Ashwater : PotionModel {
    SIM_MODEL(Ashwater, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        if (!target) return;
        CardList cards = CardSelectCmd::FromHand(ctx, target.Player(), CardSelectorPrefs(0, 999999999),
                                                  nullptr, *this);
        for (CardModel card : cards) CardCmd::Exhaust(ctx, card);
    }
};

}  // namespace sts2
