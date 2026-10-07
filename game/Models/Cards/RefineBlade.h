#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Commands/ForgeCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct RefineBlade : CardModel {
    SIM_MODEL(RefineBlade, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        ForgeCmd::Forge(DynamicVars().Forge().IntValue(), Owner(), *this);
        PowerCmd::Apply(PowerId::ENERGY_NEXT_TURN_POWER, ctx, Owner().Creature(), DynamicVars().Energy().BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
