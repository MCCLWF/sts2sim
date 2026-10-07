// Models/Potions/BloodPotion.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct BloodPotion : PotionModel {
    SIM_MODEL(BloodPotion, PotionModel)
    void OnUse(PlayerChoiceContext&, Creature target) const {
        CreatureCmd::Heal(target, Dec(target.MaxHp()) * DynamicVars()["HealPercent"].BaseValue() / Dec(100));
    }
};
}
