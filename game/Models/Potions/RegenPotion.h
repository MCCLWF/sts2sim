#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct RegenPotion : PotionModel {
    SIM_MODEL(RegenPotion, PotionModel)
    bool CanBeGeneratedInCombat() const { return false; }
    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        PowerCmd::Apply(PowerId::REGEN_POWER, ctx, target, DynamicVars()["RegenPower"].BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
