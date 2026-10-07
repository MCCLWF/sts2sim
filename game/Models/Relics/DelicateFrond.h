// Models/Relics/DelicateFrond.cs (v0.111.0).
#pragma once

#include "game/Commands/PotionCmd.h"
#include "game/Factories/PotionFactory.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct DelicateFrond : RelicModel {
    SIM_MODEL(DelicateFrond, RelicModel)

    void BeforeCombatStart() const {
        Player owner = Owner();
        while (owner.HasOpenPotionSlots()) {
            PotionId potion = PotionFactory::CreateRandomPotionOutOfCombat(
                owner, owner.RunState().Rng().CombatPotionGeneration());
            if (!PotionCmd::TryToProcure(potion, owner).success) break;
        }
    }
};

}  // namespace sts2
