// Models/Relics/Kunai.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct Kunai : RelicModel {
    SIM_MODEL(Kunai, RelicModel)

    bool ShowCounter() const { return CombatManager(sim_).IsInProgress(); }
    int32_t DisplayAmount() const { return extra(0) % DynamicVars().Cards().IntValue(); }

    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants, CombatState) const {
        if (!participants.contains(Owner().Creature())) return;
        extra(0) = 0;
        SetStatus(RelicStatus::Normal);
    }

    void AfterCardPlayed(PlayerChoiceContext& choiceContext, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner() || !CombatManager(sim_).IsInProgress() ||
            play->Card.Type() != CardType::Attack) return;
        int32_t attacksPlayed = ++extra(0);
        int32_t perActivation = DynamicVars().Cards().IntValue();
        if (attacksPlayed % perActivation == 0)
            PowerCmd::Apply(PowerId::DEXTERITY_POWER, choiceContext, Owner().Creature(),
                            DynamicVars().Dexterity().BaseValue(), Owner().Creature(), CardModel{});
        SetStatus(attacksPlayed % perActivation == perActivation - 1 ? RelicStatus::Active : RelicStatus::Normal);
    }

    void AfterCombatEnd(CombatRoom) const {
        extra(0) = 0;
        SetStatus(RelicStatus::Normal);
    }
};

}  // namespace sts2
