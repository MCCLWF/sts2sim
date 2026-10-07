#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct RipAndTear : CardModel {
    SIM_MODEL(RipAndTear, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).WithHitCount(2).FromCard(*this, play)
            .TargetingRandomOpponents(CombatState()).Execute(ctx);
    }

};

}  // namespace sts2
