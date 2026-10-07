// Models/Potions/PotionOfBinding.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"
namespace sts2 {
struct PotionOfBinding : PotionModel {
    SIM_MODEL(PotionOfBinding, PotionModel)
    void OnUse(PlayerChoiceContext& ctx, Creature) const {
        // Source targeting is AllEnemies and its OnUse intentionally ignores
        // the argument, including the auto-target's null value.
        CreatureList targets = Owner().Creature().CombatState().HittableEnemies();
        PowerCmd::Apply(PowerId::WEAK_POWER, ctx, targets, DynamicVars()["VulnerablePower"].BaseValue(), Owner().Creature(), CardModel{});
        PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, targets, DynamicVars()["WeakPower"].BaseValue(), Owner().Creature(), CardModel{});
    }
};
}
