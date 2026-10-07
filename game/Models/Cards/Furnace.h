#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Furnace : CardModel {
    SIM_MODEL(Furnace, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::FURNACE_POWER, ctx, Owner().Creature(), DynamicVars().Forge().BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
