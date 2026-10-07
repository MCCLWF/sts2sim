// Models/Relics/ArtOfWar.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct ArtOfWar : RelicModel {
    SIM_MODEL(ArtOfWar, RelicModel)
    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner() || !CombatManager(sim_).IsInProgress() ||
            play->Card.Type() != CardType::Attack || extra(0)) return;
        SetStatus(RelicStatus::Normal);
        extra(1) = 1;
    }
    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner().Creature())) return;
        extra(0) = extra(1);
        extra(1) = 0;
    }
    void AfterEnergyReset(Player player) const {
        if (player != Owner()) return;
        SetStatus(RelicStatus::Active);
        if (Owner().PlayerCombatState().TurnNumber() <= 1) return;
        if (!extra(0)) PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner());
        extra(0) = 0;
        extra(1) = 0;
    }
    void AfterCombatEnd(CombatRoom) const {
        SetStatus(RelicStatus::Normal);
        extra(0) = 0;
        extra(1) = 0;
    }
};

}  // namespace sts2
