#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct FlickFlack : CardModel {
    SIM_MODEL(FlickFlack, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play)
            .TargetingAllOpponents(CombatState()).Execute(ctx);
    }
};

}  // namespace sts2
