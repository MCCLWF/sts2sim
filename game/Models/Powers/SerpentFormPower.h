#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct SerpentFormPower : PowerModel {
    SIM_MODEL(SerpentFormPower, PowerModel)

    void AfterApplied(Creature, CardModel) const {}  // VFX-only in the source.
    void AfterRemoved(Creature) const {}              // VFX-only in the source.

    void BeforeCardPlayed(const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner().Player()) return;
        CombatState().CaptureCardPlayPowerAmount(*this, play->Card, Amount());
    }

    void AfterCardPlayed(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner().Player()) return;
        int32_t damage = 0;
        if (!CombatState().TakeCardPlayPowerAmount(*this, play->Card, damage) || damage <= 0) return;
        CreatureList enemies = Owner().CombatState().HittableEnemies();
        int32_t index = Owner().Player().RunState().Rng().CombatTargets().next_item_index(static_cast<int32_t>(enemies.size()));
        if (index >= 0)
            CreatureCmd::Damage(ctx, enemies[static_cast<size_t>(index)], Dec(damage), ValueProp::Unpowered, Owner());
    }
};

}  // namespace sts2
