// Models/Relics/PaelsTears.cs (v0.111.0)
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct PaelsTears : RelicModel {
    SIM_MODEL(PaelsTears, RelicModel)

    // Saved mutable state for the source HadLeftoverEnergy property.
    bool HadLeftoverEnergy() const { return extra(0) != 0; }
    void SetHadLeftoverEnergy(bool value) const { extra(0) = value ? 1 : 0; }

    void BeforeSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner().Creature())) return;
        SetHadLeftoverEnergy(Owner().PlayerCombatState().Energy() > 0);
    }

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        if (!participants.contains(Owner().Creature()) || !HadLeftoverEnergy()) return;
        Flash();
        PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner());
    }

    void AfterCombatEnd(CombatRoom) const { SetHadLeftoverEnergy(false); }
};

}  // namespace sts2
