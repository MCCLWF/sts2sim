// Models/Potions/MazalethsGift.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct MazalethsGift : PotionModel {
    SIM_MODEL(MazalethsGift, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        PowerCmd::Apply(PowerId::RITUAL_POWER, ctx, target, DynamicVars()["RitualPower"].BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
