// Models/Powers/AdaptablePower.cs
#pragma once
#include "game/Models/PowerModel.h"
#include "game/Models/MonsterModel.h"
namespace sts2 {
struct AdaptablePower : PowerModel {
    SIM_MODEL(AdaptablePower, PowerModel)
    bool IsReviving() const { return extra(0) != 0; }
    void DoRevive() const { extra(0) = 0; }
    void AfterDeath(PlayerChoiceContext&, Creature creature, bool wasRemovalPrevented, float) const {
        if (wasRemovalPrevented || creature != Owner() || creature.Monster().MonsterId_() != MonsterId::TEST_SUBJECT) return;
        extra(0) = 1;
        sim_->g->run.test_subject_kills++;
        creature.Monster().SetMoveImmediate("RESPAWN_MOVE", true);
    }
    bool ShouldAllowHitting(Creature creature) const { return creature != Owner() || !IsReviving(); }
    bool ShouldStopCombatFromEnding() const { return true; }
    bool ShouldCreatureBeRemovedFromCombatAfterDeath(Creature creature) const { return creature != Owner(); }
    bool ShouldPowerBeRemovedAfterOwnerDeath() const { return false; }
};
}  // namespace sts2
