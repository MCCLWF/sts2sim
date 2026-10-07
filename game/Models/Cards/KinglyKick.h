// Models/Cards/KinglyKick.cs
#pragma once

#include "game/Commands/DamageCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct KinglyKick : CardModel {
    SIM_MODEL(KinglyKick, CardModel)

    void OnPlay(PlayerChoiceContext& choiceContext, const CardPlay* cardPlay) const {
        if (!cardPlay || !cardPlay->Target) throw GameError("cardPlay.Target is null");
        DamageCmd::Attack(DynamicVars().Damage().BaseValue())
            .FromCard(*this, cardPlay)
            .Targeting(cardPlay->Target)
            .Execute(choiceContext);
    }
    void AfterCardDrawn(PlayerChoiceContext&, CardModel card, bool) const {
        if (card == *this) EnergyCost().AddThisCombat(-1);
    }
};

}  // namespace sts2
