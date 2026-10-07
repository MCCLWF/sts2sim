// Models/Relics/PaelsFlesh.cs (v0.111.0).
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct PaelsFlesh : RelicModel {
    SIM_MODEL(PaelsFlesh, RelicModel)

    int32_t DisplayAmount() const {
        return Owner().PlayerCombatState() ? Owner().PlayerCombatState().TurnNumber() : 1;
    }
    bool ShowCounter() const {
        return CombatManager(sim_).IsInProgress() && Status() == RelicStatus::Normal;
    }
    Dec ModifyMaxEnergy(Player player, Dec amount) const {
        return player == Owner() && player.PlayerCombatState() &&
                       player.PlayerCombatState().TurnNumber() >= 3
            ? amount + DynamicVars().Energy().BaseValue() : amount;
    }
    void BeforeCombatStart() const {}  // original only notifies UI of counter change
    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList&, CombatState) const {
        // Original only notifies UI when the owner participates.
    }
    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        if (!participants.contains(Owner().Creature()) ||
            !Owner().PlayerCombatState() || Owner().PlayerCombatState().TurnNumber() < 3 ||
            Status() == RelicStatus::Active) return;
        SetStatus(RelicStatus::Active);
        Flash();
    }
    void AfterCombatEnd(CombatRoom) const { SetStatus(RelicStatus::Normal); }
};

}  // namespace sts2
