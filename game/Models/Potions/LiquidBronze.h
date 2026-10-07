// Models/Potions/LiquidBronze.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct LiquidBronze : PotionModel {
    SIM_MODEL(LiquidBronze, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        PowerCmd::Apply(PowerId::THORNS_POWER, ctx, target, DynamicVars()["ThornsPower"].BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
