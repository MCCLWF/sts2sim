// Models/Cards/MindBlast.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct MindBlast : CardModel {
    SIM_MODEL(MindBlast, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("MindBlast target is null");
        DamageCmd::Attack(DynamicVars().CalculatedDamage()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
    }
};

}  // namespace sts2
