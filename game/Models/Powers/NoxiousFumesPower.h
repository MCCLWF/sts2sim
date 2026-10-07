#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct NoxiousFumesPower : PowerModel {
    SIM_MODEL(NoxiousFumesPower, PowerModel)

    void AfterSideTurnStart(CombatSide, const CreatureList& participants, sts2::CombatState combatState) const {
        if (!participants.contains(Owner())) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::POISON_POWER, ctx, combatState.HittableEnemies(), Dec(Amount()), Owner(), CardModel{});
    }
};

}  // namespace sts2
