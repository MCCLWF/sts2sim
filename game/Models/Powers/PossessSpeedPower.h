// Models/Powers/PossessSpeedPower.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct PossessSpeedPower : PowerModel {
    SIM_MODEL(PossessSpeedPower, PowerModel)

    void AfterPowerAmountChanged(PlayerChoiceContext&, PowerModel power, Dec amount, Creature applier, CardModel) const {
        if (applier != Owner() || !power.Owner().IsPlayer() || power.PowerId_() != PowerId::DEXTERITY_POWER || amount >= Dec(0)) return;
        extra(0) += amount.to_int();
    }

    void AfterDeath(PlayerChoiceContext& ctx, Creature creature, bool wasRemovalPrevented, float) const {
        if (wasRemovalPrevented || creature != Owner() || extra(0) == 0) return;
        CreatureList players;
        for (Player player : Owner().CombatState().Players()) players.push_back(player.Creature());
        PowerCmd::Apply(PowerId::DEXTERITY_POWER, ctx, players, Dec(0) - Dec(extra(0)), Creature{}, CardModel{});
    }
};

}  // namespace sts2
