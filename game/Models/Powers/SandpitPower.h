// Models/Powers/SandpitPower.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct SandpitPower : PowerModel {
    SIM_MODEL(SandpitPower, PowerModel)

    void AfterApplied(Creature, CardModel) const {}  // Source work here is positional presentation.
    void AfterPowerAmountChanged(PlayerChoiceContext&, PowerModel, Dec, Creature, CardModel) const {}  // Position/music only.
    void AfterCreatureAddedToCombat(Creature) const {}  // Repositions battlefield visuals.
    void AfterOstyRevived(Creature) const {}  // Repositions battlefield visuals.
    void BeforeSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList&) const {}  // Repositions battlefield visuals.

    void AfterSideTurnStartLate(CombatSide side, const CreatureList&, sts2::CombatState) const {
        if (side == CombatSide::Enemy) PowerCmd::Decrement(*this);
    }

    void AfterRemoved(Creature oldOwner) const {
        Creature target = Target();
        if (oldOwner.IsDead() || !target || target.IsDead()) return;
        CreatureCmd::Kill(target, true);
    }
};

}  // namespace sts2
