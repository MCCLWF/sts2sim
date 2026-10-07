#pragma once

#include "game/Commands/ForgeCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct TheSmith : CardModel {
    SIM_MODEL(TheSmith, CardModel)
    int32_t CanonicalStarCost() const { return 4; }
    void OnPlay(PlayerChoiceContext&, const CardPlay*) const { ForgeCmd::Forge(DynamicVars().Forge().IntValue(), Owner(), *this); }
};
}  // namespace sts2
