// Models/Relics/Kusarigama.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct Kusarigama : RelicModel {
    SIM_MODEL(Kusarigama, RelicModel)
    bool ShowCounter() const { return CombatManager(sim_).IsInProgress(); }
    int32_t DisplayAmount() const { return extra(0) % DynamicVars().Cards().IntValue(); }
    void BeforeCombatStart() const { extra(0) = 0; SetStatus(RelicStatus::Normal); }
    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner().Creature())) { extra(0) = 0; SetStatus(RelicStatus::Normal); }
    }
    void AfterCardPlayed(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner() || !CombatManager(sim_).IsInProgress() ||
            play->Card.Type() != CardType::Attack) return;
        int32_t count = ++extra(0);
        int32_t perActivation = DynamicVars().Cards().IntValue();
        SetStatus(count % perActivation == perActivation - 1 ? RelicStatus::Active : RelicStatus::Normal);
        if (count % perActivation != 0) return;
        CreatureList enemies = Owner().Creature().CombatState().HittableEnemies();
        Creature target = Owner().RunState().Rng().CombatTargets().NextItem(enemies);
        if (target) CreatureCmd::Damage(ctx, target, DynamicVars().Damage(), Owner().Creature());
    }
    // Status = Normal, then IsActivating = false, whose setter calls UpdateDisplay: the status is taken again from
    // the attack count, which combat end does not reset.
    void AfterCombatEnd(CombatRoom) const {
        int32_t perActivation = DynamicVars().Cards().IntValue();
        SetStatus(extra(0) % perActivation == perActivation - 1 ? RelicStatus::Active : RelicStatus::Normal);
    }
};

}  // namespace sts2
