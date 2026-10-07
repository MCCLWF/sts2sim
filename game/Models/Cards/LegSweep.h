// Models/Cards/LegSweep.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct LegSweep : CardModel {
    SIM_MODEL(LegSweep, CardModel)
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, play->Target, DynamicVars().Weak().BaseValue(), Owner().Creature(), *this);
    }
};
}
