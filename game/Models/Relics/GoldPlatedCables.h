// Models/Relics/GoldPlatedCables.cs
#pragma once
#include "game/Models/OrbModel.h"
#include "game/Models/RelicModel.h"
namespace sts2 {
struct GoldPlatedCables : RelicModel { SIM_MODEL(GoldPlatedCables, RelicModel)
    int32_t ModifyOrbPassiveTriggerCounts(OrbModel orb, int32_t triggerCount) const {
        if (orb.Owner() != Owner()) return triggerCount;
        const auto& orbs = Owner().PlayerCombatState().st().orbs;
        if (orbs.empty()) throw GameError("GoldPlatedCables: orb queue is empty");
        if (orbs[0] != orb.Index()) return triggerCount;
        return triggerCount + 1;
    }
    void AfterModifyingOrbPassiveTriggerCount(OrbModel) const { Flash(); }
};
}
