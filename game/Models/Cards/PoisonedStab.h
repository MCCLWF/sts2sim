// Models/Cards/PoisonedStab.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct PoisonedStab : CardModel {
    SIM_MODEL(PoisonedStab, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        PowerCmd::Apply(PowerId::POISON_POWER, ctx, play->Target, DynamicVars().Poison().BaseValue(),
                        Owner().Creature(), *this);
    }
};
}
