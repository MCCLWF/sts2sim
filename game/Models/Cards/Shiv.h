#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Shiv : CardModel {
    SIM_MODEL(Shiv, CardModel)

    sts2::TargetType TargetType() const {
        return Owner() && Owner().Creature().HasPower(PowerId::FAN_OF_KNIVES_POWER)
                   ? sts2::TargetType::AllEnemies
                   : TargetType_Default();
    }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (Owner() && Owner().Creature().HasPower(PowerId::FAN_OF_KNIVES_POWER)) {
            DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).TargetingAllOpponents(CombatState()).Execute(ctx);
            return;
        }
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
    }
};

}  // namespace sts2
