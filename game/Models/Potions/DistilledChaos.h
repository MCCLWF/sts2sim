#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct DistilledChaos : PotionModel {
    SIM_MODEL(DistilledChaos, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        if (target) CardPileCmd::AutoPlayFromDrawPile(ctx, target.Player(), DynamicVars().Repeat().IntValue(),
                                                      CardPilePosition::Top, false);
    }
};

}  // namespace sts2
