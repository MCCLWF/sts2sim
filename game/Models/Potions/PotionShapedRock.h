// Models/Potions/PotionShapedRock.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct PotionShapedRock : PotionModel {
    SIM_MODEL(PotionShapedRock, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        CreatureCmd::Damage(ctx, target, DynamicVars().Damage().BaseValue(), DynamicVars().Damage().Props(), Owner().Creature(), CardModel{}, nullptr);
    }
};
}
