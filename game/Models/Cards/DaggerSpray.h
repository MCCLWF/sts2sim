// Models/Cards/DaggerSpray.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct DaggerSpray : CardModel {
    SIM_MODEL(DaggerSpray, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        DamageCmd::Attack(DynamicVars().Damage().BaseValue())
            .WithHitCount(2).FromCard(*this, play).TargetingAllOpponents(CombatState()).Execute(ctx);
    }
};
}
