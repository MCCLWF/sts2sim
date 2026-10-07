// Models/Potions/FocusPotion.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct FocusPotion : PotionModel {
    SIM_MODEL(FocusPotion, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        PowerCmd::Apply(PowerId::FOCUS_POWER, ctx, target, DynamicVars()["FocusPower"].BaseValue(), target, CardModel{});
    }
};
}
