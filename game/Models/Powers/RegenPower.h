#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct RegenPower : PowerModel {
    SIM_MODEL(RegenPower, PowerModel)

    void BeforeSideTurnEndEarly(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner()) || Owner().IsDead()) return;
        CreatureCmd::Heal(Owner(), Dec(Amount()));
        PowerCmd::Decrement(*this);
    }
};

}  // namespace sts2
