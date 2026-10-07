#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct GamblersBrew : PotionModel {
    SIM_MODEL(GamblersBrew, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        if (!target) return;
        Player player = target.Player();
        CardList cards = CardSelectCmd::FromHandForDiscard(ctx, player, CardSelectorPrefs(0, 999999999), nullptr, *this);
        CardCmd::DiscardAndDraw(ctx, cards, static_cast<int32_t>(cards.size()));
    }
};

}  // namespace sts2
