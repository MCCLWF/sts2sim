#pragma once

#include "game/Commands/OstyCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {

struct BoneBrew : PotionModel {
    SIM_MODEL(BoneBrew, PotionModel)

    void OnUse(PlayerChoiceContext& ctx, Creature target) const {
        if (!target) throw GameError("Potion target is null");
        OstyCmd::Summon(ctx, target.Player(), DynamicVars().Summon().BaseValue(), *this);
    }
};

}  // namespace sts2
