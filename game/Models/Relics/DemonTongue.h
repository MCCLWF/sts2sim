// Models/Relics/DemonTongue.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct DemonTongue : RelicModel {
    SIM_MODEL(DemonTongue, RelicModel)

    void AfterDamageReceived(PlayerChoiceContext&, Creature target, const DamageResult& result,
                             ValueProp, Creature, CardModel) const {
        Creature owner = Owner().Creature();
        if (!owner.CombatState() || owner.CombatState().CurrentSide() != owner.Side() ||
            target != owner || result.UnblockedDamage <= 0 || extra(0)) return;
        extra(0) = 1;
        CreatureCmd::Heal(owner, Dec(result.UnblockedDamage));
    }

    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants,
                             CombatState) const {
        if (participants.contains(Owner().Creature())) extra(0) = 0;
    }
};

}  // namespace sts2
