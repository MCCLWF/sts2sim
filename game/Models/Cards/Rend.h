#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct Rend : CardModel {
    SIM_MODEL(Rend, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("Rend target is null");
        DamageCmd::Attack(DynamicVars().CalculatedDamage()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
    }
};

}  // namespace sts2
