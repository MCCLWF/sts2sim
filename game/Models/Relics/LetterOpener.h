// Models/Relics/LetterOpener.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct LetterOpener : RelicModel {
    SIM_MODEL(LetterOpener, RelicModel)

    bool ShowCounter() const { return CombatManager(sim_).IsInProgress(); }
    int32_t DisplayAmount() const { return extra(0) % 3; }

    void BeforeCombatStart() const {
        extra(0) = 0;
        SetStatus(RelicStatus::Normal);
    }

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, CombatState) const {
        if (!participants.contains(Owner().Creature()) || Owner().PlayerCombatState().TurnNumber() == 1) return;
        extra(0) = 0;
        SetStatus(RelicStatus::Normal);
    }

    void AfterCardPlayed(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner() || !CombatManager(sim_).IsInProgress() ||
            play->Card.Type() != CardType::Skill) return;
        ++extra(0);
        if (extra(0) % 3 == 0) {
            SetStatus(RelicStatus::Normal);
            CreatureCmd::Damage(ctx, Owner().Creature().CombatState().HittableEnemies(), DynamicVars().Damage(), Owner().Creature());
        } else {
            SetStatus(extra(0) % 3 == 2 ? RelicStatus::Active : RelicStatus::Normal);
        }
    }

    void AfterCombatEnd(CombatRoom) const { SetStatus(RelicStatus::Normal); }
};

}  // namespace sts2
