// Models/Relics/PhylacteryUnbound.cs
#pragma once

#include "game/Commands/OstyCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct PhylacteryUnbound : RelicModel {
    SIM_MODEL(PhylacteryUnbound, RelicModel)

    bool SpawnsPets() const { return true; }
    void BeforeCombatStart() const {
        PlayerChoiceContext context = PlayerChoiceContext::Throwing();
        OstyCmd::Summon(context, Owner(), DynamicVars()["StartOfCombat"].BaseValue(), *this);
    }
    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        if (!participants.contains(Owner().Creature())) return;
        PlayerChoiceContext context = PlayerChoiceContext::Throwing();
        OstyCmd::Summon(context, Owner(), DynamicVars()["StartOfTurn"].BaseValue(), *this);
    }
};

}  // namespace sts2
