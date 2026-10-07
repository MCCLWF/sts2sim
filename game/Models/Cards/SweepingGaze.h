#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Entities/Players/PlayerCombatState.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct SweepingGaze : CardModel {
    SIM_MODEL(SweepingGaze, CardModel)

    bool ShouldGlowRedInternal() const {
        Creature osty = Owner().PlayerCombatState().GetPet(MonsterId::OSTY);
        return !osty || !osty.IsAlive();
    }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        Creature osty = Owner().PlayerCombatState().GetPet(MonsterId::OSTY);
        if (!osty || !osty.IsAlive()) return;
        DamageCmd::Attack(DynamicVars().OstyDamage().BaseValue()).FromOsty(osty, *this, play)
            .TargetingRandomOpponents(CombatState()).Execute(ctx);
    }

};

}  // namespace sts2
