#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct NeutronAegis : CardModel {
    SIM_MODEL(NeutronAegis, CardModel)
    int32_t CanonicalStarCost() const { return 5; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::PLATING_POWER, ctx, Owner().Creature(), DynamicVars()["PlatingPower"].BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
