// Models/Potions/WeakPotion.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct WeakPotion : PotionModel {
    SIM_MODEL(WeakPotion, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, target, DynamicVars().Weak().BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
