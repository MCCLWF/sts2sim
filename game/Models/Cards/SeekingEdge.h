#pragma once

#include "game/Commands/ForgeCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct SeekingEdge : CardModel {
    SIM_MODEL(SeekingEdge, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::SEEKING_EDGE_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), *this);
        ForgeCmd::Forge(DynamicVars().Forge().IntValue(), Owner(), *this);
    }
};
}  // namespace sts2
