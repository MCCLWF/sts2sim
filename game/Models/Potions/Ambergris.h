// Models/Potions/Ambergris.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct Ambergris : PotionModel {
    SIM_MODEL(Ambergris, PotionModel)

    void OnUse(PlayerChoiceContext& choiceContext, Creature target) const {
        if (!target) return;
        CreatureCmd::Heal(target, Dec(target.MaxHp()) * DynamicVars()["HealPercent"].BaseValue() / Dec(100));
        if (CombatManager(sim_).IsInProgress())
            PowerCmd::Apply(PowerId::AMBERGRIS_POWER, choiceContext, target, Dec(1),
                            Owner().Creature(), CardModel{});
    }
};

}  // namespace sts2
