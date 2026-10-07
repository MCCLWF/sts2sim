#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Pinpoint : CardModel {
    SIM_MODEL(Pinpoint, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("Pinpoint: cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play)
            .Targeting(play->Target).Execute(ctx);
    }

    void AfterCardEnteredCombat(CardModel card) const {
        if (card != *this || card.CloneOf()) return;
        EnergyCost().AddThisTurn(-CombatState().st().player_skill_cards_finished_this_turn);
    }

    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner() || play->Card.Type() != CardType::Skill) return;
        EnergyCost().AddThisTurn(-1);
    }
};

}  // namespace sts2
