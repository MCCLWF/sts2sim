// Models/Powers/ReattachPower.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"
#include "game/Models/MonsterModel.h"

namespace sts2 {

struct ReattachPower : PowerModel {
    SIM_MODEL(ReattachPower, PowerModel)

    bool IsReviving() const { return extra(0) != 0; }
    bool AreAllOtherSegmentsDead() const {
        for (auto teammate : CombatState().GetTeammatesOf(Owner())) {
            if (teammate != Owner() && teammate.HasPower(PowerId::REATTACH_POWER) && !teammate.IsDead()) return false;
        }
        return true;
    }
    void DoReattach() const {
        if (!AreAllOtherSegmentsDead()) {
            extra(0) = 0;
            CreatureCmd::Heal(Owner(), Dec(Amount()));
        }
    }
    void AfterDeath(PlayerChoiceContext&, Creature creature, bool wasRemovalPrevented, float) const {
        if (wasRemovalPrevented || creature != Owner()) return;
        if (!AreAllOtherSegmentsDead() || !Owner().IsDead()) {
            extra(0) = 1;
            MonsterModel monster = creature.Monster();
            if (!monster) return;
            switch (monster.MonsterId_()) {
            case MonsterId::DECIMILLIPEDE_SEGMENT_FRONT:
            case MonsterId::DECIMILLIPEDE_SEGMENT_MIDDLE:
            case MonsterId::DECIMILLIPEDE_SEGMENT_BACK:
                Owner().Monster().SetMoveImmediate("DEAD_MOVE");
                break;
            default: break;
            }
        }
    }
    bool ShouldAllowHitting(Creature creature) const { return creature != Owner() || !IsReviving(); }
    bool ShouldCreatureBeRemovedFromCombatAfterDeath(Creature creature) const { return creature != Owner(); }
    bool ShouldPowerBeRemovedAfterOwnerDeath() const { return false; }
    bool ShouldOwnerDeathTriggerFatal() const { return AreAllOtherSegmentsDead(); }
};

}  // namespace sts2
