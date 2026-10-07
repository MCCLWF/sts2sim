// Models/Cards/Shockwave.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Shockwave : CardModel {
    SIM_MODEL(Shockwave, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        for (Creature enemy : CombatState().HittableEnemies()) {
            PowerCmd::Apply(PowerId::WEAK_POWER, ctx, enemy, DynamicVars()["Power"].BaseValue(), Owner().Creature(), *this);
            PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, enemy, DynamicVars()["Power"].BaseValue(), Owner().Creature(), *this);
        }
    }
};
}
