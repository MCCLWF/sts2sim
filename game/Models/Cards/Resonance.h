#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Resonance : CardModel {
    SIM_MODEL(Resonance, CardModel)
    int32_t CanonicalStarCost() const { return 2; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        int32_t gain = DynamicVars()["StrengthPower"].IntValue();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, Owner().Creature(), Dec(gain), Owner().Creature(), *this);
        for (Creature enemy : CombatState().HittableEnemies())
            PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, enemy, Dec(-1), Owner().Creature(), *this);
    }
};
}  // namespace sts2
