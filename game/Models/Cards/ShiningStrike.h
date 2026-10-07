// Models/Cards/ShiningStrike.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct ShiningStrike : CardModel {
    SIM_MODEL(ShiningStrike, CardModel)

    void OnPlay(PlayerChoiceContext& choiceContext, const CardPlay* cardPlay) const {
        if (!cardPlay || !cardPlay->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue())
            .FromCard(*this, cardPlay)
            .Targeting(cardPlay->Target)
            .Execute(choiceContext);
        PlayerCmd::GainStars(DynamicVars().Stars().BaseValue(), Owner());
    }
    CardLocation GetResultLocationForCardPlay() const {
        CardLocation location = GetResultLocationForCardPlay_Default();
        if (location.pileType == PileType::Discard) {
            location.pileType = PileType::Draw;
            location.position = CardPilePosition::Top;
        }
        return location;
    }
};

}  // namespace sts2
