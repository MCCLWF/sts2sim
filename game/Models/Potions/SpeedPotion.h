#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct SpeedPotion : PotionModel {
    SIM_MODEL(SpeedPotion, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        PowerCmd::Apply(PowerId::SPEED_POTION_POWER, ctx, target, DynamicVars()["DexterityPower"].BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
