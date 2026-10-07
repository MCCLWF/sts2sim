// Models/Potions/HeartOfIron.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct HeartOfIron : PotionModel {
    SIM_MODEL(HeartOfIron, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        PowerCmd::Apply(PowerId::PLATING_POWER, ctx, target, DynamicVars()["PlatingPower"].BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
