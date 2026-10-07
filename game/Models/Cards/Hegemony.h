#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Hegemony : CardModel {
    SIM_MODEL(Hegemony, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        PowerCmd::Apply(PowerId::ENERGY_NEXT_TURN_POWER, ctx, Owner().Creature(), DynamicVars().Energy().BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
