// Models/Potions/StrengthPotion.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct StrengthPotion : PotionModel {
    SIM_MODEL(StrengthPotion, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        PowerCmd::Apply(PowerId::STRENGTH_POWER, ctx, target, DynamicVars().Strength().BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
