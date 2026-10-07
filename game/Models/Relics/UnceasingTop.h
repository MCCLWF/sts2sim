// Models/Relics/UnceasingTop.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct UnceasingTop : RelicModel {
    SIM_MODEL(UnceasingTop, RelicModel)

    void AfterHandEmptied(PlayerChoiceContext& choiceContext, Player player) const {
        if (player != Owner()) return;
        PlayerTurnPhase phase = player.PlayerCombatState().Phase();
        if (phase != PlayerTurnPhase::AutoPrePlay && phase != PlayerTurnPhase::Play &&
            phase != PlayerTurnPhase::AutoPostPlay) return;
        CardPileCmd::Draw(choiceContext, player);
    }
};

}  // namespace sts2
