// Models/Cards/Neutralize.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Neutralize : CardModel {
    SIM_MODEL(Neutralize, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, play->Target, DynamicVars().Weak().BaseValue(),
                        Owner().Creature(), *this);
    }
};

}  // namespace sts2
