// Models/Potions/FairyInABottle.cs
#pragma once

#include <algorithm>

#include "game/Commands/CreatureCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct FairyInABottle : PotionModel {
    SIM_MODEL(FairyInABottle, PotionModel)

    bool CanBeGeneratedInCombat() const { return false; }
    bool ShouldDie(Creature creature) const { return creature != Owner().Creature(); }

    void OnUse(PlayerChoiceContext&, Creature target) const {
        CreatureCmd::Heal(target, std::max(Dec(target.MaxHp()) * Dec::lit(3, 1), Dec(1)));
    }

    void AfterPreventingDeath(Creature creature) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        OnUseWrapper(ctx, creature);
    }
};

}  // namespace sts2
