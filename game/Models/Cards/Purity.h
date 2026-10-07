#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Purity : CardModel {
    SIM_MODEL(Purity, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardList selected = CardSelectCmd::FromHand(ctx, Owner(), CardSelectorPrefs(0, DynamicVars().Cards().IntValue()), nullptr, *this);
        for (CardModel card : selected) CardCmd::Exhaust(ctx, card);
    }

};

}  // namespace sts2
