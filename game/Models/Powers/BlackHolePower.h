#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {
struct BlackHolePower : PowerModel {
    SIM_MODEL(BlackHolePower, PowerModel)
    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (play && play->Resources.StarsSpent > 0 && play->Card.Owner() == Owner().Player() && play->IsLastInSeries())
            DamageAllEnemies();
    }
    void AfterStarsGained(int32_t amount, Player player) const {
        if (amount > 0 && player == Owner().Player()) DamageAllEnemies();
    }
private:
    void DamageAllEnemies() const {
        sts2::CombatState combat = PowerModel::CombatState();
        if (!combat) return;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(ctx, combat.HittableEnemies(), Dec(Amount()), ValueProp::Unpowered, Owner());
    }
};
}  // namespace sts2
