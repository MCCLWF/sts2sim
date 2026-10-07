#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct AutomationPower : PowerModel {
    SIM_MODEL(AutomationPower, PowerModel)

    void SetCardsLeft(int32_t value) const { extra(0) = value; }
    int32_t CardsLeft() const { return extra(0); }

    void AfterCardDrawn(PlayerChoiceContext&, CardModel card, bool) const {
        if (!card || card.Owner().Creature() != Owner()) return;
        if (CardsLeft() <= 0) SetCardsLeft(10);
        SetCardsLeft(CardsLeft() - 1);
        if (CardsLeft() <= 0) {
            PlayerCmd::GainEnergy(Dec(Amount()), Owner().Player());
            SetCardsLeft(10);
        }
    }
};

}  // namespace sts2
