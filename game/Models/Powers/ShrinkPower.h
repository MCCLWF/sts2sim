// Models/Powers/ShrinkPower.cs. StackType (Single while IsInfinite) is in
// PowerModel::StackType; the ApplierName StringVar is display text.
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct ShrinkPower : PowerModel {
    SIM_MODEL(ShrinkPower, PowerModel)

    bool IsInfinite() const { return Amount() < 0; }

    void AfterApplied(Creature applier, CardModel cardSource) const {}  // scaling visuals, ApplierName text
    void AfterRemoved(Creature oldOwner) const {}                       // scaling visuals

    void AfterSideTurnEnd(PlayerChoiceContext& choiceContext, CombatSide side, const CreatureList& participants) const {
        if (!IsInfinite() && participants.contains(Owner())) PowerCmd::Decrement(*this);
    }

    void AfterDeath(PlayerChoiceContext& choiceContext, Creature creature, bool wasRemovalPrevented, float deathAnimLength) const {
        if (!wasRemovalPrevented && creature == Applier()) PowerCmd::Remove(*this);
    }

    Dec ModifyDamageMultiplicative(Creature target, Dec amount, ValueProp props, Creature dealer, CardModel cardSource,
                                   const CardPlay* cardPlay) const {
        if (Owner() != dealer) return Dec(1);
        if (!IsPoweredAttack(props)) return Dec(1);
        return (Dec(100) - DynamicVars()["DamageDecrease"].BaseValue()) / Dec(100);
    }
};

}  // namespace sts2
