#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Entities/Cards/CardPile.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Clash : CardModel {
    SIM_MODEL(Clash, CardModel)

    bool IsPlayable() const {
        for (CardModel card : CardPile::GetCards(Owner(), {PileType::Hand})) {
            if (card.Type() != CardType::Attack) return false;
        }
        return true;
    }

    bool ShouldGlowGoldInternal() const { return IsPlayable(); }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || !play->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue()).FromCard(*this, play).Targeting(play->Target)
            .Execute(ctx);
    }

};

}  // namespace sts2
