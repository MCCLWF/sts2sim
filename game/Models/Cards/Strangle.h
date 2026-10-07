#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Strangle : CardModel {
    SIM_MODEL(Strangle, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("Strangle: cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        PowerCmd::Apply(PowerId::STRANGLE_POWER, ctx, play->Target, DynamicVars()["StranglePower"].BaseValue(),
                        Owner().Creature(), *this);
    }
};

}  // namespace sts2
