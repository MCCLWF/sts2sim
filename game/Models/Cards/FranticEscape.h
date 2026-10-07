// Models/Cards/FranticEscape.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
#include "game/Models/Powers/SandpitPower.h"

namespace sts2 {

struct FranticEscape : CardModel {
    SIM_MODEL(FranticEscape, CardModel)

    int32_t MaxUpgradeLevel() const { return 0; }
    bool CanBeGeneratedInCombat() const { return false; }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        Creature player = Owner().Creature();
        for (Creature enemy : CombatState().Enemies()) {
            if (!enemy.HasPower(PowerId::SANDPIT_POWER)) continue;
            SandpitPower sandpit(enemy.GetPower(PowerId::SANDPIT_POWER));
            if (sandpit.Target() == player) {
                PowerCmd::ModifyAmount(ctx, sandpit, Dec(1), enemy, *this);
                break;
            }
        }
        EnergyCost().AddThisCombat(1);
    }
};

}  // namespace sts2
