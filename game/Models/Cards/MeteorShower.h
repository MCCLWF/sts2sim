#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct MeteorShower : CardModel {
    SIM_MODEL(MeteorShower, CardModel)
    int32_t CanonicalStarCost() const { return 2; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).TargetingAllOpponents(CombatState()).Execute(ctx);
        CreatureList enemies = CombatState().HittableEnemies();
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, enemies, DynamicVars().Weak().BaseValue(), Owner().Creature(), *this);
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, enemies, DynamicVars().Vulnerable().BaseValue(), Owner().Creature(), *this);
    }
};
}  // namespace sts2
