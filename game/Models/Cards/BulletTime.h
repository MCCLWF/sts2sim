#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct BulletTime : CardModel {
    SIM_MODEL(BulletTime, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        for (CardModel card : Owner().PlayerCombatState().Hand().Cards()) {
            if (!card.EnergyCost().CostsX()) card.SetToFreeThisTurn();
        }
        PowerCmd::Apply(PowerId::NO_DRAW_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), *this);
    }
};

}  // namespace sts2
