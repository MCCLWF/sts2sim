#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct DyingStar : CardModel {
    SIM_MODEL(DyingStar, CardModel)
    int32_t CanonicalStarCost() const { return 3; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        CreatureList enemies = CombatState().HittableEnemies();
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).TargetingAllOpponents(CombatState()).Execute(ctx);
        for (Creature enemy : enemies)
            PowerCmd::Apply(PowerId::DYING_STAR_POWER, ctx, enemy, DynamicVars()["StrengthLoss"].BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
