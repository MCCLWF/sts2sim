#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct GigantificationPotion : PotionModel {
    SIM_MODEL(GigantificationPotion, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        PowerCmd::Apply(PowerId::GIGANTIFICATION_POWER, ctx, target, DynamicVars()["GigantificationPower"].BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
