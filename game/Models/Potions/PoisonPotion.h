// Models/Potions/PoisonPotion.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct PoisonPotion : PotionModel {
    SIM_MODEL(PoisonPotion, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        PowerCmd::Apply(PowerId::POISON_POWER, ctx, target, DynamicVars().Poison().BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
