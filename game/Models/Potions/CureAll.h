#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct CureAll : PotionModel {
    SIM_MODEL(CureAll, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        Player player = target.Player();
        PlayerCmd::GainEnergy(DynamicVars()["Energy"].BaseValue(), player);
        CardPileCmd::Draw(ctx, DynamicVars()["Cards"].BaseValue(), player);
    }
};

}  // namespace sts2
