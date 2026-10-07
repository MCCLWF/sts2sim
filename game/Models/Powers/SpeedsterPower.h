#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct SpeedsterPower : PowerModel {
    SIM_MODEL(SpeedsterPower, PowerModel)

    void AfterCardDrawn(PlayerChoiceContext& ctx, CardModel card, bool fromHandDraw) const {
        if (fromHandDraw || card.Owner().Creature() != Owner() || Owner().CombatState().CurrentSide() != Owner().Side()) return;
        CreatureCmd::Damage(ctx, Owner().CombatState().HittableEnemies(), Dec(Amount()), ValueProp::Unpowered, Owner());
    }
};

}  // namespace sts2
