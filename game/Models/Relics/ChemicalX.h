#pragma once

#include "game/Models/RelicModel.h"

namespace sts2 {

struct ChemicalX : RelicModel {
    SIM_MODEL(ChemicalX, RelicModel)

    void BeforeCardPlayed(const CardPlay* cardPlay) const {
        if (!cardPlay || cardPlay->Card.Owner() != Owner()) return;
        if (!cardPlay->Card.EnergyCost().CostsX() && !cardPlay->Card.HasStarCostX()) return;
        Flash();
    }

    int32_t ModifyXValue(CardModel card, int32_t originalValue) const {
        if (card.Owner() != Owner()) return originalValue;
        return originalValue + DynamicVars()["Increase"].IntValue();
    }
};

}  // namespace sts2
