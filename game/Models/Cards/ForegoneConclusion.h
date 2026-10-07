#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct ForegoneConclusion : CardModel {
    SIM_MODEL(ForegoneConclusion, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::FOREGONE_CONCLUSION_POWER, ctx, Owner().Creature(), DynamicVars().Cards().BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
