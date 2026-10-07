// Models/Potions/FirePotion.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct FirePotion : PotionModel {
    SIM_MODEL(FirePotion, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        CreatureCmd::Damage(ctx, target, DynamicVars().Damage().BaseValue(), DynamicVars().Damage().Props(), Owner().Creature(), CardModel{}, nullptr);
    }
};
}
