// Models/Potions/BeetleJuice.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct BeetleJuice : PotionModel {
    SIM_MODEL(BeetleJuice, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        PowerCmd::Apply(PowerId::SHRINK_POWER, ctx, target, DynamicVars().Repeat().BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
