// Models/Potions/DexterityPotion.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct DexterityPotion : PotionModel {
    SIM_MODEL(DexterityPotion, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        PowerCmd::Apply(PowerId::DEXTERITY_POWER, ctx, target, DynamicVars().Dexterity().BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
