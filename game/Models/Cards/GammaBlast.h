#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct GammaBlast : CardModel {
    SIM_MODEL(GammaBlast, CardModel)
    int32_t CanonicalStarCost() const { return 3; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, play->Target, DynamicVars().Weak().BaseValue(), Owner().Creature(), *this);
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, play->Target, DynamicVars().Vulnerable().BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
