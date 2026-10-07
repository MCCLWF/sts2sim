#pragma once

#include "game/Models/Cards/Soul.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct PotOfGhouls : PotionModel {
    SIM_MODEL(PotOfGhouls, PotionModel)

    void OnUse(PlayerChoiceContext&, Creature target) const {
        if (!target) throw GameError("Potion target is null");
        Soul::CreateInHand(target.Player(), DynamicVars().Cards().IntValue(), target.CombatState(), Owner());
    }
};

}  // namespace sts2
