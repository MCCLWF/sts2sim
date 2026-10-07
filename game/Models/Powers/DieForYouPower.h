#pragma once

#include "game/Models/PowerModel.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2 {

struct DieForYouPower : PowerModel {
    SIM_MODEL(DieForYouPower, PowerModel)
    bool ShouldPlayVfx() const { return false; }
    Creature ModifyUnblockedDamageTarget(Creature target, Dec, ValueProp props, Creature) const {
        Player petOwner = Owner().PetOwner();
        if (!petOwner || target != petOwner.Creature() || Owner().IsDead() || !IsPoweredAttack(props))
            return target;
        return Owner();
    }
    bool ShouldAllowHitting(Creature creature) const { return creature.IsAlive(); }
    bool ShouldCreatureBeRemovedFromCombatAfterDeath(Creature creature) const { return creature != Owner(); }
    bool ShouldPowerBeRemovedAfterOwnerDeath() const { return false; }
};

}  // namespace sts2
