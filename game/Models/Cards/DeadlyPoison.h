// Models/Cards/DeadlyPoison.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct DeadlyPoison : CardModel {
    SIM_MODEL(DeadlyPoison, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        PowerCmd::Apply(PowerId::POISON_POWER, ctx, play->Target, DynamicVars().Poison().BaseValue(),
                        Owner().Creature(), *this);
    }
};
}
