#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct RampartPower : PowerModel {
    SIM_MODEL(RampartPower, PowerModel)

    void AfterSideTurnStart(CombatSide side, const CreatureList&, sts2::CombatState combatState) const {
        if (side != CombatSide::Player || !combatState || combatState.st().player_taking_extra_turn) return;
        for (Creature enemy : combatState.Enemies()) {
            if (enemy.Monster() && enemy.Monster().MonsterId_() == MonsterId::TURRET_OPERATOR) {
                CreatureCmd::GainBlock(enemy, Dec(Amount()), ValueProp::Unpowered, nullptr);
            }
        }
    }
};

}  // namespace sts2
