// Models/Potions/FruitJuice.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct FruitJuice : PotionModel {
    SIM_MODEL(FruitJuice, PotionModel)
    bool CanBeGeneratedInCombat() const { return false; }
    void OnUse(PlayerChoiceContext&, Creature target) const {
        CreatureCmd::GainMaxHp(target, DynamicVars().MaxHp().BaseValue());
    }
};
}
