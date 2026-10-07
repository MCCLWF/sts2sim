// Models/Powers/TenderPower.cs
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Entities/Cards/CardPlay.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct TenderPower : PowerModel {
    SIM_MODEL(TenderPower, PowerModel)

    int32_t CardsPlayedThisTurn() const { return extra(0); }
    void SetCardsPlayedThisTurn(int32_t value) const { extra(0) = value; }

    void AfterCardPlayed(PlayerChoiceContext& choiceContext, const CardPlay* cardPlay) const {
        if (cardPlay == nullptr || cardPlay->Card.Owner() != Owner().Player()) return;
        SetCardsPlayedThisTurn(CardsPlayedThisTurn() + 1);
        Flash();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, choiceContext, Owner(), Dec(-1), Applier(), CardModel{}, true);
        PowerCmd::Apply(PowerId::DEXTERITY_POWER, choiceContext, Owner(), Dec(-1), Applier(), CardModel{}, true);
    }

    void AfterSideTurnEnd(PlayerChoiceContext& choiceContext, CombatSide, const CreatureList& participants) const {
        if (!participants.contains(Owner())) return;
        PowerCmd::Apply(PowerId::STRENGTH_POWER, choiceContext, Owner(), Dec(CardsPlayedThisTurn()), Applier(), CardModel{}, true);
        PowerCmd::Apply(PowerId::DEXTERITY_POWER, choiceContext, Owner(), Dec(CardsPlayedThisTurn()), Applier(), CardModel{}, true);
        SetCardsPlayedThisTurn(0);
    }
};

}  // namespace sts2
