#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {
struct ConquerorPower : PowerModel {
    SIM_MODEL(ConquerorPower, PowerModel)
    Dec ModifyDamageMultiplicative(Creature target, Dec, ValueProp props, Creature, CardModel source, const CardPlay*) const {
        if (!source || source.CardId_() != CardId::SOVEREIGN_BLADE || !IsPoweredAttack(props) || target != Owner()) return Dec(1);
        return Dec(2);
    }
    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner())) PowerCmd::Decrement(*this);
    }
};
}  // namespace sts2
