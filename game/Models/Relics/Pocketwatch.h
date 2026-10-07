// Models/Relics/Pocketwatch.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Pocketwatch : RelicModel {
    SIM_MODEL(Pocketwatch, RelicModel)

    bool ShowCounter() const { return CombatManager(sim_).IsInProgress(); }
    int32_t DisplayAmount() const { return extra(0); }

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner() || !CombatManager(sim_).IsInProgress()) return;
        ++extra(0);
        RefreshCounter();
    }

    Dec ModifyHandDraw(Player player, Dec count) const {
        if (player != Owner() || Owner().PlayerCombatState().TurnNumber() == 1 ||
            extra(1) > DynamicVars()["CardThreshold"].IntValue()) return count;
        return count + DynamicVars().Cards().BaseValue();
    }

    void AfterModifyingHandDraw() const { Flash(); }

    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants, CombatState) const {
        if (!participants.contains(Owner().Creature())) return;
        extra(1) = extra(0);
        extra(0) = 0;
    }

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        if (participants.contains(Owner().Creature())) RefreshCounter();
    }

    void AfterCombatEnd(CombatRoom) const {
        extra(0) = 0;
        extra(1) = 0;
        SetStatus(RelicStatus::Normal);
    }

private:
    void RefreshCounter() const {
        SetStatus(extra(0) <= DynamicVars()["CardThreshold"].IntValue() ? RelicStatus::Active : RelicStatus::Normal);
    }
};

}  // namespace sts2
