// Models/Powers/SteamEruptionPower.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/Monsters/WaterfallGiant.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct SteamEruptionPower : PowerModel {
    SIM_MODEL(SteamEruptionPower, PowerModel)

    void AfterDeath(PlayerChoiceContext&, Creature creature, bool wasRemovalPrevented, float) const {
        if (wasRemovalPrevented || creature != Owner()) return;
        WaterfallGiant(Owner().Monster()).TriggerAboutToBlowState();
    }

    bool ShouldStopCombatFromEnding() const { return true; }
    bool ShouldCreatureBeRemovedFromCombatAfterDeath(Creature creature) const { return creature != Owner(); }
    bool ShouldPowerBeRemovedAfterOwnerDeath() const { return false; }
};

}  // namespace sts2
