#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Apotheosis : CardModel {
    SIM_MODEL(Apotheosis, CardModel)

    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        for (CardModel card : Owner().PlayerCombatState().AllCards()) {
            if (card != *this && card.IsUpgradable()) CardCmd::Upgrade(card);
        }
    }

};

}  // namespace sts2
