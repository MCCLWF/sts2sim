#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct NoBlockPower : PowerModel {
    SIM_MODEL(NoBlockPower, PowerModel)

    Dec ModifyBlockMultiplicative(Creature target, Dec block, ValueProp props, CardModel cardSource,
                                  const CardPlay*) const {
        if (target != Owner() || has_flag(props, ValueProp::Unpowered) || !cardSource) return Dec(1);
        return Dec(0);
    }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide side, const CreatureList&) const {
        if (side == CombatSide::Enemy) PowerCmd::Decrement(*this);
    }
};

}  // namespace sts2
