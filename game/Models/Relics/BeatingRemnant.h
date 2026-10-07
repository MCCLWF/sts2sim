// Models/Relics/BeatingRemnant.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct BeatingRemnant : RelicModel {
    SIM_MODEL(BeatingRemnant, RelicModel)
    Dec ModifyHpLostAfterOsty(Creature target, Dec amount, ValueProp, Creature, CardModel) const {
        if (!CombatManager(sim_).IsInProgress() || target != Owner().Creature()) return amount;
        Dec remaining = DynamicVars()["MaxHpLoss"].BaseValue() - Dec(extra(0));
        return amount < remaining ? amount : remaining;
    }
    void AfterModifyingHpLostAfterOsty() const {}
    void AfterDamageReceived(PlayerChoiceContext&, Creature target, const DamageResult& result, ValueProp,
                             Creature, CardModel) const {
        if (CombatManager(sim_).IsInProgress() && target == Owner().Creature())
            extra(0) += result.UnblockedDamage;
    }
    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants, CombatState) const {
        if (participants.contains(Owner().Creature())) extra(0) = 0;
    }
};

}  // namespace sts2
