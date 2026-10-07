#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Entropy : CardModel {
    SIM_MODEL(Entropy, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::ENTROPY_POWER, ctx, Owner().Creature(), DynamicVars().Cards().BaseValue(), Owner().Creature(), *this);
    }

};

}  // namespace sts2
