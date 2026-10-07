#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct FlankingPower : PowerModel {
    SIM_MODEL(FlankingPower, PowerModel)

    // The source writes the applier's platform display name into a StringVar;
    // this text is UI-only and has no simulation effect.
    void AfterApplied(Creature, CardModel) const {}

    Dec ModifyDamageMultiplicative(Creature target, Dec, ValueProp props, Creature dealer, CardModel,
                                   const CardPlay*) const {
        if (target != Owner() || !IsPoweredAttack(props) || dealer == Applier()) return Dec(1);
        return Dec(Amount());
    }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner())) PowerCmd::Remove(*this);
    }
};

}  // namespace sts2
