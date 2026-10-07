// Models/Potions/FyshOil.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct FyshOil : PotionModel {
    SIM_MODEL(FyshOil, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, target, DynamicVars().Strength().BaseValue(), Owner().Creature(), CardModel{});
        PowerCmd::Apply(PowerId::DEXTERITY_POWER, ctx, target, DynamicVars().Dexterity().BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
