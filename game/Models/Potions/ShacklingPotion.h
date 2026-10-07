#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct ShacklingPotion : PotionModel {
    SIM_MODEL(ShacklingPotion, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature) const {
        PowerCmd::Apply(PowerId::SHACKLING_POTION_POWER, ctx, Owner().Creature().CombatState().HittableEnemies(), DynamicVars()["StrengthPower"].BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
