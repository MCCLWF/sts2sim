#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct FlexPotion : PotionModel {
    SIM_MODEL(FlexPotion, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        PowerCmd::Apply(PowerId::FLEX_POTION_POWER, ctx, target, DynamicVars()["StrengthPower"].BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
