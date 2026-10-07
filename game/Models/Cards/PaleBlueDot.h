#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct PaleBlueDot : CardModel {
    SIM_MODEL(PaleBlueDot, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::PALE_BLUE_DOT_POWER, ctx, Owner().Creature(), DynamicVars().Cards().BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
