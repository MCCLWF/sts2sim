// Models/Relics/RunicCapacitor.cs
#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/RelicModel.h"
namespace sts2 {
struct RunicCapacitor : RelicModel { SIM_MODEL(RunicCapacitor, RelicModel)
    void AfterSideTurnStart(CombatSide, const CreatureList& participants, sts2::CombatState) const {
        if (participants.contains(Owner().Creature()) && Owner().PlayerCombatState().TurnNumber() <= 1)
            OrbCmd::AddSlots(Owner(), DynamicVars().Repeat().IntValue());
    }
};
}
