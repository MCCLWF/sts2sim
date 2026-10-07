// Models/Powers/SlowPower.cs
#pragma once

#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct SlowPower : PowerModel {
    SIM_MODEL(SlowPower, PowerModel)

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay*) const {
        DynamicVars()["SlowAmount"].UpgradeValueBy(Dec(1));
        DynamicVars()["DisplayAmount"].SetBaseValue(DynamicVars()["SlowAmount"].BaseValue() * Dec(10));
    }

    Dec ModifyDamageMultiplicative(Creature target, Dec, ValueProp props, Creature, CardModel, const CardPlay*) const {
        if (target != Owner() || !IsPoweredAttack(props)) return Dec(1);
        return Dec(1) + Dec::lit(1, 1) * DynamicVars()["SlowAmount"].BaseValue();
    }

    void AfterModifyingDamageAmount(CardModel) const {}

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, sts2::CombatState) const {
        if (!participants.contains(Owner())) return;
        DynamicVars()["SlowAmount"].SetBaseValue(Dec(0));
        DynamicVars()["DisplayAmount"].SetBaseValue(Dec(0));
    }
};

}  // namespace sts2
