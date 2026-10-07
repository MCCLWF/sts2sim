#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Royalties : CardModel {
    SIM_MODEL(Royalties, CardModel)
    bool CanBeGeneratedInCombat() const { return false; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::ROYALTIES_POWER, ctx, Owner().Creature(), DynamicVars().Gold().BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
