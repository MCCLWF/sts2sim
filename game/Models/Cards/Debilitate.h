#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Debilitate : CardModel {
    SIM_MODEL(Debilitate, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        PowerCmd::Apply(PowerId::DEBILITATE_POWER, ctx, play->Target,
                        DynamicVars()["DebilitatePower"].BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
