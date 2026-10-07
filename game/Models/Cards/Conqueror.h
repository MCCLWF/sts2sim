#pragma once

#include "game/Commands/ForgeCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Conqueror : CardModel {
    SIM_MODEL(Conqueror, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        ForgeCmd::Forge(DynamicVars().Forge().IntValue(), Owner(), *this);
        PowerCmd::Apply(PowerId::CONQUEROR_POWER, ctx, play->Target, Dec(1), Owner().Creature(), *this);
    }
};
}  // namespace sts2
