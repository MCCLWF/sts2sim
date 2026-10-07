#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/AfflictionModel.h"

namespace sts2 {

struct Hexed : AfflictionModel {
    SIM_MODEL(Hexed, AfflictionModel)

    void AfterCardEnteredCombat(CardModel card) const {
        if (card != Card() || card.Owner().Creature().HasPower(PowerId::HEX_POWER)) return;
        CardCmd::ClearAffliction(Card());
    }
};

}  // namespace sts2
