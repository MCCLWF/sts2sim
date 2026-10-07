#pragma once

#include "game/Commands/ForgeCmd.h"
#include "game/Models/PotionModel.h"

namespace sts2 {
struct KingsCourage : PotionModel {
    SIM_MODEL(KingsCourage, PotionModel)
    void OnUse(PlayerChoiceContext&, Creature target) const {
        if (!target || !target.Player()) throw GameError("KingsCourage requires a player target");
        ForgeCmd::Forge(DynamicVars().Forge().IntValue(), target.Player(), *this);
    }
};
}  // namespace sts2
