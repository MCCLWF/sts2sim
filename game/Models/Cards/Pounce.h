#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Pounce : CardModel {
    SIM_MODEL(Pounce, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("Pounce: cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play)
            .Targeting(play->Target).Execute(ctx);
        PowerCmd::Apply(PowerId::FREE_SKILL_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), *this);
    }
};

}  // namespace sts2
