// Models/Powers/InfestedPower.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/Encounters/PhrogParasiteElite.h"
#include "game/Models/Monsters/Wriggler.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct InfestedPower : PowerModel {
    SIM_MODEL(InfestedPower, PowerModel)

    void AfterDeath(PlayerChoiceContext&, Creature target, bool wasRemovalPrevented, float) const {
        if (wasRemovalPrevented || target != Owner()) return;
        sts2::CombatState combat = Owner().CombatState();
        if (!combat) return;
        for (int32_t i = 0; i < 4; ++i) {
            Wriggler wriggler(MonsterModel::ToMutable(sim_, MonsterId::WRIGGLER));
            wriggler.SetStartStunned(true);
            CreatureCmd::Add(wriggler, combat, Owner().Side(), PhrogParasiteElite::GetWrigglerSlotName(i));
        }
    }

    bool ShouldStopCombatFromEnding() const { return true; }
};

}  // namespace sts2
