#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct GrandFinale : CardModel {
    SIM_MODEL(GrandFinale, CardModel)

    bool IsPlayable() const { return Owner().PlayerCombatState().DrawPile().Count() == 0; }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play)
            .TargetingAllOpponents(CombatState()).Execute(ctx);
    }
};

}  // namespace sts2
