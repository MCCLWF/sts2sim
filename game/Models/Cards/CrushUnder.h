#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct CrushUnder : CardModel {
    SIM_MODEL(CrushUnder, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        CreatureList enemies = CombatState().HittableEnemies();
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).TargetingAllOpponents(CombatState()).Execute(ctx);
        PowerCmd::Apply(PowerId::CRUSH_UNDER_POWER, ctx, enemies, DynamicVars()["StrengthLoss"].BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
