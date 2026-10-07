#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Restlessness : CardModel {
    SIM_MODEL(Restlessness, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        bool onlyCard = true;
        for (CardModel card : Owner().PlayerCombatState().Hand().Cards()) {
            if (card != *this) {
                onlyCard = false;
                break;
            }
        }
        if (!onlyCard) return;
        for (int32_t i = 0; i < DynamicVars().Cards().IntValue(); ++i) CardPileCmd::Draw(ctx, Owner());
        PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner());
    }

};

}  // namespace sts2
