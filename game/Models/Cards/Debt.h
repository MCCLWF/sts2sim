// Models/Cards/Debt.cs
#pragma once

#include <algorithm>

#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Debt : CardModel {
    SIM_MODEL(Debt, CardModel)
    int32_t MaxUpgradeLevel() const { return 0; }
    bool HasTurnEndInHandEffect() const { return true; }
    void OnTurnEndInHand(PlayerChoiceContext&) const {
        PlayerCmd::LoseGold(Dec(std::min(DynamicVars().Gold().IntValue(), Owner().Gold())), Owner());
    }
};

}  // namespace sts2
