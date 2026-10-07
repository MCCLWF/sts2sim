#pragma once
#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Void : CardModel { SIM_MODEL(Void, CardModel)
    int32_t MaxUpgradeLevel() const { return 0; }
    void AfterCardDrawn(PlayerChoiceContext&, CardModel card, bool) const {
        if (card == *this) PlayerCmd::LoseEnergy(DynamicVars().Energy().BaseValue(), Owner());
    }
};
}
