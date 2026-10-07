#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct PillarOfCreation : CardModel {
    SIM_MODEL(PillarOfCreation, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::PILLAR_OF_CREATION_POWER, ctx, Owner().Creature(), DynamicVars().Block().BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
