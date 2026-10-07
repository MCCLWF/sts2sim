// Models/Relics/HistoryCourse.cs (v0.111.0).
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct HistoryCourse : RelicModel {
    SIM_MODEL(HistoryCourse, RelicModel)

    void AfterAutoPrePlayPhaseEntered(PlayerChoiceContext& choiceContext, Player player) const {
        if (player != Owner() || player.PlayerCombatState().TurnNumber() == 1) return;
        const CombatData& combat = sim_->g->combat;
        const int32_t previousTurn = player.PlayerCombatState().TurnNumber() - 1;
        CardRef ref = 0;
        if (combat.player_last_nondupe_attack_turn == previousTurn)
            ref = combat.player_last_nondupe_attack_card;
        else if (combat.player_previous_nondupe_attack_turn == previousTurn)
            ref = combat.player_previous_nondupe_attack_card;
        if (!ref) return;
        CardModel card(sim_, ref);
        if (card.Owner() != player || card.Type() != CardType::Attack || card.IsDupe()) return;
        CardCmd::AutoPlay(choiceContext, card.CreateDupe(), Creature{});
    }
};

}  // namespace sts2
