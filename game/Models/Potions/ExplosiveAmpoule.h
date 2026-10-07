#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct ExplosiveAmpoule : PotionModel {
    SIM_MODEL(ExplosiveAmpoule, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature) const {
        Creature player = Owner().Creature();
        CreatureCmd::Damage(ctx, player.CombatState().HittableEnemies(), DynamicVars().Damage().BaseValue(), DynamicVars().Damage().Props(), player, CardModel{}, nullptr);
    }
};
}
