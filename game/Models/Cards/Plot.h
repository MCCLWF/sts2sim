#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct Plot : CardModel {
    SIM_MODEL(Plot, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        Dec amount = CurrentUpgradeLevel() > 0 ? Dec(3) : Dec(2);
        for (Creature ally : CombatState().Allies())
            if (ally.IsAlive() && ally.IsPlayer()) PowerCmd::Apply(PowerId::DRAW_CARDS_NEXT_TURN_POWER, ctx, ally, amount, Owner().Creature(), *this);
    }
};
}  // namespace sts2
