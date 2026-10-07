#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct CalculatedGamble : CardModel {
    SIM_MODEL(CalculatedGamble, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardList hand = Owner().PlayerCombatState().Hand().Cards();
        CardCmd::DiscardAndDraw(ctx, hand, static_cast<int32_t>(hand.size()));
    }
};

}  // namespace sts2
