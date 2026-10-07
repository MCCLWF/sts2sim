// Models/Relics/BigMushroom.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct BigMushroom : RelicModel {
    SIM_MODEL(BigMushroom, RelicModel)

    bool HasUponPickupEffect() const { return true; }

    void AfterObtained() const {
        CreatureCmd::GainMaxHp(Owner().Creature(), DynamicVars().MaxHp().BaseValue());
    }

    void AfterRoomEntered(AbstractRoom) const {}

    Dec ModifyHandDraw(Player player, Dec count) const {
        if (player != Owner() || !Owner().PlayerCombatState() ||
            Owner().PlayerCombatState().TurnNumber() != 1) return count;
        return count - DynamicVars().Cards().BaseValue();
    }
};

}  // namespace sts2
