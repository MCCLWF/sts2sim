#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct KnowThyPlace : CardModel {
    SIM_MODEL(KnowThyPlace, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, play->Target, DynamicVars().Weak().BaseValue(), Owner().Creature(), *this);
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, play->Target, DynamicVars().Vulnerable().BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
