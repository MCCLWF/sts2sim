// Models/Cards/Apparition.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Apparition : CardModel {
    SIM_MODEL(Apparition, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::INTANGIBLE_POWER, ctx, Owner().Creature(),
                        DynamicVars()["IntangiblePower"].BaseValue(), Owner().Creature(), *this);
    }
};

}  // namespace sts2
