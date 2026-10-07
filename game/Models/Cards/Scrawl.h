#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Scrawl : CardModel {
    SIM_MODEL(Scrawl, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        int32_t draw = CardPile::MaxCardsInHand - Owner().PlayerCombatState().Hand().Count();
        CardPileCmd::Draw(ctx, draw, Owner());
    }

};

}  // namespace sts2
