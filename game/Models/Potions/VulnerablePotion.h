// Models/Potions/VulnerablePotion.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct VulnerablePotion : PotionModel {
    SIM_MODEL(VulnerablePotion, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, target, DynamicVars().Vulnerable().BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
