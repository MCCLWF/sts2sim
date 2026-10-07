// Models/Powers/RitualPower.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct RitualPower : PowerModel {
    SIM_MODEL(RitualPower, PowerModel)

    bool WasJustAppliedByEnemy() const { return extra(0) != 0; }
    void SetWasJustAppliedByEnemy(bool value) const { extra(0) = value; }

    void AfterApplied(Creature, CardModel) const {
        if (Owner().IsEnemy()) SetWasJustAppliedByEnemy(true);
    }

    void AfterSideTurnEnd(PlayerChoiceContext& choiceContext, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        if (WasJustAppliedByEnemy()) {
            SetWasJustAppliedByEnemy(false);
            return;
        }
        PowerCmd::Apply(PowerId::STRENGTH_POWER, choiceContext, Owner(), Dec(Amount()), Owner(), CardModel{});
    }
};

}  // namespace sts2
