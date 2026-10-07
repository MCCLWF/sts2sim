// Models/Powers/FrailPower.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct FrailPower : PowerModel {
    SIM_MODEL(FrailPower, PowerModel)

    Dec ModifyBlockMultiplicative(Creature target, Dec block, ValueProp props, CardModel cardSource, const CardPlay* cardPlay) const {
        if (Owner() != target) return Dec(1);
        if (!IsPoweredCardOrMonsterMoveBlock(props)) return Dec(1);
        return Dec::lit(75, 2);  // 0.75m
    }

    void AfterSideTurnEnd(PlayerChoiceContext& choiceContext, CombatSide side, const CreatureList& participants) const {
        if (side == CombatSide::Enemy) PowerCmd::TickDownDuration(*this);
    }
};

}  // namespace sts2
