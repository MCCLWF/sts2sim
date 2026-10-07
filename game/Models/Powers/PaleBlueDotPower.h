#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {
struct PaleBlueDotPower : PowerModel {
    SIM_MODEL(PaleBlueDotPower, PowerModel)
    void AfterCardPlayed(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner().Player() || extra(0) != 0) return;
        if (sim_->g->combat.player_cards_finished_this_turn >= DynamicVars()["CardPlay"].IntValue()) {
            extra(0) = 1;
            PowerCmd::Apply(PowerId::DRAW_CARDS_NEXT_TURN_POWER, ctx, Owner(), Dec(Amount()), Owner(), CardModel{});
        }
    }
    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner())) extra(0) = 0;
    }
};
}  // namespace sts2
