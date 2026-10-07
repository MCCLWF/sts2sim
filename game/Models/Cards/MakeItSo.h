#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {
struct MakeItSo : CardModel {
    SIM_MODEL(MakeItSo, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target).Execute(ctx);
    }
    void AfterCardPlayedLate(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner() || play->Card.Type() != CardType::Skill || Pile().Type() == PileType::Hand) return;
        if (sim_->g->combat.player_skill_cards_finished_this_turn % DynamicVars().Cards().IntValue() == 0)
            CardPileCmd::Add(*this, PileType::Hand);
    }
};
}  // namespace sts2
