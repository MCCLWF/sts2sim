#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct MayhemPower : PowerModel {
    SIM_MODEL(MayhemPower, PowerModel)

    void AfterAutoPrePlayPhaseEntered(PlayerChoiceContext& ctx, Player player) const {
        if (player != Owner().Player()) return;
        CardPileCmd::AutoPlayFromDrawPile(ctx, player, Amount(), CardPilePosition::Top, false);
    }
};

}  // namespace sts2
